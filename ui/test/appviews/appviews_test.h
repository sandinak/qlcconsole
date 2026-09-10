/*
  Q Light Controller Plus
  appviews_test.h

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

#ifndef APPVIEWS_TEST_H
#define APPVIEWS_TEST_H

#include <QObject>

class App;

class AppViews_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Picking a view from the View menu switches to it even when an earlier
     *  tab has been detached and every index after it has shifted. */
    void jumpingToADockedViewSurvivesAnEarlierDetach();

    /** Picking a view that is ALREADY detached brings its own window forward
     *  instead of doing nothing, which is what an index-based jump did: there
     *  was no tab left to select. */
    void jumpingToADetachedViewRaisesItsWindow();

private:
    App *m_app;
};

#endif
