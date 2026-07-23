/*
  Q Light Controller Plus
  audiocapture_macpermission.mm

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "audiocapture_macpermission.h"

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

MacAudioPermission macCheckAudioInputPermission(bool requestIfNeeded)
{
    if (@available(macOS 10.14, *))
    {
        AVAuthorizationStatus status =
            [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

        switch (status)
        {
            case AVAuthorizationStatusAuthorized:
                return MacAudioPermissionAuthorized;
            case AVAuthorizationStatusDenied:
                return MacAudioPermissionDenied;
            case AVAuthorizationStatusRestricted:
                return MacAudioPermissionRestricted;
            case AVAuthorizationStatusNotDetermined:
            default:
                break;
        }

        // Undetermined: we can raise the system prompt, but only if we're allowed
        // to and it's safe to do so.
        if (!requestIfNeeded)
            return MacAudioPermissionNotDetermined;

        // requestAccessForMediaType: HARD-CRASHES if the running image carries no
        // NSMicrophoneUsageDescription. That string lives in the .app bundle's
        // Info.plist and, for the bare dev binary, in an embedded __info_plist
        // section (see main/CMakeLists.txt). If neither is present, refuse to
        // prompt rather than crash.
        if ([[NSBundle mainBundle]
                objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"] == nil)
            return MacAudioPermissionNoUsageDescription;

        // Fire the prompt asynchronously — never block this capture thread waiting
        // on the user (a blocked thread would also stall app shutdown while the
        // dialog is open). Capture fails this round; once access is granted,
        // re-enabling audio input starts cleanly.
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                 completionHandler:^(BOOL) { }];
        return MacAudioPermissionNotDetermined;
    }

    return MacAudioPermissionAuthorized; // pre-10.14: no TCC microphone gate
}

const char *macAudioPermissionMessage(MacAudioPermission perm)
{
    switch (perm)
    {
        case MacAudioPermissionNotDetermined:
            return "QLC+ just asked macOS for microphone access. Click Allow, then "
                   "switch the audio input off and on again.";
        case MacAudioPermissionDenied:
            return "QLC+ was denied microphone access. Open System Settings > "
                   "Privacy & Security > Microphone, enable QLC+, then switch the "
                   "audio input off and on again.";
        case MacAudioPermissionRestricted:
            return "Microphone access is restricted on this Mac by a policy "
                   "(Screen Time or device management) and can't be enabled from "
                   "QLC+.";
        case MacAudioPermissionNoUsageDescription:
            return "This build can't request microphone access. Launch the bundled "
                   "QLC+.app, or grant access in System Settings > Privacy & "
                   "Security > Microphone.";
        case MacAudioPermissionAuthorized:
        default:
            return "";
    }
}
