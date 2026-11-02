/*
  Q Light Controller Plus - Unit test
  encoderbankui_test.h

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

#ifndef ENCODERBANKUI_TEST_H
#define ENCODERBANKUI_TEST_H

#include <QObject>

class QTreeWidgetItem;
class Workbench;
class ShowTreeView;
class Fixture;
class Scene;
class Doc;

/* A MIDI controller's encoders turning what Build has selected (EncoderBank,
   docs/features/encoder-banks.md): synthetic CCs through EncoderBank, on a
   Workbench shown offscreen. */
class EncoderBankUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aTurnEditsTheSelectedFixture();
    void aGestureIsOneUndoStep();
    void aGroupTurnsEveryMemberAndAPushGoesHome();
    void aDimmerLookIsEditedByTheLookEditor();
    void aColourLookTurnsHueAndRgb();
    void aPanTiltLookIsNudgedWithUndo();
    void aBeamLookTakesTheBeamBank();
    void aLookDrivenChannelSaysSo();
    void operateRefusesAndSaysWhy();
    void ringsAndTheLineFollowTheSelection();
    void aBankChangeMovesWhatTheEncodersTurn();
    void nothingSelectedSaysSo();
    void aHiddenWorkbenchIgnoresTheEncoders();
    void theControlMapPanelSetsTheBankUp();
    void theControlMapPanelLearnsBankButtonsAndRings();
    void anAimAtALineTurnsItsSpread();
    void anAimAtAPointSaysItHasNothingToTurn();
    void aStrobeLookTurnsItsRate();
    void aShutterLookStepsItsState();
    void aShowTreeLayerTakesATurnAsOneUndoStep();
    void aShowTreeLayerRefusesInOperate();

private:
    Fixture *mover(const QString &name, quint32 address);
    /** A scene targeting @a fixtures, loaded in @a wb with the first
     *  fixture's row selected. */
    Scene *sceneWith(Workbench &wb, const QList<Fixture *> &fixtures);
    /** One CC turn of encoder @a n (0-7) by @a detents (two's complement). */
    void turn(int n, int detents);
    void push(int n, bool down);
    void gather();
    /** A cue with one layer on a group of two movers (fixtures @a a, @a b, dimmer 51), its Workbench
     *  docked in @a view with the layer selected; the layer's scene is @a scene. */
    Workbench *dockedLayer(ShowTreeView *view, Fixture *a, Fixture *b, Scene **scene);

private:
    Doc *m_doc = nullptr;
};

#endif
