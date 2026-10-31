/*
  Q Light Controller Plus
  cuesheetdialog_test.cpp

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

#include <QTemporaryDir>
#include <QTextBrowser>
#include <QToolButton>
#include <QComboBox>
#include <QMenu>
#include <QtTest>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "cuesheetdialog.h"
#include "showtreeview.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "doc.h"

#include "cuesheetdialog_test.h"

using namespace ShowTree;

void CueSheetDialog_Test::init()
{
    m_doc = new Doc(this);
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Dim 1");
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Front");
    m_doc->addFixtureGroup(g);
    g->assignFixture(fxi->id());
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName("Half");
    p->setValue(128);
    m_doc->addPalette(p);

    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Dance Hysteria");
    const quint32 list = st->addCueList(m_prod, "Main");
    const quint32 a = st->addCue(list, -1, "House");
    const quint32 b = st->addCue(list, 0, "Walk-in");
    Look half;
    half.key = KeyDimmer;
    half.palettes << p->id();
    Target front;
    front.groups << g->id();
    QVERIFY(st->setLayerLook(a, st->addLayer(a, front), half));
    QVERIFY(b != invalidId());
}

void CueSheetDialog_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void CueSheetDialog_Test::showsTheCueSheetAndTheTrackingSheet()
{
    CueSheetDialog dlg(m_doc, m_prod);
    QCOMPARE(dlg.page(), CueSheetDialog::CueSheetPage);
    QVERIFY(dlg.title().startsWith("Cue sheet"));
    QVERIFY(dlg.title().contains("Dance Hysteria"));
    QTextBrowser *view = dlg.findChild<QTextBrowser *>("cueSheetView");
    QVERIFY(view != nullptr);
    QVERIFY(view->toPlainText().contains("Walk-in"));
    QVERIFY(view->toPlainText().contains("Fade in"));

    dlg.setPage(CueSheetDialog::TrackingPage);
    QVERIFY(dlg.title().startsWith("Tracking sheet"));
    QVERIFY(view->toPlainText().contains("Front"));
    QVERIFY(view->toPlainText().contains("Half"));
    QVERIFY(view->toPlainText().contains("(Half)"));         // tracked into Walk-in
    QCOMPARE(dlg.findChild<QComboBox *>("cueSheetPage")->currentIndex(), 1);
}

void CueSheetDialog_Test::savesWhatIsShown()
{
    CueSheetDialog dlg(m_doc, m_prod);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;

    QVERIFY2(dlg.saveCsv(dir.filePath("cues.csv"), &error), qPrintable(error));
    QFile cues(dir.filePath("cues.csv"));
    QVERIFY(cues.open(QIODevice::ReadOnly));
    QVERIFY(cues.readAll().startsWith("Cue list,Song,Cue,Name"));

    dlg.setPage(CueSheetDialog::TrackingPage);
    QVERIFY2(dlg.saveCsv(dir.filePath("tracking.csv"), &error), qPrintable(error));
    QFile tracking(dir.filePath("tracking.csv"));
    QVERIFY(tracking.open(QIODevice::ReadOnly));
    QVERIFY(tracking.readAll().contains("(Half)"));

    QVERIFY2(dlg.saveXlsx(dir.filePath("t.xlsx"), &error), qPrintable(error));
    QVERIFY2(dlg.savePdf(dir.filePath("t.pdf"), &error), qPrintable(error));
    QVERIFY(QFileInfo(dir.filePath("t.pdf")).size() > 0);
    QVERIFY(QFileInfo(dir.filePath("t.xlsx")).size() > 0);
    QVERIFY(!dlg.saveCsv(dir.filePath("none/x.csv"), &error));
    QVERIFY(!error.isEmpty());
}

void CueSheetDialog_Test::theShowTreeMoreMenuOpensIt()
{
    ShowTreeView view(nullptr, m_doc);
    QToolButton *more = view.findChild<QToolButton *>("stMore");
    QVERIFY(more != nullptr && more->menu() != nullptr);
    QAction *sheets = nullptr;
    for (QAction *a : more->menu()->actions())
        if (a->objectName() == "stCueSheetAction")
            sheets = a;
    QVERIFY(sheets != nullptr);
    QCOMPARE(sheets->text(), QString("Cue Sheet…"));
    // One production in the show: it is the one the sheet is of.
    more->menu()->aboutToShow();
    QVERIFY(sheets->isEnabled());
    // The menu-item count stays: the three it had, plus this one, in the same menu.
    QCOMPARE(more->menu()->actions().count(), 4);
}

QTEST_MAIN(CueSheetDialog_Test)
