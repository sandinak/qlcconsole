/*
  Q Light Controller Plus - Test Unit
  librarybuild_test.cpp

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
#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QDropEvent>
#include <QTabWidget>
#include <QSplitter>
#include <QLineEdit>
#include <QMimeData>
#include <QPointer>
#include <QWidget>
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "showtreeview.h"
#include "workbenchcommand.h"
#include "workbench.h"
#undef protected
#undef private
#ifdef QLC_AI_ASSIST
#include "heldmodel.h"
#include "aiassist.h"
#endif

#include "functionstreewidget.h"
#include "showtreecontroller.h"
#include "operationregistry.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "librarybuild_test.h"

namespace
{

/* Build and the Show Tree as two tabs of one window, as the app has them. */
struct Rig
{
    QWidget window;
    QTabWidget *tabs = nullptr;
    ProgrammingManager *pm = nullptr;
    ShowTreeView *view = nullptr;
    Scene *mine = nullptr;

    explicit Rig(Doc *doc, bool withTree = true)
    {
        window.resize(1500, 800);
        tabs = new QTabWidget(&window);
        tabs->resize(1500, 800);
        pm = new ProgrammingManager(tabs, doc);
        tabs->addTab(pm, "Build");
        if (withTree)
        {
            view = new ShowTreeView(tabs, doc);
            tabs->addTab(view, "Show Tree");
        }
        mine = new Scene(doc);
        mine->setName("Mine");
        doc->addFunction(mine);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);
        tabs->setCurrentWidget(pm);
        pm->showFunction(mine->id());
    }
};

/* A drop as the view delivers it, on @a onto's row. */
bool dropOn(ProgrammingOutline *o, QMimeData *mime, QTreeWidgetItem *onto)
{
    const QPoint pos = onto != nullptr ? o->visualItemRect(onto).center() : QPoint(10, o->viewport()->height() - 4);
    QDropEvent ev(QPointF(pos), Qt::CopyAction | Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    o->dropEvent(&ev);
    const bool ok = ev.isAccepted();
    delete mime;
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    return ok;
}

}

void LibraryBuild_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
    FixtureGroup *towers = new FixtureGroup(m_doc);
    towers->setName("Towers");
    m_doc->addFixtureGroup(towers);
    towers->assignFixture(m_fixtures[0]);
    towers->assignFixture(m_fixtures[1]);
    m_towers = towers->id();
    FixtureGroup *usts = new FixtureGroup(m_doc);
    usts->setName("USTs");
    m_doc->addFixtureGroup(usts);
    usts->assignFixture(m_fixtures[2]);
    usts->assignFixture(m_fixtures[3]);
    m_usts = usts->id();
    m_blue = colour(QColor(Qt::blue), "Blue");
    m_red = colour(QColor(Qt::red), "Red");
    m_half = dimmer(128, "Half");
    m_doc->setMode(Doc::Design);
}

void LibraryBuild_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

quint32 LibraryBuild_Test::colour(const QColor &c, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color, m_doc);
    p->setName(name);
    p->setValue(QLCPalette::colorToString(c, QColor(Qt::black)));
    m_doc->addPalette(p);
    return p->id();
}

quint32 LibraryBuild_Test::dimmer(int level, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName(name);
    p->setValue(level);
    m_doc->addPalette(p);
    return p->id();
}

void LibraryBuild_Test::theLibraryIsOneInstance()
{
    Rig rig(m_doc);
    QVERIFY(rig.pm->isLibraryShared());
    QVERIFY(rig.view->isLibraryLent());
    // The very widget and model of the Show Tree's Library, in Build's pane.
    QTreeWidget *lib = rig.view->library();
    QVERIFY(lib != nullptr);
    QVERIFY(rig.pm->libraryHolder()->isAncestorOf(lib));
    QVERIFY(rig.pm->libraryHolder()->isVisible());
    QVERIFY(lib->isVisible());
    QVERIFY(!rig.pm->m_fixGroupPane->isVisible());
    QVERIFY(!rig.view->isAncestorOf(lib) || rig.pm->libraryHolder()->isAncestorOf(lib));
    QCOMPARE(rig.view->libraryPane()->parentWidget(), rig.pm->libraryHolder());
    // It lists what the tab lists: groups, fixtures and palettes.
    QStringList names;
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
        names << (*it)->text(0);
    QVERIFY2(names.filter("Towers").count() > 0, qPrintable(names.join("|")));
    QVERIFY(names.contains("Blue"));

    // Recent is the one list: what Build applies is Recent in the Show Tree.
    rig.pm->m_workbench->load(rig.mine->id());
    QCOMPARE(rig.pm->currentSceneId(), rig.mine->id());
    rig.view->selectLibraryPalette(m_blue);
    QVERIFY(rig.view->applyLibrarySelection());
    QCOMPARE(rig.view->recentPalettes().value(0), m_blue);

    // To the Show Tree: the Library goes home, whole, in its splitter.
    rig.tabs->setCurrentWidget(rig.view);
    QTRY_VERIFY(!rig.view->isLibraryLent());
    QVERIFY(!rig.pm->isLibraryShared());
    QVERIFY(rig.view->libraryPane()->parentWidget() == rig.view->m_mainSplit);
    QVERIFY(rig.view->library()->isVisible());
    QCOMPARE(rig.view->library(), lib);
    QVERIFY(rig.pm->m_fixGroupPane->isVisibleTo(rig.pm));
    QVERIFY(!rig.pm->libraryHolder()->isVisibleTo(rig.pm));
    QCOMPARE(rig.view->recentPalettes().value(0), m_blue);

    // ...and back to Build: lent again.
    rig.tabs->setCurrentWidget(rig.pm);
    QTRY_VERIFY(rig.pm->isLibraryShared());
    QVERIFY(rig.pm->libraryHolder()->isAncestorOf(lib));
}

void LibraryBuild_Test::aDropInBuildAppliesAndUndoes()
{
    Rig rig(m_doc);
    ProgrammingOutline *rows = rig.pm->m_workbench->rows();
    QVERIFY(rows != nullptr);
    QTreeWidgetItem *layer = rows->layerItem(0);
    QVERIFY(layer != nullptr);
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(!st->canUndoShared());

    // A group, dragged from the Library onto the layer.
    QVERIFY(rig.view->selectLibraryGroups({ m_towers }) == 1);
    QMimeData *groupMime = rig.view->librarySelectionMime();
    QVERIFY(groupMime != nullptr);
    QVERIFY(dropOn(rows, groupMime, rows->layerItem(0)));
    QVERIFY(rig.mine->fixtureGroups().contains(m_towers));
    // ...then a palette.
    rig.view->selectLibraryPalette(m_blue);
    QMimeData *palMime = rig.view->librarySelectionMime();
    QVERIFY(palMime != nullptr);
    QVERIFY(dropOn(rows, palMime, rows->layerItem(0)));
    QVERIFY(rig.mine->palettes().contains(m_blue));

    // One step each, on the shared history.
    QVERIFY(st->canUndoShared());
    rig.pm->slotUndo();
    QVERIFY(!rig.mine->palettes().contains(m_blue));
    QVERIFY(rig.mine->fixtureGroups().contains(m_towers));
    QVERIFY(st->undoShared());
    QVERIFY(!rig.mine->fixtureGroups().contains(m_towers));
    QVERIFY(st->redoShared());
    QVERIFY(rig.mine->fixtureGroups().contains(m_towers));

    // Return in the Library does the same for the selection: the second
    // palette, the hint saying where it goes.
    rig.view->selectLibraryPalette(m_red);
    QVERIFY2(rig.view->libraryReturnHint().contains("Mine"), qPrintable(rig.view->libraryReturnHint()));
    QVERIFY(rig.view->applyLibrarySelection());
    QVERIFY(rig.mine->palettes().contains(m_red));
    QVERIFY(st->undoShared());
    QVERIFY(!rig.mine->palettes().contains(m_red));
}

void LibraryBuild_Test::commandWordsInBuild()
{
    Rig rig(m_doc);
    BuildCommandBar *bar = rig.pm->commandBar();
    QVERIFY(bar != nullptr && bar->isVisible());
    ShowTreeController *st = m_doc->showTree();

    // Groups and a value: the scene gets the group and the palette.
    QVERIFY2(bar->runLine("Towers colour Blue"), qPrintable(bar->message()));
    QVERIFY(rig.mine->fixtureGroups().contains(m_towers));
    QVERIFY(rig.mine->palettes().contains(m_blue));
    // No cue was made or touched: it is the scene's.
    QVERIFY(st->productions().isEmpty());

    // One undo step for the whole line.
    QVERIFY2(bar->runLine("USTs colour Red dimmer 50"), qPrintable(bar->message()));
    QVERIFY(rig.mine->fixtureGroups().contains(m_usts));
    QVERIFY(rig.mine->palettes().contains(m_red));
    const QList<quint32> after = rig.mine->palettes();
    QCOMPARE(after.count(), 3);                          // Blue, Red, and a dimmer 50
    QVERIFY(bar->runLine("undo"));
    QVERIFY(bar->message().contains("Undid"));
    QVERIFY(!rig.mine->fixtureGroups().contains(m_usts));
    QVERIFY(!rig.mine->palettes().contains(m_red));
    QVERIFY(rig.mine->palettes().contains(m_blue));

    // A value alone: the scene's own look. Reuses the palette that plays it.
    QVERIFY2(bar->runLine("dimmer 50"), qPrintable(bar->message()));
    QCOMPARE(rig.mine->palettes().count(), 2);
    QVERIFY(bar->runLine("palette colour \"Amber\" #ffb000"));
    bool amber = false;
    for (const QLCPalette *p : m_doc->palettes())
        amber = amber || p->name() == "Amber";
    QVERIFY(amber);

    // A palette by name that isn't there, and a word that isn't one.
    QVERIFY(!bar->runLine("Towers colour Nonesuch"));
    QVERIFY(bar->message().contains("Nonesuch"));
    QVERIFY(!bar->runLine("Towers fade 2"));

    // An effect's fade is the cue's.
    QVERIFY(!bar->runLine("Towers effect Nonesuch fade 2"));

    // The line clears on success, stays on failure (as the tree's).
    bar->line()->setText("Towers colour Red");
    QTest::keyClick(bar->line(), Qt::Key_Return);
    QVERIFY(bar->line()->text().isEmpty());
    QVERIFY(rig.mine->palettes().contains(m_red));
    bar->line()->setText("Towers colour Nonesuch");
    QTest::keyClick(bar->line(), Qt::Key_Return);
    QCOMPARE(bar->line()->text(), QString("Towers colour Nonesuch"));
}

void LibraryBuild_Test::showTreeOnlyWordsAreRefused()
{
    Rig rig(m_doc);
    BuildCommandBar *bar = rig.pm->commandBar();
    ShowTreeController *st = m_doc->showTree();
    const QStringList lines = { "cue 1.0.1 \"Verse\"", "list \"01-Opener\"", "record", "include 0.0.3", "skip",
                                "mib on", "reusable", "detach", "convert", "look \"Hit\"", "wb", "go", "production \"P\"",
                                "release Towers", "textcolour red" };
    for (const QString &line : lines)
    {
        QVERIFY2(!bar->runLine(line), qPrintable(line));
        QVERIFY2(bar->message().contains("Show Tree only"), qPrintable(line + ": " + bar->message()));
    }
    // Nothing was made, in the tree or in the scene.
    QVERIFY(st->productions().isEmpty());
    QVERIFY(rig.mine->palettes().isEmpty());
    QVERIFY(rig.mine->fixtureGroups().isEmpty());
    QVERIFY(!st->canUndoShared());
    QVERIFY(ShowTreeCommand::showTreeOnlyWord("Cue"));
    QVERIFY(!ShowTreeCommand::showTreeOnlyWord("palette"));
    QVERIFY(!ShowTreeCommand::showTreeOnlyWord("colour"));
}

void LibraryBuild_Test::helpAndQuestions()
{
    Rig rig(m_doc);
    BuildCommandBar *bar = rig.pm->commandBar();
    // ? and help: the language, in the pane.
    QVERIFY(bar->runLine("?"));
    QVERIFY(bar->isHelpShown());
    QVERIFY2(bar->helpHtml().contains("colour"), "help lists the words");
    QVERIFY(bar->runLine("help colour"));
    QVERIFY(bar->helpHtml().contains("Blue") || bar->helpHtml().contains("colour"));
    // The registry's questions run here too, changing nothing.
    QVERIFY2(bar->runLine("groups"), qPrintable(bar->message()));
    QVERIFY(bar->helpHtml().contains("Towers"));
    QVERIFY2(bar->runLine("fixtures"), qPrintable(bar->message()));
    QVERIFY(rig.mine->palettes().isEmpty());
    // "? question" is AI Assist's: with no provider it says so and nothing
    // runs (it is the registry's `?` word, not a grammar error).
    bar->runLine("? which groups are there");
    QVERIFY(!bar->message().contains("Show Tree only"));

    // With the Workbench lent to the Show Tree there is nothing loaded.
    Workbench *lent = rig.pm->lendWorkbench();
    QVERIFY(lent != nullptr);
    QVERIFY(!bar->runLine("Towers colour Blue"));
    QVERIFY(bar->message().contains("Workbench"));
    rig.pm->returnWorkbench(lent);
    QVERIFY(bar->runLine("Towers colour Blue"));
}

/* 0.4.1: Build's command bar has the Show Tree's one-line fallback: a
   typo gets "Did you mean" (Esc puts back what was typed), plain words go
   to AI Assist with the line busy meanwhile, and what the assistant
   proposes runs as a line of the bar -- onto the scene. The Show Tree's
   line, beside it, shows nothing of a question Build asked. */
void LibraryBuild_Test::commandBarTypoOrLanguage()
{
#ifndef QLC_AI_ASSIST
    QSKIP("built without AI Assist");
#else
    HeldModel model;
    QSettings().setValue(QStringLiteral("ai/enabled"), true);
    QSettings().setValue(QStringLiteral("ai/provider"), QStringLiteral("ollama"));
    QSettings().setValue(QStringLiteral("ai/endpoint"), model.endpoint());
    QSettings().setValue(QStringLiteral("ai/model"), QStringLiteral("test-model"));
    {
        AiAssist ai(m_doc);
        ai.settingsChanged();
        Rig rig(m_doc);
        BuildCommandBar *bar = rig.pm->commandBar();
        QLineEdit *line = bar->line();
        QAction *spinner = bar->findChild<QAction *>("aiBusySpinner");
        QVERIFY(spinner != nullptr);
        QVERIFY(line->placeholderText().contains("plain words"));

        // A typo: the fix in the line; Esc puts back what was typed.
        line->setText("Towrs colour Blu");
        QVERIFY(!bar->runLine(line->text()));
        QCOMPARE(line->text(), QStringLiteral("Towers colour Blue"));
        QVERIFY2(bar->message().contains("Did you mean"), qPrintable(bar->message()));
        line->setFocus();
        QTest::keyClick(line, Qt::Key_Escape);
        QCOMPARE(line->text(), QStringLiteral("Towrs colour Blu"));
        QTest::qWait(50);
        QCOMPARE(model.requests, 0);
        QVERIFY(rig.mine->palettes().isEmpty());

        // Plain words: asked; the bar is busy, the Show Tree's line isn't.
        line->setText("make the towers blue for the chorus");
        QTest::keyClick(line, Qt::Key_Return);
        QVERIFY(line->text().isEmpty());
        QVERIFY2(bar->message().startsWith("Asking AI Assist (test-model)"), qPrintable(bar->message()));
        QVERIFY(line->actions().contains(spinner));
        QTRY_COMPARE_WITH_TIMEOUT(model.requests, 1, 3000);
        QAction *treeSpinner = rig.view->findChild<QAction *>("aiBusySpinner");
        QVERIFY(treeSpinner != nullptr);
        QVERIFY(!rig.view->commandLine()->actions().contains(treeSpinner));
        QVERIFY(!rig.view->commandMessage().contains("Asking"));
        QTest::keyClick(line, Qt::Key_Escape);
        QVERIFY(!line->actions().contains(spinner));

        // What it proposes is a line of the bar: Enter puts it on the scene.
        const QJsonObject args { { "targets", QJsonArray { "Towers" } },
                                 { "looks", QJsonArray { QJsonObject { { "attribute", "colour" }, { "value", "Blue" } } } } };
        const QJsonObject call { { "id", "call_1" }, { "type", "function" },
                                 { "function", QJsonObject { { "name", "set_look" },
                                                             { "arguments", QString::fromUtf8(QJsonDocument(args).toJson()) } } } };
        model.replies = {
            QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject { { "message", QJsonObject {
                { "role", "assistant" }, { "content", "" }, { "tool_calls", QJsonArray { call } } } } } } } }).toJson(),
            QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject { { "message", QJsonObject {
                { "role", "assistant" }, { "content", "Towers colour Blue: Enter applies it." } } } } } } }).toJson() };
        line->setText("paint the towers blue please");
        QTest::keyClick(line, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(line->text(), QStringLiteral("Towers colour Blue"), 5000);
        QVERIFY(!line->actions().contains(spinner));
        QVERIFY(rig.mine->palettes().isEmpty());            // nothing yet
        QVERIFY2(bar->message().contains("Enter applies"), qPrintable(bar->message()));
        QTest::keyClick(line, Qt::Key_Return);
        QVERIFY2(rig.mine->fixtureGroups().contains(m_towers), qPrintable(bar->message()));
        QVERIFY(rig.mine->palettes().contains(m_blue));
        QVERIFY(line->text().isEmpty());
        QVERIFY(m_doc->showTree()->productions().isEmpty());   // no cue was made

        // Operate: plain words ask no one, and say AI Assist is Design's.
        m_doc->setMode(Doc::Operate);
        QTest::qWait(100);
        const int before = model.requests;
        QVERIFY(!bar->runLine("make the towers blue for the chorus"));
        QVERIFY2(bar->message().contains("Design mode only"), qPrintable(bar->message()));
        QTest::qWait(50);
        QCOMPARE(model.requests, before);
        m_doc->setMode(Doc::Design);

        // QLC_SHOT_DIR: the bar at three sizes -- a typo's fix, plain words
        // with AI Assist working (seconds showing), and its proposal.
        const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
        if (!shots.isEmpty())
        {
            QDir().mkpath(shots);
            const QList<QByteArray> proposal = {
                QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject { { "message", QJsonObject {
                    { "role", "assistant" }, { "content", "" }, { "tool_calls", QJsonArray { call } } } } } } } }).toJson(),
                QJsonDocument(QJsonObject { { "choices", QJsonArray { QJsonObject { { "message", QJsonObject {
                    { "role", "assistant" }, { "content", "Towers colour Blue: Enter applies it." } } } } } } }).toJson() };
            for (const QSize &size : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
            {
                const QString tag = QString("%1x%2").arg(size.width()).arg(size.height());
                rig.window.resize(size);
                rig.tabs->resize(size);
                QTest::qWait(100);
                line->setText("Towrs colour Blu");
                QVERIFY(!bar->runLine(line->text()));
                QTest::qWait(50);
                rig.window.grab().save(shots + "/build-typo-didyoumean-" + tag + ".png");
                line->clear();
                model.replies.clear();
                line->setText("make the towers blue for the chorus");
                QTest::keyClick(line, Qt::Key_Return);
                QTest::qWait(2300);
                rig.window.grab().save(shots + "/build-language-busy-" + tag + ".png");
                QTest::keyClick(line, Qt::Key_Escape);
                model.replies = proposal;
                line->setText("paint the towers blue please");
                QTest::keyClick(line, Qt::Key_Return);
                QTRY_COMPARE_WITH_TIMEOUT(line->text(), QStringLiteral("Towers colour Blue"), 5000);
                QTest::qWait(50);
                rig.window.grab().save(shots + "/build-language-proposal-" + tag + ".png");
                QTest::keyClick(line, Qt::Key_Escape);
                line->clear();
            }
        }
    }
    for (const char *k : { "ai/enabled", "ai/provider", "ai/endpoint", "ai/model" })
        QSettings().remove(QString::fromLatin1(k));
#endif
}

void LibraryBuild_Test::withoutAShowTreeView()
{
    Rig rig(m_doc, false);
    QVERIFY(!rig.pm->isLibraryShared());
    QVERIFY(rig.pm->m_fixGroupPane->isVisible());
    QVERIFY(rig.pm->commandBar() != nullptr);
    QVERIFY2(rig.pm->commandBar()->runLine("Towers colour Blue"), qPrintable(rig.pm->commandBar()->message()));
    QVERIFY(rig.mine->palettes().contains(m_blue));
}

QTEST_MAIN(LibraryBuild_Test)
