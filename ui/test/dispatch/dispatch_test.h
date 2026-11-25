/*
  Q Light Controller Plus - Unit test
  dispatch_test.h

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

#ifndef DISPATCH_TEST_H
#define DISPATCH_TEST_H

#include <QObject>

class IOPluginStub;
class Doc;

/* The one dispatch point (docs/design/CONTROL_SURFACES_EVAL.md risk 3): a
   control this computer's desk has never also fires a show listener -- a
   Virtual Console button or slider, the show's Grand Master input, a
   channel group, a speed master or the encoder bank -- and the desk's STOP
   fires behind a modal window and with Control Surfaces off for the show. */
class Dispatch_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void staticControlSkipsShowListeners_data();
    void staticControlSkipsShowListeners();
    void stopFiresBehindAModalAndWithTheFeatureOff();

private:
    void send(quint32 channel, uchar value);
    void tick(int n = 3);
    /** This computer's Back on @a channel of universe 1's device, or none. */
    void deskOn(quint32 channel, bool on);

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif
