/*
  Q Light Controller Plus - Unit test
  grandmaster_test.h

  Copyright (c) Jano Svitok

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

#ifndef GRANDMASTER_TEST_H
#define GRANDMASTER_TEST_H

#include <QObject>

class GrandMaster;

class GrandMaster_Test final : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase();
    void cleanupTestCase();

    void initial();
    void channelMode();
    void valueMode();
    void value();

    /** Board Q316: after a load, a replug or a wake the Grand Master
     *  fader waits to pick up; a pull to 0 always applies at once. */
    void pickupAfterLoadReplugWake();
    /** A device's dump in the first 500 ms moves nothing, even a 0. */
    void deviceDumpIgnored();

private:

    GrandMaster *m_gm;
};

#endif
