/*
  Q Light Controller Plus - qlcconsole
  monkey_test.h

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

#ifndef MONKEY_TEST_H
#define MONKEY_TEST_H

#include <QObject>

class App;

/* The "student mashing everything" case: a deterministic monkey that walks
   every tab and dialog firing random clicks, keys and tab switches at a fixed
   seed. CI runs a short budget (QLC_MONKEY_MS, default 60000). A crash,
   assert or hang fails it, and the seed makes it reproducible. Set
   QLC_MONKEY_MS to a larger number for a soak. */
class Monkey_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void aSeededMonkeyNeverCrashes();

private:
    App *m_app = nullptr;
};

#endif
