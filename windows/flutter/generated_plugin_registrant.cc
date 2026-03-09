//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <camera_recorder/camera_recorder_plugin_c_api.h>
#include <just_audio_windows/just_audio_windows_plugin.h>

void RegisterPlugins(flutter::PluginRegistry* registry) {
  CameraRecorderPluginCApiRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("CameraRecorderPluginCApi"));
  JustAudioWindowsPluginRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("JustAudioWindowsPlugin"));
}
