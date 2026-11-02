/*
  Q Light Controller Plus
  groupmasters_test.h

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

#ifndef GROUPMASTERS_TEST_H
#define GROUPMASTERS_TEST_H

#include <QObject>
#include <QList>

class FixtureGroup;
class Universe;
class Scene;
class Doc;

class GroupMasters_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aCueAtFullUnderAHalfMasterIsHalf();
    void aMasterMovesTheStageWithoutAFrame();
    void theGrandMasterMultipliesWithIt();
    void anOverrideHitIsScaledToo();
    void overlappingGroupsMultiply();
    void partOfAFixtureLeavesItsOtherHeads();
    void parkAndIgnoreGmAreNotScaled();
    void saveAndLoad();
    void aDeletedGroupLetsGo();
    void aReopenedShowStillScalesByItsMaster();

private:
    void frame(int frames = 1);
    FixtureGroup *group(const QString &name, const QList<quint32> &fixtures, int level = -1);
    Scene *look(const QList<quint32> &fixtures, uchar dimmer, uchar pan);
    Universe *uni() const;

    Doc *m_doc = nullptr;
    QList<quint32> m_movers;   // MAC250+ Mode 1: shutter, dimmer (1), ..., pan (7), tilt
    quint32 m_pack = 0;        // a 4-channel dimmer pack: four heads
};

#endif
