/*
  Q Light Controller Plus
  hidrescan_test.h

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

#ifndef HIDRESCAN_TEST_H
#define HIDRESCAN_TEST_H

#include <QObject>

class HIDPlugin;
class FakeHIDBackend;

/* A controller that stops answering while the HID plugin opens it (a USB
   pad switching mode, re-enumerating, held "busy" by macOS): the app, the
   other devices and the plugin's own teardown never wait on it. */
class HIDRescan_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void aStuckOpenNeverHoldsTheGui();
    void anOpenThatTakesAWhileArrivesLate();
    void aPadThatGoesMidOpenIsNotAdded();
    void aPadReplugedAtTheSamePathOpens();
    void aPadBackAtTheSamePathReplacesTheOld();
    /** inputReopened(): once per line whose device changed (replaced, or
     *  shifted onto it by a drop before it), never an unchanged line. */
    void everyChangedLineIsToldOnce();
    void aPadListedTwiceIsOneDevice();
    void oneOpenPerPathHoweverOftenItRescans();
    void oneScanAtATime();
    void thePluginGoesWithAnOpenStuck();

private:
    /** Release the fake and wait for every call into it to return. */
    void releaseFake();

    HIDPlugin *m_plugin = nullptr;
    FakeHIDBackend *m_fake = nullptr;
};

#endif
