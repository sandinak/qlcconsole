/*
  Q Light Controller Plus
  deviceprofile.h

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

#ifndef DEVICEPROFILE_H
#define DEVICEPROFILE_H

#include <QList>
#include <QString>

/** @addtogroup ui_devices UI - Devices
 * @{
 */

/** One entry in the Device Profile library.
 *
 *  A profile is nothing but a shortcut for declaring a hand-patched target
 *  (InputOutputMap::ManualTarget, see engine/src/inputoutputmap.h) with its
 *  ports already filled in, instead of typing each net:subnet:universe by
 *  hand through "Add a target on this interface..." -- there is no
 *  engine-side notion of a profile, and instantiating one just calls the
 *  same addManualTarget()/addManualPort() a person would have called
 *  themselves. See resources/deviceprofiles/<Manufacturer>/ for the on-disk
  format -- one subdirectory per manufacturer, same shape as fixtures.
 */
struct DeviceProfile
{
    QString manufacturer;
    QString model;
    /** Matched against QLCIOPlugin::name(). Only plugins whose
     *  supportsOutputTargets() is true can host a manual target, so this is
     *  "Art-Net" for every profile today. */
    QString protocol;
    /** Art-Net style port addresses -- (net << 8) | (sub << 4) | universe --
     *  already expanded from the file's Ports="N" count or explicit <Port>
     *  entries. Callers just want the finished list. */
    QList<quint32> ports;
};

/** Load the Device Profile library: the bundled catalog plus anything a
 *  user dropped into their own DeviceProfiles directory, concatenated.
 *  Malformed or unreadable files are skipped rather than failing the whole
 *  load -- one bad hand-edited file should not hide every other profile. */
QList<DeviceProfile> loadDeviceProfiles();

/** @} */

#endif
