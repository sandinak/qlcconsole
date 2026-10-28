/*
  Q Light Controller Plus
  refinesession_test.h

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

#ifndef REFINESESSION_TEST_H
#define REFINESESSION_TEST_H

#include <QObject>
#include <QList>

class Doc;

class RefineSession_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** No cue, Operate, a cue without a layer of its own: refused. */
    void refusesWhatItCannotRefine();
    /** Degrees back from DMX, as positionToValues() makes it. */
    void degreesFromDmx();
    /** Triggers step layers; Y goes Layer -> Group -> Fixture -> Layer
     *  (Group only where the layer has more than one); bumpers step. */
    void scopesAndSteps();
    /** At Layer scope the layer's own look moves: pending until A, then
     *  one Show Tree step, undone by Back. */
    void layerScopeEditsTheLayerLook();
    /** At Fixture scope a child layer on that fixture holds the exception;
     *  the layer is untouched; X takes it off. */
    void fixtureScopeMakesAnException();
    /** Zoom and focus are kept as the layer's (or the fixture's) own. */
    void zoomAndFocus();
    /** A layer aimed at a target is not moved by degrees at Layer scope. */
    void anAimedLayerAsksForAFixture();
    /** The rest of the layer dims while a fixture is picked; what is said. */
    void theOperatorIsToldWhereTheyAre();
    /** The verbs by role; starting Refine ends Focus mode and back. */
    void oneToolOwnsTheController();
    /** Q112a: in Operate the show runs on; what is kept is held on the rig
     *  until the next GO, and back in Design the preview shows it. */
    void operateHoldsWhatIsKept();
    /** Q112b: the rest of the layer is dimmed only where the cue has it
     *  brighter than the dim level -- never raised. */
    void dimsOnlyWhatIsBrighter();
    /** Q112c: a group's position moves each fixture from its own, kept on
     *  each fixture, so a fan inside the group stays a fan. */
    void groupScopeKeepsTheFan();
    /** Refine on a Build scene: the scene is the layer, an exception is a
     *  fixture out of the palette with the scene's own values. */
    void aBuildSceneIsRefinedTheSameWay();
    /** Q126 in Refine: the light bar, the buzz, both bumpers, what is said. */
    void refineTellsYouMoreToo();
    /** A layer only tracked in: said where a keep goes (this cue, by
     *  default), Layer scope only until it is own, or kept at its source. */
    void aTrackedLayerKeepsHereOrAtItsSource();
    /** Q132: A held for a second keeps everything; a hold never answers
     *  a warning (a shared palette's "press A again"), only a fresh press. */
    void aHeldANeverAnswersAWarning();
    /** The review: a reusable layer changes in every use -- warned first,
     *  a second A goes on (as Build's shared palette). */
    void aReusableLayerWarnsFirst();

    /** Quiet in Operate on a real running list: no preview, no blink, no
     *  dim; moved shows, kept holds; the cue takes it at its next GO. */
    void operateIsQuietOnARunningList();
    /** Holding (Q140): no stick, no button; released, it moves again. */
    void holdingTakesNoStick();
    /** The gobo page: a gobo picked at Layer scope is seen on the rig at
     *  once, kept by A as one Show Tree step on the layer's Gobo look. */
    void aGoboAtLayerScopeIsPreviewedAndKept();

    /* The coverage review's gaps (class-refinesession, G1..G30). */
    /** Into Operate on a cue not running: the Design dims and the pending
     *  values leave the rig at once, not at the next change. */
    void operateDropsTheDesignFeedback();
    /** A cue that moved on is said once, not on every tick. */
    void aMovedOnCueIsSaidOnce();
    /** A kept fixture exception is refined again from its own value. */
    void aKeptExceptionIsRefinedAgain();
    /** A reusable layer warns at every keep, and a warning not answered
     *  does not outlive the session. */
    void theSharedWarningIsAskedEveryKeep();
    /** A fixture exception on a reusable layer is this cue's own: no
     *  warning; the warning lists who else uses it until B. */
    void anExceptionOnAReusableLayerIsThisCuesOwn();
    /** X is one Show Tree step at Group and at Fixture scope. */
    void clearingAnExceptionIsOneUndoStep();
    /** X on the Beam page leaves the exception's colour. */
    void xOnTheBeamPageKeepsTheColour();
    /** A group keeps zoom, focus, colour and gobo; a position alone makes
     *  no empty group child. */
    void groupScopeKeepsEveryAttribute();
    /** cuePosition() reads each fixture's own fanned or aimed value. */
    void cuePositionReadsEachFixture();
    /** Zoom starts at the wide end, is sent absolute and only when moved;
     *  a focus on channel 0 is found. */
    void zoomAndFocusStartWhereTheyAre();
    /** A layer keep keeps the palette's fanning. */
    void aLayerKeepKeepsTheFan();
    /** Two tracked layers stay apart; a keep here stays on that layer; two
     *  keeps at the source add up. */
    void twoTrackedLayersStayApart();
    /** Stick Y tilts, the response scales it, a held arm ends with the
     *  session. */
    void theSticksAndTheHeldArm();
    /** A layer tracked in dark is not offered. */
    void aDarkTrackedLayerIsNotOffered();
    /** A fixture with no pan range in its physical maps through 360. */
    void degreesWithoutAPanRange();
    /** Colour palettes step on from the cue's own, and round. */
    void palettesStepFromTheCuesOwn();
    /** Layers step round both ways and start fresh. */
    void layersStepRoundAndStartFresh();
    /** Dim levels per fixture of a fan, and from the rig when the cue has
     *  no Dimmer look (read again after Operate). */
    void dimsReadTheFanAndTheRig();
    /** pickFixture() from a Workbench row, the first one too. */
    void pickFixtureFromARow();
    /** A layer or fixture removed under a pending edit: no crash. */
    void aLayerOrFixtureGoneUnderAnEdit();
    /** The flash only when enabled; it runs on its timer. */
    void theFlashIsGatedAndTimed();
    /** stop() leaves nothing behind. */
    void stopLeavesNothingBehind();
    /** Each action tells the panel (changed()). */
    void everyActionSaysChanged();
    /** pendingZoom/Focus only for what is pending; keep() with nothing. */
    void pendingIsOnlyWhatIsPending();
    /** Both bumpers land on the edit's layer and fixture. */
    void nextPendingLandsOnTheEdit();
    /** The preview follows X with nothing to clear, and undo. */
    void theRigFollowsXAndUndo();
    /** "group 1 of 2", the crew's word. */
    void groupAndCrewWordsAreSaid();
    /** Lights feedback off: no dims; an unpatched light never dims. */
    void lightsFeedbackOffAndUnpatched();
    /** In a scene, a layer nudge leaves a fixture exception alone. */
    void aSceneLayerNudgeLeavesAnException();
    /** Nothing is sent past the universe's last channel. */
    void outputStaysInTheUniverse();

private:
    /** AT left (spots 1, 2) and AT right (spots 3, 4) as m_layer's target. */
    void splitIntoTwoGroups(quint32 *left, quint32 *right);
    Doc *m_doc = nullptr;
    QList<quint32> m_spots;
    quint32 m_group = 0;
    quint32 m_cue = 0;
    quint32 m_layer = 0;
    quint32 m_position = 0;
};

#endif
