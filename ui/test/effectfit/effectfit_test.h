/*
  Q Light Controller Plus - Test Unit
  effectfit_test.h

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

#ifndef EFFECTFIT_TEST_H
#define EFFECTFIT_TEST_H

#include <QObject>

class Doc;

class EffectFit_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Fixtures x effects of each kind: what runs where. */
    void whatFitsWhat_data();
    void whatFitsWhat();
    /** A mixed set fits an effect if any of it does, and says how many. */
    void aMixedSetRunsOnSome();
    /** No fixtures known: everything fits. */
    void freeSpinRunsOnHeadsThatFreeRotate();
    void noTargetsListsEverything();
    /** A non-WLED look finds no WLED effect by name until "Show All Effects". */
    void searchHidesWhatDoesNotFit();
    /** The effect a look already runs stays listed, with the reason. */
    void theEffectInUseIsNeverDropped();

    /** Pictures of the picker on three kinds of group, QLC_SHOT_DIR set
     *  (skipped otherwise): "all" = Show All Effects, i.e. what it listed
     *  before, and "fitted". */
    void shots();

private:
    Doc *m_doc = nullptr;
};

#endif
