// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "capture_controller.h"

#include <comdef.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "com_heap_ptr.h"
#include "photo_handler.h"
#include "preview_handler.h"
#include "record_handler.h"
#include "string_utils.h"
#include "texture_handler.h"

namespace camera_windows {

using Microsoft::WRL::ComPtr;

bool FindBestMediaType(DWORD source_stream_index, IMFCaptureSource* source,
                       IMFMediaType** target_media_type, uint32_t max_height,
                       uint32_t max_width, uint32_t target_width,
                       uint32_t target_height, uint32_t* target_frame_width,
                       uint32_t* target_frame_height,
                       float minimum_accepted_framerate = 15.f,
                       float target_framerate = 0.f,
                       HRESULT* last_enumeration_hr = nullptr,
                       int* enumerated_type_count = nullptr);

namespace {

bool WriteBmp24File(const std::string& file_path, const uint8_t* bgr,
                    uint32_t width, uint32_t height) {
  if (!bgr || width == 0 || height == 0) {
    return false;
  }

  const uint32_t row_stride = ((width * 3 + 3) / 4) * 4;
  const uint32_t pixel_data_size = row_stride * height;
  const uint32_t file_size = 54 + pixel_data_size;

  std::vector<uint8_t> buffer(file_size, 0);
  buffer[0] = 'B';
  buffer[1] = 'M';
  auto write_u32 = [&buffer](size_t offset, uint32_t value) {
    buffer[offset] = static_cast<uint8_t>(value & 0xFF);
    buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  };
  write_u32(2, file_size);
  write_u32(10, 54);
  write_u32(14, 40);
  write_u32(18, width);
  write_u32(22, height);
  write_u32(26, 1);
  write_u32(28, 24);
  write_u32(34, pixel_data_size);

  uint8_t* dst = buffer.data() + 54;
  for (int32_t y = static_cast<int32_t>(height) - 1; y >= 0; --y) {
    const uint8_t* src_row = bgr + static_cast<size_t>(y) * width * 3;
    uint8_t* dst_row = dst + static_cast<size_t>(height - 1 - y) * row_stride;
    for (uint32_t x = 0; x < width; ++x) {
      const size_t src_idx = static_cast<size_t>(x) * 3;
      const size_t dst_idx = static_cast<size_t>(x) * 3;
      dst_row[dst_idx] = src_row[src_idx];
      dst_row[dst_idx + 1] = src_row[src_idx + 1];
      dst_row[dst_idx + 2] = src_row[src_idx + 2];
    }
  }

  std::ofstream out(file_path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(buffer.data()),
            static_cast<std::streamsize>(buffer.size()));
  return out.good();
}

}  // namespace

CameraResult GetCameraResult(HRESULT hr) {
  if (SUCCEEDED(hr)) {
    return CameraResult::kSuccess;
  }

  return hr == E_ACCESSDENIED ? CameraResult::kAccessDenied
                              : CameraResult::kError;
}

CaptureControllerImpl::CaptureControllerImpl(
    CaptureControllerListener* listener)
    : capture_controller_listener_(listener),
      media_settings_(
          PlatformMediaSettings(PlatformResolutionPreset::max, true)),
      CaptureController() {}

CaptureControllerImpl::~CaptureControllerImpl() {
  ResetCaptureController();
  capture_controller_listener_ = nullptr;
};

// static
bool CaptureControllerImpl::EnumerateVideoCaptureDeviceSources(
    IMFActivate*** devices, UINT32* count) {
  ComPtr<IMFAttributes> attributes;

  HRESULT hr = MFCreateAttributes(&attributes, 1);
  if (FAILED(hr)) {
    return false;
  }

  hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (FAILED(hr)) {
    return false;
  }

  hr = MFEnumDeviceSources(attributes.Get(), devices, count);
  if (FAILED(hr)) {
    return false;
  }

  return true;
}

HRESULT CaptureControllerImpl::CreateDefaultAudioCaptureSource() {
  audio_source_ = nullptr;
  ComHeapPtr<IMFActivate*> devices;
  UINT32 count = 0;

  ComPtr<IMFAttributes> attributes;
  HRESULT hr = MFCreateAttributes(&attributes, 1);

  if (SUCCEEDED(hr)) {
    hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                             MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
  }

  if (SUCCEEDED(hr)) {
    hr = MFEnumDeviceSources(attributes.Get(), &devices, &count);
  }

  if (SUCCEEDED(hr) && count > 0) {
    ComHeapPtr<wchar_t> audio_device_id;
    UINT32 audio_device_id_size;

    // Use first audio device.
    hr = devices[0]->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, &audio_device_id,
        &audio_device_id_size);

    if (SUCCEEDED(hr)) {
      ComPtr<IMFAttributes> audio_capture_source_attributes;
      hr = MFCreateAttributes(&audio_capture_source_attributes, 2);

      if (SUCCEEDED(hr)) {
        hr = audio_capture_source_attributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
      }

      if (SUCCEEDED(hr)) {
        hr = audio_capture_source_attributes->SetString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID,
            audio_device_id);
      }

      if (SUCCEEDED(hr)) {
        hr = MFCreateDeviceSource(audio_capture_source_attributes.Get(),
                                  audio_source_.GetAddressOf());
      }
    }
  }

  return hr;
}

HRESULT CaptureControllerImpl::CreateVideoCaptureSourceForDevice(
    const std::string& video_device_id) {
  video_source_ = nullptr;

  ComPtr<IMFAttributes> video_capture_source_attributes;

  HRESULT hr = MFCreateAttributes(&video_capture_source_attributes, 2);
  if (FAILED(hr)) {
    return hr;
  }

  hr = video_capture_source_attributes->SetGUID(
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (FAILED(hr)) {
    return hr;
  }

  hr = video_capture_source_attributes->SetString(
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
      Utf16FromUtf8(video_device_id).c_str());
  if (FAILED(hr)) {
    return hr;
  }

  hr = MFCreateDeviceSource(video_capture_source_attributes.Get(),
                            video_source_.GetAddressOf());
  return hr;
}

HRESULT CaptureControllerImpl::CreateD3DManagerWithDX11Device() {
  // TODO: Use existing ANGLE device

  HRESULT hr = S_OK;
  hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                         D3D11_CREATE_DEVICE_VIDEO_SUPPORT, nullptr, 0,
                         D3D11_SDK_VERSION, &dx11_device_, nullptr, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  // Enable multithread protection
  ComPtr<ID3D10Multithread> multi_thread;
  hr = dx11_device_.As(&multi_thread);
  if (FAILED(hr)) {
    return hr;
  }

  multi_thread->SetMultithreadProtected(TRUE);

  hr = MFCreateDXGIDeviceManager(&dx_device_reset_token_,
                                 dxgi_device_manager_.GetAddressOf());
  if (FAILED(hr)) {
    return hr;
  }

  hr = dxgi_device_manager_->ResetDevice(dx11_device_.Get(),
                                         dx_device_reset_token_);
  return hr;
}

HRESULT CaptureControllerImpl::CreateCaptureEngine() {
  assert(!video_device_id_.empty());

  HRESULT hr = S_OK;
  ComPtr<IMFAttributes> attributes;

  // Creates capture engine only if not already initialized by test framework
  if (!capture_engine_) {
    ComPtr<IMFCaptureEngineClassFactory> capture_engine_factory;

    hr = CoCreateInstance(CLSID_MFCaptureEngineClassFactory, nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&capture_engine_factory));
    if (FAILED(hr)) {
      return hr;
    }

    // Creates CaptureEngine.
    hr = capture_engine_factory->CreateInstance(CLSID_MFCaptureEngine,
                                                IID_PPV_ARGS(&capture_engine_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = CreateD3DManagerWithDX11Device();

  if (FAILED(hr)) {
    return hr;
  }

  // Creates video source only if not already initialized by test framework
  if (!video_source_) {
    hr = CreateVideoCaptureSourceForDevice(video_device_id_);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Creates audio source only if not already initialized by test framework
  if (media_settings_.enable_audio() && !audio_source_) {
    hr = CreateDefaultAudioCaptureSource();
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!capture_engine_callback_handler_) {
    capture_engine_callback_handler_ =
        ComPtr<CaptureEngineListener>(new CaptureEngineListener(this));
  }

  hr = MFCreateAttributes(&attributes, 2);
  if (FAILED(hr)) {
    return hr;
  }

  hr = attributes->SetUnknown(MF_CAPTURE_ENGINE_D3D_MANAGER,
                              dxgi_device_manager_.Get());
  if (FAILED(hr)) {
    return hr;
  }

  hr = attributes->SetUINT32(MF_CAPTURE_ENGINE_USE_VIDEO_DEVICE_ONLY,
                             !media_settings_.enable_audio());
  if (FAILED(hr)) {
    return hr;
  }

  // Check MF_CAPTURE_ENGINE_INITIALIZED event handling
  // for response process.
  hr = capture_engine_->Initialize(capture_engine_callback_handler_.Get(),
                                   attributes.Get(), audio_source_.Get(),
                                   video_source_.Get());
  return hr;
}

void CaptureControllerImpl::ResetCaptureController() {
  if (record_handler_ && record_handler_->CanStop()) {
    StopRecord();
  }

  if (preview_handler_) {
    StopPreview();
  }

  // Shuts down the media foundation platform object.
  // Releases all resources including threads.
  // Application should call MFShutdown the same number of times as MFStartup
  if (media_foundation_started_) {
    MFShutdown();
  }

  // States
  media_foundation_started_ = false;
  capture_engine_state_ = CaptureEngineState::kNotInitialized;
  preview_frame_width_ = 0;
  preview_frame_height_ = 0;
  capture_engine_callback_handler_ = nullptr;
  capture_engine_ = nullptr;
  audio_source_ = nullptr;
  video_source_ = nullptr;
  base_preview_media_type_ = nullptr;
  base_capture_media_type_ = nullptr;

  if (dxgi_device_manager_) {
    dxgi_device_manager_->ResetDevice(dx11_device_.Get(),
                                      dx_device_reset_token_);
  }
  dxgi_device_manager_ = nullptr;
  dx11_device_ = nullptr;

  record_handler_ = nullptr;
  preview_handler_ = nullptr;
  photo_handler_ = nullptr;
  texture_handler_ = nullptr;
}

bool CaptureControllerImpl::InitCaptureDevice(
    flutter::TextureRegistrar* texture_registrar, const std::string& device_id,
    const PlatformMediaSettings& media_settings) {
  assert(capture_controller_listener_);

  if (IsInitialized()) {
    capture_controller_listener_->OnCreateCaptureEngineFailed(
        CameraResult::kError, "Capture device already initialized");
    return false;
  } else if (capture_engine_state_ == CaptureEngineState::kInitializing) {
    capture_controller_listener_->OnCreateCaptureEngineFailed(
        CameraResult::kError, "Capture device already initializing");
    return false;
  }

  capture_engine_state_ = CaptureEngineState::kInitializing;
  media_settings_ = media_settings;
  texture_registrar_ = texture_registrar;
  video_device_id_ = device_id;

  // MFStartup must be called before using Media Foundation.
  if (!media_foundation_started_) {
    HRESULT hr = MFStartup(MF_VERSION);

    if (FAILED(hr)) {
      capture_controller_listener_->OnCreateCaptureEngineFailed(
          GetCameraResult(hr), "Failed to create camera");
      ResetCaptureController();
      return false;
    }

    media_foundation_started_ = true;
  }

  HRESULT hr = CreateCaptureEngine();
  if (FAILED(hr)) {
    capture_controller_listener_->OnCreateCaptureEngineFailed(
        GetCameraResult(hr), "Failed to create camera");
    ResetCaptureController();
    return false;
  }

  return true;
}

void CaptureControllerImpl::SetMirrorPreviewState(bool mirror) {
  // 仅使用 TextureHandler 软件镜像，不调用 IMFCaptureSource::SetMirrorState，
  // 避免部分设备/驱动在 GetSource 或 SetMirrorState 时崩溃。
  if (texture_handler_) {
    texture_handler_->SetMirrorPreviewState(mirror);
  }
}

void CaptureControllerImpl::TakePicture(const std::string& file_path) {
  assert(capture_engine_callback_handler_);
  assert(capture_engine_);

  if (!IsInitialized()) {
    return OnPicture(CameraResult::kError, "Not initialized");
  }

  HRESULT hr = S_OK;

  if (!base_capture_media_type_) {
    // Enumerates mediatypes and finds media type for video capture.
    hr = FindBaseMediaTypes();
    if (FAILED(hr)) {
      return OnPicture(GetCameraResult(hr),
                       "Failed to initialize photo capture");
    }
  }

  if (!photo_handler_) {
    photo_handler_ = std::make_unique<PhotoHandler>();
  } else if (photo_handler_->IsTakingPhoto()) {
    return OnPicture(CameraResult::kError, "Photo already requested");
  }

  // Check MF_CAPTURE_ENGINE_PHOTO_TAKEN event handling
  // for response process.
  hr = photo_handler_->TakePhoto(file_path, capture_engine_.Get(),
                                 base_capture_media_type_.Get());
  if (FAILED(hr)) {
    // Destroy photo handler on error cases to make sure state is resetted.
    photo_handler_ = nullptr;
    return OnPicture(GetCameraResult(hr), "Failed to take photo");
  }
}

bool CaptureControllerImpl::CapturePreviewFrame(const std::string& file_path) {
  if (!IsInitialized() || !texture_handler_) {
    return false;
  }

  std::vector<uint8_t> bgr;
  uint32_t width = 0;
  uint32_t height = 0;
  if (!texture_handler_->CopyPreviewFrameBgr(&bgr, &width, &height)) {
    return false;
  }

  return WriteBmp24File(file_path, bgr.data(), width, height);
}

uint32_t CaptureControllerImpl::GetMaxPreviewHeight() const {
  switch (media_settings_.resolution_preset()) {
    case PlatformResolutionPreset::low:
      return 240;
    case PlatformResolutionPreset::medium:
      return 480;  // 支持640×480
    case PlatformResolutionPreset::high:
      return 720;
    case PlatformResolutionPreset::veryHigh:
      return 1080;
    case PlatformResolutionPreset::ultraHigh:
      return 2160;
    case PlatformResolutionPreset::max:
    default:
      // no limit.
      return 0xffffffff;
  }
}

void CaptureControllerImpl::GetCaptureSizeLimits(
    uint32_t* max_width, uint32_t* max_height, uint32_t* target_width,
    uint32_t* target_height) const {
  assert(max_width && max_height && target_width && target_height);
  *target_width = 0;
  *target_height = 0;

  if (media_settings_.video_width() && media_settings_.video_height() &&
      *media_settings_.video_width() > 0 &&
      *media_settings_.video_height() > 0) {
    *target_width = static_cast<uint32_t>(*media_settings_.video_width());
    *target_height = static_cast<uint32_t>(*media_settings_.video_height());
    *max_width = *target_width;
    *max_height = *target_height;
    return;
  }

  *max_width = 0xffffffffu;
  *max_height = GetMaxPreviewHeight();
}

bool CaptureControllerImpl::HasExplicitVideoSize() const {
  return media_settings_.video_width() && media_settings_.video_height() &&
         *media_settings_.video_width() > 0 &&
         *media_settings_.video_height() > 0;
}

HRESULT CaptureControllerImpl::AlignCaptureMediaTypeForRecording(
    IMFCaptureSource* source) {
  if (!HasExplicitVideoSize() || !source) {
    return S_OK;
  }

  const uint32_t target_w =
      static_cast<uint32_t>(*media_settings_.video_width());
  const uint32_t target_h =
      static_cast<uint32_t>(*media_settings_.video_height());

  auto type_matches = [&](IMFMediaType* type) -> bool {
    if (!type) {
      return false;
    }
    uint32_t w = 0;
    uint32_t h = 0;
    return SUCCEEDED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h)) &&
           w == target_w && h == target_h;
  };

  if (type_matches(base_capture_media_type_.Get())) {
    return S_OK;
  }

  if (type_matches(base_preview_media_type_.Get())) {
    ComPtr<IMFMediaType> copy;
    HRESULT hr = MFCreateMediaType(&copy);
    if (FAILED(hr)) {
      return hr;
    }
    hr = base_preview_media_type_->CopyAllItems(copy.Get());
    if (FAILED(hr)) {
      return hr;
    }
    base_capture_media_type_ = std::move(copy);
    return S_OK;
  }

  uint32_t max_width = 0;
  uint32_t max_height = 0;
  uint32_t limit_target_w = 0;
  uint32_t limit_target_h = 0;
  GetCaptureSizeLimits(&max_width, &max_height, &limit_target_w,
                       &limit_target_h);

  const DWORD stream_indices[] = {
      (DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_RECORD,
      (DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW,
      0};

  for (DWORD stream_index : stream_indices) {
    ComPtr<IMFMediaType> candidate;
    uint32_t frame_w = 0;
    uint32_t frame_h = 0;
    if (FindBestMediaType(stream_index, source, candidate.GetAddressOf(),
                          max_height, max_width, limit_target_w,
                          limit_target_h, &frame_w, &frame_h) &&
        frame_w == target_w && frame_h == target_h) {
      base_capture_media_type_ = std::move(candidate);
      return S_OK;
    }
  }

  return S_OK;
}

namespace {
std::string HResultToString(HRESULT hr) {
  _com_error err(hr);
  char hex_buf[32] = {0};
  std::snprintf(hex_buf, sizeof(hex_buf), "0x%08lX",
                static_cast<unsigned long>(hr));
  std::string msg = hex_buf;
  msg.append(" (");
  msg.append(Utf8FromUtf16(std::wstring(err.ErrorMessage())));
  msg.append(")");
  return msg;
}

void DebugLog(const std::string& msg) {
  std::string with_newline = msg;
  with_newline.append("\n");
  OutputDebugStringA(with_newline.c_str());
  std::fprintf(stderr, "%s", with_newline.c_str());
  std::fflush(stderr);
}
}  // namespace

HRESULT CaptureControllerImpl::ApplyRecordingDeviceFormat(
    IMFCaptureSource* source, IMFMediaType* media_type) {
  if (!source || !media_type) {
    return E_INVALIDARG;
  }

  const DWORD stream_indices[] = {
      (DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_RECORD,
      0};

  for (DWORD stream_index : stream_indices) {
    HRESULT hr = source->SetCurrentDeviceMediaType(stream_index, media_type);
    if (SUCCEEDED(hr)) {
      DebugLog("ApplyRecordingDeviceFormat: stream " +
               std::to_string(stream_index) + " set");
      return hr;
    }
    DebugLog("ApplyRecordingDeviceFormat: stream " +
             std::to_string(stream_index) +
             " failed. hr=" + HResultToString(hr));
  }
  return E_FAIL;
}

namespace {
bool EndsWithInsensitive(const std::string& value, const std::string& suffix) {
  if (suffix.size() > value.size()) return false;
  const size_t offset = value.size() - suffix.size();
  for (size_t i = 0; i < suffix.size(); i++) {
    const char a = static_cast<char>(std::tolower(
        static_cast<unsigned char>(value[offset + i])));
    const char b = static_cast<char>(
        std::tolower(static_cast<unsigned char>(suffix[i])));
    if (a != b) return false;
  }
  return true;
}
}  // namespace

// Finds best media type for given source stream index and size limits.
bool FindBestMediaType(DWORD source_stream_index, IMFCaptureSource* source,
                       IMFMediaType** target_media_type, uint32_t max_height,
                       uint32_t max_width, uint32_t target_width,
                       uint32_t target_height, uint32_t* target_frame_width,
                       uint32_t* target_frame_height,
                       float minimum_accepted_framerate,
                       float target_framerate,
                       HRESULT* last_enumeration_hr,
                       int* enumerated_type_count) {
  assert(source);
  ComPtr<IMFMediaType> media_type;

  uint32_t best_width = 0;
  uint32_t best_height = 0;
  float best_framerate = 0.f;
  int64_t best_score = INT64_MIN;
  HRESULT last_hr = S_OK;
  int type_count = 0;

  ComPtr<IMFMediaType> closest_media_type;
  uint32_t closest_width = 0;
  uint32_t closest_height = 0;
  int64_t closest_score = INT64_MIN;

  // Loop native media types.
  for (int i = 0;; i++) {
    last_hr = source->GetAvailableDeviceMediaType(source_stream_index, i,
                                                  media_type.GetAddressOf());
    if (FAILED(last_hr)) {
      break;
    }
    type_count++;

    uint32_t frame_rate_numerator, frame_rate_denominator;
    if (FAILED(MFGetAttributeRatio(media_type.Get(), MF_MT_FRAME_RATE,
                                   &frame_rate_numerator,
                                   &frame_rate_denominator)) ||
        !frame_rate_denominator) {
      continue;
    }

    float frame_rate =
        static_cast<float>(frame_rate_numerator) / frame_rate_denominator;
    if (frame_rate < minimum_accepted_framerate) {
      continue;
    }

    uint32_t frame_width;
    uint32_t frame_height;
    if (FAILED(MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE,
                                  &frame_width, &frame_height))) {
      continue;
    }

    const bool in_bounds =
        frame_height <= max_height && frame_width <= max_width;

    if (!in_bounds && target_width > 0 && target_height > 0) {
      const int64_t dw =
          llabs(static_cast<int64_t>(frame_width) -
                static_cast<int64_t>(target_width));
      const int64_t dh =
          llabs(static_cast<int64_t>(frame_height) -
                static_cast<int64_t>(target_height));
      const int64_t score = 500000000LL - dw * 1000000LL - dh * 1000000LL;
      if (score > closest_score) {
        closest_score = score;
        media_type.CopyTo(closest_media_type.GetAddressOf());
        closest_width = frame_width;
        closest_height = frame_height;
      }
    }

    if (!in_bounds) {
      continue;
    }

    int64_t score = 0;
    if (target_width > 0 && target_height > 0) {
      if (frame_width == target_width && frame_height == target_height) {
        score = 1000000000LL;
      } else {
        const int64_t dw =
            llabs(static_cast<int64_t>(frame_width) -
                  static_cast<int64_t>(target_width));
        const int64_t dh =
            llabs(static_cast<int64_t>(frame_height) -
                  static_cast<int64_t>(target_height));
        score = 500000000LL - dw * 1000000LL - dh * 1000000LL;
      }
    } else {
      score = static_cast<int64_t>(frame_width) * frame_height;
    }

    if (target_framerate > 0.f &&
        std::fabs(frame_rate - target_framerate) < 1.0f) {
      score += 10000000LL;
    }
    score += static_cast<int64_t>(frame_rate * 1000.f);

    if (score > best_score) {
      best_score = score;
      media_type.CopyTo(target_media_type);
      best_width = frame_width;
      best_height = frame_height;
      best_framerate = frame_rate;
    }
  }

  if (*target_media_type == nullptr && closest_media_type) {
    closest_media_type.CopyTo(target_media_type);
    best_width = closest_width;
    best_height = closest_height;
  }

  if (target_frame_width && target_frame_height) {
    *target_frame_width = best_width;
    *target_frame_height = best_height;
  }

  if (last_enumeration_hr) {
    *last_enumeration_hr = last_hr;
  }
  if (enumerated_type_count) {
    *enumerated_type_count = type_count;
  }
  return *target_media_type != nullptr;
}

HRESULT CaptureControllerImpl::FindBaseMediaTypes() {
  if (!IsInitialized()) {
    return E_FAIL;
  }

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = capture_engine_->GetSource(&source);
  if (FAILED(hr)) {
    return hr;
  }

  return FindBaseMediaTypesForSource(source.Get());
}

HRESULT CaptureControllerImpl::FindBaseMediaTypesForSource(
    IMFCaptureSource* source) {
  // 获取目标帧速率
  float target_fps = 30.0f; // 默认30fps
  if (media_settings_.frames_per_second() && *media_settings_.frames_per_second() > 0) {
    target_fps = static_cast<float>(*media_settings_.frames_per_second());
  }

  uint32_t max_width = 0;
  uint32_t max_height = 0;
  uint32_t target_width = 0;
  uint32_t target_height = 0;
  GetCaptureSizeLimits(&max_width, &max_height, &target_width, &target_height);

  // Find base media type for previewing.
  const DWORD kPreferredPreviewStream =
      (DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW;
  HRESULT preview_enum_hr = S_OK;
  int preview_enum_count = 0;
  if (!FindBestMediaType(kPreferredPreviewStream, source,
                         base_preview_media_type_.GetAddressOf(), max_height,
                         max_width, target_width, target_height,
                         &preview_frame_width_, &preview_frame_height_, 15.0f,
                         target_fps, &preview_enum_hr, &preview_enum_count)) {
    // 某些设备/驱动对 “preferred stream” 常量不支持，回退到 stream 0。
    DebugLog("FindBestMediaType(preview) failed. preferred_stream=" +
             std::to_string(kPreferredPreviewStream) +
             ", enumerated_types=" + std::to_string(preview_enum_count) +
             ", last_hr=" + HResultToString(preview_enum_hr));
    preview_enum_hr = S_OK;
    preview_enum_count = 0;
    if (!FindBestMediaType(0, source, base_preview_media_type_.GetAddressOf(),
                           max_height, max_width, target_width, target_height,
                           &preview_frame_width_, &preview_frame_height_, 15.0f,
                           target_fps, &preview_enum_hr, &preview_enum_count)) {
      DebugLog("FindBestMediaType(preview) failed on fallback stream=0. "
               "enumerated_types=" +
               std::to_string(preview_enum_count) +
               ", last_hr=" + HResultToString(preview_enum_hr));
      return FAILED(preview_enum_hr) ? preview_enum_hr : E_FAIL;
    }
  }

  // Find base media type for record and photo capture.
  const DWORD kPreferredRecordStream =
      (DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_RECORD;
  HRESULT record_enum_hr = S_OK;
  int record_enum_count = 0;
  if (!FindBestMediaType(kPreferredRecordStream, source,
                         base_capture_media_type_.GetAddressOf(), max_height,
                         max_width, target_width, target_height, nullptr,
                         nullptr, 15.0f, target_fps, &record_enum_hr,
                         &record_enum_count)) {
    DebugLog("FindBestMediaType(record) failed. preferred_stream=" +
             std::to_string(kPreferredRecordStream) +
             ", enumerated_types=" + std::to_string(record_enum_count) +
             ", last_hr=" + HResultToString(record_enum_hr));
    record_enum_hr = S_OK;
    record_enum_count = 0;
    if (!FindBestMediaType(0, source, base_capture_media_type_.GetAddressOf(),
                           max_height, max_width, target_width, target_height,
                           nullptr, nullptr, 15.0f, target_fps, &record_enum_hr,
                           &record_enum_count)) {
      DebugLog("FindBestMediaType(record) failed on fallback stream=0. "
               "enumerated_types=" +
               std::to_string(record_enum_count) +
               ", last_hr=" + HResultToString(record_enum_hr));
      return FAILED(record_enum_hr) ? record_enum_hr : E_FAIL;
    }
  }

  return S_OK;
}

void CaptureControllerImpl::StartRecord(const std::string& file_path) {
  assert(capture_engine_);

  if (!IsInitialized()) {
    return OnRecordStarted(CameraResult::kError,
                           "Camera not initialized. Camera should be "
                           "disposed and reinitialized.");
  }

  DebugLog("StartRecord: enter. path=" + file_path);

  // Uncompressed raw recording mode:
  // If file extension is .bgra or .raw, write BGRA32 frames from preview sample
  // callback directly to the output file (sequential frames). A sidecar JSON
  // file with metadata is created at "<file_path>.json".
  const bool wants_raw = EndsWithInsensitive(file_path, ".bgra") ||
                         EndsWithInsensitive(file_path, ".raw");
  if (wants_raw) {
    if (raw_recording_enabled_) {
      return OnRecordStarted(CameraResult::kError,
                             "Raw recording already in progress.");
    }
    if (!preview_handler_ || !preview_handler_->IsRunning()) {
      return OnRecordStarted(CameraResult::kError,
                             "Preview must be running for raw recording.");
    }

    raw_record_file_path_ = file_path;
    raw_record_stream_.open(raw_record_file_path_,
                            std::ios::binary | std::ios::out | std::ios::trunc);
    if (!raw_record_stream_.is_open()) {
      raw_record_file_path_.clear();
      return OnRecordStarted(CameraResult::kError,
                             "Failed to open raw output file for writing.");
    }

    // Write metadata file. Stride can be derived by external encoder from the
    // actual frame byte size; preview is configured as RGB32/BGRA32.
    {
      std::ofstream meta(raw_record_file_path_ + ".json",
                         std::ios::out | std::ios::trunc);
      if (meta.is_open()) {
        const int64_t fps = media_settings_.frames_per_second()
                                ? *media_settings_.frames_per_second()
                                : 0;
        meta << "{\n"
             << "  \"pixel_format\": \"BGRA32\",\n"
             << "  \"width\": " << preview_frame_width_ << ",\n"
             << "  \"height\": " << preview_frame_height_ << ",\n"
             << "  \"fps\": " << fps << ",\n"
             << "  \"container\": \"raw_frames\",\n"
             << "  \"notes\": \"Frames are written sequentially; each frame is "
                "data_length bytes from preview callback.\"\n"
             << "}\n";
      }
    }

    raw_recording_enabled_ = true;
    raw_recording_started_ = true;
    DebugLog("StartRecord: raw recording started (BGRA32).");
    return OnRecordStarted(CameraResult::kSuccess, "");
  }

  if (!base_preview_media_type_ && !base_capture_media_type_) {
    HRESULT hr = FindBaseMediaTypes();
    if (FAILED(hr)) {
      DebugLog("StartRecord: FindBaseMediaTypes failed. hr=" + HResultToString(hr));
      return OnRecordStarted(GetCameraResult(hr),
                             "Failed to initialize video recording");
    }
  }

  // Windows 上不再用预览帧管线录像；始终用引擎录像，录像结束后由 Flutter 用本地 FFmpeg 做 hflip 转码。
  if (!base_capture_media_type_) {
    HRESULT hr = FindBaseMediaTypes();
    if (FAILED(hr)) {
      return OnRecordStarted(GetCameraResult(hr),
                             "Failed to initialize video recording");
    }
  }

  if (!record_handler_) {
    record_handler_ = std::make_unique<RecordHandler>(media_settings_);
  } else if (!record_handler_->CanStart()) {
    return OnRecordStarted(
        CameraResult::kError,
        "Recording cannot be started. Previous recording must be stopped "
        "first.");
  }

  HRESULT hr = S_OK;
  ComPtr<IMFCaptureSource> source;
  hr = capture_engine_->GetSource(&source);
  if (FAILED(hr)) {
    DebugLog("StartRecord: GetSource failed. hr=" + HResultToString(hr));
    return OnRecordStarted(GetCameraResult(hr),
                           "Failed to get capture engine source");
  }

  hr = AlignCaptureMediaTypeForRecording(source.Get());
  if (FAILED(hr)) {
    DebugLog("StartRecord: AlignCaptureMediaTypeForRecording failed. hr=" +
             HResultToString(hr));
  }

  IMFMediaType* record_media_type = base_capture_media_type_.Get();
  if (HasExplicitVideoSize() && base_preview_media_type_) {
    uint32_t preview_w = 0;
    uint32_t preview_h = 0;
    if (SUCCEEDED(MFGetAttributeSize(base_preview_media_type_.Get(),
                                   MF_MT_FRAME_SIZE, &preview_w, &preview_h)) &&
        preview_w ==
            static_cast<uint32_t>(*media_settings_.video_width()) &&
        preview_h ==
            static_cast<uint32_t>(*media_settings_.video_height())) {
      record_media_type = base_preview_media_type_.Get();
    }
  }

  hr = ApplyRecordingDeviceFormat(source.Get(), record_media_type);
  if (FAILED(hr)) {
    DebugLog("StartRecord: ApplyRecordingDeviceFormat failed. hr=" +
             HResultToString(hr));
  }

  uint32_t record_w = 0;
  uint32_t record_h = 0;
  if (SUCCEEDED(MFGetAttributeSize(record_media_type, MF_MT_FRAME_SIZE,
                                   &record_w, &record_h))) {
    DebugLog("StartRecord: recording with media type " +
             std::to_string(record_w) + "x" + std::to_string(record_h));
  }

  hr = record_handler_->StartRecord(file_path, capture_engine_.Get(),
                                      record_media_type);
  if (FAILED(hr)) {
    DebugLog("StartRecord: RecordHandler::StartRecord failed. hr=" +
             HResultToString(hr));
    record_handler_ = nullptr;
    return OnRecordStarted(GetCameraResult(hr),
                           "Failed to start video recording. hr=" +
                               HResultToString(hr));
  }
}

void CaptureControllerImpl::StopRecord() {
  assert(capture_controller_listener_);

  if (!IsInitialized()) {
    return OnRecordStopped(CameraResult::kError,
                           "Camera not initialized. Camera should be "
                           "disposed and reinitialized.");
  }

  if (raw_recording_enabled_) {
    if (raw_record_stream_.is_open()) {
      raw_record_stream_.flush();
      raw_record_stream_.close();
    }
    const std::string path = raw_record_file_path_;
    raw_record_file_path_.clear();
    raw_recording_enabled_ = false;
    raw_recording_started_ = false;
    DebugLog("StopRecord: raw recording stopped. path=" + path);
    return OnRecordStopped(CameraResult::kSuccess, "", path);
  }

  if (!record_handler_ || !record_handler_->CanStop()) {
    return OnRecordStopped(CameraResult::kError,
                           "Recording cannot be stopped.");
  }

  HRESULT hr = record_handler_->StopRecord(capture_engine_.Get());
  if (FAILED(hr)) {
    return OnRecordStopped(GetCameraResult(hr),
                           "Failed to stop video recording");
  }
}

// Starts capturing preview frames using preview handler
// After first frame is captured, OnPreviewStarted is called
void CaptureControllerImpl::StartPreview() {
  assert(capture_engine_callback_handler_);
  assert(capture_engine_);
  assert(texture_handler_);

  DebugLog("StartPreview: enter");
  if (!IsInitialized() || !texture_handler_) {
    DebugLog("StartPreview: not initialized or missing texture handler");
    return OnPreviewStarted(CameraResult::kError,
                            "Camera not initialized. Camera should be "
                            "disposed and reinitialized.");
  }

  HRESULT hr = S_OK;

  ComPtr<IMFCaptureSource> source;
  hr = capture_engine_->GetSource(&source);
  if (FAILED(hr)) {
    DebugLog("StartPreview: GetSource failed. hr=" + HResultToString(hr));
    return OnPreviewStarted(GetCameraResult(hr),
                            "Failed to get capture engine source. hr=" +
                                HResultToString(hr));
  }

  if (!base_preview_media_type_) {
    // Enumerates mediatypes and finds media type for video capture.
    DebugLog("StartPreview: base_preview_media_type_ not set; enumerating media types...");
    hr = FindBaseMediaTypesForSource(source.Get());
    if (FAILED(hr)) {
      DebugLog("StartPreview: FindBaseMediaTypesForSource failed. hr=" +
               HResultToString(hr));
      return OnPreviewStarted(GetCameraResult(hr),
                              "Failed to initialize video preview. hr=" +
                                  HResultToString(hr));
    }
  }

  const DWORD kPreferredPreviewStream =
      (DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW;
  hr = source->SetCurrentDeviceMediaType(kPreferredPreviewStream,
                                        base_preview_media_type_.Get());
  if (FAILED(hr)) {
    DebugLog("StartPreview: SetCurrentDeviceMediaType(preview) failed. "
             "preferred_stream=" +
             std::to_string(kPreferredPreviewStream) +
             ", hr=" + HResultToString(hr) + ". Falling back to stream=0.");
    hr = source->SetCurrentDeviceMediaType(0, base_preview_media_type_.Get());
    if (FAILED(hr)) {
      DebugLog("StartPreview: SetCurrentDeviceMediaType(preview) failed on "
               "fallback stream=0. hr=" +
               HResultToString(hr));
      return OnPreviewStarted(GetCameraResult(hr),
                              "Failed to set video preview output format. hr=" +
                                  HResultToString(hr));
    }
  }

  texture_handler_->UpdateTextureSize(preview_frame_width_,
                                      preview_frame_height_);
  DebugLog("StartPreview: selected preview size=" +
           std::to_string(preview_frame_width_) + "x" +
           std::to_string(preview_frame_height_));

  // TODO(loic-sharma): This does not handle duplicate calls properly.
  // See: https://github.com/flutter/flutter/issues/108404
  if (!preview_handler_) {
    preview_handler_ = std::make_unique<PreviewHandler>();
  } else if (preview_handler_->IsInitialized()) {
    DebugLog("StartPreview: preview already initialized; returning success");
    return OnPreviewStarted(CameraResult::kSuccess, "");
  } else {
    DebugLog("StartPreview: preview handler exists but not initialized");
    return OnPreviewStarted(CameraResult::kError, "Preview already exists");
  }

  // Check MF_CAPTURE_ENGINE_PREVIEW_STARTED event handling for response
  // process.
  hr = preview_handler_->StartPreview(capture_engine_.Get(),
                                      base_preview_media_type_.Get(),
                                      capture_engine_callback_handler_.Get());

  if (FAILED(hr)) {
    // Destroy preview handler on error cases to make sure state is resetted.
    preview_handler_ = nullptr;
    DebugLog("StartPreview: PreviewHandler::StartPreview failed. hr=" +
             HResultToString(hr));
    return OnPreviewStarted(GetCameraResult(hr),
                            "Failed to start video preview. hr=" +
                                HResultToString(hr));
  }
  DebugLog("StartPreview: StartPreview requested; waiting for first frame/event");
}

// Stops preview. Called by destructor
// Use PausePreview and ResumePreview methods to for
// pausing and resuming the preview.
// Check MF_CAPTURE_ENGINE_PREVIEW_STOPPED event handling for response
// process.
HRESULT CaptureControllerImpl::StopPreview() {
  assert(capture_engine_);

  if (!IsInitialized() || !preview_handler_) {
    return S_OK;
  }

  // Requests to stop preview.
  return preview_handler_->StopPreview(capture_engine_.Get());
}

// Marks preview as paused.
// When preview is paused, captured frames are not processed for preview
// and flutter texture is not updated
void CaptureControllerImpl::PausePreview() {
  assert(capture_controller_listener_);

  if (!preview_handler_ || !preview_handler_->IsInitialized()) {
    return capture_controller_listener_->OnPausePreviewFailed(
        CameraResult::kError, "Preview not started");
  }

  if (preview_handler_->PausePreview()) {
    capture_controller_listener_->OnPausePreviewSucceeded();
  } else {
    capture_controller_listener_->OnPausePreviewFailed(
        CameraResult::kError, "Failed to pause preview");
  }
}

// Marks preview as not paused.
// When preview is not paused, captured frames are processed for preview
// and flutter texture is updated.
void CaptureControllerImpl::ResumePreview() {
  assert(capture_controller_listener_);

  if (!preview_handler_ || !preview_handler_->IsInitialized()) {
    return capture_controller_listener_->OnResumePreviewFailed(
        CameraResult::kError, "Preview not started");
  }

  if (preview_handler_->ResumePreview()) {
    capture_controller_listener_->OnResumePreviewSucceeded();
  } else {
    capture_controller_listener_->OnResumePreviewFailed(
        CameraResult::kError, "Failed to pause preview");
  }
}

// Handles capture engine events.
// Called via IMFCaptureEngineOnEventCallback implementation.
// Implements CaptureEngineObserver::OnEvent.
void CaptureControllerImpl::OnEvent(IMFMediaEvent* event) {
  if (!IsInitialized() &&
      capture_engine_state_ != CaptureEngineState::kInitializing) {
    return;
  }

  GUID extended_type_guid;
  if (SUCCEEDED(event->GetExtendedType(&extended_type_guid))) {
    std::string error;

    HRESULT event_hr;
    if (FAILED(event->GetStatus(&event_hr))) {
      return;
    }

    if (FAILED(event_hr)) {
      // Reads system error
      _com_error err(event_hr);
      error = Utf8FromUtf16(err.ErrorMessage());
    }

    CameraResult event_result = GetCameraResult(event_hr);
    if (extended_type_guid == MF_CAPTURE_ENGINE_ERROR) {
      OnCaptureEngineError(event_result, error);
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_INITIALIZED) {
      OnCaptureEngineInitialized(event_result, error);
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_PREVIEW_STARTED) {
      // Preview is marked as started after first frame is captured.
      // This is because, CaptureEngine might inform that preview is started
      // even if error is thrown right after.
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_PREVIEW_STOPPED) {
      OnPreviewStopped(event_result, error);
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_RECORD_STARTED) {
      OnRecordStarted(event_result, error);
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_RECORD_STOPPED) {
      OnRecordStopped(event_result, error);
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_PHOTO_TAKEN) {
      OnPicture(event_result, error);
    } else if (extended_type_guid == MF_CAPTURE_ENGINE_CAMERA_STREAM_BLOCKED) {
      // TODO: Inform capture state to flutter.
    } else if (extended_type_guid ==
               MF_CAPTURE_ENGINE_CAMERA_STREAM_UNBLOCKED) {
      // TODO: Inform capture state to flutter.
    }
  }
}

// Handles Picture event and informs CaptureControllerListener.
void CaptureControllerImpl::OnPicture(CameraResult result,
                                      const std::string& error) {
  if (result == CameraResult::kSuccess && photo_handler_) {
    if (capture_controller_listener_) {
      std::string path = photo_handler_->GetPhotoPath();
      capture_controller_listener_->OnTakePictureSucceeded(path);
    }
    photo_handler_->OnPhotoTaken();
  } else {
    if (capture_controller_listener_) {
      capture_controller_listener_->OnTakePictureFailed(result, error);
    }
    // Destroy photo handler on error cases to make sure state is resetted.
    photo_handler_ = nullptr;
  }
}

// Handles CaptureEngineInitialized event and informs
// CaptureControllerListener.
void CaptureControllerImpl::OnCaptureEngineInitialized(
    CameraResult result, const std::string& error) {
  if (capture_controller_listener_) {
    if (result != CameraResult::kSuccess) {
      capture_controller_listener_->OnCreateCaptureEngineFailed(
          result, "Failed to initialize capture engine");
      ResetCaptureController();
      return;
    }

    // Create texture handler and register new texture.
    texture_handler_ = std::make_unique<TextureHandler>(texture_registrar_);
    // 默认镜像 true；采集源镜像仅在 SetMirrorPreviewState 被调用时设置，避免初始化时 GetSource/SetMirrorState 导致崩溃
    texture_handler_->SetMirrorPreviewState(true);

    int64_t texture_id = texture_handler_->RegisterTexture();
    if (texture_id >= 0) {
      capture_controller_listener_->OnCreateCaptureEngineSucceeded(texture_id);
      capture_engine_state_ = CaptureEngineState::kInitialized;
    } else {
      capture_controller_listener_->OnCreateCaptureEngineFailed(
          CameraResult::kError, "Failed to create texture_id");
      // Reset state
      ResetCaptureController();
    }
  }
}

// Handles CaptureEngineError event and informs CaptureControllerListener.
void CaptureControllerImpl::OnCaptureEngineError(CameraResult result,
                                                 const std::string& error) {
  if (capture_controller_listener_) {
    capture_controller_listener_->OnCaptureError(result, error);
  }

  // TODO: If MF_CAPTURE_ENGINE_ERROR is returned,
  // should capture controller be reinitialized automatically?
}

// Handles PreviewStarted event and informs CaptureControllerListener.
// This should be called only after first frame has been received or
// in error cases.
void CaptureControllerImpl::OnPreviewStarted(CameraResult result,
                                             const std::string& error) {
  if (result == CameraResult::kSuccess) {
    DebugLog("OnPreviewStarted: success");
  } else {
    DebugLog("OnPreviewStarted: failed. error=" + error);
  }
  if (preview_handler_ && result == CameraResult::kSuccess) {
    preview_handler_->OnPreviewStarted();
  } else {
    // Destroy preview handler on error cases to make sure state is resetted.
    preview_handler_ = nullptr;
  }

  if (capture_controller_listener_) {
    if (result == CameraResult::kSuccess && preview_frame_width_ > 0 &&
        preview_frame_height_ > 0) {
      capture_controller_listener_->OnStartPreviewSucceeded(
          preview_frame_width_, preview_frame_height_);
    } else {
      capture_controller_listener_->OnStartPreviewFailed(result, error);
    }
  }
};

// Handles PreviewStopped event.
void CaptureControllerImpl::OnPreviewStopped(CameraResult result,
                                             const std::string& error) {
  // Preview handler is destroyed if preview is stopped as it
  // does not have any use anymore.
  preview_handler_ = nullptr;
};

// Handles RecordStarted event and informs CaptureControllerListener.
void CaptureControllerImpl::OnRecordStarted(CameraResult result,
                                            const std::string& error) {
  if (result == CameraResult::kSuccess && record_handler_) {
    record_handler_->OnRecordStarted();
    if (capture_controller_listener_) {
      capture_controller_listener_->OnStartRecordSucceeded();
    }
  } else {
    if (capture_controller_listener_) {
      capture_controller_listener_->OnStartRecordFailed(result, error);
    }

    // Destroy record handler on error cases to make sure state is resetted.
    record_handler_ = nullptr;
  }
};

// Handles RecordStopped event and informs CaptureControllerListener.
void CaptureControllerImpl::OnRecordStopped(CameraResult result,
                                            const std::string& error,
                                            const std::string& optional_record_path) {
  if (capture_controller_listener_) {
    if (result == CameraResult::kSuccess) {
      std::string path =
          optional_record_path.empty() && record_handler_
              ? record_handler_->GetRecordPath()
              : optional_record_path;
      capture_controller_listener_->OnStopRecordSucceeded(path);
    } else {
      capture_controller_listener_->OnStopRecordFailed(result, error);
    }
  }

  if (result == CameraResult::kSuccess && record_handler_) {
    record_handler_->OnRecordStopped();
  } else if (!optional_record_path.empty() || result != CameraResult::kSuccess) {
    if (result != CameraResult::kSuccess) {
      record_handler_ = nullptr;
    }
  }
}

// Updates texture handlers buffer with given data.
// Called via IMFCaptureEngineOnSampleCallback implementation.
// Implements CaptureEngineObserver::UpdateBuffer.
bool CaptureControllerImpl::UpdateBuffer(uint8_t* buffer,
                                         uint32_t data_length) {
  if (!texture_handler_) {
    return false;
  }
  if (raw_recording_enabled_ && raw_record_stream_.is_open()) {
    raw_record_stream_.write(reinterpret_cast<const char*>(buffer),
                             data_length);
  }
  return texture_handler_->UpdateBuffer(buffer, data_length);
}

// Handles capture time update from each processed frame.
// Called via IMFCaptureEngineOnSampleCallback implementation.
// Implements CaptureEngineObserver::UpdateCaptureTime.
void CaptureControllerImpl::UpdateCaptureTime(uint64_t capture_time_us) {
  if (!IsInitialized()) {
    return;
  }

  last_capture_time_us_ = capture_time_us;
  if (preview_handler_ && preview_handler_->IsStarting()) {
    DebugLog("UpdateCaptureTime: first frame arrived; marking preview started");
  }
  if (record_handler_ && record_handler_->CanStop()) {
    record_handler_->UpdateRecordingTime(capture_time_us);
  }

  if (preview_handler_ && preview_handler_->IsStarting()) {
    OnPreviewStarted(CameraResult::kSuccess, "");
  }
}

}  // namespace camera_windows
