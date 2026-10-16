/*
  Q Light Controller Plus
  e131_test.h

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

#ifndef E131_TEST_H
#define E131_TEST_H

#include <QObject>

class E131_Test final : public QObject
{
    Q_OBJECT

private slots:
    /* Send failure reporting through SendHealth (whose rules artnet_test
       covers in full): results are injected, nothing is sent. */
    void transientFailuresAreNotReported();
    void sustainedFailureIsReportedOnceAndRecovers();
    void alternatingResultsDoNotToggle();
    void aNewTargetStartsClean();
    void pluginGroupsAProblemsUniverses();
};

#endif
