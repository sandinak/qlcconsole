/*
  Q Light Controller Plus - Unit test
  programmercontroller_test.h

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

#ifndef PROGRAMMERCONTROLLER_TEST_H
#define PROGRAMMERCONTROLLER_TEST_H

#include <QObject>

class Doc;
class QLCFixtureDef;

class ProgrammerController_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /** Setting a raw value makes the programmer dirty and shows up in
     *  programmerValues(); clearing drops it and un-dirties. */
    void paintingAProgrammerValueMarksItDirty();
    /** proposedSaveBuckets() sorts painted channels into Position / Color /
     *  Intensity / Special by QLCChannel::group(), with the exact per-bucket
     *  default name each category's own naming rule produces. */
    void proposedSaveBucketsCategorizeByChannelRole();
    /** When one FixtureGroup covers every edited fixture in a category, the
     *  whole category collapses into a single bucket tagged with the
     *  LARGEST such group -- not any smaller group that also covers them. */
    void proposedSaveBucketsPickTheLargestCoveringGroup();
    /** When no single group covers every edited fixture in a category, it
     *  falls back to one bucket per fixture's own smallest containing
     *  group. */
    void proposedSaveBucketsFallBackPerFixtureWhenNoGroupCoversAll();
    /** saveBucketAsScene() bakes the bucket's values into a new, real Scene
     *  with the requested name/path -- and does NOT clear programmer state
     *  itself; that is the caller's job. */
    void saveBucketAsSceneBakesExactValuesAndDoesNotClearProgrammerState();
    /** An empty @c name argument falls back to bucket.defaultName -- but an
     *  empty @c path argument does NOT fall back to bucket.defaultPath the
     *  same way; the Scene just keeps its own ordinary default path. Only
     *  one of the two arguments has ergonomic fallback behaviour. */
    void saveBucketAsSceneEmptyPathDoesNotFallBackToTheBucketsDefaultPath();
    /** findMatchingScene() is an exact, byte-for-byte value match against
     *  existing Scenes -- not a fuzzy/subset match -- and excludes a given
     *  scene id from the search (so a scene never "matches" itself). */
    void findMatchingSceneFindsByteForByteMatchesOnly();
    /** saveBucketAsScene() has no built-in de-duplication: saving the same
     *  bucket twice creates two independent, functionally-identical
     *  Scenes with no complaint. */
    void savingTheSameBucketTwiceCreatesTwoIndependentScenes();
    /** saveBucketAsGroupScene() builds a palette-driven Scene that follows
     *  group membership at run time when the bucket's category can be
     *  expressed as one palette (Color here) -- and falls back to baking
     *  per-fixture values into the SAME scene when it can't (a mixed
     *  Special bucket spanning Gobo/Speed/Intensity channel groups). */
    void saveBucketAsGroupSceneBuildsAPaletteAndFallsBackWhenItCant();
    /** saveBucketAsGroupScene() refuses outright when the bucket carries no
     *  real FixtureGroup id (the "Custom"/no-group bucket shape). */
    void saveBucketAsGroupSceneRequiresARealFixtureGroup();

    /** Park records where a fixture sits; the park is saved with the show,
     *  a new show drops it, and loading the show parks it again. */
    void aParkIsSavedWithTheShow();
    /** A parked fixture holds its output with nothing running and against a
     *  cue that sets its channels; unparked, it follows the cue. */
    void aParkedFixtureHoldsItsOutput();
    /** Highlight drives the selected fixtures to full over a running cue,
     *  and turning it off gives them back to the cue. */
    void highlightDrivesTheSelectionToFull();
    /** Highlight wins over a running cue where the cue's value is higher
     *  (cyan to 0), and Park wins over Highlight. */
    void highlightBeatsARunningCue();
    /** Control Map learn: the next press (not a release) binds a control to
     *  a function; each press then toggles it; a second learn on the same
     *  control replaces the binding; clear removes it. */
    void aLearnedControlTogglesItsFunction();

private:
    Doc *m_doc;
    QLCFixtureDef *m_scannerDef;   //!< Futurelight DJScan250: Pan/Tilt/Colour-wheel/Gobo/GoboRotation/Shutter
    QLCFixtureDef *m_rgbDef;       //!< Generic Generic-RGB, mode "RGB Dimmer": Red/Green/Blue/Dimmer
};

#endif
