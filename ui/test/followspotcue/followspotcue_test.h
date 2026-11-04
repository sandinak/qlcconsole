/*
  Q Light Controller Plus
  followspotcue_test.h

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

#ifndef FOLLOWSPOTCUE_TEST_H
#define FOLLOWSPOTCUE_TEST_H

#include <QObject>
#include <QString>

class App;
class QTemporaryDir;
class ShowTreeView;

class FollowSpotCue_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void aCueFollowSpotMovesFromEveryRoute();
    void theStickIsWhicheverPadIsPatched();
    void everyCueSelectedShowsItsEffect();
    void hisShow();
    void everyCueFiresItsEffects();

private:
    bool open(const QString &path);
    ShowTreeView *showTree() const;

private:
    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
};

#endif
