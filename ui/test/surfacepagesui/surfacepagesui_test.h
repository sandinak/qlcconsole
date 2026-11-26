/*
  Q Light Controller Plus - Unit test
  surfacepagesui_test.h

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

#ifndef SURFACEPAGESUI_TEST_H
#define SURFACEPAGESUI_TEST_H

#include <QTemporaryDir>
#include <QObject>

class IOPluginStub;
class SimMidiDevice;
class Workbench;
class Fixture;
class Scene;
class Doc;

/* Surface pages in the app (docs/design/SURFACE_PAGES.md, slice 1), on a
   simulated APC40 mk2: a page's knob turns pan on the Workbench's
   selection in Design and nothing in Operate, where the show's own
   assignment works; a page's fader picks up first, then sets, one undo
   step a move; the map, the footer chip and the hub's pages row name the
   page; export, import and the show's offer from the hub. */
class SurfacePagesUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void aPageKnobTurnsPanInDesignOnly();
    void aPageFaderPicksUpThenSets();
    void aPageSwitchMakesAFaderPickUpAgain();
    void theMapAndTheChipNameThePage();
    void exportAndImportFromTheHub();
    void theHubOffersTheShowsSet();

private:
    Fixture *mover(const QString &name, quint32 address);
    Scene *sceneWith(Workbench &wb, Fixture *f);
    SimMidiDevice *plugApc();

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    SimMidiDevice *m_apc = nullptr;
    QTemporaryDir m_settings;
    QTemporaryDir *m_userSets = nullptr;
};

#endif
