/*
  Q Light Controller Plus
  appviews_test.cpp

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

#include <QDockWidget>
#include <QtTest>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QXmlStreamReader>
#include <QFile>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QTimer>
#include <QMessageBox>

#include "cuerunner.h"
#include "showtreemodel.h"
#include "showtreeview.h"
#include "showtreecontroller.h"
#include <QtMath>
#include "appviews_test.h"

#define private public
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixture.h"
#include "doc.h"
#include "monitor.h"
#include "monitorproperties.h"
#define protected public
#include "app.h"
#include "appsettings.h"
#include "shutterautoopen.h"
#include <QTabWidget>
#include "measurementunits.h"
#include "functionmanager.h"
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "efxeditor.h"
#include "efx.h"
#define private public
#define protected public
#include "virtualconsole.h"
#include "fixturemanager.h"
#include "inputoutputmanager.h"
#undef protected
#undef private
#include "helpbrowser.h"
#include "simpledesk.h"
#include "showmanager.h"
#include "showconverter.h"
#include <QToolButton>
#include "inputoutputmap.h"
#include "networkroute.h"
#include "outputpatch.h"
#include "iopluginstub.h"
#include "ioplugincache.h"
#include "qlcfile.h"
#include "showstatus.h"
#include "speedmastersfooter.h"
#undef protected
#undef private

void AppViews_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
}

void AppViews_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

/* Put a detached view back where it came from, the same way closing its
   window does in the app. */
static void reattach(App *app, const QString &label)
{
    foreach (DetachedContext *dw, app->findChildren<DetachedContext *>())
    {
        QWidget *w = dw->centralWidget();
        if (w != NULL && w->property("tabLabel").toString() == label)
        {
            dw->close();
            return;
        }
    }
}

void AppViews_Test::jumpingToADockedViewSurvivesAnEarlierDetach()
{
    QTabWidget *tabs = m_app->m_tab;
    QVERIFY2(tabs->count() > 3, "need several tabs to shift one past another");

    const QString first = tabs->tabBar()->tabData(0).toString();
    const QString third = tabs->tabBar()->tabData(2).toString();
    QVERIFY(first.isEmpty() == false);
    QVERIFY(third.isEmpty() == false);
    QVERIFY(first != third);

    /* Detaching the FIRST tab pulls it out of the tab bar, so every view after
       it now sits one index lower than the order it was built in. A jump that
       remembers construction order lands on the wrong page from here on. */
    m_app->slotDetachContext(0);
    QCOMPARE(tabs->tabBar()->tabData(1).toString(), third);

    m_app->showContext(third);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), third);

    reattach(m_app, first);
}

void AppViews_Test::jumpingToADetachedViewRaisesItsWindow()
{
    QTabWidget *tabs = m_app->m_tab;
    QVERIFY(tabs->count() > 2);

    const QString label = tabs->tabBar()->tabData(1).toString();
    QVERIFY(label.isEmpty() == false);

    m_app->slotDetachContext(1);

    DetachedContext *dw = NULL;
    foreach (DetachedContext *w, m_app->findChildren<DetachedContext *>())
    {
        QWidget *c = w->centralWidget();
        if (c != NULL && c->property("tabLabel").toString() == label)
            dw = w;
    }
    QVERIFY2(dw != NULL, "detaching produced no window for that view");

    /* Buried behind something else. Picking the view from the menu has to
       bring it back -- with an index-based jump there was no tab left to
       select, so nothing happened at all. */
    dw->hide();
    QVERIFY(dw->isVisible() == false);

    m_app->showContext(label);
    QVERIFY2(dw->isVisible(),
             "picking an already-detached view did not bring its window forward");

    reattach(m_app, label);
}

void AppViews_Test::theWindowMenuFindsAndPutsBackBrokenOutViews()
{
    /* "the menubar needs a windows thing .. if I break out a tab it's hard to
       find": every view in a Window menu, marked when it is in its own window,
       and put back from there. */
    QMenu *windowMenu = m_app->menuBar()->findChild<QMenu *>("windowMenu");
    QVERIFY(windowMenu != nullptr);
    QTabWidget *tabs = m_app->m_tab;
    const int views = tabs->count();
    // And Functions and Shows, which have no tab in the qlcconsole view.
    QCOMPARE(m_app->m_windowJumps.size(), views + 2);
    // The Rig Overview is a window too: offered here as well as under View.
    QVERIFY(windowMenu->actions().contains(m_app->m_rigOverviewAction));
    QCOMPARE(m_app->m_rigOverviewAction->shortcut(),
             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

    const QString label = tabs->tabBar()->tabData(2).toString();
    m_app->slotDetachContext(2);
    QCOMPARE(m_app->detachedContexts(), QStringList() << label);
    m_app->updateWindowMenu();
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == label)
            jump = a;
    QVERIFY(jump != nullptr);
    QVERIFY2(jump->text().contains("in its own window"), qPrintable(jump->text()));
    QVERIFY(m_app->m_windowDockMenu->isEnabled());
    QCOMPARE(m_app->m_windowDockMenu->actions().size(), 1);
    QCOMPARE(m_app->m_windowDockMenu->actions().first()->text(), label);

    // Put back from the menu: a tab again, and shown.
    m_app->m_windowDockMenu->actions().first()->trigger();
    QCOMPARE(tabs->count(), views);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), label);
    m_app->updateWindowMenu();
    QVERIFY(!jump->text().contains("in its own window"));
    QVERIFY(!m_app->m_windowDockAll->isEnabled());

    // Two out, all put back at once.
    m_app->slotDetachContext(1);
    m_app->slotDetachContext(1);
    QCOMPARE(m_app->detachedContexts().size(), 2);
    m_app->reattachAllContexts();
    QVERIFY(m_app->detachedContexts().isEmpty());
    QCOMPARE(tabs->count(), views);
}

/* Functions has no tab ("I keep hitting it by mistake instead of
   Programming"), but opens from the Window menu in its own window; closed,
   it is put away again -- not made a tab. */
void AppViews_Test::shiftClickBreaksATabOutDoubleClickDoesNot()
{
    QTabWidget *tabs = m_app->m_tab;
    QTabBar *bar = tabs->tabBar();
    QVERIFY(tabs->count() > 2);
    const bool was = m_app->shiftClickBreaksOut();
    m_app->setShiftClickBreaksOut(true);
    const int count = tabs->count();
    const QString label = bar->tabData(1).toString();
    const QPoint at = bar->tabRect(1).center();

    // A plain double-click selects the tab and nothing more.
    QTest::mouseDClick(bar, Qt::LeftButton, Qt::NoModifier, at);
    QCoreApplication::processEvents();
    QCOMPARE(tabs->count(), count);
    QVERIFY(m_app->detachedContexts().isEmpty());

    // Shift-click breaks it out.
    QTest::mouseClick(bar, Qt::LeftButton, Qt::ShiftModifier, at);
    QTRY_COMPARE(tabs->count(), count - 1);
    QCOMPARE(m_app->detachedContexts(), QStringList({ label }));
    reattach(m_app, label);
    QTRY_COMPARE(tabs->count(), count);

    // Every tab, Programming included ("1 programming 2. nothing at all").
    for (int i = 0; i < count; ++i)
    {
        const QString each = bar->tabData(i).toString();
        tabs->setCurrentIndex(i);                    // the tab you're on, too
        QTest::mouseClick(bar, Qt::LeftButton, Qt::ShiftModifier, bar->tabRect(i).center());
        QTRY_VERIFY2(m_app->detachedContexts().contains(each), qPrintable(each + " did not break out"));
        reattach(m_app, each);
        QTRY_COMPARE(tabs->count(), count);
    }

    // Right-click a tab: Open in Its Own Window -- every tab, and whether or
    // not Shift-click is on ("can we have a right hand click menu on the tab").
    for (int i = 0; i < count; ++i)
    {
        const QString each = bar->tabData(i).toString();
        QMenu *menu = m_app->tabContextMenu(i);
        QAction *open = menu->findChild<QAction *>("tabOpenInWindow");
        QVERIFY(open != nullptr);
        QCOMPARE(open->text(), QString("Open in Its Own Window"));
        open->trigger();
        delete menu;
        QTRY_VERIFY2(m_app->detachedContexts().contains(each), qPrintable(each + " did not open in a window"));
        reattach(m_app, each);
        QTRY_COMPARE(tabs->count(), count);
    }

    // Turned off from the Window menu: a Shift-click is just a click.
    QAction *toggle = m_app->menuBar()->findChild<QAction *>("windowShiftClick");
    QVERIFY(toggle != nullptr);
    QVERIFY(toggle->isChecked());
    toggle->trigger();
    QVERIFY(m_app->shiftClickBreaksOut() == false);
    QTest::mouseClick(bar, Qt::LeftButton, Qt::ShiftModifier, bar->tabRect(1).center());
    QCoreApplication::processEvents();
    QCOMPARE(tabs->count(), count);

    // Move to Its Own Window takes the view in front, gesture or no gesture.
    tabs->setCurrentIndex(2);
    const QString front = bar->tabData(2).toString();
    m_app->updateWindowMenu();
    QAction *move = m_app->menuBar()->findChild<QAction *>("windowBreakOut");
    QVERIFY(move != nullptr);
    QCOMPARE(move->text(), QString("Move %1 to Its Own Window").arg(front));
    move->trigger();
    QCOMPARE(m_app->detachedContexts(), QStringList({ front }));
    reattach(m_app, front);
    QTRY_COMPARE(tabs->count(), count);

    m_app->setShiftClickBreaksOut(was);
}

void AppViews_Test::theBuildTabWearsAHardHat()
{
    QTabWidget *tabs = m_app->m_tab;
    int build = -1;
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabBar()->tabData(i).toString() == QStringLiteral("Build"))
            build = i;
    QVERIFY(build >= 0);
    const QIcon icon = tabs->tabIcon(build);
    QCOMPARE(icon.name(), QStringLiteral("tab:build"));

    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == QStringLiteral("Build"))
            jump = a;
    QVERIFY(jump != nullptr);
    QCOMPARE(jump->icon().name(), QStringLiteral("tab:build"));

    // Drawn at the size asked for: hat yellow with an amber edge, at 1x and 2x
    for (qreal dpr : { 1.0, 2.0 })
    {
        const QImage img = icon.pixmap(QSize(16, 16), dpr).toImage();
        QCOMPARE(img.width(), int(16 * dpr));
        int yellow = 0, amber = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.alpha() < 200)
                    continue;
                if (c.red() > 230 && c.green() > 180 && c.blue() < 90)
                    ++yellow;
                else if (c.red() > 120 && c.red() < 190 && c.green() < 140 && c.blue() < 40)
                    ++amber;
            }
        QVERIFY2(yellow > img.width() * img.height() / 8, qPrintable(QString::number(yellow)));
        QVERIFY2(amber > img.width(), qPrintable(QString::number(amber)));
        const QString dir = QString::fromLocal8Bit(qgetenv("APPVIEWS_SHOTS"));
        if (!dir.isEmpty())
            img.save(QStringLiteral("%1/build-hat@%2x.png").arg(dir).arg(int(dpr)));
    }
}

void AppViews_Test::windowFunctionsOpensTheFunctionsTab()
{
    QTabWidget *tabs = m_app->m_tab;
    const QString functions = QString("Functions");
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    for (int i = 0; i < tabs->count(); ++i)
        QVERIFY2(tabs->tabBar()->tabData(i).toString() != functions, "Functions should have no tab");
    // Hidden, not gone: the one switch (setFunctionsWindowShown) shows it.
    QVERIFY(FunctionManager::instance() != nullptr);
    QVERIFY(!m_app->functionsWindowShown());
    QVERIFY(m_app->m_parkedContexts.contains(FunctionManager::instance()));
    QVERIFY(!FunctionManager::instance()->isVisible());
    ProgrammingManager *build = ProgrammingManager::instance();
    QVERIFY(build != nullptr);
    // No Classic arrangement left in Build (Q73).
    QVERIFY(build->findChild<QToolButton *>("arrangeClassic") == nullptr);
    const int views = tabs->count();

    // Still in the Window menu, at its old place; no shortcut, as it has no
    // tab here (Q69: ⇧⌘4 is the view's fourth tab, Build).
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == functions)
            jump = a;
    QVERIFY2(jump != nullptr, "Functions should be in the Window menu");
    QCOMPARE(jump->objectName(), QString("windowFunctions"));
    QVERIFY(jump->shortcut().isEmpty());
    QCOMPARE(m_app->m_windowJumps.indexOf(jump), 3);
    QCOMPARE(m_app->m_windowJumps.at(4)->data().toString(), QString("Build"));
    QCOMPARE(m_app->m_windowJumps.at(4)->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_4));

    // A show saved with the Functions tab in front doesn't switch view.
    m_app->showContext("Virtual Console");
    m_app->setActiveWindow("FunctionManager");
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), QString("Virtual Console"));

    // Opening a function from outside Build: Build's Workbench.
    Doc *doc = m_app->m_doc;
    EFX *efx = new EFX(doc);
    efx->setName("Window Functions EFX");
    QVERIFY(doc->addFunction(efx));
    m_app->showContext("Virtual Console");
    QVERIFY(ProgrammingManager::openFunction(efx->id()));
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(build));
    QCOMPARE(build->workbench()->loadedFunction(), efx->id());
    QVERIFY(build->workbench()->findChild<EFXEditor *>() != nullptr);
    doc->deleteFunction(efx->id());

    // Window → Functions: the stock tab, in Everything, with the notice.
    jump->trigger();
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QVERIFY(m_app->functionsWindowShown());
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(FunctionManager::instance()));
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QVERIFY2(m_app->m_viewNoticeText->text().contains("Functions isn't in the qlcconsole view"),
             qPrintable(m_app->m_viewNoticeText->text()));
    QCOMPARE(tabs->count(), views + 2);   // Everything: + Functions, Shows
    m_app->setTabLabelMode(App::TabTextOnly);
    for (int i = 0; i < tabs->count(); ++i)
        QCOMPARE(tabs->tabText(i), tabs->tabBar()->tabData(i).toString());
    m_app->setTabLabelMode(App::TabIconAndText);

    // Back in qlcconsole: no tab again.
    m_app->m_viewNoticeButton->click();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->count(), views);
    QCOMPARE(tabs->indexOf(FunctionManager::instance()), -1);
}

/* F1 opens the guide for what is in front: each view its own, a dialog or
   view inside one its own, and the Help menu lists the guides. */
void AppViews_Test::windowEffectsIsBuildEffects()
{
    QAction *effects = m_app->menuBar()->findChild<QAction *>("windowEffects");
    QVERIFY(effects != nullptr);
    QCOMPARE(effects->text(), QString("Effects"));   // no "…": no window of its own
    ProgrammingManager *pm = ProgrammingManager::instance();
    QVERIFY(pm != nullptr);
    pm->setSection(ProgrammingManager::ScenesSection);
    const int windowsBefore = QApplication::topLevelWidgets().count();
    effects->trigger();
    QCOMPARE(m_app->m_tab->currentWidget(), static_cast<QWidget *>(pm));
    QCOMPARE(pm->section(), ProgrammingManager::EffectsSection);
    QCOMPARE(QApplication::topLevelWidgets().count(), windowsBefore);
    foreach (QWidget *w, QApplication::topLevelWidgets())
        QVERIFY2(QString(w->metaObject()->className()) != "EffectsPage", "an Effects window opened");
    pm->setSection(ProgrammingManager::ScenesSection);
}

void AppViews_Test::helpOpensOnWhatIsInFront()
{
    QTabWidget *tabs = m_app->m_tab;
    // Every tab, the Lighting Studio and Shows included (Q79: Everything).
    m_app->setWorkspaceView(App::EverythingView);
    auto back = qScopeGuard([this]() { m_app->setWorkspaceView(App::ConsoleView); });
    QHash<QString, QString> want = {
        { "Connections", "other-improvements.md#connections--patch" },
        { "Fixtures", "fixture-manager.md" },
        { "Lighting Studio", "lighting-studio.md" },
        { "Build", "build.md" },
        { "Virtual Console", "getting-started.md#running-the-show" },
        { "Shows", "shows.md" } };
    for (int i = 0; i < tabs->count(); ++i)
    {
        const QString label = tabs->tabBar()->tabData(i).toString();
        if (!want.contains(label))
            continue;
        tabs->setCurrentIndex(i);
        QCOMPARE(m_app->helpTopicFor(tabs->widget(i)), want.value(label));
        want.remove(label);
    }
    QVERIFY2(want.isEmpty(), qPrintable("no tab for: " + want.keys().join(", ")));
    QCOMPARE(m_app->helpTopicFor(FunctionManager::instance()), QString("other-improvements.md#window-menu"));

    // Something inside a view that names its own topic wins over the view.
    QWidget *inner = new QWidget(tabs->currentWidget());
    inner->setProperty("helpTopic", QStringLiteral("effects.md"));
    QWidget *deeper = new QWidget(inner);
    QCOMPARE(m_app->helpTopicFor(deeper), QString("effects.md"));
    delete inner;

    QVERIFY(m_app->m_helpContextAction != nullptr);
    QVERIFY(m_app->m_helpContextAction->shortcuts().contains(QKeySequence(Qt::Key_F1)));
    QMenu *help = m_app->menuBar()->findChild<QMenu *>("helpMenu");
    QVERIFY(help != nullptr);
    QStringList items;
    foreach (QAction *a, help->actions())
        items << a->text();
    for (const char *want2 : { "Getting Started", "What's New", "Keyboard and Mouse", "Troubleshooting" })
        QVERIFY2(items.contains(QString(want2)), qPrintable(QString("Help lacks %1: %2").arg(want2, items.join(" | "))));

    // And it opens, on the topic.
    HelpBrowser *browser = HelpBrowser::open(QStringLiteral("lighting-studio.md#mounting-a-fixture"));
    QVERIFY(browser != nullptr && browser->isVisible());
    QCOMPARE(browser->currentFile(), QString("lighting-studio.md"));
    QCOMPARE(browser->currentHeading(), QString("Mounting a fixture"));
    browser->close();
}

/* The Virtual Console's add-widget keys (Design mode) are not keys the
   app already uses: two shortcuts on one key fire neither -- ⇧⌘R and ⇧⌘M
   did nothing on the console tab. */
void AppViews_Test::virtualConsoleKeysDontClashWithTheApp()
{
    VirtualConsole *vc = VirtualConsole::instance();
    QVERIFY(vc != nullptr);
    vc->enableEdit();
    QSet<QString> app;
    foreach (QAction *a, m_app->findChildren<QAction *>())
    {
        QWidget *owner = qobject_cast<QWidget *>(a->parent());
        if (owner == vc || (owner != nullptr && vc->isAncestorOf(owner)))
            continue;
        foreach (const QKeySequence &k, a->shortcuts())
        {
            if (!k.isEmpty())
                app.insert(k.toString());
        }
    }
    const QList<QAction *> adds = { vc->m_addButtonMatrixAction, vc->m_addAnimationAction,
                                    vc->m_addButtonAction, vc->m_addSliderAction };
    for (QAction *a : adds)
        QVERIFY2(!app.contains(a->shortcut().toString()),
                 qPrintable(QString("%1 (%2) is also an app shortcut").arg(a->text(), a->shortcut().toString())));
    QCOMPARE(vc->m_addButtonMatrixAction->shortcut(), QKeySequence("CTRL+SHIFT+G"));
    QCOMPARE(vc->m_addAnimationAction->shortcut(), QKeySequence("CTRL+SHIFT+N"));
    vc->disableEdit();
}

void AppViews_Test::classicLooksLikeQlcPlus()
{
    m_app->setWorkspaceView(App::ClassicView);
    QToolBar *main = m_app->findChild<QToolBar *>("classicToolbar");
    QVERIFY(main != nullptr);
    QVERIFY(!main->isHidden());
    const QList<QAction *> acts = main->actions();
    for (QAction *a : { m_app->m_fileNewAction, m_app->m_fileOpenAction, m_app->m_fileSaveAction,
                        m_app->m_rigOverviewAction, m_app->m_addressToolAction, m_app->m_dumpDmxAction })
        QVERIFY2(acts.contains(a), qPrintable(a->text()));

    // The stock Fixtures and Functions toolbars.
    FixtureManager *fm = FixtureManager::instance();
    QVERIFY(fm != nullptr);
    QCOMPARE(fm->m_classicToolbarActions.count(), 15);       // 12 buttons, 3 separators
    for (QAction *a : fm->m_classicToolbarActions)
        QVERIFY(a->isVisible());
    QVERIFY(!fm->m_addFixtureButtonAction->isVisible());
    FunctionManager *fn = FunctionManager::instance();
    QVERIFY(fn != nullptr);
    for (QAction *a : fn->m_classicToolbarActions)
        QVERIFY(a->isVisible());
    QVERIFY(!fn->m_addButtonAction->isVisible());

    // The footer: no load, timecode or Next.
    QVERIFY(m_app->m_statusLoadLabel->isHidden());
    QVERIFY(m_app->m_statusTimecodeLabel->isHidden());
    QVERIFY(m_app->m_statusStandby->isHidden());
    const QString shots = qEnvironmentVariable("QLC_UX_SHOTS");
    if (!shots.isEmpty())
    {
        m_app->resize(1400, 860);
        m_app->showContext("Fixtures");
        QVERIFY(m_app->grab().save(shots + "/classic-fixtures.png"));
        m_app->showContext("Functions");
        QVERIFY(m_app->grab().save(shots + "/classic-functions.png"));
    }

    // Connections opens on QLC+'s own Universes page in Classic...
    InputOutputManager *io = InputOutputManager::instance();
    QVERIFY(io != nullptr);
    QCOMPARE(io->m_ioTabs->tabText(io->m_ioTabs->currentIndex()), QString("Universes"));

    // Back in qlcconsole: none of it, and Connections opens on Devices.
    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(io->m_ioTabs->tabText(io->m_ioTabs->currentIndex()), QString("Devices"));
    QVERIFY(main->isHidden());
    for (QAction *a : fm->m_classicToolbarActions)
        QVERIFY(!a->isVisible());
    QVERIFY(fm->m_addFixtureButtonAction->isVisible());
    QVERIFY(!m_app->m_statusTimecodeLabel->isHidden());
}

void AppViews_Test::editUndoSaysWhereItWorks()
{
    QAction *undo = m_app->menuBar()->findChild<QAction *>("editUndo");
    QVERIFY(undo != nullptr);
    QMenu *edit = nullptr;
    for (QAction *a : m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->menu()->actions().contains(undo))
            edit = a->menu();
    QVERIFY(edit != nullptr);
    ShowTreeController *st = m_app->m_doc->showTree();
    QVERIFY(st != nullptr);
    const quint32 prod = st->addProduction(QStringLiteral("Undo test"));   // a Show Tree step

    // The Virtual Console: no undo of its own, and it doesn't reach into
    // the Show Tree's.
    m_app->showContext("Virtual Console");
    emit edit->aboutToShow();
    QCOMPARE(undo->text(), QString("Nothing to undo on this tab"));
    QVERIFY(!undo->isEnabled());
    undo->trigger();
    QVERIFY(st->production(prod) != nullptr);

    // Fixtures: the rig's history, the Lighting Studio's too.
    Fixture *fx = new Fixture(m_app->m_doc);
    fx->setName("Undo me");
    fx->setChannels(1);
    QVERIFY(m_app->m_doc->addFixture(fx));
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    const QByteArray before = mon->rigSnapshot();
    fx->setName("Renamed");
    mon->pushRigUndo(before);
    m_app->showContext("Fixtures");
    emit edit->aboutToShow();
    QVERIFY2(undo->isEnabled() && undo->text().startsWith("Undo "), qPrintable(undo->text()));
    undo->trigger();
    QCOMPARE(fx->name(), QString("Undo me"));
    m_app->m_doc->deleteFixture(fx->id());

    // The Show Tree: its step, named, and undone.
    m_app->showContext("Show Tree");
    emit edit->aboutToShow();
    QVERIFY2(undo->isEnabled() && undo->text().startsWith("Undo "), qPrintable(undo->text()));
    undo->trigger();
    QTRY_VERIFY(st->production(prod) == nullptr);
}

void AppViews_Test::theRunTabIsTheCueRunner()
{
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == QLatin1String("Run"))
            jump = a;
    QVERIFY(jump != nullptr);
    // The qlcconsole view's eighth tab (Q69, Q79).
    QCOMPARE(jump->shortcut(), QKeySequence("CTRL+SHIFT+8"));
    jump->trigger();
    QWidget *page = m_app->m_tab->currentWidget();
    CueRunner *runner = qobject_cast<CueRunner *>(page);
    QVERIFY(runner != nullptr);
    QCOMPARE(runner->runnerSize(), CueRunner::Full);
    QCOMPARE(runner->follow(), ShowTree::invalidId());    // the current production
    QCOMPARE(page->property("helpTopic").toString(), QString("show-tree.md#the-cue-runner"));
    // The others are numbered by the view too (⇧⌘7 is Simple Desk).
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == QLatin1String("Simple Desk"))
            QCOMPARE(a->shortcut(), QKeySequence("CTRL+SHIFT+7"));
}

void AppViews_Test::theRigOverviewOpensWithoutTakingYouToTheStudio()
{
    QTabWidget *tabs = m_app->m_tab;
    // Somewhere that is not the Lighting Studio.
    int other = -1;
    for (int i = 0; i < tabs->count(); ++i)
        if (!tabs->tabBar()->tabData(i).toString().contains("Studio"))
            other = i;
    QVERIFY(other >= 0);
    tabs->setCurrentIndex(other);
    const QString was = tabs->tabBar()->tabData(other).toString();

    m_app->slotRigOverview();
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), was);
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr && mon->m_overviewDlg != nullptr);
    // Its own window, kept above the main window (a floating panel of it),
    // never docked -- so the main window keeps its own minimum width.
    QVERIFY(!mon->m_overviewDlg->isHidden());
    QCOMPARE(mon->m_overviewDlg->parentWidget(), static_cast<QWidget *>(m_app));
    QVERIFY(mon->m_overviewDlg->isFloating());
    QCOMPARE(mon->m_overviewDlg->allowedAreas(), Qt::NoDockWidgetArea);
    QVERIFY2(m_app->minimumSizeHint().width() < 1000,
             qPrintable(QString::number(m_app->minimumSizeHint().width())));
    QPointer<QDockWidget> dock = mon->m_overviewDlg;
    mon->m_overviewDlg->close();
    QTRY_VERIFY(dock.isNull());
}

void AppViews_Test::aShowsDefinitionsComeFromTheShowCache()
{
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
#ifdef Q_OS_WIN
    // Windows takes the home folder from USERPROFILE, not HOME.
    const QByteArray realProfile = qgetenv("USERPROFILE");
    qputenv("USERPROFILE", home.path().toUtf8());
#endif

    // A definition that is only in the show cache.
    QLCFixtureDefCache *defs = m_app->m_doc->fixtureDefCache();
    QFile src(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/Futurelight/Futurelight-DJScan250.qxf");
    QVERIFY(src.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(src.readAll());
    text.replace(QRegularExpression("<Manufacturer>[^<]*</Manufacturer>"), "<Manufacturer>Branson</Manufacturer>");
    text.replace(QRegularExpression("<Model>[^<]*</Model>"), "<Model>Cached Head</Model>");
    QFile cached(QLCFixtureDefCache::showCacheFile("Branson", "Cached Head"));
    QVERIFY(cached.open(QIODevice::WriteOnly));
    cached.write(text.toUtf8());
    cached.close();
    QVERIFY(defs->fixtureDef("Branson", "Cached Head") == nullptr);

    // A show that uses it.
    QTemporaryDir shows;
    const QString show = shows.filePath("cached.qlcc");
    QFile f(show);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<?xml version=\"1.0\"?><!DOCTYPE Workspace><Workspace><Engine>"
            "<Fixture><Manufacturer>Branson</Manufacturer><Model>Cached Head</Model>"
            "<Mode>Mode 1</Mode><ID>0</ID><Name>Head</Name><Universe>0</Universe>"
            "<Address>0</Address><Channels>6</Channels></Fixture></Engine></Workspace>");
    f.close();

    m_app->prepareShowDefinitions(show);
    QVERIFY2(defs->fixtureDef("Branson", "Cached Head") != nullptr,
             "the show's definition did not come from the show cache");

    qputenv("HOME", realHome);
#ifdef Q_OS_WIN

    qputenv("USERPROFILE", realProfile);
#endif
}

/* Answers the next modal message box with @a answer, keeping its text. */
static void answerNextBox(QMessageBox::StandardButton answer, QString *text)
{
    QTimer::singleShot(0, [answer, text]() {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000)
        {
            if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            {
                if (text != nullptr)
                    *text = box->text();
                box->button(answer)->click();
                return;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
    });
}

void AppViews_Test::missingDefinitionsAreAskedAboutAndFetchedBack()
{
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
#ifdef Q_OS_WIN
    const QByteArray realProfile = qgetenv("USERPROFILE");
    qputenv("USERPROFILE", home.path().toUtf8());
#endif
    auto restoreHome = qScopeGuard([&]() {
        qputenv("HOME", realHome);
#ifdef Q_OS_WIN
        qputenv("USERPROFILE", realProfile);
#endif
    });

    // A show of two heads whose definitions this computer hasn't got.
    QTemporaryDir shows;
    const QString show = shows.filePath("away.qlcc");
    QFile f(show);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<?xml version=\"1.0\"?><!DOCTYPE Workspace><Workspace><Engine>"
            "<Fixture><Manufacturer>Branson</Manufacturer><Model>Late Head</Model>"
            "<Mode>Mode 1</Mode><ID>0</ID><Name>Late</Name><Universe>0</Universe>"
            "<Address>0</Address><Channels>6</Channels></Fixture>"
            "<Fixture><Manufacturer>Branson</Manufacturer><Model>Never Head</Model>"
            "<Mode>Mode 1</Mode><ID>1</ID><Name>Never</Name><Universe>0</Universe>"
            "<Address>10</Address><Channels>6</Channels></Fixture>"
            "</Engine></Workspace>");
    f.close();
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    Doc *doc = m_app->doc();
    QCOMPARE(doc->fixtures().count(), 2);
    for (Fixture *fx : doc->fixtures())
        QVERIFY2(fx->definitionUnavailable(), qPrintable(fx->name()));

    // Save asks first and names them; Cancel saves nothing.
    QString asked;
    answerNextBox(QMessageBox::Cancel, &asked);
    QVERIFY(!m_app->confirmSaveWithoutDefinitions());
    QVERIFY2(asked.contains("Branson Late Head") && asked.contains("Branson Never Head"),
             qPrintable(asked));

    // One definition arrives in the user folder.
    QFile src(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/Futurelight/Futurelight-DJScan250.qxf");
    QVERIFY(src.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(src.readAll());
    text.replace(QRegularExpression("<Manufacturer>[^<]*</Manufacturer>"), "<Manufacturer>Branson</Manufacturer>");
    text.replace(QRegularExpression("<Model>[^<]*</Model>"), "<Model>Late Head</Model>");
    QDir userDefs = QLCFixtureDefCache::userDefinitionDirectory();
    QVERIFY(userDefs.mkpath("."));
    QFile late(userDefs.absoluteFilePath("Branson-Late-Head.qxf"));
    QVERIFY(late.open(QIODevice::WriteOnly));
    late.write(text.toUtf8());
    late.close();

    QString told;
    answerNextBox(QMessageBox::Ok, &told);
    m_app->recoverUnavailableDefinitions(m_app);
    Fixture *lateFx = doc->fixture(0);
    Fixture *neverFx = doc->fixture(1);
    QVERIFY(!lateFx->definitionUnavailable());
    QVERIFY(lateFx->fixtureDef() != nullptr);
    QCOMPARE(lateFx->fixtureDef()->model(), QStringLiteral("Late Head"));
    QCOMPARE(lateFx->fixtureMode()->name(), QStringLiteral("Mode 1"));
    QVERIFY(neverFx->definitionUnavailable());
    QVERIFY2(told.contains("1 fixture(s) have their definitions back") && told.contains("Never"),
             qPrintable(told));

    m_app->clearDocument();
}

void AppViews_Test::settingsChoosesTheUnits()
{
    QCOMPARE(MeasurementUnits::preferredLength(), MeasurementUnits::Millimetres);
    {
        AppSettings dlg(m_app);
        QComboBox *units = dlg.findChild<QComboBox *>(QStringLiteral("settingsUnitsCombo"));
        QVERIFY(units != nullptr);
        QCOMPARE(units->count(), 5);
        QCOMPARE(units->currentData().toInt(), int(MeasurementUnits::Millimetres));
        units->setCurrentIndex(units->findData(int(MeasurementUnits::Inches)));
        dlg.accept();
    }
    QCOMPARE(MeasurementUnits::preferredLength(), MeasurementUnits::Inches);
    {
        AppSettings dlg(m_app);
        QComboBox *units = dlg.findChild<QComboBox *>(QStringLiteral("settingsUnitsCombo"));
        QCOMPARE(units->currentData().toInt(), int(MeasurementUnits::Inches));
        dlg.reject();   // Cancel keeps it
    }
    QCOMPARE(MeasurementUnits::preferredLength(), MeasurementUnits::Inches);
    MeasurementUnits::setPreferredLength(MeasurementUnits::Millimetres);
}

void AppViews_Test::settingsAdvancedHoldsAutoOpen()
{
    ShutterAutoOpen *ao = m_app->doc()->shutterAutoOpen();
    QVERIFY(ao->isEnabled());
    {
        AppSettings dlg(m_app);
        QTabWidget *tabs = dlg.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(1), QStringLiteral("Appearance"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("Advanced"));
        QWidget *adv = tabs->widget(2);
        QCheckBox *box = adv->findChild<QCheckBox *>(QStringLiteral("settingsAutoOpenShutters"));
        QVERIFY(box != nullptr);
        QVERIFY(box->isChecked());
        // The expert settings moved there; everyday ones stay on General.
        // Show control is set up in Connections ▸ Devices only (Q84).
        QVERIFY(adv->findChild<QPushButton *>(QStringLiteral("settingsOpenShowControl")) == nullptr);
        QVERIFY(adv->findChild<QComboBox *>(QStringLiteral("settingsUnitsCombo")) == nullptr);
        QVERIFY(tabs->widget(0)->findChild<QComboBox *>(QStringLiteral("settingsUnitsCombo")) != nullptr);
        dlg.showAdvanced();
        QCOMPARE(tabs->currentIndex(), 2);

        box->setChecked(false);
        dlg.reject();                   // Cancel keeps it
    }
    QVERIFY(ao->isEnabled());
    {
        AppSettings dlg(m_app);
        dlg.findChild<QCheckBox *>(QStringLiteral("settingsAutoOpenShutters"))->setChecked(false);
        dlg.accept();
    }
    QVERIFY(ao->isEnabled() == false);
    QVERIFY(m_app->doc()->isModified());
    {
        AppSettings dlg(m_app);
        QVERIFY(dlg.findChild<QCheckBox *>(QStringLiteral("settingsAutoOpenShutters"))->isChecked() == false);
    }
    ao->setEnabled(true);
    m_app->doc()->resetModified();
}

void AppViews_Test::settingsAppearanceHoldsHowTheWindowLooks()
{
    const int wasTheme = m_app->theme();
    const int wasMode = m_app->tabLabelMode();
    const bool wasGM = m_app->footerChipShown(App::FooterGrandMaster);
    const SpeedMasterButton::BeatStyle wasStyle = SpeedMastersFooter::beatStyle();
    const SpeedMastersFooter::BeatWhen wasWhen = SpeedMastersFooter::beatWhen();
    auto restore = qScopeGuard([=]() {
        m_app->setTheme(wasTheme);
        m_app->setTabLabelMode(wasMode);
        m_app->setFooterChipShown(App::FooterGrandMaster, wasGM);
        SpeedMastersFooter::setBeatStyle(wasStyle);
        SpeedMastersFooter::setBeatWhen(wasWhen);
        m_app->reloadFooterBeatOptions();
    });
    m_app->setTheme(App::ThemeDefault);
    m_app->setTabLabelMode(App::TabIconAndText);
    m_app->setFooterChipShown(App::FooterGrandMaster, true);
    SpeedMastersFooter::setBeatStyle(SpeedMasterButton::Blink);
    SpeedMastersFooter::setBeatWhen(SpeedMastersFooter::WhilePlaying);

    {
        AppSettings dlg(m_app);
        QTabWidget *tabs = dlg.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QWidget *look = tabs->widget(1);
        QCOMPARE(look->objectName(), QStringLiteral("settingsAppearance"));
        QComboBox *theme = look->findChild<QComboBox *>("settingsTheme");
        QComboBox *style = look->findChild<QComboBox *>("settingsToolbarStyle");
        QComboBox *beat = look->findChild<QComboBox *>("settingsFooterBeatStyle");
        QComboBox *when = look->findChild<QComboBox *>("settingsFooterBeatWhen");
        QVERIFY(theme && style && beat && when);
        QCOMPARE(theme->count(), 6);
        QCOMPARE(theme->currentData().toInt(), int(App::ThemeDefault));
        QCOMPARE(style->currentData().toInt(), int(App::TabIconAndText));
        for (const char *name : { "settingsFooterLoad", "settingsFooterPower",
                                  "settingsFooterGrandMaster", "settingsFooterSpeedMasters" })
            QVERIFY2(look->findChild<QCheckBox *>(name) != nullptr, name);
        QVERIFY(look->findChild<QCheckBox *>("settingsFooterGrandMaster")->isChecked());
        // Toolbar style is here only, not on General.
        QVERIFY(tabs->widget(0)->findChild<QComboBox *>("settingsToolbarStyle") == nullptr);

        theme->setCurrentIndex(theme->findData(int(App::ThemeRedShift)));
        style->setCurrentIndex(style->findData(int(App::TabTextOnly)));
        look->findChild<QCheckBox *>("settingsFooterGrandMaster")->setChecked(false);
        beat->setCurrentIndex(beat->findData(int(SpeedMasterButton::Bar)));
        when->setCurrentIndex(when->findData(int(SpeedMastersFooter::Always)));
        dlg.accept();
    }
    // The same settings, under the same keys, as the View menu set.
    QSettings settings;
    QCOMPARE(m_app->theme(), int(App::ThemeRedShift));
    QCOMPARE(settings.value("workspace/theme").toInt(), int(App::ThemeRedShift));
    QCOMPARE(m_app->tabLabelMode(), int(App::TabTextOnly));
    QCOMPARE(settings.value("workspace/tabLabelMode").toInt(), int(App::TabTextOnly));
    QVERIFY(m_app->footerChipShown(App::FooterGrandMaster) == false);
    QCOMPARE(settings.value("workspace/showFooterGM").toBool(), false);
    QVERIFY(m_app->m_statusGrandMasterBox->isHidden());
    QCOMPARE(SpeedMastersFooter::beatStyle(), SpeedMasterButton::Bar);
    QCOMPARE(SpeedMastersFooter::beatWhen(), SpeedMastersFooter::Always);

    // Cancel keeps what is set.
    {
        AppSettings dlg(m_app);
        QComboBox *theme = dlg.findChild<QComboBox *>("settingsTheme");
        QCOMPARE(theme->currentData().toInt(), int(App::ThemeRedShift));
        theme->setCurrentIndex(theme->findData(int(App::ThemeBlue)));
        dlg.reject();
    }
    QCOMPARE(m_app->theme(), int(App::ThemeRedShift));
}

void AppViews_Test::theViewMenuIsOnlyViews()
{
    QMenu *view = nullptr;
    foreach (QAction *a, m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->text().remove('&') == "View")
            view = a->menu();
    QVERIFY(view != nullptr);
    QStringList items;
    foreach (QAction *a, view->actions())
        if (!a->isSeparator())
            items << a->text();
    QCOMPARE(items, QStringList() << "Workspace" << "Address Tool" << "Rig Overview…"
                                  << "Focus Mode…");
}

void AppViews_Test::theGridFollowsTheUnitPreference()
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
                       QStringLiteral("qlcplus"), QStringLiteral("qlcconsole"));
    auto forget = [&settings]() {
        settings.remove(MeasurementUnits::settingsKey());
        settings.remove(QStringLiteral("units/seededFromShow"));
    };
    forget();

    // A show as an older version saved it: a 40 x 30 ft stage, in feet.
    QTemporaryDir dir;
    const QString show = dir.filePath(QStringLiteral("feet.qxw"));
    m_app->clearDocument();
    MonitorProperties *mp = m_app->doc()->monitorProperties();
    mp->setGridUnits(MonitorProperties::Feet);   // relabel, as a file says it
    mp->setGridSize(QVector3D(40, 10, 30));
    QCOMPARE(m_app->saveXML(show), QFile::NoError);

    // Never chosen: the first show opened, in feet, makes it feet.
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    mp = m_app->doc()->monitorProperties();
    QCOMPARE(MeasurementUnits::preferredLength(), MeasurementUnits::Feet);
    QCOMPARE(mp->gridUnits(), MonitorProperties::Feet);
    QCOMPARE(mp->gridSize(), QVector3D(40, 10, 30));
    QLabel *label = Monitor::instance()->findChild<QLabel *>(QStringLiteral("monitorGridUnitsLabel"));
    QVERIFY(label != nullptr);
    QVERIFY(label->text().startsWith(QStringLiteral("Feet")));
    QVERIFY(Monitor::instance()->findChild<QComboBox *>(QStringLiteral("monitorGridUnitsCombo")) == nullptr);

    // Only once: chosen mm, the same show opens in metres, the same stage.
    MeasurementUnits::setPreferredLength(MeasurementUnits::Millimetres);
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    mp = m_app->doc()->monitorProperties();
    QCOMPARE(MeasurementUnits::preferredLength(), MeasurementUnits::Millimetres);
    QCOMPARE(mp->gridUnits(), MonitorProperties::Meters);
    QVERIFY(qAbs(mp->gridSize().x() - 12.192f) < 1e-3f);
    QVERIFY(qAbs(mp->gridSize().z() - 9.144f) < 1e-3f);
    QVERIFY(label->text().startsWith(QStringLiteral("Metres")));

    // Saved in the person's unit, whole numbers for older readers.
    const QString saved = dir.filePath(QStringLiteral("metres.qxw"));
    QCOMPARE(m_app->saveXML(saved), QFile::NoError);
    QFile f(saved);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray xml = f.readAll();
    QVERIFY(xml.contains("Units=\"0\""));
    QVERIFY(xml.contains("Width=\"12\""));
    QVERIFY(xml.contains("ExactWidth=\"12.192"));

    // Changing it in Settings converts the open show and redraws.
    {
        AppSettings dlg(m_app);
        QComboBox *units = dlg.findChild<QComboBox *>(QStringLiteral("settingsUnitsCombo"));
        units->setCurrentIndex(units->findData(int(MeasurementUnits::Inches)));
        dlg.accept();
    }
    QCOMPARE(mp->gridUnits(), MonitorProperties::Feet);
    QVERIFY(qAbs(mp->gridSize().x() - 40.0f) < 1e-3f);
    QVERIFY(label->text().startsWith(QStringLiteral("Feet")));

    // New: in the person's unit too.
    m_app->clearDocument();
    QCOMPARE(m_app->doc()->monitorProperties()->gridUnits(), MonitorProperties::Feet);

    // (No second New straight after: MasterTimer::stop() right behind a
    // start() can wait forever -- see TODO.md.)
    forget();
}

void AppViews_Test::rigNetworkFooterSaysWhyOutputIsHeld()
{
    typedef InputOutputMap::TransmitGateStatus S;
    S st;
    st.state = S::Held;
    st.held = 1;
    st.networkOutputs = 1;
    st.networks << "172.18.0.0/20";
    st.connected << "en0 10.4.12.7";
    bool alarm = false;
    QCOMPARE(App::rigNetworkFooterText(st, &alarm),
             QString("Output held — not on the rig network (172.18.0.0/20). "
                     "Connected: en0 10.4.12.7"));
    QVERIFY(alarm);
    st.networkOutputs = 3;
    QVERIFY(App::rigNetworkFooterText(st).startsWith("1 of 3 outputs held — not on the rig network"));
    st.state = S::NotSet;
    QVERIFY(App::rigNetworkFooterText(st, &alarm).startsWith("Rig network not set"));
    QVERIFY(alarm == false);
    st.state = S::Clear;
    QVERIFY(App::rigNetworkFooterText(st).isEmpty());

    /* The real footer, on a laptop at school, patched to the school's own
       interface: the line is right there, and held. The plugin is the
       engine's test stub dressed as Art-Net, so nothing is ever sent. */
    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        cache->load(stubDir);
    IOPluginStub *stub = static_cast<IOPluginStub *>(cache->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    stub->m_nameOverride = QStringLiteral("ArtNet");
    stub->m_lineNames = QStringList() << QStringLiteral("10.4.12.7");
    stub->m_openOutputs.clear();

    NetworkRoute::LocalAddress school;
    school.iface = "en0";
    school.ip = QHostAddress("10.4.12.7");
    school.prefix = 16;
    NetworkRoute::setResolverForTest(
        [school]() { return QList<NetworkRoute::LocalAddress>() << school; },
        [](const QHostAddress &) { return QHostAddress("10.4.12.7"); });

    m_app->clearDocument();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QLabel *chip = m_app->findChild<QLabel *>("footerRigNetwork");
    QVERIFY(chip != nullptr);
    QVERIFY(chip->isHidden());

    QXmlStreamReader xml(QStringLiteral(
        "<InputOutputMap>\n"
        " <RigNetwork>172.18.0.0/20</RigNetwork>\n"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"10.4.12.7\" Line=\"0\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n"));
    xml.readNextStartElement();
    QVERIFY(iom->loadXML(xml));
    QCOMPARE(iom->outputPatchesCount(0), 1);
    QVERIFY(iom->outputPatch(0, 0)->isHeld());
    QVERIFY(stub->m_openOutputs.isEmpty());
    QVERIFY(chip->isHidden() == false);
    QCOMPARE(chip->text(), QString("Output held — not on the rig network (172.18.0.0/20). "
                                   "Connected: en0 10.4.12.7"));
    QVERIFY(chip->styleSheet().contains("#e60000"));
    QVERIFY(chip->toolTip().contains("Universe 1"));

    // Lifting it for the session: the line opens, and the chip says so
    iom->setTransmitOverride(true);
    QCOMPARE(stub->m_openOutputs, QList<quint32>() << 0);
    QVERIFY(chip->text().startsWith("Transmitting anyway"));

    // Unpatched: nothing left to say
    iom->setTransmitOverride(false);
    QVERIFY(stub->m_openOutputs.isEmpty());
    iom->forgetOfflinePatches(0);
    QVERIFY(chip->isHidden());
    iom->setRigNetworks(QStringList());
    m_app->clearDocument();
    NetworkRoute::setResolverForTest(nullptr, nullptr);
    stub->m_nameOverride.clear();
    stub->m_lineNames.clear();
    stub->m_openOutputs.clear();
}

void AppViews_Test::rigNetworkSettingsRoundTrip()
{
    QSettings settings;
    const QVariant before = settings.value(App::rigNetworkGateSettingKey());

    m_app->clearDocument();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    iom->setRigNetworks(QStringList() << "172.18.0.0/20");
    m_app->doc()->resetModified();
    QVERIFY(m_app->doc()->isModified() == false);
    {
        AppSettings dlg(m_app);
        QLineEdit *edit = dlg.findChild<QLineEdit *>("settingsRigNetworks");
        QCheckBox *only = dlg.findChild<QCheckBox *>("settingsRigNetworkOnly");
        QLabel *error = dlg.findChild<QLabel *>("settingsRigNetworksError");
        QPushButton *anyway = dlg.findChild<QPushButton *>("settingsTransmitAnyway");
        QDialogButtonBox *box = dlg.findChild<QDialogButtonBox *>();
        QVERIFY(edit && only && error && anyway && box);
        QCOMPARE(edit->text(), QString("172.18.0.0/20"));
        QVERIFY(only->isChecked());
        QVERIFY(anyway->isEnabled());

        // Validated as typed: OK waits for something that parses
        edit->setText("172.18.0.0/20, 172.18/20");
        QVERIFY(error->isHidden() == false);
        QVERIFY(error->text().contains("172.18/20"));
        QVERIFY(box->button(QDialogButtonBox::Ok)->isEnabled() == false);

        edit->setText("172.18.0.0/20, 10.20.1.9/16");
        QVERIFY(error->isHidden());
        QVERIFY(box->button(QDialogButtonBox::Ok)->isEnabled());
        only->setChecked(false);
        dlg.accept();
    }
    QCOMPARE(iom->rigNetworks(), QStringList() << "172.18.0.0/20" << "10.20.0.0/16");
    QVERIFY(m_app->doc()->isModified());   // the networks are the show's
    QVERIFY(iom->transmitGateEnabled() == false);
    QCOMPARE(QSettings().value(App::rigNetworkGateSettingKey()).toBool(), false);

    {
        AppSettings dlg(m_app);
        QCOMPARE(dlg.findChild<QLineEdit *>("settingsRigNetworks")->text(),
                 QString("172.18.0.0/20, 10.20.0.0/16"));
        QVERIFY(dlg.findChild<QCheckBox *>("settingsRigNetworkOnly")->isChecked() == false);
        dlg.findChild<QCheckBox *>("settingsRigNetworkOnly")->setChecked(true);
        dlg.reject();   // Cancel changes nothing
    }
    QVERIFY(iom->transmitGateEnabled() == false);

    {
        AppSettings dlg(m_app);
        dlg.findChild<QCheckBox *>("settingsRigNetworkOnly")->setChecked(true);
        dlg.accept();
    }
    QVERIFY(iom->transmitGateEnabled());

    // A show with none but network patches is offered its own, not given them
    m_app->clearDocument();
    IOPluginStub *stub = static_cast<IOPluginStub *>(
            m_app->doc()->ioPluginCache()->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY(stub != nullptr);   // loaded by the footer test before this one
    stub->m_nameOverride = QStringLiteral("ArtNet");
    stub->m_lineNames = QStringList() << QStringLiteral("10.4.12.7");
    NetworkRoute::LocalAddress school;
    school.iface = "en0";
    school.ip = QHostAddress("10.4.12.7");
    school.prefix = 16;
    NetworkRoute::setResolverForTest(
        [school]() { return QList<NetworkRoute::LocalAddress>() << school; },
        [](const QHostAddress &) { return QHostAddress("10.4.12.7"); });
    QXmlStreamReader xml(QStringLiteral(
        "<InputOutputMap>\n"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\" Network=\"172.18.2.0/24\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n"));
    xml.readNextStartElement();
    QVERIFY(iom->loadXML(xml));
    QCOMPARE(iom->outputPatchesCount(0), 1);
    {
        AppSettings dlg(m_app);
        QLineEdit *edit = dlg.findChild<QLineEdit *>("settingsRigNetworks");
        QVERIFY(edit->text().isEmpty());
        QVERIFY(edit->placeholderText().contains("172.18.2.0/24"));
        QPushButton *use = dlg.findChild<QPushButton *>("settingsRigUseSuggested");
        QVERIFY(use != nullptr);
        use->click();
        QCOMPARE(edit->text(), QString("172.18.2.0/24"));
        dlg.reject();
        QVERIFY(iom->rigNetworks().isEmpty());
    }
    iom->forgetOfflinePatches(0);
    iom->setRigNetworks(QStringList());
    NetworkRoute::setResolverForTest(nullptr, nullptr);
    m_app->clearDocument();
    stub->m_nameOverride.clear();
    stub->m_lineNames.clear();
    stub->m_openOutputs.clear();

    if (before.isValid())
        settings.setValue(App::rigNetworkGateSettingKey(), before);
    else
        settings.remove(App::rigNetworkGateSettingKey());
}

QTEST_MAIN(AppViews_Test)

/* WCAG relative luminance and contrast. */
static double luminance(QRgb c)
{
    auto lin = [](int v) {
        const double s = v / 255.0;
        return s <= 0.03928 ? s / 12.92 : qPow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(qRed(c)) + 0.7152 * lin(qGreen(c)) + 0.0722 * lin(qBlue(c));
}

static double contrast(QRgb a, QRgb b)
{
    const double la = luminance(a), lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

void AppViews_Test::theDropAreaReadsInEveryTheme()
{
    ShowTreeController *st = m_app->doc()->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L");
    const quint32 cue = st->addCue(list, -1, "One");
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    QVERIFY(view->selectNode(cue));
    QLabel *zone = view->findChild<QLabel *>("stCueDropZone");
    QVERIFY(zone != nullptr);
    zone->resize(420, 90);

    const int was = m_app->theme();
    const int themes[] = { App::ThemeDefault, App::ThemeTan, App::ThemeBlue,
                           App::ThemeQLCOriginal, App::ThemeRedShift, App::ThemeVSCodeDark };
    for (int theme : themes)
    {
        m_app->setTheme(theme);
        QCoreApplication::processEvents();
        // The words, against what is behind them: the commonest colour is
        // the page, the one furthest from it the text.
        const QImage img = zone->grab().toImage();
        QHash<QRgb, int> counts;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                ++counts[img.pixel(x, y) | 0xff000000];
        QRgb bg = 0;
        int most = -1;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            if (it.value() > most)
            {
                most = it.value();
                bg = it.key();
            }
        double best = 1.0;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            best = qMax(best, contrast(it.key(), bg));
        QVERIFY2(best >= 4.5, qPrintable(QString("theme %1: text contrast %2").arg(theme).arg(best)));
    }
    m_app->setTheme(was);
    QVERIFY(st->removeProduction(st->productions().first().id));
}

void AppViews_Test::aSkippedCueReadsInEveryTheme()
{
    ShowTreeController *st = m_app->doc()->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "Skips", ShowTree::ListMode::Tracking);
    QList<quint32> cues;
    for (int i = 0; i < 4; ++i)
        cues << st->addCue(list, i - 1, QStringLiteral("Cue number %1").arg(i + 1));
    QVERIFY(st->setCuesSkipped({ cues[1], cues[2] }, true));
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    QTabWidget *tabs = m_app->m_tab;
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->widget(i) == view || tabs->widget(i)->isAncestorOf(view))
            tabs->setCurrentIndex(i);
    m_app->resize(1400, 800);
    m_app->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(m_app));
    const QString dir = qEnvironmentVariable("QLC_SKIPCUE_THEMES");

    const int was = m_app->theme();
    const int themes[] = { App::ThemeDefault, App::ThemeTan, App::ThemeBlue,
                           App::ThemeQLCOriginal, App::ThemeRedShift, App::ThemeVSCodeDark };
    for (int theme : themes)
    {
        m_app->setTheme(theme);
        QCoreApplication::processEvents();
        QTest::qWait(30);           // the tree takes the theme's greys again
        QTreeWidget *tree = view->tree();
        tree->expandAll();
        tree->clearSelection();
        QCoreApplication::processEvents();
        QTreeWidgetItem *row = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cues[1])
                row = *it;
        QVERIFY(row != nullptr);
        QVERIFY(row->font(0).strikeOut());
        // The words of the row against what is behind them, as for the drop
        // area: the commonest colour is the row, the one furthest from it
        // the text -- the struck name and the ⦸ badge.
        const QRect rect = tree->visualItemRect(row);
        const QImage img = tree->viewport()->grab(rect).toImage();
        QHash<QRgb, int> counts;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                ++counts[img.pixel(x, y) | 0xff000000];
        QRgb bg = 0;
        int most = -1;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            if (it.value() > most)
            {
                most = it.value();
                bg = it.key();
            }
        // Greyed reads as greyed (WCAG's 3:1 for large or incidental text),
        // never gone.
        const double text = contrast(row->foreground(0).color().rgb(), bg);
        QVERIFY2(text >= 3.0, qPrintable(QString("theme %1: skipped text contrast %2").arg(theme).arg(text)));
        if (!dir.isEmpty())
            QVERIFY(view->grab().save(QStringLiteral("%1/skipped-theme-%2.png").arg(dir).arg(theme)));
    }
    m_app->setTheme(was);
    m_app->hide();
    QVERIFY(st->removeProduction(st->productions().first().id));
}

/* Every offered text colour, on a cue of the Show Tree and of the Run tab,
   in each of the six themes: drawn readable (4.5:1) against what is really
   behind the row -- a variant of it where the colour itself isn't. */
void AppViews_Test::textColoursReadInEveryTheme()
{
    ShowTreeController *st = m_app->doc()->showTree();
    const quint32 prod = st->addProduction("Colours");
    const quint32 list = st->addCueList(prod, "Song", ShowTree::ListMode::Tracking);
    QList<quint32> cues;
    for (int i = 0; i < ShowTree::textColours().count(); ++i)
    {
        cues << st->addCue(list, i - 1, QStringLiteral("Cue in %1").arg(ShowTree::textColours().at(i).name));
        QVERIFY(st->setCuesColour({ cues.last() }, ShowTree::textColours().at(i).hex));
    }
    st->setCurrentProduction(prod);
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    CueRunner *runner = nullptr;
    QTabWidget *tabs = m_app->m_tab;
    for (int i = 0; i < tabs->count(); ++i)
        if (CueRunner *r = qobject_cast<CueRunner *>(tabs->widget(i)))
            runner = r;
    QVERIFY(runner != nullptr);
    m_app->resize(1400, 800);
    m_app->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(m_app));
    const QString dir = qEnvironmentVariable("QLC_CUECOLOUR_SHOTS");

    // The colour most of a row's pixels are: what is behind its text.
    auto behind = [](QWidget *viewport, const QRect &rect) {
        const QImage img = viewport->grab(rect).toImage();
        QHash<QRgb, int> counts;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                ++counts[img.pixel(x, y) | 0xff000000];
        QRgb bg = 0;
        int most = -1;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            if (it.value() > most)
            {
                most = it.value();
                bg = it.key();
            }
        return bg;
    };

    const int was = m_app->theme();
    const int themes[] = { App::ThemeDefault, App::ThemeTan, App::ThemeBlue,
                           App::ThemeQLCOriginal, App::ThemeRedShift, App::ThemeVSCodeDark };
    for (int theme : themes)
    {
        m_app->setTheme(theme);
        QCoreApplication::processEvents();
        QTest::qWait(30);

        // The Show Tree.
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->widget(i) == view || tabs->widget(i)->isAncestorOf(view))
                tabs->setCurrentIndex(i);
        QTreeWidget *tree = view->tree();
        tree->expandAll();
        tree->clearSelection();
        QCoreApplication::processEvents();
        for (int k = 0; k < cues.count(); ++k)
        {
            QTreeWidgetItem *row = nullptr;
            for (QTreeWidgetItemIterator it(tree); *it; ++it)
                if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                    && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cues[k])
                    row = *it;
            QVERIFY(row != nullptr);
            tree->scrollToItem(row);
            const QColor fg = row->foreground(0).color();
            const double c = contrast(fg.rgb(), behind(tree->viewport(), tree->visualItemRect(row)));
            QVERIFY2(c >= 4.5, qPrintable(QString("theme %1, tree, %2: %3")
                                          .arg(theme).arg(ShowTree::textColours().at(k).name).arg(c)));
        }
        if (!dir.isEmpty())
            QVERIFY(view->grab().save(QStringLiteral("%1/showtree-theme-%2.png").arg(dir).arg(theme)));

        // The Run tab.
        tabs->setCurrentWidget(runner);
        runner->refresh();
        QCoreApplication::processEvents();
        QTreeWidget *rt = runner->cueTree();
        QCOMPARE(rt->topLevelItemCount(), cues.count());
        for (int k = 0; k < cues.count(); ++k)
        {
            QTreeWidgetItem *row = rt->topLevelItem(k);
            rt->scrollToItem(row);
            const QColor fg = row->foreground(2).color();
            const double c = contrast(fg.rgb(), behind(rt->viewport(), rt->visualItemRect(row)));
            QVERIFY2(c >= 4.5, qPrintable(QString("theme %1, Run tab, %2: %3")
                                          .arg(theme).arg(ShowTree::textColours().at(k).name).arg(c)));
        }
        const double next = ShowTree::contrastRatio(runner->textColour("next"),
                                                     runner->palette().color(QPalette::Window));
        QVERIFY2(next >= 4.5, qPrintable(QString("theme %1, NEXT: %2").arg(theme).arg(next)));
        if (!dir.isEmpty())
            QVERIFY(runner->grab().save(QStringLiteral("%1/run-theme-%2.png").arg(dir).arg(theme)));
    }
    m_app->setTheme(was);
    m_app->hide();
    QVERIFY(st->removeProduction(prod));
}

/* The labels on the tab bar, in order (identities, not the text shown). */
static QStringList tabLabels(QTabWidget *tabs)
{
    QStringList out;
    for (int i = 0; i < tabs->count(); ++i)
        out << tabs->tabBar()->tabData(i).toString();
    return out;
}

static void viewShot(App *app, const QString &name)
{
    const QString dir = qEnvironmentVariable("APPVIEWS_SHOTS");
    if (dir.isEmpty())
        return;
    app->resize(1400, 860);
    QCoreApplication::processEvents();
    QVERIFY(app->grab().save(dir + "/" + name));
}

void AppViews_Test::eachWorkspaceViewShowsItsTabs()
{
    QTabWidget *tabs = m_app->m_tab;
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);     // a new show
    const QStringList classic = { "Fixtures", "Functions", "Shows", "Virtual Console",
                                  "Simple Desk", "Connections" };
    const QStringList console = { "Connections", "Fixtures", "Lighting Studio", "Build",
                                  "Show Tree", "Virtual Console", "Simple Desk", "Run" };
    const QStringList everything = { "Connections", "Fixtures", "Lighting Studio", "Functions",
                                     "Build", "Show Tree", "Shows", "Virtual Console",
                                     "Simple Desk", "Run" };
    QCOMPARE(tabLabels(tabs), console);

    QMenu *menu = m_app->menuBar()->findChild<QMenu *>("viewWorkspaceMenu");
    QVERIFY(menu != nullptr);
    QAction *toClassic = m_app->menuBar()->findChild<QAction *>("viewWorkspaceClassic");
    QAction *toConsole = m_app->menuBar()->findChild<QAction *>("viewWorkspaceConsole");
    QAction *toAll = m_app->menuBar()->findChild<QAction *>("viewWorkspaceEverything");
    QVERIFY(toClassic && toConsole && toAll);
    QCOMPARE(toClassic->text(), QString("Classic QLC+"));
    QCOMPARE(toConsole->text(), QString("qlcconsole"));
    QCOMPARE(toAll->text(), QString("Everything"));
    QVERIFY(toConsole->isChecked() && !toClassic->isChecked() && !toAll->isChecked());
    viewShot(m_app, "view-qlcconsole.png");

    const QList<QWidget *> widgets = { FunctionManager::instance(), ProgrammingManager::instance(),
                                       ShowTreeView::instance(), SimpleDesk::instance(),
                                       Monitor::instance() };
    for (QWidget *w : widgets)
        QVERIFY(w != nullptr);

    // Classic QLC+: stock order, and Connections reads as QLC+ calls it.
    toClassic->trigger();
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QVERIFY(toClassic->isChecked() && !toConsole->isChecked());
    QCOMPARE(tabLabels(tabs), classic);
    QVERIFY(m_app->functionsWindowShown());
    const int io = tabs->indexOf(InputOutputManager::instance());
    QCOMPARE(io, 5);
    QCOMPARE(tabs->tabText(io), QString("Inputs/Outputs"));
    m_app->updateWindowMenu();
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == "Connections")
            QCOMPARE(a->text(), QString("Inputs/Outputs"));
    tabs->setCurrentIndex(io);
    QVERIFY2(m_app->windowTitle().endsWith(" - Inputs/Outputs"), qPrintable(m_app->windowTitle()));
    // Hidden, alive: the same widgets, off the bar.
    QCOMPARE(tabs->indexOf(ProgrammingManager::instance()), -1);
    QVERIFY(ProgrammingManager::instance() == widgets.at(1));
    QVERIFY(ShowTreeView::instance() == widgets.at(2));
    QVERIFY(!ShowTreeView::instance()->isVisible());
    // Text-only and icon-only modes keep the QLC+ name.
    m_app->setTabLabelMode(App::TabTextOnly);
    QCOMPARE(tabs->tabText(io), QString("Inputs/Outputs"));
    m_app->setTabLabelMode(App::TabIconAndText);
    tabs->setCurrentIndex(0);
    viewShot(m_app, "view-classic.png");

    // One switch: View ▸ Workspace. Settings has none (Q67).
    {
        AppSettings dlg(m_app, m_app);
        QVERIFY(dlg.findChild<QComboBox *>("settingsWorkspaceView") == nullptr);
        dlg.accept();
    }
    QCOMPARE(m_app->workspaceView(), App::ClassicView);   // OK left it alone
    toAll->trigger();
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QVERIFY(toAll->isChecked());
    QCOMPARE(tabLabels(tabs), everything);
    QCOMPARE(tabs->tabText(tabs->indexOf(InputOutputManager::instance())), QString("Connections"));
    viewShot(m_app, "view-everything.png");

    toConsole->trigger();
    QCOMPARE(tabLabels(tabs), console);
    QVERIFY(!m_app->functionsWindowShown());
    m_app->doc()->resetModified();
}

void AppViews_Test::aJumpToAHiddenTabShowsEverything()
{
    QTabWidget *tabs = m_app->m_tab;
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QVERIFY(!m_app->m_viewNotice->isVisible());
    QAction *desk = nullptr;
    QAction *functions = nullptr;
    QAction *tree = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
    {
        if (a->data().toString() == "Simple Desk")
            desk = a;
        if (a->data().toString() == "Functions")
            functions = a;
        if (a->data().toString() == "Show Tree")
            tree = a;
    }
    QVERIFY(desk && functions && tree);

    // Window → Functions in qlcconsole: the stock tab, so Everything (Q73).
    functions->trigger();
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(FunctionManager::instance()));
    m_app->setWorkspaceView(App::ConsoleView);

    // Window → Simple Desk: a tab in qlcconsole (Q71), so no view change.
    desk->trigger();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(SimpleDesk::instance()));
    QVERIFY(!m_app->m_viewNotice->isVisible());

    // Window → Show Tree from Classic QLC+: not there, so Everything, with
    // a note and a way back.
    m_app->resize(1400, 860);
    m_app->show();
    m_app->setWorkspaceView(App::ClassicView);
    tree->trigger();
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(ShowTreeView::instance()));
    QVERIFY(m_app->m_viewNotice->isVisible());
    QVERIFY2(m_app->m_viewNoticeText->text().contains("Show Tree isn't in the Classic QLC+ view"),
             qPrintable(m_app->m_viewNoticeText->text()));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QString("Back to Classic QLC+"));
    viewShot(m_app, "view-hidden-jump-note.png");
    m_app->m_viewNoticeButton->click();
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QVERIFY(!m_app->m_viewNotice->isVisible());
    QCOMPARE(tabs->indexOf(ShowTreeView::instance()), -1);

    // The same from Classic: Window → Build shows Everything.
    m_app->setWorkspaceView(App::ClassicView);
    m_app->showContext("Build");
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(ProgrammingManager::instance()));
    // × puts the note away; the view stays.
    m_app->m_viewNotice->findChild<QToolButton *>("viewNoticeClose")->click();
    QVERIFY(!m_app->m_viewNotice->isVisible());
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->hide();
    m_app->doc()->resetModified();
}

/* The Window menu's tab jumps as shown: label and shortcut, in menu order,
   up to the Rig Overview; "|" marks the line before the hidden tabs. */
static QStringList windowJumpsAsShown(App *app)
{
    app->updateWindowMenu();
    QMenu *menu = app->menuBar()->findChild<QMenu *>("windowMenu");
    QStringList out;
    if (menu == nullptr)
        return out;
    foreach (QAction *a, menu->actions())
    {
        if (a == app->m_rigOverviewAction)
            break;
        if (a->isSeparator())
        {
            if (a->objectName() == "windowHiddenTabs" && a->isVisible())
                out << "|";
            continue;
        }
        const QString key = a->shortcut().toString(QKeySequence::PortableText);
        out << (key.isEmpty() ? a->text() : a->text() + " " + key.right(1));
    }
    return out;
}

void AppViews_Test::windowShortcutsFollowTheView()
{
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(windowJumpsAsShown(m_app),
             QStringList({ "Connections 1", "Fixtures 2", "Lighting Studio 3", "Build 4",
                           "Show Tree 5", "Virtual Console 6", "Simple Desk 7", "Run 8",
                           "|", "Functions", "Shows" }));

    m_app->setWorkspaceView(App::ClassicView);
    QCOMPARE(windowJumpsAsShown(m_app),
             QStringList({ "Fixtures 1", "Functions 2", "Shows 3", "Virtual Console 4",
                           "Simple Desk 5", "Inputs/Outputs 6",
                           "|", "Lighting Studio", "Build", "Show Tree", "Run" }));
    // ⇧⌘1 is the first tab on screen, and it goes there.
    QAction *first = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1))
            first = a;
    QVERIFY(first != nullptr);
    first->trigger();
    QCOMPARE(m_app->m_tab->currentWidget(), static_cast<QWidget *>(FixtureManager::instance()));
    QCOMPARE(m_app->workspaceView(), App::ClassicView);

    m_app->setWorkspaceView(App::EverythingView);
    QCOMPARE(windowJumpsAsShown(m_app),
             QStringList({ "Connections 1", "Fixtures 2", "Lighting Studio 3", "Functions 4",
                           "Build 5", "Show Tree 6", "Shows 7", "Virtual Console 8",
                           "Simple Desk 9", "Run 0" }));

    // A tab in a window of its own keeps its number.
    m_app->setWorkspaceView(App::ConsoleView);
    QTabWidget *tabs = m_app->m_tab;
    m_app->slotDetachContext(tabs->indexOf(VirtualConsole::instance()));
    QStringList shown = windowJumpsAsShown(m_app);
    QCOMPARE(shown.value(5), QString("Virtual Console — in its own window 6"));
    QCOMPARE(shown.value(6), QString("Simple Desk 7"));
    reattach(m_app, "Virtual Console");
    QCOMPARE(windowJumpsAsShown(m_app).value(5), QString("Virtual Console 6"));
    m_app->doc()->resetModified();
}

void AppViews_Test::aBrokenOutViewSurvivesAViewSwitch()
{
    QTabWidget *tabs = m_app->m_tab;
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    auto windowOf = [this](QWidget *w) -> DetachedContext * {
        foreach (DetachedContext *dw, m_app->findChildren<DetachedContext *>())
            if (dw->centralWidget() == w)
                return dw;
        return nullptr;
    };

    // The Virtual Console (in both views) and Build (qlcconsole only) out in
    // windows.
    QWidget *vc = VirtualConsole::instance();
    QWidget *build = ProgrammingManager::instance();
    m_app->slotDetachContext(tabs->indexOf(vc));
    m_app->slotDetachContext(tabs->indexOf(build));
    DetachedContext *vcWin = windowOf(vc);
    DetachedContext *buildWin = windowOf(build);
    QVERIFY(vcWin && buildWin);

    m_app->setWorkspaceView(App::ClassicView);
    QCOMPARE(windowOf(vc), vcWin);                    // still in their windows
    QCOMPARE(windowOf(build), buildWin);
    QVERIFY(buildWin->isVisible() && build->isVisible());
    QCOMPARE(tabs->indexOf(vc), -1);
    QCOMPARE(tabLabels(tabs), QStringList({ "Fixtures", "Functions", "Shows",
                                            "Simple Desk", "Connections" }));
    // Build has no tab in Classic: put back from the menu it isn't offered...
    QCOMPARE(m_app->detachedContexts(), QStringList({ "Virtual Console" }));
    // ...and closing its window puts it away, not on the bar.
    buildWin->close();
    QCOMPARE(tabs->indexOf(build), -1);
    QVERIFY(!build->isVisible());
    // The Virtual Console goes back to its place in Classic's order.
    vcWin->close();
    QCOMPARE(tabs->indexOf(vc), 3);

    // Out again, and back to qlcconsole: Build is a tab again, the VC stays out.
    m_app->slotDetachContext(tabs->indexOf(vc));
    vcWin = windowOf(vc);
    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(windowOf(vc), vcWin);
    QCOMPARE(tabs->indexOf(build), 3);
    vcWin->close();
    QCOMPARE(tabLabels(tabs), QStringList({ "Connections", "Fixtures", "Lighting Studio", "Build",
                                            "Show Tree", "Virtual Console", "Simple Desk", "Run" }));
    m_app->doc()->resetModified();
}

void AppViews_Test::theViewIsSavedWithTheShow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("views.qxw");
    Doc *doc = m_app->doc();
    m_app->clearDocument();
    QVERIFY(!doc->isModified());

    // Switching marks the show modified (it is saved with it)...
    m_app->setWorkspaceView(App::ClassicView);
    QVERIFY(doc->isModified());
    // ...but setting the view it already has does not.
    doc->resetModified();
    m_app->setWorkspaceView(App::ClassicView);
    QVERIFY(!doc->isModified());
    // Not an undo step: the Show Tree's undo history doesn't move.
    QCOMPARE(m_app->saveXML(path), QFile::NoError);

    // An attribute on <Workspace>, nothing else new: QLC+ reads past it.
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QXmlStreamReader xml(&f);
        QVERIFY(xml.readNextStartElement());
        QCOMPARE(xml.name().toString(), QString("Workspace"));
        QCOMPARE(xml.attributes().value("View").toString(), QString("classic"));
        while (xml.readNextStartElement())
        {
            QVERIFY2(xml.name() != QLatin1String("View"), "the view should be an attribute");
            xml.skipCurrentElement();
        }
        QVERIFY(!xml.hasError());
    }

    for (App::WorkspaceView v : { App::ClassicView, App::EverythingView, App::ConsoleView })
    {
        m_app->setWorkspaceView(v);
        QCOMPARE(m_app->saveXML(path), QFile::NoError);
        m_app->clearDocument();
        QCOMPARE(m_app->workspaceView(), App::ConsoleView);    // a new show
        QCOMPARE(m_app->loadXML(path), QFile::NoError);
        QCOMPARE(m_app->workspaceView(), v);
        QVERIFY(!doc->isModified());
    }

    /* An older or newer build: a view it doesn't know is no view at all
       (the default), and the show loads. */
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray xml = f.readAll();
        f.close();
        QVERIFY(xml.contains("View=\"qlcconsole\""));
        xml.replace("View=\"qlcconsole\"", "View=\"tomorrow\"");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(xml);
        f.close();
        m_app->clearDocument();
        QCOMPARE(m_app->loadXML(path), QFile::NoError);
        QCOMPARE(m_app->workspaceView(), App::ConsoleView);     // saved by qlcconsole
    }
    m_app->clearDocument();
}

/* A workspace as QLC+ 4.14 writes one: no view, its own creator. */
static void writeStockShow(const QString &path, const QString &creator, bool tree)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE Workspace>\n"
        "<Workspace xmlns=\"http://www.qlcplus.org/Workspace\" CurrentWindow=\"FixtureManager\">\n"
        " <Creator>\n"
        "  <Name>" + creator.toUtf8() + "</Name>\n"
        "  <Version>4.14.2</Version>\n"
        "  <Author>someone</Author>\n"
        " </Creator>\n"
        " <Engine>\n"
        "  <InputOutputMap>\n"
        "   <Universe Name=\"Universe 1\" ID=\"0\"/>\n"
        "  </InputOutputMap>\n";
    if (tree)
        xml += "  <ShowTree NextId=\"3\"><Production ID=\"1\" Name=\"Act One\"/></ShowTree>\n";
    xml += " </Engine>\n"
           "</Workspace>\n";
    f.write(xml);
}

void AppViews_Test::aShowWithoutAViewOpensInItsDefault()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("stock.qxw");

    // An old QLC+ show: Classic QLC+.
    writeStockShow(path, "Q Light Controller Plus", false);
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QCOMPARE(m_app->m_tab->currentWidget(), static_cast<QWidget *>(FixtureManager::instance()));
    QVERIFY(!m_app->doc()->isModified());

    // One with a Show Tree: qlcconsole.
    writeStockShow(path, "Q Light Controller Plus", true);
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(!m_app->doc()->showTree()->productions().isEmpty());
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);

    // Saved by qlcconsole before views: qlcconsole.
    writeStockShow(path, "qlcconsole", false);
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);

    // A new show (and first launch, initTestCase's): qlcconsole.
    writeStockShow(path, "Q Light Controller Plus", false);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QVERIFY(m_app->slotFileNew());
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
}

void AppViews_Test::upgradingAClassicShowOffersTheConsoleView()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("old.qxw");
    writeStockShow(path, "Q Light Controller Plus", false);
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    m_app->resize(1400, 860);
    m_app->show();

    QAction *upgrade = m_app->findChild<QAction *>("fileUpgradeShow");
    QVERIFY(upgrade != nullptr);
    ShowTreeView *view = ShowTreeView::instance();
    ShowTreeController *st = m_app->doc()->showTree();

    // Upgrading nothing: no offer.
    int opened = 0;
    view->setConverterRunner([&](ShowConverter *) { ++opened; });
    upgrade->trigger();
    QCOMPARE(opened, 1);
    QVERIFY(!m_app->m_viewNotice->isVisible());

    // Upgraded (the show has a Show Tree now): one quiet line, one button.
    view->setConverterRunner([&](ShowConverter *) { ++opened; st->addProduction("Upgraded"); });
    upgrade->trigger();
    QCOMPARE(opened, 2);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);   // offered, not done
    QVERIFY(m_app->m_viewNotice->isVisible());
    QCOMPARE(m_app->m_viewNoticeButton->text(), QString("Switch to the qlcconsole view"));
    viewShot(m_app, "view-upgrade-offer.png");
    // Once: dismissed, another upgrade doesn't ask again.
    m_app->hideViewNotice();
    upgrade->trigger();
    QVERIFY(!m_app->m_viewNotice->isVisible());
    QVERIFY(!m_app->offerConsoleViewAfterUpgrade());

    // Taken, from a fresh load of the same show: the qlcconsole view.
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    st->addProduction("Upgraded");
    QVERIFY(m_app->offerConsoleViewAfterUpgrade());
    m_app->m_viewNoticeButton->click();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QVERIFY(m_app->doc()->isModified());
    QVERIFY(!m_app->m_viewNotice->isVisible());
    view->setConverterRunner(nullptr);
    m_app->hide();
    m_app->clearDocument();
}

void AppViews_Test::autoOpenFollowsWhoSavedTheShow()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("stock.qxw");
    ShutterAutoOpen *ao = m_app->doc()->shutterAutoOpen();

    // A QLC+ 4.x show never had it: off, the DMX QLC+ sends.
    writeStockShow(path, "Q Light Controller Plus", false);
    m_app->clearDocument();
    QVERIFY(ao->isEnabled());
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(ao->isEnabled() == false);
    QVERIFY(!m_app->doc()->isModified());

    // Saved by qlcconsole before the setting was always written: on.
    writeStockShow(path, "qlcconsole", false);
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(ao->isEnabled());

    // A new show: on, and saved so, so it reloads on.
    writeStockShow(path, "Q Light Controller Plus", false);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(ao->isEnabled() == false);
    QVERIFY(m_app->slotFileNew());
    QVERIFY(ao->isEnabled());
    const QString saved = dir.filePath("new.qxw");
    QCOMPARE(m_app->saveXML(saved), QFile::NoError);
    QFile f(saved);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("<ShutterAutoOpen Enabled=\"1\"/>"));
    f.close();
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(saved), QFile::NoError);
    QVERIFY(ao->isEnabled());
    QVERIFY(m_app->slotFileNew());
}

void AppViews_Test::theReadinessChipIsGreyWithNoOutputs()
{
    QLabel *chip = m_app->findChild<QLabel *>("footerReadiness");
    QVERIFY(chip != nullptr);
    m_app->clearDocument();
    m_app->clearStatusMessage();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    ShowStatus *status = ShowStatus::instance();
    QVERIFY(m_app->rigHasOutputs() == false);
    auto isRed = [chip]() { return chip->styleSheet().contains("#e04030"); };

    // Whatever an earlier test left registered goes first.
    const QList<ShowStatus::Entry> before = status->allEntries();
    for (const ShowStatus::Entry &e : before)
        status->clearStatus(e.key);
    QCOMPARE(chip->text(), QString("No outputs: preview only"));
    QCOMPARE(chip->toolTip().contains("Click to open Connections"), true);

    // Universes with fixtures going nowhere: what the chip already says.
    status->setStatus("universes.nooutput", ShowStatus::Warning,
                      "1 universe(s) with fixtures but no output");
    QCOMPARE(chip->text(), QString("No outputs: preview only"));
    QVERIFY(chip->styleSheet().contains("#909090"));
    QVERIFY(!isRed());
    QVERIFY(chip->toolTip().contains("Nothing is patched to an output"));

    // Something else to look at: counted, still grey.
    status->setStatus("test.problem", ShowStatus::Warning, "A test problem");
    QCOMPARE(chip->text(), QString("No outputs: preview only · 1 to check"));
    QVERIFY(!isRed());
    QVERIFY(chip->toolTip().contains("A test problem"));

    // A universe with an output: a rig. Now a problem is red.
    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        cache->load(stubDir);
    IOPluginStub *stub = static_cast<IOPluginStub *>(cache->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", 0, false, 0));
    QVERIFY(m_app->rigHasOutputs());
    QVERIFY2(chip->text().startsWith("Not ready"), qPrintable(chip->text()));
    QVERIFY(isRed());

    status->clearStatus("test.problem");
    status->clearStatus("universes.nooutput");
    QCOMPARE(chip->text(), QString("Ready"));
    QVERIFY(!isRed());

    // Unpatched again: back to the grey preview line.
    QVERIFY(iom->setOutputPatch(0, KOutputNone, "", QLCIOPlugin::invalidLine(), false, 0));
    QCOMPARE(iom->outputPatchesCount(0), 0);
    QVERIFY(m_app->rigHasOutputs() == false);
    QCOMPARE(chip->text(), QString("No outputs: preview only"));

    m_app->clearDocument();
    stub->m_openOutputs.clear();
}

void AppViews_Test::quittingWithTheShowTreeShowing()
{
    QTabWidget *tabs = m_app->m_tab;
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    int at = -1;
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->widget(i) == view || tabs->widget(i)->isAncestorOf(view))
            at = i;
    QVERIFY(at >= 0);
    tabs->setCurrentIndex(at);
    m_app->showNormal();
    QCoreApplication::processEvents();
    QVERIFY(view->isVisible());

    // Not hidden first: App goes while it is on screen, as at quit.
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
    QVERIFY(ShowTreeView::instance() == nullptr);
}
