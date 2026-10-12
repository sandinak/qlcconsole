/*
  Q Light Controller Plus
  programmingguide_test.cpp

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

#include <QtTest>
#include <QSettings>

#define private public
#include "programmingmanager.h"
#undef private

#include "programmingguide_test.h"

#define GUIDE_KEY QStringLiteral("programming/guideSeen")

void ProgrammingGuide_Test::initTestCase()
{
    /* This reads and writes the REAL settings, which is where the flag has to
       live for "once, ever" to mean anything. Borrow it and put it back. */
    m_saved = QSettings().value(GUIDE_KEY);
}

void ProgrammingGuide_Test::cleanupTestCase()
{
    QSettings s;
    if (m_saved.isValid())
        s.setValue(GUIDE_KEY, m_saved);
    else
        s.remove(GUIDE_KEY);
}

void ProgrammingGuide_Test::theGuideShowsOnceAndThenNeverAgain()
{
    /* The Programming tab used to spend four permanent lines of its canvas
       explaining itself. That explanation is now a first-run popup -- which is
       only an improvement if it really is FIRST-run. A hint that forgets to
       record itself nags on every launch, and the person who wrote it never
       sees that, because their own flag is already set. */
    QSettings().remove(GUIDE_KEY);

    QVERIFY2(ProgrammingManager::takeFirstRunGuideFlag(),
             "a fresh install should be offered the guide");

    for (int i = 0; i < 5; ++i)
        QVERIFY2(ProgrammingManager::takeFirstRunGuideFlag() == false,
                 "the guide offered itself again after it had been shown");

    /* And the flag is where a later release can find it, not just in memory:
       a static bool would pass the loop above and still nag every launch. */
    QCOMPARE(QSettings().value(GUIDE_KEY, false).toBool(), true);

    QSettings().remove(GUIDE_KEY);
    QVERIFY2(ProgrammingManager::takeFirstRunGuideFlag(),
             "clearing the setting should offer the guide again");
}

QTEST_MAIN(ProgrammingGuide_Test)
