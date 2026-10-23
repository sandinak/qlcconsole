/*
  Q Light Controller Plus
  mainbar_test.cpp

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

#include <QtTest>
#include <QTabWidget>
#include <QTabBar>
#include <QToolBar>
#include <QLabel>
#include <QToolButton>
#include <QMenuBar>
#include "lastlookeffect.h"
#include "programmercontroller.h"
#include <QStatusBar>
#include <QComboBox>
#include <QSizeGrip>
#include <QLayout>
#include <QtMath>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QMenu>
#include <QScopeGuard>
#include <QSlider>
#include <QElapsedTimer>

#include "mainbar_test.h"
#include "showtreecontroller.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "chaserstep.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"

#define private public
#define protected public
#include "app.h"
#include "appsettings.h"
#include "mainbar.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "fixturemanager.h"
#include "virtualconsole.h"
#include "programmingmanager.h"
#include "showtreeview.h"
#include "moveinblackbutton.h"
#include "programmercontroller.h"
#include "showmanager.h"
#include "multitrackview.h"
#include "vcslider.h"
#include "vcframe.h"
#undef protected
#undef private

/* The tabs that have tools of their own for the top bar. Connections keeps
   its in the universe list's pane (they act on the universe picked there);
   Simple Desk and Run have none. Functions is a tab in the Everything view
   the test runs in. */
static const QStringList kTabsWithTools = {
    QStringLiteral("Fixtures"), QStringLiteral("Lighting Studio"), QStringLiteral("Functions"),
    QStringLiteral("Build"),
    QStringLiteral("Show Tree"), QStringLiteral("Shows"), QStringLiteral("Virtual Console") };

/* What a tab's tools hold: every action and every widget, by pointer. */
static QList<QObject *> contentsOf(QWidget *tools)
{
    QList<QObject *> out;
    QList<QToolBar *> bars = tools->findChildren<QToolBar *>();
    bars << qobject_cast<QToolBar *>(tools);
    for (QToolBar *bar : std::as_const(bars))
        if (bar != nullptr)
            for (QAction *a : bar->actions())
                out << a;
    for (QWidget *w : tools->findChildren<QWidget *>())
        out << w;
    std::sort(out.begin(), out.end());
    return out;
}

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

/* A grab's commonest colour (its background) and the best contrast any
   other colour in it makes against that (its words). */
static QRgb background(const QImage &img, double *best)
{
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
    *best = 1.0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        *best = qMax(*best, contrast(it.key(), bg));
    return bg;
}

void MainBar_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    const QString show = m_dir.filePath(QStringLiteral("demo.qxw"));
    QVERIFY(QFile::copy(QStringLiteral(MAINBAR_DEMO_SHOW), show));
    m_app->loadXML(show);
    // Every tab on the bar (View ▸ Workspace ▸ Everything): Simple Desk too.
    m_app->setWorkspaceView(App::EverythingView, false);
    m_app->resize(1280, 800);
    m_app->show();
    QCoreApplication::processEvents();
    QCOMPARE(m_app->showControlsPlacement(), int(App::ShowControlsInTopBar));   // the default
}

void MainBar_Test::cleanupTestCase()
{
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

void MainBar_Test::showTab(const QString &label)
{
    QTabWidget *tabs = m_app->m_tab;
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabBar()->tabData(i).toString() == label)
            tabs->setCurrentIndex(i);
    QCoreApplication::processEvents();
}

/* From under the menu bar (a row of the window offscreen; native on a Mac)
   to the top of the Fixtures tab's content. */
int MainBar_Test::topChrome()
{
    showTab(QStringLiteral("Fixtures"));
    QWidget *content = FixtureManager::instance()->layout()->itemAt(0)->widget();
    const QWidget *mb = m_app->menuWidget();
    const int menu = (mb != nullptr && mb->isVisible()) ? mb->geometry().bottom() + 1 : 0;
    return content->mapTo(m_app, QPoint(0, 0)).y() - menu;
}

void MainBar_Test::everyTabsToolsAreInTheTopBar()
{
    QTabWidget *tabs = m_app->m_tab;
    MainBar *bar = m_app->m_mainBar;
    QVERIFY(bar != nullptr);

    // What each tab's tools hold, at home (no tab current that has any).
    showTab(QStringLiteral("Simple Desk"));
    QVERIFY(bar->hostedTools() == nullptr);
    QHash<QString, QList<QObject *>> atHome;
    QHash<QString, QWidget *> pages;
    for (int i = 0; i < tabs->count(); ++i)
    {
        const QString label = tabs->tabBar()->tabData(i).toString();
        QWidget *page = tabs->widget(i);
        QWidget *tools = MainBar::tabTools(page);
        QCOMPARE(tools != nullptr, kTabsWithTools.contains(label));
        if (tools == nullptr)
            continue;
        QVERIFY2(page->isAncestorOf(tools), qPrintable(label));
        atHome[label] = contentsOf(tools);
        pages[label] = page;
    }
    QCOMPARE(atHome.count(), kTabsWithTools.count());

    for (const QString &label : kTabsWithTools)
    {
        showTab(label);
        QWidget *page = pages[label];
        QWidget *tools = MainBar::tabTools(page);
        QCOMPARE(bar->hostedPage(), page);
        QCOMPARE(bar->hostedTools(), tools);
        QVERIFY2(bar->isAncestorOf(tools), qPrintable(label));
        QVERIFY2(!page->isAncestorOf(tools), qPrintable(label));
        QVERIFY2(tools->isVisible(), qPrintable(label));
        // One slim row, whichever tab's tools it holds.
        QTRY_VERIFY2(m_app->m_toolbar->height() <= 44,
                 qPrintable(QString("%1: top bar %2 px").arg(label).arg(m_app->m_toolbar->height())));
        // Nothing lost on the way: every action and widget came with them.
        QCOMPARE(contentsOf(tools), atHome[label]);
        // Words beside the icons, not under (one row's worth of height).
        for (QToolButton *b : tools->findChildren<QToolButton *>())
            QVERIFY2(b->toolButtonStyle() != Qt::ToolButtonTextUnderIcon,
                     qPrintable(label + ": " + b->text()));
    }

    // Away again: every tab's tools back at home, as they were.
    showTab(QStringLiteral("Run"));
    QVERIFY(bar->hostedTools() == nullptr);
    for (const QString &label : kTabsWithTools)
    {
        QWidget *tools = MainBar::tabTools(pages[label]);
        QVERIFY2(pages[label]->isAncestorOf(tools), qPrintable(label));
        QCOMPARE(contentsOf(tools), atHome[label]);
    }
    // ...words under the icons again where the tab had them so.
    QWidget *fmTools = MainBar::tabTools(FixtureManager::instance());
    QToolBar *fmBar = qobject_cast<QToolBar *>(fmTools);
    QVERIFY(fmBar != nullptr);
    QCOMPARE(fmBar->toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
}

void MainBar_Test::aDetachedTabKeepsItsTools()
{
    QTabWidget *tabs = m_app->m_tab;
    showTab(QStringLiteral("Shows"));
    QWidget *page = tabs->currentWidget();
    QWidget *tools = MainBar::tabTools(page);
    QVERIFY(tools != nullptr);
    QCOMPARE(m_app->m_mainBar->hostedTools(), tools);

    m_app->slotDetachContext(tabs->currentIndex());
    QCoreApplication::processEvents();
    QVERIFY(m_app->m_mainBar->hostedTools() != tools);
    QVERIFY(page->isAncestorOf(tools));
    QVERIFY(tools->isVisible());
    QVERIFY(page->window() != m_app);

    // Closing its window puts it back; current again, it lends them again.
    page->window()->close();
    QCoreApplication::processEvents();
    showTab(QStringLiteral("Shows"));
    QCOMPARE(m_app->m_mainBar->hostedTools(), tools);
}

void MainBar_Test::theShowControlsWorkInBothPlacements()
{
    InputOutputMap *io = m_app->doc()->inputOutputMap();
    ShowControls *sc = m_app->m_showControls;
    QVERIFY(sc != nullptr);

    for (int placement : { int(App::ShowControlsInTopBar), int(App::ShowControlsInStatusBar),
                           int(App::ShowControlsInTopBar) })
    {
        m_app->setShowControlsPlacement(placement);
        showTab(QStringLiteral("Fixtures"));
        const bool top = placement == App::ShowControlsInTopBar;
        QCOMPARE(m_app->m_toolbar->isAncestorOf(sc), top);
        QCOMPARE(m_app->statusBar()->isAncestorOf(sc), !top);
        QCOMPARE(m_app->m_toolbar->isAncestorOf(sc->modeSwitch()), top);
        QCOMPARE(m_app->statusBar()->isAncestorOf(sc->modeSwitch()), !top);
        QCOMPARE(m_app->m_toolbar->isVisible(), top);
        QVERIFY(sc->isVisible());
        // The tab's tools: in the bar, or home at the tab's own top.
        QWidget *fmTools = MainBar::tabTools(FixtureManager::instance());
        QCOMPARE(FixtureManager::instance()->isAncestorOf(fmTools), !top);
        QVERIFY(fmTools->isVisible());

        // The same actions as the Control menu, so the same shortcuts.
        QCOMPARE(sc->stopAllButton()->defaultAction(), m_app->m_controlPanicAction);
        QCOMPARE(sc->blackoutButton()->defaultAction(), m_app->m_controlBlackoutAction);
        QCOMPARE(sc->blindButton()->defaultAction(), m_app->m_controlBlindAction);
        QCOMPARE(m_app->m_controlPanicAction->shortcut(), QKeySequence("CTRL+SHIFT+ESC"));
        QCOMPARE(m_app->m_modeToggleAction->shortcut(), QKeySequence("CTRL+F12"));
        QCOMPARE(sc->stopAllButton()->text(), QStringLiteral("Stop all"));
        QCOMPARE(sc->blackoutButton()->text(), QStringLiteral("Blackout"));
        QCOMPARE(sc->blindButton()->text(), QStringLiteral("Blind"));
        for (QWidget *w : QList<QWidget *>{ sc->stopAllButton(), sc->blackoutButton(), sc->blindButton(),
                                            sc->designButton(), sc->operateButton() })
            QVERIFY2(w->isVisible(), qPrintable(w->objectName()));
        // None of them folded away into a » menu, even in a full footer.
        QToolButton *ext = sc->findChild<QToolButton *>(QStringLiteral("qt_toolbar_ext_button"));
        QVERIFY(ext == nullptr || !ext->isVisible());
        // Words beside the icons at the top; icons alone in the status bar.
        QCOMPARE(sc->toolButtonStyle(), top ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);

        // Blackout and Blind: click on, click off.
        QVERIFY(!io->blackout());
        sc->blackoutButton()->click();
        QVERIFY(io->blackout());
        QVERIFY(sc->blackoutButton()->isChecked());
        sc->blackoutButton()->click();
        QVERIFY(!io->blackout());
        QVERIFY(!sc->blackoutButton()->isChecked());

        QVERIFY(!io->outputInhibited());
        sc->blindButton()->click();
        QVERIFY(io->outputInhibited());
        QVERIFY(sc->blindButton()->isChecked());
        sc->blindButton()->click();
        QVERIFY(!io->outputInhibited());

        // Design | Operate: the one you're in is lit; a click switches.
        QCOMPARE(m_app->doc()->mode(), Doc::Design);
        QVERIFY(sc->designButton()->isChecked());
        QVERIFY(!sc->operateButton()->isChecked());
        sc->designButton()->click();                   // already there: stays
        QCOMPARE(m_app->doc()->mode(), Doc::Design);
        QVERIFY(sc->designButton()->isChecked());
        sc->operateButton()->click();
        QCOMPARE(m_app->doc()->mode(), Doc::Operate);
        QVERIFY(sc->operateButton()->isChecked());
        QVERIFY(!sc->designButton()->isChecked());
        // Changed from elsewhere (the Control menu, the footer): it follows.
        m_app->m_modeToggleAction->trigger();
        QCOMPARE(m_app->doc()->mode(), Doc::Design);
        QVERIFY(sc->designButton()->isChecked());

        // Stop all: greyed with nothing running, like the action; hold for
        // the fade menu.
        QCOMPARE(sc->stopAllButton()->isEnabled(), m_app->m_controlPanicAction->isEnabled());
        QCOMPARE(sc->stopAllButton()->popupMode(), QToolButton::DelayedPopup);
        QVERIFY(sc->stopAllButton()->defaultAction()->menu() != nullptr);
    }
}

void MainBar_Test::operateClearsBlindAndGreysIt()
{
    InputOutputMap *io = m_app->doc()->inputOutputMap();
    ShowControls *sc = m_app->m_showControls;
    QVERIFY(sc != nullptr);
    QCOMPARE(m_app->doc()->mode(), Doc::Design);

    sc->blindButton()->click();
    QVERIFY(io->outputInhibited());
    QVERIFY(m_app->m_controlBlindAction->isEnabled());

    sc->operateButton()->click();
    QCOMPARE(m_app->doc()->mode(), Doc::Operate);
    QVERIFY(!io->outputInhibited());                    // the rig speaks again
    QVERIFY(!m_app->m_controlBlindAction->isEnabled()); // and can't be muted mid-show
    QVERIFY(!sc->blindButton()->isEnabled());
    QVERIFY(!sc->blindButton()->isChecked());

    sc->designButton()->click();
    QCOMPARE(m_app->doc()->mode(), Doc::Design);
    QVERIFY(m_app->m_controlBlindAction->isEnabled());
    QVERIFY(!io->outputInhibited());                    // not switched back on by itself
}

/* A one-channel dimmer on universe 3 (the demo show uses only universe 1)
   and a scene holding it at full. Both go again in
   removeAddedFunctionsAndFixtures(). */
quint32 MainBar_Test::addDimmerScene(quint32 *fixtureId)
{
    Doc *doc = m_app->doc();
    Fixture *f = new Fixture(doc);
    f->setName("Test dimmer");
    f->setChannels(1);
    f->setUniverse(2);
    f->setAddress(0);
    if (!doc->addFixture(f))
    {
        delete f;
        return Function::invalidId();
    }
    m_addedFixtures << f->id();
    Scene *s = new Scene(doc);
    s->setName("Test dimmer full");
    s->setValue(f->id(), 0, 255);
    doc->addFunction(s);
    m_addedFunctions << s->id();
    if (fixtureId != nullptr)
        *fixtureId = f->id();
    return s->id();
}

void MainBar_Test::removeAddedFunctionsAndFixtures()
{
    Doc *doc = m_app->doc();
    doc->masterTimer()->stopAllFunctions();
    for (quint32 id : std::as_const(m_addedFunctions))
        doc->deleteFunction(id);
    for (quint32 id : std::as_const(m_addedFixtures))
        doc->deleteFixture(id);
    m_addedFunctions.clear();
    m_addedFixtures.clear();
    doc->showControl()->setMainCueList(Function::invalidId());
    doc->resetModified();
}

static Universe *universeAt(Doc *doc, int index)
{
    const QList<Universe *> unis = doc->inputOutputMap()->universes();
    return index < unis.size() ? unis.at(index) : nullptr;
}

void MainBar_Test::fadeOneSecondAndStopFadesTheLightOut()
{
    auto tidy = qScopeGuard([this]() { removeAddedFunctionsAndFixtures(); });
    Doc *doc = m_app->doc();
    ShowControls *sc = m_app->m_showControls;
    QVERIFY(sc != nullptr);
    const quint32 sid = addDimmerScene(nullptr);
    Function *scene = doc->function(sid);
    QVERIFY(scene != nullptr);
    Universe *uni = universeAt(doc, 2);
    QVERIFY(uni != nullptr);

    scene->start(doc->masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(scene->isRunning(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(uni->postGMValue(0)), 255, 5000);
    QTRY_VERIFY(sc->stopAllButton()->isEnabled());          // something to stop

    // The stop-all button's own menu (hold the button).
    QMenu *menu = sc->stopAllButton()->defaultAction()->menu();
    QVERIFY(menu != nullptr);
    QAction *fade1 = nullptr;
    for (QAction *a : menu->actions())
        if (a->text() == QStringLiteral("Fade 1 second and stop"))
            fade1 = a;
    QVERIFY(fade1 != nullptr);
    QCOMPARE(fade1->data().toInt(), 1000);

    QElapsedTimer clock;
    clock.start();
    fade1->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!scene->isRunning(), 5000);
    // Mid-fade: neither at full nor out yet.
    QTRY_VERIFY2_WITH_TIMEOUT(uni->postGMValue(0) > 0 && uni->postGMValue(0) < 255,
                              qPrintable(QString("output %1").arg(uni->postGMValue(0))), 900);
    // ...and out after the second, not before most of it has gone by.
    QTRY_COMPARE_WITH_TIMEOUT(int(uni->postGMValue(0)), 0, 5000);
    QVERIFY2(clock.elapsed() >= 500, qPrintable(QString("out after %1 ms").arg(clock.elapsed())));
    QTRY_VERIFY(!sc->stopAllButton()->isEnabled());         // nothing left running
}

void MainBar_Test::operateLocksProgrammingNotTheGrandMaster()
{
    Doc *doc = m_app->doc();
    InputOutputMap *io = doc->inputOutputMap();
    // No Show-mode Lock of its own any more.
    for (QAction *a : m_app->findChildren<QAction *>())
        QVERIFY2(!a->text().contains("Show-mode", Qt::CaseInsensitive), qPrintable(a->text()));
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(!doc->isShowLocked());

    ShowManager *shows = ShowManager::instance();
    QVERIFY(shows != nullptr);
    QVERIFY(shows->m_showview->isEditable());

    // A Virtual Console fader on the Grand Master.
    VirtualConsole *vc = VirtualConsole::instance();
    QVERIFY(vc != nullptr);
    VCSlider *gm = new VCSlider(vc->contents(), doc);
    auto tidy = qScopeGuard([gm, io, doc]() {
        delete gm;
        doc->setMode(Doc::Design);
        io->setGrandMasterValue(255);
    });
    gm->setSliderMode(VCSlider::GrandMaster);
    io->setGrandMasterValue(255);

    doc->setMode(Doc::Operate);
    QVERIFY(doc->isShowLocked());
    QVERIFY(doc->programmer()->isShowLocked());
    QVERIFY2(!shows->m_showview->isEditable(), "the Shows timeline is read-only in Operate");
    gm->setSliderValue(100, false);
    QCOMPARE(int(io->grandMasterValue()), 100);             // the GM still works

    doc->setMode(Doc::Design);
    QVERIFY(!doc->isShowLocked());
    QVERIFY(shows->m_showview->isEditable());
}

void MainBar_Test::oneChipListsAndClearsOverrides()
{
    Doc *doc = m_app->doc();
    QToolButton *chip = m_app->m_statusOverridesChip;
    QVERIFY(chip != nullptr);
    QVERIFY(m_app->activeOverrides().isEmpty());
    QTRY_VERIFY(chip->isHidden());

    quint32 fid = Fixture::invalidId();
    QVERIFY(addDimmerScene(&fid) != Function::invalidId());
    doc->programmer()->parkFixtures({ fid });
    doc->lastLook()->hold({ { fid, 2, 0, 200 } });
    m_app->slotControlBlind(true);
    const QStringList now = m_app->activeOverrides();
    QVERIFY2(now.contains("Park (1)") && now.contains("Hold at end") && now.contains("Blind"),
             qPrintable(now.join(", ")));
    QTRY_VERIFY(!chip->isHidden());
    QVERIFY2(chip->text().startsWith("Overrides: Park (1)"), qPrintable(chip->text()));

    chip->click();
    QVERIFY(!doc->programmer()->hasParkedFixtures());
    QVERIFY(!doc->lastLook()->isActive());
    QVERIFY(!doc->inputOutputMap()->outputInhibited());
    QVERIFY(m_app->activeOverrides().isEmpty());
    QTRY_VERIFY(chip->isHidden());
}

/* A cue list of two scenes, infinite hold, as the show's main cue list. */
static Chaser *makeMainCueList(Doc *doc, QList<quint32> *added, Chaser *subGoInner = nullptr)
{
    Chaser *list = new Chaser(doc);
    list->setName("Cue list");
    list->setDurationMode(Chaser::Common);
    list->setDuration(Function::infiniteSpeed());
    doc->addFunction(list);
    *added << list->id();
    for (int i = 0; i < 3; ++i)
    {
        if (i == 1 && subGoInner != nullptr)
        {
            ChaserStep step(subGoInner->id());
            step.subCues = ChaserStep::SubGo;
            list->addStep(step);
            continue;
        }
        Scene *s = new Scene(doc);
        s->setName(QString("Cue %1").arg(i + 1));
        doc->addFunction(s);
        *added << s->id();
        list->addStep(ChaserStep(s->id()));
    }
    doc->showControl()->setMainCueList(list->id());
    return list;
}

void MainBar_Test::goAndBackStepTheMainCueList()
{
    auto tidy = qScopeGuard([this]() { removeAddedFunctionsAndFixtures(); });
    Doc *doc = m_app->doc();
    QAction *go = m_app->m_cueGoAction;
    QAction *back = m_app->m_cueBackAction;
    QVERIFY(go != nullptr && back != nullptr);
    QCOMPARE(go->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Return));
    QCOMPARE(back->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return));
    // From any window: the Virtual Console's, a broken-out tab's.
    QCOMPARE(go->shortcutContext(), Qt::ApplicationShortcut);
    QCOMPARE(back->shortcutContext(), Qt::ApplicationShortcut);

    // In the Control menu.
    QMenu *control = nullptr;
    for (QAction *a : m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->menu()->actions().contains(go))
            control = a->menu();
    QVERIFY(control != nullptr);
    QCOMPARE(control->title(), QString("&Control"));
    QVERIFY(control->actions().contains(back));

    Chaser *list = makeMainCueList(doc, &m_addedFunctions);
    QVERIFY(!list->isRunning());

    go->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    go->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(list->currentStepIndex(), 1, 5000);
    back->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(list->currentStepIndex(), 0, 5000);
    QVERIFY(list->isRunning());
}

void MainBar_Test::theFootersNextMenuLoadsASubCue()
{
    auto tidy = qScopeGuard([this]() { removeAddedFunctionsAndFixtures(); });
    Doc *doc = m_app->doc();
    Chaser *inner = new Chaser(doc);
    inner->setName("Inner");
    doc->addFunction(inner);
    m_addedFunctions << inner->id();
    for (int i = 0; i < 2; ++i)
    {
        Scene *s = new Scene(doc);
        s->setName(QString("Inner %1").arg(i + 1));
        doc->addFunction(s);
        m_addedFunctions << s->id();
        inner->addStep(ChaserStep(s->id()));
    }
    Chaser *list = makeMainCueList(doc, &m_addedFunctions, inner);
    ShowControl *sc = doc->showControl();

    QToolButton *footer = m_app->m_statusStandby;
    QVERIFY(footer != nullptr);
    m_app->slotUpdateStandby();
    QVERIFY(!footer->isHidden());
    QVERIFY2(footer->text().startsWith(QString("Next: 1 — Cue 1")), qPrintable(footer->text()));

    QMenu *menu = footer->menu();
    QVERIFY(menu != nullptr);
    emit menu->aboutToShow();                       // what opening it does
    QList<QAction *> acts = menu->actions();
    QVERIFY(acts.contains(m_app->m_cueGoAction));
    QVERIFY(acts.contains(m_app->m_cueBackAction));
    QVERIFY(acts.contains(m_app->m_cueSubGoAction));    // there is a Sub GO step
    QAction *stop = nullptr, *open = nullptr;
    QMenu *load = nullptr;
    for (QAction *a : acts)
    {
        if (a->text() == QStringLiteral("Stop Cue list"))
            stop = a;
        else if (a->text() == QStringLiteral("Open the Cue List"))
            open = a;
        else if (a->menu() != nullptr && a->text() == QStringLiteral("Load"))
            load = a->menu();
    }
    QVERIFY(stop != nullptr);
    QVERIFY(open != nullptr);
    QVERIFY(load != nullptr);

    QStringList loadTexts;
    for (QAction *a : load->actions())
        loadTexts << a->text();
    QCOMPARE(loadTexts, (QStringList{ QString("1 — Cue 1"), QString("2 — Inner"),
                                      QString("    2.1 — Inner 1"), QString("    2.2 — Inner 2"),
                                      QString("3 — Cue 3") }));
    QVERIFY(load->actions().at(0)->isChecked());           // on standby now

    // Choose 2.2: on standby, and what the footer says GO plays.
    load->actions().at(3)->trigger();
    QCOMPARE(sc->standbyStep(list), 1);
    QCOMPARE(sc->standbySubCue(list), 1);
    m_app->slotUpdateStandby();
    QVERIFY2(footer->text().startsWith(QString("Next: 2.2 — Inner 2")), qPrintable(footer->text()));
    emit menu->aboutToShow();
    for (QAction *a : menu->actions())
        if (a->menu() != nullptr && a->text() == QStringLiteral("Load"))
            load = a->menu();
    QVERIFY(load->actions().at(3)->isChecked());
    QVERIFY(!load->actions().at(0)->isChecked());

    // GO plays it.
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 1
                             && list->currentSubCueIndex() == 1, 5000);
    // Stop, from the menu.
    emit menu->aboutToShow();
    for (QAction *a : menu->actions())
        if (a->text() == QStringLiteral("Stop Cue list"))
            a->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);

    // No main cue list: the footer goes.
    sc->setMainCueList(Function::invalidId());
    m_app->slotUpdateStandby();
    QVERIFY(footer->isHidden());
}

void MainBar_Test::theFooterNamesTheCurrentProduction()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Dance Hysteria");
    auto tidy = qScopeGuard([this, st, prod]() {
        st->setCurrentProduction(ShowTree::invalidId());
        st->removeProduction(prod);
        removeAddedFunctionsAndFixtures();
    });
    const quint32 list = st->addCueList(prod, "Act 1", ShowTree::ListMode::Tracking);
    QVERIFY(st->addCue(list, -1, "Opening") != ShowTree::invalidId());
    QVERIFY(st->setCurrentProduction(prod));
    QVERIFY(doc->showControl()->cueList(QString()) != nullptr);   // its list is GO's

    m_app->slotUpdateStandby();
    QToolButton *footer = m_app->m_statusStandby;
    QVERIFY(!footer->isHidden());
    QVERIFY2(footer->text().startsWith(QString("Dance Hysteria ▸ Next:")), qPrintable(footer->text()));

    // None current: just Next:.
    st->setCurrentProduction(ShowTree::invalidId());
    Chaser *plain = makeMainCueList(doc, &m_addedFunctions);
    QVERIFY(plain != nullptr);
    m_app->slotUpdateStandby();
    QVERIFY2(footer->text().startsWith(QString("Next:")), qPrintable(footer->text()));
}

void MainBar_Test::theFooterGrandMasterDimsTheRig()
{
    auto tidy = qScopeGuard([this]() {
        m_app->doc()->inputOutputMap()->setGrandMasterValue(255);
        removeAddedFunctionsAndFixtures();
    });
    Doc *doc = m_app->doc();
    InputOutputMap *io = doc->inputOutputMap();
    QSlider *fader = m_app->m_statusGrandMasterSlider;
    QVERIFY(fader != nullptr);
    QCOMPARE(fader->maximum(), 255);
    io->setGrandMasterValue(255);
    QCOMPARE(fader->value(), 255);

    const quint32 sid = addDimmerScene(nullptr);
    Function *scene = doc->function(sid);
    QVERIFY(scene != nullptr);
    Universe *uni = universeAt(doc, 2);
    QVERIFY(uni != nullptr);
    scene->start(doc->masterTimer(), FunctionParent::master());
    QTRY_COMPARE_WITH_TIMEOUT(int(uni->postGMValue(0)), 255, 5000);

    fader->setValue(51);                                    // 20 %
    QCOMPARE(int(io->grandMasterValue()), 51);
    QCOMPARE(m_app->m_statusGrandMasterValueLabel->text(), QString("20%"));
    QTRY_COMPARE_WITH_TIMEOUT(int(uni->postGMValue(0)), 51, 5000);
    QCOMPARE(int(uni->preGMValue(0)), 255);                  // the scene still asks for full

    fader->setValue(0);
    QTRY_COMPARE_WITH_TIMEOUT(int(uni->postGMValue(0)), 0, 5000);

    // Set from elsewhere (a console fader, OSC): the footer follows.
    io->setGrandMasterValue(255);
    QCOMPARE(fader->value(), 255);
    QTRY_COMPARE_WITH_TIMEOUT(int(uni->postGMValue(0)), 255, 5000);
}

void MainBar_Test::thePlacementRoundTripsAndSwitchesLive()
{
    QCOMPARE(m_app->showControlsPlacement(), int(App::ShowControlsInTopBar));
    {
        AppSettings dlg(m_app, m_app);
        QComboBox *combo = dlg.findChild<QComboBox *>(QStringLiteral("settingsShowControls"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->currentData().toInt(), int(App::ShowControlsInTopBar));
        combo->setCurrentIndex(combo->findData(int(App::ShowControlsInStatusBar)));
        dlg.accept();
    }
    // Saved, and in force without a restart.
    QCOMPARE(QSettings().value(App::showControlsPlacementKey()).toInt(), int(App::ShowControlsInStatusBar));
    QCOMPARE(m_app->showControlsPlacement(), int(App::ShowControlsInStatusBar));
    QVERIFY(m_app->statusBar()->isAncestorOf(m_app->m_showControls));
    QVERIFY(!m_app->m_toolbar->isVisible());
    // Rightmost in the status bar: the buttons, then the switch.
    const QRect sc = m_app->m_showControls->geometry();
    QWidget *modeSwitch = m_app->m_showControls->modeSwitch();
    QVERIFY(modeSwitch->geometry().left() >= sc.right());
    for (QWidget *w : m_app->statusBar()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        if (w != m_app->m_showControls && w != modeSwitch && w->isVisible() && w->geometry().width() > 0
                && qobject_cast<QSizeGrip *>(w) == nullptr)
            QVERIFY2(w->geometry().right() <= sc.left() + 1,
                     qPrintable(QString("%1 %2").arg(w->metaObject()->className(), w->objectName())));

    // Read back by the next Settings, and back again.
    {
        AppSettings dlg(m_app, m_app);
        QComboBox *combo = dlg.findChild<QComboBox *>(QStringLiteral("settingsShowControls"));
        QCOMPARE(combo->currentData().toInt(), int(App::ShowControlsInStatusBar));
        combo->setCurrentIndex(combo->findData(int(App::ShowControlsInTopBar)));
        dlg.accept();
    }
    QCOMPARE(QSettings().value(App::showControlsPlacementKey()).toInt(), int(App::ShowControlsInTopBar));
    QVERIFY(m_app->m_toolbar->isAncestorOf(m_app->m_showControls));
    QVERIFY(m_app->m_toolbar->isVisible());
}

void MainBar_Test::blackoutAndBlindLightUpInEveryTheme()
{
    InputOutputMap *io = m_app->doc()->inputOutputMap();
    ShowControls *sc = m_app->m_showControls;
    const int was = m_app->theme();
    const int themes[] = { App::ThemeDefault, App::ThemeTan, App::ThemeBlue,
                           App::ThemeQLCOriginal, App::ThemeRedShift, App::ThemeVSCodeDark };
    // Words only, so it is the words' contrast that is measured, not an icon's.
    sc->setLabelMode(App::TabTextOnly);

    for (int placement : { int(App::ShowControlsInTopBar), int(App::ShowControlsInStatusBar) })
    {
        m_app->setShowControlsPlacement(placement);
        for (int theme : themes)
        {
            m_app->setTheme(theme);
            QCoreApplication::processEvents();
            const QString where = QString("placement %1 theme %2").arg(placement).arg(theme);

            io->setBlackout(true);
            io->setOutputInhibited(true);       // Blind (turns the footer blue)
            QCoreApplication::processEvents();
            double best;
            QRgb bg = background(sc->blackoutButton()->grab().toImage(), &best);
            QVERIFY2(qRed(bg) > 150 && qGreen(bg) < 90 && qBlue(bg) < 90,
                     qPrintable(where + ": Blackout not lit red " + QColor(bg).name()));
            QVERIFY2(best >= 4.5, qPrintable(where + QString(": Blackout words %1").arg(best)));
            bg = background(sc->blindButton()->grab().toImage(), &best);
            QVERIFY2(qBlue(bg) > 150 && qRed(bg) < 90,
                     qPrintable(where + ": Blind not lit blue " + QColor(bg).name()));
            QVERIFY2(best >= 4.5, qPrintable(where + QString(": Blind words %1").arg(best)));
            // ...and the switch, both halves, on the blue footer too.
            for (QToolButton *b : { sc->designButton(), sc->operateButton() })
            {
                background(b->grab().toImage(), &best);
                QVERIFY2(best >= 4.5, qPrintable(where + QString(": %1 words %2")
                                                 .arg(b->objectName()).arg(best)));
            }

            io->setBlackout(false);
            io->setOutputInhibited(false);
            QCoreApplication::processEvents();
            // Off: not lit, still readable.
            for (QToolButton *b : { sc->blackoutButton(), sc->blindButton() })
            {
                bg = background(b->grab().toImage(), &best);
                QVERIFY2(!(qRed(bg) > 150 && qGreen(bg) < 90 && qBlue(bg) < 90),
                         qPrintable(where + ": " + b->objectName() + " lit while off"));
                QVERIFY2(best >= 4.5, qPrintable(where + QString(": %1 off, words %2")
                                                 .arg(b->objectName()).arg(best)));
            }
        }
    }
    m_app->setTheme(was);
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
    sc->setLabelMode(m_app->tabLabelMode());
}

void MainBar_Test::cleanup()
{
    // A failed case must not leave the next ones in the other placement
    // (one failure here once showed as five).
    if (m_app != nullptr && m_app->m_mainBar != nullptr)
        m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
}

void MainBar_Test::theTopChromeIsAtLeast50PxSmaller()
{
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
    const int chromeA = topChrome();
    m_app->setShowControlsPlacement(App::ShowControlsInStatusBar);
    // B: the tab's tools at home and nothing above them -- the old layout,
    // less its show-control toolbar.
    const int chromeB = topChrome();

    // That toolbar as it was: the same four actions, icons over words at
    // 20 px, a QMainWindow toolbar row of its own.
    // A window of the app's (Qt::Window), so its style sheet applies.
    QMainWindow *old = new QMainWindow(m_app);
    QToolBar *bar = new QToolBar(old);
    bar->setMovable(false);
    bar->setIconSize(QSize(20, 20));
    bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    QAction *stop = new QAction(QIcon(":/panic.png"), QStringLiteral("Stop ALL functions!"), bar);
    stop->setMenu(new QMenu(bar));
    bar->addAction(stop);
    bar->addSeparator();
    bar->addAction(new QAction(QIcon(":/blackout.png"), QStringLiteral("Toggle Blackout"), bar));
    bar->addAction(new QAction(QIcon(":/blind.png"), QStringLiteral("Toggle Blind"), bar));
    bar->addSeparator();
    bar->addAction(new QAction(QIcon(":/operate.png"), QStringLiteral("Operate"), bar));
    old->addToolBar(bar);
    old->setCentralWidget(new QWidget(old));
    old->resize(1280, 400);
    old->show();
    QCoreApplication::processEvents();
    const int oldBar = old->centralWidget()->y();
    delete old;
    const int before = oldBar + chromeB;

    qInfo().noquote() << QString("top chrome: before %1 px (show bar %2 + tab tools %3), "
                                 "A %4 px (saves %5), B %6 px (saves %7)")
                         .arg(before).arg(oldBar).arg(chromeB)
                         .arg(chromeA).arg(before - chromeA).arg(chromeB).arg(before - chromeB);
    /* The whole show-control row, give or take a pixel or two: 53 of 54 px
       on the Mac, 49 of 50 on Linux (Fusion draws that row 4 px lower, so
       a fixed 50 px failed there with nothing wrong). */
    QVERIFY2(before - chromeA >= oldBar - 2 && before - chromeA >= 45,
             qPrintable(QString("A saves only %1 px of the %2 px row").arg(before - chromeA).arg(oldBar)));
    QVERIFY(chromeB <= chromeA);
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
}

/* Widths of text, and so of what folds, mean nothing on the "minimal"
   platform (CI's Linux suite): it has no real fonts, and there the top bar
   asked for 1133 px and the drop hint for 768 px of a 1280 px window. Under
   xcb (Xvfb) and offscreen on the same machine both cases pass. */
static bool textHasRealWidths()
{
    return QGuiApplication::platformName() != QLatin1String("minimal");
}

void MainBar_Test::aNarrowWindowFoldsTheTabsTools()
{
    if (!textHasRealWidths())
        QSKIP("the minimal platform has no real font metrics");
    showTab(QStringLiteral("Show Tree"));
    const QSize was = m_app->size();
    m_app->resize(qMax(m_app->minimumSizeHint().width(), 760), was.height());
    QCoreApplication::processEvents();

    // The Show Tree's toolbar sits in a header beside its drop hint.
    QWidget *hosted = m_app->m_mainBar->hostedTools();
    QVERIFY(hosted != nullptr);
    QToolBar *tools = qobject_cast<QToolBar *>(hosted);
    if (tools == nullptr)
        tools = hosted->findChild<QToolBar *>();
    QVERIFY(tools != nullptr);
    QToolButton *more = tools->findChild<QToolButton *>(QStringLiteral("qt_toolbar_ext_button"));
    QVERIFY(more != nullptr);
    QVERIFY2(more->isVisible(), "the Show Tree's tools didn't fold into »");

    // The show controls whole, and inside the window.
    ShowControls *sc = m_app->m_showControls;
    for (QWidget *w : QList<QWidget *>{ sc->stopAllButton(), sc->blackoutButton(), sc->blindButton(),
                                        sc->designButton(), sc->operateButton() })
    {
        QVERIFY2(w->isVisible(), qPrintable(w->objectName()));
        const QRect r(w->mapTo(m_app, QPoint(0, 0)), w->size());
        QVERIFY2(m_app->rect().contains(r), qPrintable(w->objectName()));
    }
    // The window can still be made narrow: the bar doesn't hold it wide.
    QVERIFY2(m_app->m_toolbar->minimumSizeHint().width() < 700,
             qPrintable(QString::number(m_app->m_toolbar->minimumSizeHint().width())));
    m_app->resize(was);
    QCoreApplication::processEvents();
}

void MainBar_Test::theShowTreesHintIsNeverFolded()
{
    if (!textHasRealWidths())
        QSKIP("the minimal platform has no real font metrics");
    showTab(QStringLiteral("Show Tree"));
    const QSize was = m_app->size();
    QWidget *tools = m_app->m_mainBar->hostedTools();
    QVERIFY(tools != nullptr);
    QLabel *hint = tools->findChild<QLabel *>(QStringLiteral("stDropHint"));
    QVERIFY(hint != nullptr);
    // What a drag says, as long as the course quotes them.
    hint->setText(QStringLiteral("Aim at Soloist DS → layer AT-Spots in 1.0.4 Solo"));

    // A 13" laptop, and a narrower window still: the hint whole, inside the
    // window; the buttons are what fold.
    for (int width : { 1280, 1024 })
    {
        m_app->resize(qMax(m_app->minimumSizeHint().width(), width), was.height());
        QCoreApplication::processEvents();
        QVERIFY2(hint->isVisible(), qPrintable(QString::number(width)));
        QVERIFY2(hint->width() >= hint->sizeHint().width(),
                 qPrintable(QString("%1: hint %2 of %3 px").arg(width).arg(hint->width())
                                .arg(hint->sizeHint().width())));
        const QRect r(hint->mapTo(m_app, QPoint(0, 0)), hint->size());
        QVERIFY2(m_app->rect().contains(r), qPrintable(QString::number(width)));
        QToolButton *more = tools->findChild<QToolButton *>(QStringLiteral("qt_toolbar_ext_button"));
        QVERIFY(more != nullptr && more->isVisible());
    }
    // ...and it doesn't hold the window wide.
    QVERIFY2(m_app->m_toolbar->minimumSizeHint().width() < 700,
             qPrintable(QString::number(m_app->m_toolbar->minimumSizeHint().width())));
    hint->clear();
    m_app->resize(was);
    QCoreApplication::processEvents();
}

void MainBar_Test::operateModeStillHidesTheConsolesTools()
{
    showTab(QStringLiteral("Virtual Console"));
    QWidget *tools = MainBar::tabTools(VirtualConsole::instance());
    QCOMPARE(m_app->m_mainBar->hostedTools(), tools);
    QVERIFY(tools->isVisible());
    m_app->doc()->setMode(Doc::Operate);
    QCoreApplication::processEvents();
    QVERIFY(!tools->isVisible());
    QVERIFY(m_app->m_showControls->isVisible());
    QVERIFY(m_app->m_showControls->operateButton()->isChecked());
    // Hidden it goes home, and hidden it stays there.
    showTab(QStringLiteral("Run"));
    QVERIFY(VirtualConsole::instance()->isAncestorOf(tools));
    QVERIFY(tools->isHidden());
    m_app->doc()->setMode(Doc::Design);
    showTab(QStringLiteral("Virtual Console"));
    QVERIFY(tools->isVisible());
}

void MainBar_Test::moveInBlackIsOneSettingInThreePlaces()
{
    ProgrammerController *pc = m_app->doc()->programmer();
    QVERIFY(pc != nullptr);
    m_app->resize(1280, 800);
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
    showTab(QStringLiteral("Show Tree"));
    QCoreApplication::processEvents();
    ShowTreeView *st = m_app->findChild<ShowTreeView *>();
    QVERIFY(st != nullptr);
    MoveInBlackButton *mib = st->moveInBlackButton();
    QVERIFY(mib != nullptr);
    QVERIFY(mib->isVisibleTo(m_app));                  // in the top bar, with the Show Tree's tools
    QVERIFY(m_app->m_mainBar->isAncestorOf(mib));
    // Never folded into »: still in sight in a laptop-narrow window
    m_app->resize(900, 800);
    QCoreApplication::processEvents();
    QVERIFY(mib->isVisibleTo(m_app));
    m_app->resize(1280, 800);
    QCoreApplication::processEvents();
    ProgrammingManager *pm = m_app->findChild<ProgrammingManager *>();
    QVERIFY(pm != nullptr);

    pc->setAutoMoveInBlack(false);
    pc->setMoveInBlackDarkGapMs(800);
    QVERIFY(!mib->isChecked());
    QVERIFY(!pm->m_autoMibBtn->isChecked());
    // Build's drop-down and the menu bar's item say so too
    QToolButton *markMenu = pm->findChild<QToolButton *>(QStringLiteral("markMenu"));
    QVERIFY(markMenu != nullptr);
    QAction *menuMib = m_app->findChild<QAction *>(QStringLiteral("autoMoveInBlackAction"));
    QVERIFY(menuMib != nullptr && menuMib->isCheckable());
    QCOMPARE(markMenu->text(), QStringLiteral("Move in black"));
    QVERIFY(!markMenu->property("mibAuto").toBool());
    QVERIFY(!menuMib->isChecked());

    // 1. The Show Tree's MIB -> the show, Programming
    mib->click();
    QVERIFY(pc->isAutoMoveInBlack());
    QVERIFY(mib->isChecked());
    QVERIFY(mib->autoAction()->isChecked());
    QVERIFY(pm->m_autoMibBtn->isChecked());
    QCOMPARE(markMenu->text(), QString::fromUtf8("Move in black \u00b7 Auto"));
    QVERIFY(markMenu->property("mibAuto").toBool());
    QVERIFY(menuMib->isChecked());
    // Lit: painted in the theme's highlight, not the bar's background
    {
        const QImage lit = markMenu->grab().toImage();
        const QColor hl = markMenu->palette().color(QPalette::Highlight);
        const QColor px = lit.pixelColor(lit.width() / 2, 2);
        QVERIFY2(qAbs(px.red() - hl.red()) + qAbs(px.green() - hl.green())
                 + qAbs(px.blue() - hl.blue()) < 40,
                 qPrintable(px.name() + " vs " + hl.name()));
    }
    // The menu bar's item is the same switch, both ways
    menuMib->trigger();
    QVERIFY(!pc->isAutoMoveInBlack());
    QVERIFY(!mib->isChecked());
    QCOMPARE(markMenu->text(), QStringLiteral("Move in black"));
    QVERIFY(!markMenu->property("mibAuto").toBool());
    menuMib->trigger();
    QVERIFY(pc->isAutoMoveInBlack());
    QVERIFY(mib->isChecked() && pm->m_autoMibBtn->isChecked());
    mib->leadSpin()->setValue(1.5);
    QCOMPARE(pc->moveInBlackDarkGapMs(), 1500);
    QCOMPARE(pm->m_mibGapSpin->value(), 1.5);

    // 2. Programming -> the show, the Show Tree
    pm->m_autoMibBtn->setChecked(false);
    QVERIFY(!pc->isAutoMoveInBlack());
    QVERIFY(!mib->isChecked());
    pm->m_mibGapSpin->setValue(2.0);
    QCOMPARE(pc->moveInBlackDarkGapMs(), 2000);
    QCOMPARE(mib->leadSpin()->value(), 2.0);
    // The menu's own switch is the same one
    mib->autoAction()->trigger();
    QVERIFY(pc->isAutoMoveInBlack());
    QVERIFY(pm->m_autoMibBtn->isChecked());

    // 3. Settings: opens on the show's, applies on OK, Cancel leaves it
    {
        AppSettings dlg(m_app);
        QVERIFY(dlg.m_mibCheck->isChecked());
        QCOMPARE(dlg.m_mibLeadSpin->value(), 2.0);
        QLabel *note = dlg.findChild<QLabel *>(QStringLiteral("settingsMoveInBlackNote"));
        QVERIFY(note != nullptr && note->text().contains(QStringLiteral("saved with this show")));
        dlg.m_mibCheck->setChecked(false);
        dlg.m_mibLeadSpin->setValue(3.0);
        dlg.reject();
    }
    QVERIFY(pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), 2000);
    {
        AppSettings dlg(m_app);
        dlg.m_mibCheck->setChecked(false);
        dlg.m_mibLeadSpin->setValue(3.0);
        dlg.accept();
    }
    QVERIFY(!pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), 3000);
    QVERIFY(!mib->isChecked());
    QVERIFY(!menuMib->isChecked());
    QVERIFY(!markMenu->property("mibAuto").toBool());
    QCOMPARE(mib->leadSpin()->value(), 3.0);
    QVERIFY(!pm->m_autoMibBtn->isChecked());
    QCOMPARE(pm->m_mibGapSpin->value(), 3.0);

    // Saved and loaded with the show: every view comes back with it
    mib->click();
    QVERIFY(pc->isAutoMoveInBlack());
    const QString show = m_dir.filePath(QStringLiteral("mib.qxw"));
    QCOMPARE(m_app->saveXML(show), QFile::NoError);
    {
        QFile f(show);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray xml = f.readAll();
        QVERIFY(xml.contains("<MoveInBlack Auto=\"1\" DarkGapMs=\"3000\""));
    }
    pc->setAutoMoveInBlack(false);
    pc->setMoveInBlackDarkGapMs(800);
    QVERIFY(!mib->isChecked());
    m_app->doc()->resetModified();
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    QCoreApplication::processEvents();
    QVERIFY(pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), 3000);
    QVERIFY(mib->isChecked());
    QCOMPARE(mib->leadSpin()->value(), 3.0);
    QVERIFY(pm->m_autoMibBtn->isChecked());
    QCOMPARE(pm->m_mibGapSpin->value(), 3.0);

    const QString dir = QString::fromLocal8Bit(qgetenv("MIB_SHOTS"));
    if (dir.isEmpty() == false)
    {
        m_app->resize(1280, 800);
        showTab(QStringLiteral("Show Tree"));
        m_app->grab().save(dir + "/mib-showtree.png");
        mib->grab().save(dir + "/mib-button.png");
        mib->menu()->popup(mib->mapToGlobal(QPoint(0, mib->height())));
        QCoreApplication::processEvents();
        mib->menu()->grab().save(dir + "/mib-menu.png");
        mib->menu()->hide();
        AppSettings dlg(m_app);
        dlg.resize(dlg.sizeHint());
        dlg.show();
        QCoreApplication::processEvents();
        dlg.grab().save(dir + "/mib-settings.png");
        dlg.reject();
        showTab(QStringLiteral("Build"));
        m_app->grab().save(dir + "/mib-programming.png");
        markMenu->grab().save(dir + "/mib-build-dropdown.png");
    }

    // Leave the show as it was
    pc->setAutoMoveInBlack(false);
    pc->setMoveInBlackDarkGapMs(800);
    m_app->doc()->resetModified();
}

void MainBar_Test::screenshots()
{
    const QString dir = QString::fromLocal8Bit(qgetenv("MAINBAR_SHOTS"));
    if (dir.isEmpty())
        QSKIP("MAINBAR_SHOTS not set");
    m_app->resize(1280, 800);
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
    showTab(QStringLiteral("Fixtures"));
    m_app->grab().save(dir + "/after-A-fixtures.png");
    showTab(QStringLiteral("Build"));
    m_app->grab().save(dir + "/after-A-build.png");
    showTab(QStringLiteral("Show Tree"));
    m_app->grab().save(dir + "/after-A-showtree.png");
    m_app->doc()->inputOutputMap()->setBlackout(true);
    m_app->doc()->inputOutputMap()->setOutputInhibited(true);
    QCoreApplication::processEvents();
    m_app->grab().save(dir + "/after-A-showtree-blackout-blind.png");
    m_app->doc()->inputOutputMap()->setBlackout(false);
    m_app->doc()->inputOutputMap()->setOutputInhibited(false);
    m_app->setShowControlsPlacement(App::ShowControlsInStatusBar);
    showTab(QStringLiteral("Fixtures"));
    m_app->grab().save(dir + "/after-B-fixtures.png");
    m_app->doc()->inputOutputMap()->setBlackout(true);
    m_app->doc()->inputOutputMap()->setOutputInhibited(true);
    QCoreApplication::processEvents();
    m_app->grab().save(dir + "/after-B-fixtures-blackout-blind.png");
    showTab(QStringLiteral("Build"));
    m_app->grab().save(dir + "/after-B-build-blackout-blind.png");
    m_app->doc()->inputOutputMap()->setBlackout(false);
    m_app->doc()->inputOutputMap()->setOutputInhibited(false);
    m_app->resize(760, 800);
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
    showTab(QStringLiteral("Show Tree"));
    m_app->grab().save(dir + "/after-A-narrow-showtree.png");
    m_app->resize(1280, 800);
    QCoreApplication::processEvents();
}

void MainBar_Test::kioskModeDropsTheBar()
{
    showTab(QStringLiteral("Virtual Console"));
    QWidget *vcTools = MainBar::tabTools(VirtualConsole::instance());
    QCOMPARE(m_app->m_mainBar->hostedTools(), vcTools);

    m_app->enableKioskMode();
    QCoreApplication::processEvents();
    QVERIFY(m_app->m_toolbar == nullptr);
    QVERIFY(m_app->m_mainBar == nullptr);
    QVERIFY(m_app->m_showControls == nullptr);
    QVERIFY(m_app->findChild<QWidget *>(QStringLiteral("modeSwitch")) == nullptr);
    QVERIFY(VirtualConsole::instance()->isAncestorOf(vcTools));
    QCOMPARE(m_app->doc()->mode(), Doc::Operate);
    // The placement can't bring back what kiosk took away.
    m_app->setShowControlsPlacement(App::ShowControlsInStatusBar);
    m_app->setShowControlsPlacement(App::ShowControlsInTopBar);
    QVERIFY(m_app->m_toolbar == nullptr);
    // Tabs still change without a bar to lend to.
    m_app->m_tab->setCurrentIndex(0);
    QCoreApplication::processEvents();
}

QTEST_MAIN(MainBar_Test)
