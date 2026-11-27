/*
  Q Light Controller Plus - Unit test
  showmanager_test.h

  The Shows timeline's undo: a coarse whole-timeline snapshot taken before
  each edit (ShowManager::pushUndoSnapshot) and put back by Undo.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SHOWMANAGER_TEST_H
#define SHOWMANAGER_TEST_H

#include <QObject>

class ShowManager;
class Show;
class Doc;

class ShowManager_Test : public QObject
{
    Q_OBJECT

private slots:
    /** Show only: Play in Operate on a show left running from Design is
     *  a GO -- it is the show (its gate opens), not a pause */
    void playInOperateClaimsAShowRunningFromDesign();
    void timecodeDrivesAShowOnlyFixtureOnlyWhenArmedInOperate();
    void init();
    void cleanup();

    /** A function dropped on a track is one Undo away from gone; the item
     *  that was there before stays where it was. */
    void undoTakesBackADrop();
    /** Undo of a marker edit puts the label back, and a marker linked to a
     *  cue list keeps its link. */
    void undoPutsAMarkerBack();
    /** Undo of one edit leaves every track as it was set: hold last look,
     *  solo-safe, priority, intensity and a locked item survive the rebuild. */
    void undoKeepsEveryTracksSettings();
    void aCueSnapsToAMarker();
    /** The timeline paints with an audio clip whose file is missing */
    void drawsWithoutTheAudioFile();
    /** Undo keeps which track is soloed (and which isn't) */
    void undoKeepsATracksSolo();
    /** Undo history is the show's: another show or a new document starts
     *  it fresh (it used to restore one show's tracks into another) */
    void undoStartsFreshForAnotherShowOrDocument();
    /** A function deleted elsewhere leaves no item behind on the timeline
     *  (a stale one was read again on zoom and on the next drop) */
    void aDeletedFunctionLeavesNoItemBehind();
    /** Renaming a track renames its intensity attribute, which a VC slider
     *  names and removing the track unregisters */
    void renamingATrackRenamesItsAttribute();
    /** A plain click on a one-cue chase block selects it; it doesn't move
     *  it or touch the show (a stale press position made it a move) */
    void aClickOnAChaseBlockDoesntMoveIt();
    void aGeneratedCueListKeepsItsStepsOnATrack();
    /** M998: the item editor's function deleted elsewhere (Build, an undo):
     *  the editor goes with it, and Operate calls nothing on the freed one */
    void theItemEditorGoesWithItsFunction();

    /** Audio and video files dropped from the desktop land on the track and
     *  time under the drop, one after another in name order; a file that
     *  can't be used says why; the whole drop is one Undo. */
    void droppedAudioLandsOnTheTrack();
    void droppedAudioBelowTheTracksMakesATrack();
    void droppedVideoLandsOnTheTrack();
    void anUnsupportedDropSaysWhy();
    /** The sidebar's right-click menus offer what can act on the row. */
    void sidebarMenusOfferWhatCanAct();
    /** The Timeline embedded in Build takes the same drops. */
    void theEmbeddedTimelineTakesTheSameDrops();
    /** UX shots (QLC_SHOT_DIR): the drop ghost, the dropped items and the
     *  sidebar menu at three sizes. */
    void shootsTheTimelineDrop();
    void addTrackFromTheMenuIsOneUndo();
    void renameInPlaceAndMoveUpDown();

private:
    QString makeWav(const QString &name);
    bool loadAudioDecoders();
    void dropFiles(const QStringList &files, quint32 timeMs, int row, bool drop = true,
                   class MultiTrackView *view = nullptr);
    Doc *m_doc = nullptr;
    ShowManager *m_manager = nullptr;
    Show *m_show = nullptr;
    quint32 m_sceneA = 0;
    quint32 m_sceneB = 0;
};

#endif
