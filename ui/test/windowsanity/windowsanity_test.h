/*
  Q Light Controller Plus - Test Unit
  windowsanity_test.h

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

#ifndef WINDOWSANITY_TEST_H
#define WINDOWSANITY_TEST_H

#include <QObject>
#include <QStringList>

class App;

/* Every dialog and window the app opens, opened the way the app opens it, on
   three screens side by side (1024x640, 1440x900, 1920x1080: the offscreen
   platform's own screens), and checked the way a real screen uses it: input
   comes through its WINDOW (as macOS hands it over), it belongs to the main
   window, it opens on the main window's screen and fits there, it is in
   front while it holds anything, and when it closes it leaves no grab or
   filter behind. One row per window in windows(); a new top-level dialog in
   ui/src without a row fails platforms/linux/check-window-rows.sh. See
   docs/dev/UX_RULES.md (R-WIN) and docs/dev/TEST_MAP.md. */
class WindowSanity_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** The checker itself, on windows made wrong on purpose: one that eats
     *  its window's input (as the rig-view lock did), one taller than the
     *  laptop screen, one with no parent, one that leaves an event filter or
     *  a mouse grab behind, one that holds a view with no banner. Each is
     *  found; a good one is not. */
    void theCheckerFindsWhatItShould();

    /** Fixture Properties hidden on another screen (as on another Space):
     *  the plot it holds says so, "Fixture Properties is open", and Show it,
     *  clicked through the main window, brings it back in front on the main
     *  window's screen. QLC_SHOT_DIR saves the banner at the three sizes. */
    void aHiddenEditorIsFoundFromTheBanner();

    /** Every window at every screen size. */
    void everyWindowWorksThroughItsWindow_data();
    void everyWindowWorksThroughItsWindow();

private:
    App *m_app = nullptr;
};

#endif
