/*
  Q Light Controller Plus - Test Unit
  monitor_test.h

  Copyright (C) Branson Matheson

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

#ifndef MONITOR_TEST_H
#define MONITOR_TEST_H

#include <QObject>

class Doc;

class Monitor_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void addTrussAccepted();
    void addTrussCancelled();
    void addTargetAccepted();
    void addTargetEditCancelled();
    void addPlatformEditCancelled();
    void removeSelectedTruss();
    void removeSelectedCancelled();

    /* Drop round-trips: a released drag must (a) not jump at the moment of
       the drop and (b) still be where it was dropped after the item is
       re-placed from the model. Failing either is what the operator reports
       as "snaps back" / "jumps somewhere else". One test per view, plus the
       attach/detach transitions, all through the same slotFixtureMoved() the
       real mouse release invokes. */
    void dropStaysPutTopView();
    void dropStaysPutFrontView();
    void dropStaysPutSideView();
    void dropOnTrussAttachesAndStays();
    void dropOffTrussDetachesAndStays();

private:
    Doc* m_doc;
};

#endif
