/*
  Q Light Controller Plus
  lightmapview_test.h

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

#ifndef LIGHTMAPVIEW_TEST_H
#define LIGHTMAPVIEW_TEST_H

#include <QObject>

class App;

/* The Rig Overview's light map (Capture-study gap 3): the light at subject
 * height from the output, as a heat map with the dark spots outlined, worked
 * out off the GUI thread, and nothing done while it is off. */
class LightMapView_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanupTestCase();

    /** Off: no map, no worker, nothing drawn for it. */
    void offWorksNothingOut();
    /** One light straight down: lit under it, dark across the stage, the dark
     *  outlined and counted, the legend in the show's units; the light off,
     *  the whole stage dark; the elevations say where to look. */
    void theMapShowsWhatTheOutputLights();
    /** A new subject height or dark level is read again. */
    void theSettingsAreReadAgain();
    /** The footer's Light Map: the toggle, and its two numbers in the show's
     *  units, saved with it. */
    void theFooterHasTheLightMap();
    /** The school stage rig lit in part: the map, shots at three sizes
     *  (QLC_SHOT_DIR), and what it costs a frame. */
    void aRealRigAndWhatItCosts();

private:
    void loadStageShow();
    App *m_app = nullptr;
};

#endif
