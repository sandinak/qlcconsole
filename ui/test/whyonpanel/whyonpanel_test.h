/*
  Q Light Controller Plus
  whyonpanel_test.h

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

#ifndef WHYONPANEL_TEST_H
#define WHYONPANEL_TEST_H

#include <QObject>
#include <QList>

class Doc;

/* "Why is this on?" window (WhyOnPanel): what it lists, where a click
   goes, and that it asks nothing while hidden. Offscreen. */
class WhyOnPanel_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void listsTheChainAndAClickGoesThere();
    void theSimpleDeskBeatsAScene();
    void aBareAddressAndTheChannelPicker();
    void plainWordsInATeachingShow();
    void asksNothingWhileHidden();
    void aDarkLightSaysWhyNotFirst();
    void unparkFromALine();
    void theActionsALineOffers();
    void commandIOpensOnTheSelection();
    void closingTheWindowStopsAllWork();

private:
    void tick(int n = 1);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
