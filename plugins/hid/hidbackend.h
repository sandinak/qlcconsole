/*
  Q Light Controller Plus
  hidbackend.h

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

#ifndef HIDBACKEND_H
#define HIDBACKEND_H

#include <QList>
#include <QString>
#include <functional>

class HIDDevice;

/** One device the system lists, copied out of the listing (nothing in it
 *  points into the listing, which is freed once read). */
struct HIDFound
{
    QString path;
    unsigned short vendorId = 0;
    unsigned short productId = 0;
    unsigned short usagePage = 0;
    unsigned short usage = 0;
    unsigned short releaseNumber = 0;
    int interfaceNumber = -1;
    QString manufacturer;
    QString product;
    QString serial;
    /** A USB DMX interface (HIDDMXDevice); else a joystick. */
    bool dmx = false;
    /** The system entry it is, when the listing says (0: it does not):
     *  the same path with another one is a device that came back. */
    quint64 instance = 0;
};

/**
 * Where HIDPlugin finds and opens its devices -- the system's (hidapi and
 * each platform's joystick), or a test's. Neither call is ever made on the
 * GUI thread: a device that stops answering in the middle of being opened
 * (a pad switching USB mode) holds a worker thread, never the app.
 *
 * The plugin's own seam for tests (HIDPlugin::setTestBackend()); a test
 * keeps its backend alive until every open it started has returned.
 */
class HIDBackend
{
public:
    virtual ~HIDBackend() {}

    /** The devices that may be read now: joysticks and DMX interfaces
     *  only. On the plugin's scan thread, one call at a time; may block. */
    virtual QList<HIDFound> enumerate() = 0;

    /** @p found made and opened, or null when there is nothing to read
     *  there. On a thread of its own (a DMX interface: the scan thread);
     *  may block -- the plugin gives up waiting after its time limit, and
     *  takes the device if it comes later. The device is made with no
     *  parent, on the calling thread. @p virtualPad makes a pad with no
     *  hardware behind it (HIDVirtualPad), for a test's backend. */
    virtual HIDDevice *open(const HIDFound &found,
                            const std::function<HIDDevice *(const HIDFound &)> &virtualPad) = 0;
};

#endif
