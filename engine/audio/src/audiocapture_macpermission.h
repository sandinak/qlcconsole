/*
  Q Light Controller Plus
  audiocapture_macpermission.h

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

#ifndef AUDIOCAPTURE_MACPERMISSION_H
#define AUDIOCAPTURE_MACPERMISSION_H

/** macOS microphone (TCC) authorization state for audio input capture. */
enum MacAudioPermission
{
    MacAudioPermissionAuthorized = 0,    //!< Access granted — capture may proceed.
    MacAudioPermissionDenied,            //!< User denied; change in System Settings.
    MacAudioPermissionRestricted,        //!< Blocked by policy (Screen Time / MDM).
    MacAudioPermissionNotDetermined,     //!< Prompt shown; user hasn't answered yet.
    MacAudioPermissionNoUsageDescription //!< No usage string — cannot safely prompt.
};

/**
 * Check — and, when @p requestIfNeeded is true, prompt for — macOS microphone
 * access. On macOS < 10.14 (no TCC microphone gate) it always returns
 * Authorized. When the status is undetermined the system prompt is fired
 * asynchronously (this call never blocks) and NotDetermined is returned; once
 * the user allows access, re-enabling capture starts cleanly.
 */
MacAudioPermission macCheckAudioInputPermission(bool requestIfNeeded);

/** Human-readable, actionable message for a non-authorized state ("" if OK). */
const char *macAudioPermissionMessage(MacAudioPermission perm);

#endif // AUDIOCAPTURE_MACPERMISSION_H
