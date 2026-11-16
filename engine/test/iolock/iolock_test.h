/*
  Q Light Controller Plus
  iolock_test.h

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

#ifndef IOLOCK_TEST_H
#define IOLOCK_TEST_H

#include <QObject>

class Doc;
class StuckPlugin;

/* A stuck USB device must never stop the show's DMX output (0.4.12). On a
   show machine a device re-enumerating left an IOKit call that never
   returned and wedged its hub: a DMX interface or a controller on it,
   opened or closed with the universes claimed (InputOutputMap's
   m_universeMutex, which the MasterTimer tick takes every frame), stopped
   every universe. A fake plugin whose device calls hang stands for it: the
   tick keeps its time, and the other universes keep writing, through every
   patch change that reaches it. */
class IOLock_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aDeviceThatAnswersIsPatchedAsBefore();
    void theDmxTicksWhileAStuckOutputIsPatchedReconnectedAndRemoved();
    void theDmxTicksWhileAStuckInputMovesGoesAndComesBack();
    void theDmxTicksWhileAControllerSlotsStuckDeviceMoves();

private:
    void start();
    /** The plugin's configurationChanged(), as a device coming or going says it */
    void deviceListChanged();
    /** Frames the plugin got for @p universe on @p line in (from, to), ms */
    int sentBetween(quint32 universe, quint32 line, qint64 from, qint64 to) const;

    Doc *m_doc = nullptr;
    StuckPlugin *m_plugin = nullptr;
};

#endif
