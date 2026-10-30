/*
  Q Light Controller Plus
  focuscallsui_test.h

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

#ifndef FOCUSCALLS_UI_TEST_H
#define FOCUSCALLS_UI_TEST_H

#include <QTemporaryDir>
#include <QObject>
#include <QList>

class Doc;

class FocusCallsUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** The dialog edits the show's calls; with no venue, nothing is beside
     *  the show; with a new one, it is made and applied. */
    void theDialogEditsCallsAndPicksTheVenue();
    /** The venue manager: switch, rename and delete, the rig and the file
     *  following. */
    void theVenueManager();
    /** At its own size, picking New venue leaves room for the name, and no
     *  venue button is squeezed below what it needs. */
    void theNewVenueNameHasRoom();
    /** A venue needs the show saved first. */
    void aVenueNeedsASavedShow();
    /** Running calls, the panel says the call, the scope and where saves go,
     *  and the map uses Focus calls' words. */
    void thePanelShowsTheCall();
    /** Refine's panel: the cue and layer, the map in Refine's words, the
     *  keys doing what the controller does. */
    void theRefinePanel();
    /** A focus-call cue: the Run tab says it waits for the controller; its
     *  button starts the calls; leaving goes on. */
    void aFocusCallCueWaitsForTheController();
    /** Q120: the laptop asks for the venue; typing holds the clock. */
    void theLaptopAsksForTheVenue();
    /** Q126: the change since the save on the panel, U, the plot's ghost. */
    void theChangeSinceTheSaveShows();
    /** Build's Workbench: Refine from its header and from a row. */
    void refineFromTheWorkbench();
    /** Feedback is one setting, on the panels, with Shift+W; W says the light. */
    void feedbackIsOneSettingOnEveryPanel();
    /** The voice: silent under a test, and what is announced reaches it
     *  while it is on. */
    void theVoiceHearsTheAnnouncements();
    /** Gap 9: confirmations, warnings and safety lines are queued and never
     *  dropped; only "where am I" lines go stale. */
    void theVoiceNeverDropsAConfirmation();
    /** Board Q171: a where line waits for the selection to settle (only
     *  the newest is said), then cuts a stale where line being said; an
     *  important line cuts a where line at once; a confirmation is never
     *  held or cut; a where-now line (a tool's first, entering a call) and
     *  Say the light speak at once; Off is the voice as before. */
    void theVoiceWaitsForTheSelectionToSettle();
    /** Q129: an applied venue last changed over a day ago -- "Still at
     *  ...?" on open (New venue the default), and the dialog defaults to
     *  New venue until it is confirmed. */
    void anOldVenueIsAskedAbout();
    /** Gap 8: opened with no venue file, a sibling's is offered. */
    void aSiblingsVenuesAreOffered();
    /** Gap 10: the Refine panel has the same lamp: kept, here, elsewhere. */
    void theRefinePanelHasTheLamp();
    /** "Voice to:" an output that is not there (the pad unplugged): every
     *  line goes to the system's output instead of being lost -- "Controller
     *  lost. Lights held." with the rest -- the first says why, and the
     *  output is used again when it is back. Two outputs of one name are
     *  told apart by ID (RC5; a fake `say` that records). */
    void aMissingVoiceOutputFallsBackToTheSystem();
    /** Venue focus (Focus calls): no press moves the panel's layout -- a
     *  call with a two-line note and one without, a long venue name, a long
     *  fixture name, a zoom light, a light marked dead, nudge, save, revert,
     *  scope, reference, beam, hold (RC5). */
    void theCallPanelNeverMovesTheLayout();
    /** The Refine panel likewise: pending, kept, scope, the next layer, a
     *  tracked cue's "keep in the cue it tracks from" (RC5). */
    void theRefinePanelNeverMovesTheLayout();
    /** One light's venue correction seen and cleared (the venue manager). */
    void oneCorrectionIsSeenAndCleared();
    /** Q134: the venue manager exports the focus report -- PDF, web page
     *  or CSV, by the file's ending -- with who focused, typed once. */
    void theFocusReportIsExported();
    /** Q140: a new venue starts from the last venue's corrections (the
     *  box ticked), or from the show's own. */
    void aNewVenueStartsFromTheLast();
    /** Practice: a scrambled venue from the dialog, the views drawing
     *  where the lights really land, the score at the end and the real
     *  venue back, untouched. */
    void practiceScramblesAndScores();
    /** Q146: Quick focus says why Start is greyed -- no moving heads on
     *  the plot, no stage target -- and never shows a blank Aim at; Add a
     *  target makes one and Start comes on. */
    void quickFocusSaysWhyStartIsGreyed();
    /** Q148: the Lighting Studio's Focus menu -- its entries, in order,
     *  greyed with the reason while there is nothing to focus, and on once
     *  there is. */
    void theFocusMenuGreysWhatCannotBeUsed();
    /** Q146/Q149: the Run tab's Venue focus, the Show Tree cue page's and
     *  the Workbench's Focus menus, under their new names, greyed with why. */
    void focusEntriesElsewhereSayWhy();
    /** Q143: the Practice dialogs say plainly what a scramble is. */
    void practiceExplainsTheScramble();
    /** Voice detail beside Feedback on the panel: one setting, kept; the
     *  panel shows the line confirmed. */
    void voiceDetailIsOneSettingOnEveryPanel();
    /** Quick focus lists selected lights that cannot be aimed, and leaves
     *  them out (the default) or cancels. */
    void lightsThatCannotBeAimedAreListed();
    /** Q150: the venue chip -- its text, its menu (contents and what is
     *  greyed), switching, renaming, a new venue and None from it, and it
     *  following a venue removed elsewhere. */
    void theVenueChipSaysTheVenueAndOpensItsMenu();
    /** Q150: clashes turn the chip amber with a count, Clashes (n) opens
     *  the venue page's list; the chip follows the rig changing. */
    void theVenueChipTurnsAmberWithItsClashes();
    /** Q150: "Still at <venue>?" unanswered shows on the chip and heads its
     *  menu; the answer shows on the chip. */
    void theVenueChipAsksStillAt();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
    QList<quint32> m_spots;
    quint32 m_group = 0;
    quint32 m_target = 0;
};

#endif
