/*
  Q Light Controller Plus
  callingscriptview_test.cpp

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

#include <climits>

#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QMenu>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "callingscriptview.h"
#include "inputoutputmap.h"
#include "showtreeview.h"
#include "fixturegroup.h"
#include "showcontrol.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

#include "callingscriptview_test.h"

using namespace ShowTree;

/* The calling script view, offscreen: a 300-line script with three cues
   called on lines 5, 150 and 280. */

namespace
{
const int kLines = 300;
const int kLineOf[3] = { 5, 150, 280 };

QString script()
{
    QStringList lines;
    for (int i = 0; i < kLines; ++i)
        lines << QString("Line %1 of the song").arg(i);
    return lines.join('\n');
}
} // namespace

void CallingScriptView_Test::init()
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
    p->setName("Full");
    p->setValue(255);
    m_doc->addPalette(p);

    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Dance Hysteria");
    m_list = st->addCueList(m_prod, "Main");
    m_cues[0] = st->addCue(m_list, -1, "House");
    m_cues[1] = st->addCue(m_list, 0, "Walk-in");
    m_cues[2] = st->addCue(m_list, 1, "Finale");
    Target front;
    front.groups << g->id();
    for (int i = 0; i < 3; ++i)
        QVERIFY(st->setTargetLook(m_cues[i], front, st->lookForPalette(p->id())));
    st->setCurrentProduction(m_prod);
    QVERIFY(st->setScriptText(m_prod, script(), "song.txt"));
}

void CallingScriptView_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    settle();
    delete m_doc;
    m_doc = nullptr;
}

void CallingScriptView_Test::settle(int ticks)
{
    for (int i = 0; i < ticks; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

int CallingScriptView_Test::offsetOfLine(int line) const
{
    return script().indexOf(QString("Line %1 of").arg(line));
}

void CallingScriptView_Test::cueHereCallsTheSelectedWord()
{
    CallingScriptView view(m_doc, m_prod);
    QVERIFY(!view.isEditing());                     // a script is there: read it
    QCOMPARE(view.textEdit()->toPlainText(), script());
    // The first cue is the one to place; it moves on to the next after a GO.
    QCOMPARE(view.chosenCue(), m_cues[0]);
    QTextCursor c(view.textEdit()->document());
    c.setPosition(offsetOfLine(7));
    c.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);        // "Line"
    view.textEdit()->setTextCursor(c);
    QVERIFY(view.cueHere(view.chosenCue(), true));
    QCOMPARE(view.chosenCue(), m_cues[1]);
    const CallingScript s = m_doc->showTree()->callingScript(m_prod);
    QCOMPARE(s.anchor(m_cues[0])->go, offsetOfLine(7));
    QCOMPARE(view.sheet().lineOfCue(m_cues[0]), 7);

    // Standby is the same with the other button; the cue stays chosen.
    c.setPosition(offsetOfLine(4));
    view.textEdit()->setTextCursor(c);
    QVERIFY(view.cueHere(m_cues[0], false));
    QCOMPARE(view.sheet().lineOfCue(m_cues[0], false), 4);
    QCOMPARE(view.chosenCue(), m_cues[1]);
    QVERIFY(view.removeCue(m_cues[0]));
    QCOMPARE(view.sheet().markerCount(), 0);
}

void CallingScriptView_Test::droppingACueFromTheTreeOnAWord()
{
    CallingScriptView view(m_doc, m_prod);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << quint32(m_cues[1]);
    QMimeData mime;
    mime.setData(QLatin1String(ShowTreeView::cueMimeType()), data);

    QTextCursor c(view.textEdit()->document());
    c.setPosition(offsetOfLine(2) + 3);                 // in the middle of "Line"
    const QPoint at = view.textEdit()->cursorRect(c).center();
    QDropEvent drop(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QDragEnterEvent enter(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.textEdit()->viewport(), &enter);
    QApplication::sendEvent(view.textEdit()->viewport(), &drop);
    QVERIFY(drop.isAccepted());
    // On the word: the cue is called at the start of the word.
    QCOMPARE(m_doc->showTree()->callingScript(m_prod).anchor(m_cues[1])->go, offsetOfLine(2));
    QCOMPARE(view.sheet().lineOfCue(m_cues[1]), 2);

    // Alt: standby.
    QDragEnterEvent enter2(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::AltModifier);
    QApplication::sendEvent(view.textEdit()->viewport(), &enter2);
    QDropEvent alt(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::AltModifier);
    QApplication::sendEvent(view.textEdit()->viewport(), &alt);
    QCOMPARE(view.sheet().lineOfCue(m_cues[1], false), 2);
}

void CallingScriptView_Test::marginShowsNumberNameAndNote()
{
    ShowTreeController *st = m_doc->showTree();
    Cue c = *st->cue(m_cues[1]);
    c.note = "Blinders";
    QVERIFY(st->setCue(c));
    QVERIFY(st->anchorCue(m_prod, m_cues[1], offsetOfLine(9)));
    QVERIFY(st->anchorCue(m_prod, m_cues[1], offsetOfLine(8), ShowTreeController::ScriptMark::Standby));
    CallingScriptView view(m_doc, m_prod);
    QVERIFY(view.marginWidth() > 100);
    QCOMPARE(view.marginText(9), QStringList({ "GO 2 Walk-in" }));
    QCOMPARE(view.marginText(8), QStringList({ "STBY 2 Walk-in" }));
    QVERIFY(view.marginText(10).isEmpty());
    QCOMPARE(view.sheet().lines().at(9).markers.first().note, QString("Blinders"));

    // Renumbered in the tree: the view follows.
    c.number = "7";
    QVERIFY(st->setCue(c));
    QCOMPARE(view.marginText(9), QStringList({ "GO 7 Walk-in" }));
    // Deleted: the marker is gone.
    QVERIFY(st->removeCue(m_cues[1]));
    QVERIFY(view.marginText(9).isEmpty());
    QVERIFY(view.marginText(8).isEmpty());
}

void CallingScriptView_Test::followsTheRunningCueAndMarksStandby()
{
    ShowTreeController *st = m_doc->showTree();
    for (int i = 0; i < 3; ++i)
        QVERIFY(st->anchorCue(m_prod, m_cues[i], offsetOfLine(kLineOf[i])));
    CallingScriptView view(m_doc, m_prod);
    view.resize(800, 400);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.follows());

    // Stopped: the first cue is on standby, nothing is being called.
    QCOMPARE(view.nowLine(), -1);
    QCOMPARE(view.standbyCue(), m_cues[0]);
    QCOMPARE(view.standbyLine(), kLineOf[0]);

    QVERIFY(view.go());
    settle();
    view.refresh();
    QCOMPARE(view.nowCue(), m_cues[0]);
    QCOMPARE(view.nowLine(), kLineOf[0]);
    QCOMPARE(view.standbyCue(), m_cues[1]);
    QCOMPARE(view.standbyLine(), kLineOf[1]);
    QVERIFY(view.isLineVisible(kLineOf[0]));

    // The next GO: the line being called scrolls into view, a long way down.
    QVERIFY(!view.isLineVisible(kLineOf[1]));
    QVERIFY(view.go());
    settle();
    view.refresh();
    QCOMPARE(view.nowCue(), m_cues[1]);
    QCOMPARE(view.nowLine(), kLineOf[1]);
    QVERIFY(view.isLineVisible(kLineOf[1]));
    QVERIFY(view.firstVisibleLine() > kLineOf[0]);
    QCOMPARE(view.standbyLine(), kLineOf[2]);
}

void CallingScriptView_Test::followOffLeavesTheScrollAlone()
{
    ShowTreeController *st = m_doc->showTree();
    for (int i = 0; i < 3; ++i)
        QVERIFY(st->anchorCue(m_prod, m_cues[i], offsetOfLine(kLineOf[i])));
    CallingScriptView view(m_doc, m_prod);
    view.resize(800, 400);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setFollows(false);
    QVERIFY(view.go());
    settle();
    QVERIFY(view.go());
    settle();
    view.refresh();
    QCOMPARE(view.nowLine(), kLineOf[1]);               // still marked
    QVERIFY(!view.isLineVisible(kLineOf[1]));           // but not scrolled to
    view.setFollows(true);
    QVERIFY(view.isLineVisible(kLineOf[1]));            // and Follow brings it in
}

void CallingScriptView_Test::goGoesThroughTheRunPath()
{
    ShowTreeController *st = m_doc->showTree();
    CallingScriptView view(m_doc, m_prod);
    QStringList seen;
    ShowControl *sc = m_doc->showControl();
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route route) {
        seen << QString("%1/%2").arg(ShowControl::commandName(r.command), ShowControl::routeName(route));
        return sc->execute(r);
    });
    QVERIFY(view.go());
    settle();
    QCOMPARE(seen, QStringList({ "GO/Run tab" }));
    QCOMPARE(st->runningCue(m_list), m_cues[0]);
    QPushButton *goBtn = view.findChild<QPushButton *>("scriptGo");
    QVERIFY(goBtn != nullptr && goBtn->isEnabled());
    goBtn->click();
    settle();
    QCOMPARE(seen.count(), 2);
    QCOMPARE(st->runningCue(m_list), m_cues[1]);
}

void CallingScriptView_Test::importAndPdf()
{
    QVERIFY(m_doc->showTree()->clearScript(m_prod));
    CallingScriptView view(m_doc, m_prod);
    QVERIFY(view.isEditing());                          // nothing yet: type or paste
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile f(dir.filePath("act.md"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("# Act 1\nFirst line\nSecond line\n");
    f.close();
    QString error;
    QVERIFY2(view.importFile(f.fileName(), &error), qPrintable(error));
    QVERIFY(!view.isEditing());
    QCOMPARE(view.textEdit()->toPlainText(), QString("# Act 1\nFirst line\nSecond line\n"));
    QVERIFY(view.sheet().lines().first().heading);

    QVERIFY(m_doc->showTree()->anchorCue(m_prod, m_cues[0], 10));
    QVERIFY2(view.savePdf(dir.filePath("s.pdf"), &error), qPrintable(error));
    QVERIFY(QFileInfo(dir.filePath("s.pdf")).size() > 1000);
    QVERIFY(!view.savePdf(dir.filePath("none/s.pdf"), &error));
}

void CallingScriptView_Test::typedTextKeepsCuesOnTheirWords()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->anchorCue(m_prod, m_cues[0], offsetOfLine(5)));
    CallingScriptView view(m_doc, m_prod);
    view.setEditing(true);
    QTextCursor c(view.textEdit()->document());
    c.setPosition(0);
    c.insertText("Preset\nPreset 2\n");
    // The margin waits while the text is being typed; Done writes it back.
    view.commitEdit();
    QVERIFY(!view.isEditing());
    QCOMPARE(view.sheet().lineOfCue(m_cues[0]), 7);
    QCOMPARE(st->callingScript(m_prod).title, QString("song.txt"));
}

void CallingScriptView_Test::theShowTreeMenuOpensIt()
{
    ShowTreeView tree(nullptr, m_doc);
    QToolButton *more = tree.findChild<QToolButton *>("stMore");
    QVERIFY(more != nullptr && more->menu() != nullptr);
    QAction *open = nullptr;
    for (QAction *a : more->menu()->actions())
        if (a->objectName() == "stCallingScriptAction")
            open = a;
    QVERIFY(open != nullptr);
    QCOMPARE(open->text(), QString("Calling Script…"));
    more->menu()->aboutToShow();
    QVERIFY(open->isEnabled());
    open->trigger();
    CallingScriptView *view = tree.findChild<CallingScriptView *>("callingScriptView");
    QVERIFY(view != nullptr);
    QVERIFY(view->isVisible());
    QCOMPARE(view->productionId(), m_prod);
    view->close();
}

QTEST_MAIN(CallingScriptView_Test)
