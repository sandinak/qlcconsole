/*
  Q Light Controller Plus
  wledstate.h

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

#ifndef WLEDSTATE_H
#define WLEDSTATE_H

#include <QJsonObject>
#include <QByteArray>

/**
 * A universe's bytes as the WLED state they stand for -- read exactly as
 * WLED reads its own DMX "Effect" mode (https://kno.wled.ge/interfaces/e1.31-dmx/),
 * so the WLED Effect and WLED Lantern definitions drive it unchanged:
 *
 *   15 channels: dimmer, effect, speed, intensity, palette, option
 *                (/64: 0 plain, 1 mirror, 2 reverse, 3 both), then the
 *                primary, secondary and tertiary colour (R, G, B each)
 *    4 channels: dimmer, red, green, blue (a solid colour)
 *    3 channels: red, green, blue at full
 *
 * Sent as JSON (/json/state) instead of streamed as DMX: a look is a state,
 * and a state need only be sent when it changes.
 */
namespace WledState
{
    /** The state the @p channels channels at @p address (1-based) of
     *  @p universe set, with @p transitionMs for WLED to glide to it over. */
    QJsonObject decode(const QByteArray &universe, int address, int channels,
                       int transitionMs = 100);
}

#endif
