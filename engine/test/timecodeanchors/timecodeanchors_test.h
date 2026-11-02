/*
  Q Light Controller Plus - Test Unit
  timecodeanchors_test.h

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

#ifndef TIMECODEANCHORS_TEST_H
#define TIMECODEANCHORS_TEST_H

#include <QObject>
#include <QList>
#include <QStringList>

class Doc;

/* Timecode anchors (Q198): section starts and cues timed from them, the
   follow-on shift, converting an older show, Logic's markers from a MIDI
   file, ripple / stretch, and Learn. */
class TimecodeAnchors_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void resolveCountsFromTheSectionStart();
    void anAnchorCueTimesTheCuesAfterIt();
    void movingASectionMovesItsCues();
    void laterSectionsFollowWhenAsked();
    void aNewTimeCountsFromItsSection();
    void anchorOnTheCuePage();
    void convertingAnOlderShow();
    void saveAndLoadKeepTheAnchors();
    void timecodeFiresAtTheMovedTime();
    void anchorsChangedSignal();

    void midiMarkersFromTheFixture();
    void midiSmpteOffsetAndDivision();
    void midiGarbageIsRefused();
    void markerPlanPreviewApplyAndUndo();
    void markerPlanHourOffset();
    void markersOntoAnOlderShow();

    void rippleInsertAndRemove();
    void stretchASection();
    void rippleAndStretchTyped();

    void learnSetsEachSectionStart();

private:
    /** A production numbered Show.Part.Iteration, one list following
     *  timecode: cues numbered @a numbers, sections named @a names (in
     *  order); its id. Cue ids in m_cues. */
    quint32 makeShow(const QStringList &numbers, const QStringList &names);
    int sectionIndex(quint32 list, const QString &name) const;
    qint32 timeOf(int cue) const;

    Doc *m_doc = nullptr;
    quint32 m_prod = 0;
    QList<quint32> m_cues;
};

#endif
