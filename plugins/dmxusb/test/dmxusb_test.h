/*
  Q Light Controller Plus - Unit test
  dmxusb_test.h

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

#ifndef DMXUSB_TEST_H
#define DMXUSB_TEST_H

#include <QObject>

/** A DMX USB widget that stops taking data says so (WriteHealth, read by
 *  the plugin's outputProblems() for the footer). */
class DMXUSB_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Writes that keep failing for a second make it a problem, once. */
    void aWidgetThatStopsTakingDataIsAProblem();
    /** A frame or two refused on a busy bus is not. */
    void aFewLostFramesAreNot();
    /** Writes going through again for a while clear it, once. */
    void itClearsOnceWritesHaveGoneThroughAWhile();
};

#endif
