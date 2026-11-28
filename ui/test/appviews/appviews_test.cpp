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
#include "../common/fixtureeditor.h"
#include <QToolTip>
#include <QPainter>
#include <QStatusBar>
#include <QListWidget>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QShortcut>
#include <QtTest>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QXmlStreamReader>
#include <QFile>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QTimer>
#include <QMessageBox>
#include "showfeatures.h"
#include <QSettings>
#include <QApplication>
#include <QFileDialog>

#include "cuerunner.h"
#include "showtreemodel.h"
#include "showtreeview.h"
#include "updatechecker.h"
#include "lookeditor.h"
#include "showreviewpanel.h"
#include "showreview.h"
#include "showtreecontroller.h"
#include <QtMath>
#include "appviews_test.h"
#include "venuemenu.h"
#include "controlmappanel.h"
#include "controllearndialog.h"
#include "showsettingsdialog.h"
#include "patchinventorydialog.h"
#include "showoperate.h"
#include "teachingsettings.h"
#include "focusmenu.h"
#include "venuecorrections.h"
#include "programmercontroller.h"
#include "rigowner.h"
#include "universe.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "scene.h"

#define private public
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixture.h"
#include "doc.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "targetitem.h"
#include "monitorlayerspanel.h"
#include "monitorfixtureitem.h"
#include "functionstreewidget.h"
#define protected public
#include "app.h"
#include "dmxmonitorwindow.h"
#include "appsettings.h"
#include "shutterautoopen.h"
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
#include "vcframe.h"
#include "vcbutton.h"
#include "vccuerunner.h"
#include "fixturemanager.h"
#include "inputoutputmanager.h"
#include "timecodesource.h"
#include "pmjoverlay.h"
#include "controlmap.h"
#include "qlcinputprofile.h"
#undef protected
#undef private
#include "helpbrowser.h"
#include "simpledesk.h"
#include "simpledeskengine.h"
#include "vcslider.h"
#include "showmanager.h"
#include "showconverter.h"
#include <QToolButton>
#include "inputoutputmap.h"
#include "patchundo.h"
#include "networkroute.h"
#include "outputpatch.h"
#include "iopluginstub.h"
#include "ioplugincache.h"
#include "qlcfile.h"
#include "showstatus.h"
#include "inputdevicenotices.h"
#include "speedmastersfooter.h"
#include "speedmasters.h"
#include "padrun.h"
#include "fixturetreewidget.h"
#include "outputrulekeeper.h"
#include "outputrulesdialog.h"
#include "controlsurface.h"
#include "controlsurfaceengine.h"
#include "functionparent.h"
#include "mastertimer.h"
#include "showcontrol.h"
#include "chaserstep.h"
#include "paletteops.h"
#include "itemicons.h"
#include "unsavedchangesdialog.h"
#include "changejournal.h"
#include "universe.h"
#include "chaser.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "showcontrol.h"
#include "scene.h"
#include <QBuffer>
#include <QXmlStreamWriter>
#undef protected
#undef private

void AppViews_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    // GOs here come back to back on purpose: the double-GO guard (its own
    // tests, engine/test/goguard and ui/test/goguardui) is off for them.
    m_app->doc()->showControl()->setGoGuardMs(0);
}

/* Q228 pictures (BUILDHIDE_SHOTS=dir): the Show Tree at three window
   sizes, and the tab bar of each workspace view. */
void AppViews_Test::buildHideShots()
{
    const QString dir = qEnvironmentVariable("BUILDHIDE_SHOTS");
    if (dir.isEmpty())
        QSKIP("BUILDHIDE_SHOTS not set");
    QDir().mkpath(dir);
    const QSize was = m_app->size();
    const App::WorkspaceView view = m_app->workspaceView();
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->showContext("Show Tree");
    for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        m_app->resize(size);
        for (int i = 0; i < 5; ++i)
        {
            QCoreApplication::processEvents();
            QTest::qWait(30);
        }
        QVERIFY(m_app->grab().save(QStringLiteral("%1/showtree-%2x%3.png").arg(dir).arg(size.width()).arg(size.height())));
    }
    const struct { App::WorkspaceView v; const char *name; } views[] = {
        { App::ClassicView, "classic" }, { App::ConsoleView, "qlcconsole" }, { App::EverythingView, "everything" } };
    m_app->resize(1440, 900);
    for (const auto &v : views)
    {
        m_app->setWorkspaceView(v.v);
        for (int i = 0; i < 5; ++i)
        {
            QCoreApplication::processEvents();
            QTest::qWait(30);
        }
        QVERIFY(m_app->m_tab->tabBar()->grab().save(QStringLiteral("%1/tabs-%2.png").arg(dir, v.name)));
        QVERIFY(m_app->grab().save(QStringLiteral("%1/window-%2.png").arg(dir, v.name)));
    }
    m_app->setWorkspaceView(view);
    m_app->resize(was);
    m_app->doc()->resetModified();
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
    // And Functions, Build and Shows, which have no tab in the qlcconsole view.
    QCOMPARE(m_app->m_windowJumps.size(), views + 3);
    // The Rig Overview is a window too: offered here as well as under View.
    QVERIFY(windowMenu->actions().contains(m_app->m_rigOverviewAction));
    QCOMPARE(m_app->m_rigOverviewAction->shortcut(),
             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

    // Q214: Cmd/Ctrl+D is Duplicate in every list; Dump DMX is Opt/Alt+Cmd/Ctrl+D.
    QCOMPARE(m_app->m_dumpDmxAction->shortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D));
    QVERIFY(m_app->m_dumpDmxAction->shortcut() != QKeySequence(Qt::CTRL | Qt::Key_D));
    if (!qEnvironmentVariable("WFKEYS_SHOTS").isEmpty())
    {
        // The Control menu as open (Q207 shot): Dump DMX shows its new key.
        foreach (QMenu *m, m_app->menuBar()->findChildren<QMenu *>())
            if (m->actions().contains(m_app->m_dumpDmxAction))
            {
                m->resize(m->sizeHint());
                m->grab().save(QStringLiteral("%1/%2-control-menu.png").arg(qEnvironmentVariable("WFKEYS_SHOTS"),
                                qEnvironmentVariable("WFKEYS_PREFIX", QStringLiteral("after"))));
            }
    }

    const QString label = tabs->tabBar()->tabData(2).toString();
    m_app->slotDetachContext(2);
    QCOMPARE(m_app->detachedContexts(), QStringList() << label);
    m_app->updateWindowMenu();
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == label)
            jump = a;
    QVERIFY(jump != nullptr);
    QVERIFY2(jump->text().contains("In Its Own Window"), qPrintable(jump->text()));
    QVERIFY(m_app->m_windowDockMenu->isEnabled());
    QCOMPARE(m_app->m_windowDockMenu->actions().size(), 1);
    QCOMPARE(m_app->m_windowDockMenu->actions().first()->text(), label);

    // Put back from the menu: a tab again, and shown.
    m_app->m_windowDockMenu->actions().first()->trigger();
    QCOMPARE(tabs->count(), views);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), label);
    m_app->updateWindowMenu();
    QVERIFY(!jump->text().contains("In Its Own Window"));
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
    // Build is a tab of Everything (not of the qlcconsole view, Q228).
    m_app->setWorkspaceView(App::EverythingView);
    auto back = qScopeGuard([this]() {
        m_app->setWorkspaceView(App::ConsoleView);
        m_app->doc()->resetModified();
    });
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

void AppViews_Test::theShowTreeSaysItHoldsCuesAndHits()
{
    /* "Show Tree" names the widget's shape, not the job: a newcomer looks
       for cues. The name stays (every alternative collides with a QLC+
       object this fork keeps first-class) and the words are added where
       they are looked for. */
    const QString aside = QStringLiteral("cue lists, cues and hits");
    QTabWidget *tabs = m_app->m_tab;
    int tree = -1, other = -1;
    for (int i = 0; i < tabs->count(); ++i)
    {
        if (tabs->tabBar()->tabData(i).toString() == QStringLiteral("Show Tree"))
            tree = i;
        else if (other < 0)
            other = i;
    }
    QVERIFY(tree >= 0);

    // 1. The tab's tooltip: its name in full, and what is in it.
    const QString tip = tabs->tabToolTip(tree);
    QVERIFY2(tip.startsWith(QStringLiteral("Show Tree")), qPrintable(tip));
    QVERIFY2(tip.contains(aside), qPrintable(tip));
    // No other tab grows one: a name that says the job keeps its own tooltip.
    QVERIFY(other >= 0);
    QCOMPARE(tabs->tabToolTip(other),
             m_app->tabDisplayLabel(tabs->tabBar()->tabData(other).toString()));

    // 2. The Window menu's entry says it too (no new item: the same jump).
    m_app->updateWindowMenu();
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == QStringLiteral("Show Tree"))
            jump = a;
    QVERIFY(jump != nullptr);
    QVERIFY2(jump->text().startsWith(QStringLiteral("Show Tree")), qPrintable(jump->text()));
    QVERIFY2(jump->text().contains(aside), qPrintable(jump->text()));

    // 3. F1's title for the tab: the help's topic list reads the guide's
    // first heading, so that is where the words go.
    QString title;
    const QList<QPair<QString, QString> > topics = HelpBrowser::topics();
    for (const QPair<QString, QString> &t : topics)
        if (t.first == QStringLiteral("show-tree.md"))
            title = t.second;
    QVERIFY2(title.startsWith(QStringLiteral("Show Tree")), qPrintable(title));
    QVERIFY2(title.contains(aside), qPrintable(title));
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
    // tab here (Q69: ⇧⌘4 is the view's fourth tab, the Show Tree -- Build
    // has no tab here either, Q228).
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == functions)
            jump = a;
    QVERIFY2(jump != nullptr, "Functions should be in the Window menu");
    QCOMPARE(jump->objectName(), QString("windowFunctions"));
    QVERIFY(jump->shortcut().isEmpty());
    QCOMPARE(m_app->m_windowJumps.indexOf(jump), 3);
    QCOMPARE(m_app->m_windowJumps.at(4)->data().toString(), QString("Build"));
    QVERIFY(m_app->m_windowJumps.at(4)->shortcut().isEmpty());
    QCOMPARE(m_app->m_windowJumps.at(5)->data().toString(), QString("Show Tree"));
    QCOMPARE(m_app->m_windowJumps.at(5)->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_4));

    // A show saved with the Functions tab in front doesn't switch view.
    m_app->showContext("Virtual Console");
    m_app->setActiveWindow("FunctionManager");
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), QString("Virtual Console"));

    // Opening a function from outside Build: Build's Workbench, docked in
    // the Show Tree (Build has no tab in the qlcconsole view, Q228).
    Doc *doc = m_app->m_doc;
    EFX *efx = new EFX(doc);
    efx->setName("Window Functions EFX");
    QVERIFY(doc->addFunction(efx));
    m_app->showContext("Virtual Console");
    QVERIFY(ProgrammingManager::openFunction(efx->id()));
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(ShowTreeView::instance()));
    QCOMPARE(ShowTreeView::instance()->dockedWorkbench(), build->workbench());
    QCOMPARE(build->workbench()->loadedFunction(), efx->id());
    QVERIFY(build->workbench()->findChild<EFXEditor *>() != nullptr);
    QVERIFY(ShowTreeView::instance()->closeWorkbench());
    doc->deleteFunction(efx->id());

    // Window → Functions: the stock tab, in Everything, with the notice.
    jump->trigger();
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QVERIFY(m_app->functionsWindowShown());
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(FunctionManager::instance()));
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QVERIFY2(m_app->m_viewNoticeText->text().contains("Functions isn't in the qlcconsole view"),
             qPrintable(m_app->m_viewNoticeText->text()));
    QCOMPARE(tabs->count(), views + 3);   // Everything: + Functions, Build, Shows
    m_app->setTabLabelMode(App::TabTextOnly);
    for (int i = 0; i < tabs->count(); ++i)
        QCOMPARE(tabs->tabText(i), tabs->tabBar()->tabData(i).toString() == "Shows"
                                       ? QString("Timeline") : tabs->tabBar()->tabData(i).toString());
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
    // No Window ▸ Effects: Build ▸ Effects is their home (Q68, Q97)...
    QVERIFY(m_app->menuBar()->findChild<QAction *>("windowEffects") == nullptr);
    ProgrammingManager *pm = ProgrammingManager::instance();
    QVERIFY(pm != nullptr);
    m_app->setWorkspaceView(App::EverythingView);
    m_app->showEffects();
    QCOMPARE(m_app->m_tab->currentWidget(), static_cast<QWidget *>(pm));
    QCOMPARE(pm->section(), ProgrammingManager::EffectsSection);
    pm->setSection(ProgrammingManager::ScenesSection);

    // ...where Build is a tab. The qlcconsole view has none (Q228): the Show
    // Tree's Library, at its effect palettes, and the view stays.
    m_app->setWorkspaceView(App::ConsoleView);
    QVERIFY(!m_app->buildInView());
    QLCPalette *chase = new QLCPalette(QLCPalette::Effect, m_app->doc());
    chase->setName("Chase");
    QVERIFY(m_app->doc()->addPalette(chase));
    const quint32 chaseId = chase->id();
    auto tidy = qScopeGuard([this, chaseId]() { m_app->doc()->deletePalette(chaseId); });
    ShowTreeView *tree = ShowTreeView::instance();
    QVERIFY(tree != nullptr);
    QVERIFY(QMetaObject::invokeMethod(tree, "reloadLibrary"));
    m_app->showEffects();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(m_app->m_tab->currentWidget(), static_cast<QWidget *>(tree));
    QTreeWidgetItem *at = tree->library()->currentItem();
    QVERIFY(at != nullptr);
    QCOMPARE(at->data(0, ShowTreeView::PathRole).toString(), QString("Palettes/Effects"));
    QVERIFY(m_app->m_viewNotice->isHidden());
    m_app->doc()->resetModified();
}

/* Board Q228: the qlcconsole view has no Build tab. What opened in Build --
   a function, a palette, Show Review's palette fix -- opens in the Show
   Tree there, and the view never switches to Everything on its own. */
void AppViews_Test::withBuildHiddenItsJumpsLandInTheShowTree()
{
    QTabWidget *tabs = m_app->m_tab;
    Doc *doc = m_app->doc();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QVERIFY(!m_app->buildInView());
    ProgrammingManager *pm = ProgrammingManager::instance();
    ShowTreeView *tree = ShowTreeView::instance();
    QVERIFY(pm != nullptr && tree != nullptr);
    QCOMPARE(tabs->indexOf(pm), -1);                    // hidden, still there

    Scene *scene = new Scene(doc);
    scene->setName("01-01.01-Wash");
    QVERIFY(doc->addFunction(scene));
    QLCPalette *blue = new QLCPalette(QLCPalette::Color, doc);
    blue->setName("Deep blue");
    blue->setValue(QColor(0, 0, 255));
    QVERIFY(doc->addPalette(blue));
    const quint32 sid = scene->id(), pid = blue->id();
    auto tidy = qScopeGuard([this, doc, sid, pid]() {
        if (ShowTreeView::instance() != nullptr)
            ShowTreeView::instance()->closeWorkbench();
        doc->deleteFunction(sid);
        doc->deletePalette(pid);
        m_app->setWorkspaceView(App::ConsoleView);
        doc->resetModified();
    });

    // A function (a context menu's Open, Why On, a chaser's step...): the
    // Workbench docked in the Show Tree, the function loaded in it.
    m_app->showContext("Virtual Console");
    QVERIFY(ProgrammingManager::openFunction(sid));
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(tree));
    QVERIFY(tree->isWorkbenchDocked());
    QCOMPARE(tree->dockedWorkbench(), pm->workbench());
    QCOMPARE(pm->workbench()->loadedFunction(), sid);
    QVERIFY(m_app->m_viewNotice->isHidden());
    // ↩ Show Tree: the page back, the Workbench home in Build.
    QVERIFY(tree->closeWorkbench());
    QVERIFY(!pm->isWorkbenchLent());

    // A palette: found in the Library, open in the Show Tree's Look Editor.
    LookEditor *treeLook = tree->findChild<LookEditor *>("stLookEditor");
    QVERIFY(treeLook != nullptr);
    m_app->showContext("Fixtures");
    QVERIFY(ProgrammingManager::openPalette(pid));
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(tree));
    QCOMPARE(treeLook->paletteId(), pid);
    QVERIFY(!treeLook->isHidden());
    QVERIFY(tree->library()->currentItem() != nullptr);
    QCOMPARE(tree->library()->currentItem()->data(0, ShowTreeView::IdRole).toUInt(), pid);

    // Show Review's palette fix: the same place.
    m_app->showContext("Fixtures");
    QVERIFY(m_app->reviewJump(m_app, "palette", QVariantList() << pid));
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(tree));
    QCOMPARE(treeLook->paletteId(), pid);
    QVERIFY(!treeLook->isHidden());

    // In Everything, Build has them, as before.
    m_app->setWorkspaceView(App::EverythingView);
    QVERIFY(m_app->buildInView());
    QVERIFY(ProgrammingManager::openFunction(sid));
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(pm));
    QCOMPARE(pm->workbench()->loadedFunction(), sid);

    // Window ▸ Build from the qlcconsole view is the one way there: asked
    // for by name, it shows Everything and says so (Back returns).
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->showContext("Build");
    QCOMPARE(m_app->workspaceView(), App::EverythingView);
    QVERIFY(!m_app->m_viewNotice->isHidden());
    m_app->m_viewNoticeButton->click();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
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
    // Devices and QLC+'s own Universes page; no Overview (board Q153: its
    // columns are By universe's).
    QStringList ioTabs;
    for (int i = 0; i < io->m_ioTabs->count(); i++)
        ioTabs << io->m_ioTabs->tabText(i);
    QCOMPARE(ioTabs, QStringList() << "Devices" << "Universes");

    // Back in qlcconsole: none of it, and Connections opens on Devices.
    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(io->m_ioTabs->tabText(io->m_ioTabs->currentIndex()), QString("Devices"));
    QVERIFY(main->isHidden());
    for (QAction *a : fm->m_classicToolbarActions)
        QVERIFY(!a->isVisible());
    QVERIFY(fm->m_addFixtureButtonAction->isVisible());
    // The MTC chip only once a timecode source is picked (Q98).
    QVERIFY(m_app->m_statusTimecodeLabel->isHidden());
    TimecodeSource *tc = m_app->m_doc->timecodeSource();
    tc->setSourceUniverse(0);
    QVERIFY(!m_app->m_statusTimecodeLabel->isHidden());
    tc->setSourceUniverse(-1);
    QVERIFY(m_app->m_statusTimecodeLabel->isHidden());
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
    QCOMPARE(undo->text(), App::menuTextWithKeys("Nothing to undo on this tab", QKeySequence::Undo));
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
    // The qlcconsole view's seventh tab (Q69, Q79; Build hidden, Q228).
    QCOMPARE(jump->shortcut(), QKeySequence("CTRL+SHIFT+7"));
    jump->trigger();
    QWidget *page = m_app->m_tab->currentWidget();
    CueRunner *runner = qobject_cast<CueRunner *>(page);
    QVERIFY(runner != nullptr);
    QCOMPARE(runner->runnerSize(), CueRunner::Full);
    QCOMPARE(runner->follow(), ShowTree::invalidId());    // the current production
    QCOMPARE(page->property("helpTopic").toString(), QString("show-tree.md#the-cue-runner"));
    // The others are numbered by the view too (⇧⌘6 is the Virtual Console).
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == QLatin1String("Virtual Console"))
            QCOMPARE(a->shortcut(), QKeySequence("CTRL+SHIFT+6"));
}

void AppViews_Test::noTwoApplicationShortcutsShareAKey()
{
    /* Two application-wide shortcuts on one key are ambiguous to Qt, and
       then neither runs: ⌘↩ was both GO and "start the focus calls", so
       GO did nothing. GO keeps it, and begins armed focus calls itself. */
    QMultiHash<QKeySequence, QString> owners;
    foreach (QAction *a, m_app->findChildren<QAction *>())
        if (a->shortcutContext() == Qt::ApplicationShortcut)
        {
            // One action's own list may say a key twice (F1 and HelpContents).
            QSet<QKeySequence> own;
            foreach (const QKeySequence &k, a->shortcuts())
                if (!k.isEmpty() && !own.contains(k))
                {
                    own.insert(k);
                    owners.insert(k, a->objectName().isEmpty() ? a->text() : a->objectName());
                }
        }
    foreach (QShortcut *sc, m_app->findChildren<QShortcut *>())
        if (sc->context() == Qt::ApplicationShortcut && !sc->key().isEmpty())
            owners.insert(sc->key(), QStringLiteral("QShortcut ") + sc->objectName());
    QStringList clashes;
    foreach (const QKeySequence &k, owners.uniqueKeys())
        if (owners.count(k) > 1)
            clashes << k.toString() + ": " + QStringList(owners.values(k)).join(" / ");
    QVERIFY2(clashes.isEmpty(), qPrintable(clashes.join("; ")));
    QCOMPARE(m_app->m_cueGoAction->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Return));
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
    // Told to stop floating (the title's float button, a double-click on the
    // title), it had no dock area to go to and painted over the main window.
    QVERIFY(!(dock->features() & QDockWidget::DockWidgetFloatable));
    dock->setFloating(false);
    QTRY_VERIFY(dock->isFloating());
    QVERIFY(dock->isWindow());
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
            if (QMessageBox *box = qobject_cast<QMessageBox *>(modalOrFixtureEditor()))
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

/* Q115: Connections has its own ⌘Z (Undo Patch Change), so Edit ▸ Undo
   there is that step -- not "Nothing to undo on this tab". It asks first, as
   the tab's own action does. */
void AppViews_Test::editUndoOnConnectionsIsUndoPatchChange()
{
    QAction *undo = m_app->menuBar()->findChild<QAction *>("editUndo");
    QVERIFY(undo != nullptr);
    QMenu *edit = nullptr;
    for (QAction *a : m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->menu()->actions().contains(undo))
            edit = a->menu();
    QVERIFY(edit != nullptr);
    PatchUndo *pu = m_app->m_doc->inputOutputMap()->patchUndo();
    QVERIFY(pu != nullptr);
    pu->clear();

    m_app->showContext("Connections");
    QVERIFY(qobject_cast<InputOutputManager *>(m_app->m_tab->currentWidget()) != nullptr);
    emit edit->aboutToShow();
    QCOMPARE(undo->text(), App::menuTextWithKeys("Undo", QKeySequence::Undo));
    QVERIFY(!undo->isEnabled());

    pu->captureUniverses(QStringLiteral("add a universe"));
    QVERIFY(pu->canUndo());
    emit edit->aboutToShow();
    QCOMPARE(undo->text(), App::menuTextWithKeys("Undo Patch Change", QKeySequence::Undo));
    QVERIFY(undo->isEnabled());
    // Triggered: the tab's own confirmation; No keeps the step.
    QString asked;
    answerNextBox(QMessageBox::No, &asked);
    undo->trigger();
    QVERIFY2(asked.contains("add a universe"), qPrintable(asked));
    QVERIFY(pu->canUndo());
    pu->clear();
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
    // Never chosen: feet and inches, the product's default.
    QCOMPARE(MeasurementUnits::preferredLength(), MeasurementUnits::Feet);
    {
        AppSettings dlg(m_app);
        QComboBox *units = dlg.findChild<QComboBox *>(QStringLiteral("settingsUnitsCombo"));
        QVERIFY(units != nullptr);
        QCOMPARE(units->count(), 5);
        QCOMPARE(units->currentData().toInt(), int(MeasurementUnits::Feet));
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

void AppViews_Test::reopenLastShowDecision()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString last = dir.filePath("last.qlcc");
    QFile f(last);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    const QString gone = dir.filePath("gone.qlcc");

    QTemporaryFile iniFile(dir.filePath("s-XXXXXX.ini"));
    QVERIFY(iniFile.open());
    QSettings s(iniFile.fileName(), QSettings::IniFormat);
    s.setValue("workspace/recent0", last);
    QString missing = "stale";

    // Off (the default): nothing.
    QVERIFY(App::showToOpenAtStart(QString(), s, false, &missing).isEmpty());
    QVERIFY(missing.isEmpty());
    s.setValue(App::reopenLastShowSettingKey(), true);
    // On: the top of Open Recent.
    QCOMPARE(App::showToOpenAtStart(QString(), s, false, &missing), last);
    QVERIFY(missing.isEmpty());
    // -o wins.
    QCOMPARE(App::showToOpenAtStart("/x/cli.qlcc", s, false), QString("/x/cli.qlcc"));
    QCOMPARE(App::showToOpenAtStart("/x/cli.qlcc", s, true), QString("/x/cli.qlcc"));
    // Shift skips it.
    QVERIFY(App::showToOpenAtStart(QString(), s, true, &missing).isEmpty());
    QVERIFY(missing.isEmpty());
    // A show that is gone: nothing, and it is reported.
    s.setValue("workspace/recent0", gone);
    QVERIFY(App::showToOpenAtStart(QString(), s, false, &missing).isEmpty());
    QCOMPARE(missing, gone);
    // No recent list at all.
    s.remove("workspace/recent0");
    QVERIFY(App::showToOpenAtStart(QString(), s, false, &missing).isEmpty());
    QVERIFY(missing.isEmpty());
}

void AppViews_Test::reopenLastShowCheckboxRoundTrips()
{
    QSettings().remove(App::reopenLastShowSettingKey());
    QVERIFY(!App::reopenLastShow());
    {
        AppSettings dlg(m_app);
        QCheckBox *c = dlg.findChild<QCheckBox *>(QStringLiteral("settingsReopenLastShow"));
        QVERIFY(c != nullptr);
        QVERIFY(!c->isChecked());
        QVERIFY(c->toolTip().contains("Shift"));
        c->setChecked(true);
        dlg.accept();
    }
    QVERIFY(App::reopenLastShow());
    QCOMPARE(QSettings().value("workspace/reopenLastShow").toBool(), true);
    {
        AppSettings dlg(m_app);
        QCheckBox *c = dlg.findChild<QCheckBox *>(QStringLiteral("settingsReopenLastShow"));
        QVERIFY(c->isChecked());
        c->setChecked(false);
        dlg.accept();
    }
    QVERIFY(!App::reopenLastShow());
}

/* Settings ▸ General ▸ Software Update: the channels are named for what
   is published -- Stable, and Beta (a GitHub pre-release) -- not after a
   feature that was once only in the betas ("Pre-releases too (e.g. Show
   Tree)" outlived the Show Tree betas by a dozen releases). */
void AppViews_Test::theUpdateChannelsAreStableAndBeta()
{
    AppSettings dlg(m_app);
    QComboBox *chan = dlg.findChild<QComboBox *>(QStringLiteral("settingsUpdateChannel"));
    QVERIFY(chan != nullptr);
    QCOMPARE(chan->count(), 2);
    QCOMPARE(chan->itemText(chan->findData(int(UpdateChecker::Stable))), QStringLiteral("Stable"));
    const QString beta = chan->itemText(chan->findData(int(UpdateChecker::PreRelease)));
    QVERIFY2(beta.startsWith(QLatin1String("Beta")), qPrintable(beta));
    QVERIFY2(!beta.contains(QLatin1String("Show Tree")), qPrintable(beta));
    dlg.reject();
}

void AppViews_Test::settingsAdvancedHoldsAutoOpen()
{
    ShutterAutoOpen *ao = m_app->doc()->shutterAutoOpen();
    QVERIFY(ao->isEnabled());
    {
        AppSettings dlg(m_app);
        QTabWidget *tabs = dlg.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QVERIFY(tabs != nullptr);
        // General, Appearance, Advanced, Performance; AI Assist after them
        // when it is built in (QLC_AI_ASSIST).
        QVERIFY(tabs->count() == 4 || (tabs->count() == 5 && tabs->tabText(4) == QStringLiteral("AI Assist")));
        QCOMPARE(tabs->tabText(3), QStringLiteral("Performance"));
        QVERIFY(tabs->widget(3)->findChild<QTreeWidget *>(QStringLiteral("tuningItems")) != nullptr);
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

/* Findability (board Q65, commit 385edaab9): the footer chips moved from the
   View menu to Settings ▸ Appearance ▸ Footer, but nothing tested that a
   person could still find them there. Walk the real path -- the File menu's
   Settings… action, the Appearance tab, the Footer group box -- and open
   the dialog through the menu action itself, not the AppSettings
   constructor. A future rename or move of any of the three breaks this. */
void AppViews_Test::footerChipsAreFindableFromSettingsAppearance()
{
    QMenu *file = nullptr;
    foreach (QAction *a, m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->text().remove('&') == "File")
            file = a->menu();
    QVERIFY2(file != nullptr, "the menu bar has a File menu");

    QAction *settingsAction = nullptr;
    foreach (QAction *a, file->actions())
        if (a->objectName() == QStringLiteral("appSettings"))
            settingsAction = a;
    QVERIFY2(settingsAction != nullptr, "Settings… sits on the File menu");
    QCOMPARE(settingsAction->text(), QString("&Settings…"));   // the menu text as shown

    QVERIFY(settingsAction->isEnabled());
    bool sawAppearanceTab = false, sawFooterGroup = false;
    QTimer::singleShot(100, m_app, [&]() {
        AppSettings *dlg = m_app->findChild<AppSettings *>();
        QVERIFY(dlg != nullptr);
        QTabWidget *tabs = dlg->findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QVERIFY(tabs != nullptr);
        // Its place: the second tab, named as it reads.
        QWidget *appearance = tabs->widget(1);
        sawAppearanceTab = (appearance != nullptr
                             && appearance->objectName() == QStringLiteral("settingsAppearance")
                             && tabs->tabText(1) == QString("Appearance"));
        if (appearance != nullptr)
        {
            QGroupBox *footer = appearance->findChild<QGroupBox *>(QStringLiteral("settingsFooter"));
            sawFooterGroup = (footer != nullptr && footer->title() == QString("Footer"));
            if (footer != nullptr)
                for (const char *name : { "settingsFooterLoad", "settingsFooterPower",
                                          "settingsFooterGrandMaster", "settingsFooterSpeedMasters" })
                    QVERIFY2(footer->findChild<QCheckBox *>(name) != nullptr, name);
        }
        dlg->reject();
    });
    settingsAction->trigger();
    QVERIFY2(sawAppearanceTab, "Settings… opened on a tab named Appearance, second in the dialog");
    QVERIFY2(sawFooterGroup, "the Appearance tab has a Footer group with the chip checkboxes");
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
    // 0.3.1 (k): QLC+ 4's Monitor and Full Screen are ways of looking too.
    QCOMPARE(items, QStringList() << "Workspace" << "Address Tool" << "DMX Monitor" << "Rig Overview…"
                                  << "Quick Focus…" << "Venue…" << "Blind" << "Full Screen");
}

void AppViews_Test::theGridFollowsTheUnitPreference()
{
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
                       QLCFile::settingsOrganisation(), QStringLiteral("qlcconsole"));
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
    QVERIFY(label->text().startsWith(QStringLiteral("Millimetres")));   // the person's unit, not the grid's

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
    QVERIFY(label->text().startsWith(QStringLiteral("Inches")));   // the person's unit; the grid in feet

    // New: in the person's unit too.
    m_app->clearDocument();
    QCOMPARE(m_app->doc()->monitorProperties()->gridUnits(), MonitorProperties::Feet);

    // (No second New straight after: MasterTimer::stop() right behind a
    // start() can wait forever -- see docs/project/TODO.md.)
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

void AppViews_Test::aControllerComingAndGoingIsSaid()
{
    /* A pad patched as universe 2's input goes to sleep and wakes: said in
       the notice line (amber while it is away) and in the footer. The
       plugin is the engine's test stub dressed as a device plugin. */
    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        cache->load(stubDir);
    IOPluginStub *stub = static_cast<IOPluginStub *>(cache->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    const QString pad = QStringLiteral("DualSense Wireless Controller");
    stub->m_linesAreHardware = true;
    stub->m_lineNames = QStringList() << pad;
    auto restore = qScopeGuard([this, stub]() {
        m_app->clearDocument();
        stub->m_linesAreHardware = false;
        stub->m_lineNames.clear();
        m_app->hideViewNotice();
    });

    m_app->clearDocument();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    if (iom->universesCount() < 2)
        iom->addUniverse();
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 0));
    m_app->m_inputNotices->settleNow();
    m_app->hideViewNotice();

    stub->m_lineNames = QStringList() << "Keyboard";
    stub->configure();
    m_app->m_inputNotices->settleNow();
    QVERIFY(m_app->m_viewNotice->isHidden() == false);
    QCOMPARE(m_app->m_viewNoticeText->text(),
             QString("Input device '%1' disconnected — universe 2").arg(pad));
    QVERIFY(m_app->m_viewNotice->styleSheet().contains("#f5a623"));
    bool inFooter = false;
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        if (e.key == QLatin1String("input.devices"))
            inFooter = e.summary.contains(pad);
    QVERIFY(inFooter);
    // Said once, in its own words: not also as an interface "not present".
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        QVERIFY2(e.key != QLatin1String("output.dangling") || e.summary.contains(pad) == false,
                 qPrintable(e.summary));

    stub->m_lineNames = QStringList() << "Keyboard" << pad;
    stub->configure();
    m_app->m_inputNotices->settleNow();
    QCOMPARE(m_app->m_viewNoticeText->text(),
             QString("Input device '%1' connected — universe 2").arg(pad));
    QVERIFY(m_app->m_viewNotice->styleSheet().isEmpty());
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        QVERIFY(e.key != QLatin1String("input.devices"));
}

void AppViews_Test::goInDesignOffersOperateOnce()
{
    m_app->m_doc->setMode(Doc::Design);
    m_app->hideViewNotice();
    m_app->m_operateOffered = false;
    m_app->m_cueGoAction->trigger();
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QVERIFY(m_app->m_viewNoticeText->text().contains("Operate"));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QString("Switch to Operate"));
    // Once a session.
    m_app->hideViewNotice();
    m_app->m_cueGoAction->trigger();
    QVERIFY(m_app->m_viewNotice->isHidden());
    // Never in Operate.
    m_app->m_operateOffered = false;
    m_app->m_doc->setMode(Doc::Operate);
    m_app->m_cueGoAction->trigger();
    QVERIFY(m_app->m_viewNotice->isHidden());
    m_app->m_doc->setMode(Doc::Design);
}

/* A one-channel dimmer at universe 1, address @a address, a scene putting
   it at @a level, and (with @a list) a chaser of that scene, infinite hold,
   as the show's main cue list. What it made goes in @a fixtures and
   @a functions for the caller to take away. */
static Scene *litScene(Doc *doc, quint32 address, uchar level, QList<quint32> *fixtures,
                       QList<quint32> *functions, Chaser **list = nullptr)
{
    Fixture *fx = new Fixture(doc);
    fx->setName(QStringLiteral("Lit %1").arg(address));
    fx->setChannels(1);
    fx->setUniverse(0);
    fx->setAddress(address);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    *fixtures << fx->id();
    Scene *s = new Scene(doc);
    s->setName(QStringLiteral("Lit %1 at %2").arg(address).arg(level));
    s->setValue(fx->id(), 0, level);
    doc->addFunction(s);
    *functions << s->id();
    if (list != nullptr)
    {
        Chaser *c = new Chaser(doc);
        c->setName(QStringLiteral("Main list"));
        c->setDurationMode(Chaser::Common);
        c->setDuration(Function::infiniteSpeed());
        doc->addFunction(c);
        *functions << c->id();
        c->addStep(ChaserStep(s->id()));
        doc->showControl()->setMainCueList(c->id());
        *list = c;
    }
    return s;
}

static void takeAway(Doc *doc, const QList<quint32> &fixtures, const QList<quint32> &functions)
{
    doc->masterTimer()->stopAllFunctions();
    doc->showControl()->setMainCueList(Function::invalidId());
    for (quint32 id : functions)
        doc->deleteFunction(id);
    for (quint32 id : fixtures)
        doc->deleteFixture(id);
}

/* What the functions put on @a address, as the last whole frame left it.
   preGMValues() is copied under the universe's frame lock; postGMValue()
   reads the live array, which the universe thread zeroes (every intensity
   channel) at the start of each frame and only then writes again -- a read
   landing between the two saw 0 for a dimmer at full. Idle that is a few
   microseconds a frame; on a loaded machine the thread can be preempted
   there, and a QCOMPARE (or the last check of a QTRY) failed with 0. */
static uchar outputAt(Doc *doc, quint32 address)
{
    const QList<Universe *> unis = doc->inputOutputMap()->universes();
    if (unis.isEmpty())
        return 0;
    const QByteArray frame = unis.first()->preGMValues();
    return address < quint32(frame.size()) ? uchar(frame.at(int(address))) : 0;
}

/* CHANGELOG 0.2.2: "GO in Design offers Operate (Q92), once a session, in
   the line at the top: GO still plays." */
void AppViews_Test::goInDesignStillPlays()
{
    Doc *doc = m_app->m_doc;
    QList<quint32> fixtures, functions;
    auto tidy = qScopeGuard([&]() {
        takeAway(doc, fixtures, functions);
        m_app->hideViewNotice();
    });
    Chaser *list = nullptr;
    QVERIFY(litScene(doc, 200, 170, &fixtures, &functions, &list) != nullptr);
    doc->setMode(Doc::Design);
    m_app->hideViewNotice();
    m_app->m_operateOffered = false;
    QCOMPARE(int(outputAt(doc, 200)), 0);

    m_app->m_cueGoAction->trigger();
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QCOMPARE(m_app->m_viewNoticeButton->text(), QString("Switch to Operate"));
    QCOMPARE(doc->mode(), Doc::Design);                 // offered, not switched
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 200)), 170, 5000);
}

void AppViews_Test::goHoldsThePreviewSoOnlyTheRunPlays()
{
    Doc *doc = m_app->m_doc;
    ShowTreeController *st = doc->showTree();
    QList<quint32> fixtures, functions;
    quint32 prod = ShowTree::invalidId();
    QList<quint32> groups, palettes;
    auto tidy = qScopeGuard([&]() {
        st->stopPreview();
        doc->masterTimer()->stopAllFunctions();
        if (prod != ShowTree::invalidId())
            st->removeProduction(prod);
        takeAway(doc, fixtures, functions);
        for (quint32 g : groups)
            doc->deleteFixtureGroup(g);
        for (quint32 p : palettes)
            doc->deletePalette(p);
        m_app->hideViewNotice();
        /* What the stops queued lands here, not in the next test: the
           timer's started/stopped signals once nothing runs, the Show
           Tree's sweep (a single-shot bake after a preview lets go) once it
           has fired. Waited for, not timed: a loaded machine is late. */
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
        if (const QTimer *sweep = st->findChild<QTimer *>(QString(), Qt::FindDirectChildrenOnly))
            QTRY_VERIFY_WITH_TIMEOUT(!sweep->isActive(), 5000);
        QCoreApplication::processEvents();
    });
    // Three dimmers: Run on 230, the preview on 231, a busk on 232.
    Scene *busk = litScene(doc, 232, 90, &fixtures, &functions);
    QVERIFY(litScene(doc, 230, 1, &fixtures, &functions) != nullptr);
    QVERIFY(litScene(doc, 231, 1, &fixtures, &functions) != nullptr);
    QVERIFY(busk != nullptr);
    auto groupOf = [&](quint32 fid, const QString &name) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        doc->addFixtureGroup(g);
        g->assignFixture(fid);
        groups << g->id();
        return g->id();
    };
    const quint32 runGroup = groupOf(fixtures.at(1), "Run");
    const quint32 previewGroup = groupOf(fixtures.at(2), "Preview");
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    palettes << full->id();
    prod = st->addProduction("RC8");
    const quint32 listId = st->addCueList(prod, "Full Show", ShowTree::ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");        // after One
    ShowTree::Target t1, t2;
    t1.groups << runGroup;
    t2.groups << previewGroup;
    st->setLayerLook(c1, st->addLayer(c1, t1), st->lookForPalette(full->id()));
    st->setLayerLook(c2, st->addLayer(c2, t2), st->lookForPalette(full->id()));
    Chaser *list = qobject_cast<Chaser *>(doc->function(st->cueList(listId)->chaser));
    QVERIFY(list != nullptr);
    doc->showControl()->setMainCueList(list->id());

    doc->setMode(Doc::Design);
    m_app->hideViewNotice();
    m_app->m_operateOffered = true;      // the Operate offer said already
    busk->start(doc->masterTimer(), FunctionParent::master());
    st->previewCue(c2);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Playing));
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 231)), 255, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 232)), 90, 5000);

    // GO: the preview holds before the GO acts, and the notice says so.
    m_app->m_cueGoAction->trigger();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Held));
    QVERIFY(st->previewFunctions().isEmpty());
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QCOMPARE(m_app->m_viewNoticeText->text(),
             QString("Stopped the Show Tree preview so GO plays alone — Play brings it back."));
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 230)), 255, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 231)), 0, 5000);
    // Not a preview: the busk keeps running.
    QVERIFY(busk->isRunning());
    QCOMPARE(int(outputAt(doc, 232)), 90);

    // Selecting a cue while the list runs stays off stage...
    st->previewCue(c2);
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Held));
    QTest::qWait(100);
    QCOMPARE(int(outputAt(doc, 231)), 0);
    // ...until Play, which previews instead of the run: the list goes on,
    // held off stage, so only the cue in edit shows (rigsync).
    st->resumePreview();
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 231)), 255, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 230)), 0, 5000);
    QVERIFY(list->isRunning());

    // The Run tab's GO comes the same way, before it acts.
    m_app->hideViewNotice();
    CueRunner runTab(doc, CueRunner::Full);
    runTab.go();
    QCOMPARE(int(st->previewState()), int(ShowTreeController::PreviewState::Held));
    QVERIFY(st->previewFunctions().isEmpty());
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 5000);
}

void AppViews_Test::everyRouteTakesTheOneGoPath()
{
    Doc *doc = m_app->m_doc;
    ShowTreeController *st = doc->showTree();
    ShowControl *sc = doc->showControl();
    QList<quint32> fixtures, functions;
    quint32 prod = ShowTree::invalidId();
    auto tidy = qScopeGuard([&]() {
        doc->masterTimer()->stopAllFunctions();
        if (prod != ShowTree::invalidId())
            st->removeProduction(prod);
        takeAway(doc, fixtures, functions);
        m_app->hideViewNotice();
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
        QCoreApplication::processEvents();
    });
    // The App is the run handler of its show's ShowControl.
    QVERIFY(sc->hasRunHandler());

    prod = st->addProduction("One GO");
    const quint32 listId = st->addCueList(prod, "Main", ShowTree::ListMode::NonTracking);
    for (int i = 0; i < 4; ++i)
        st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
    Chaser *list = qobject_cast<Chaser *>(doc->function(st->cueList(listId)->chaser));
    QVERIFY(list != nullptr);
    sc->setMainCueList(list->id());
    doc->setMode(Doc::Operate);
    m_app->hideViewNotice();

    QSignalSpy handled(sc, &ShowControl::commandHandled);
    auto took = [&](ShowControl::Route route, ShowControl::Command command) -> bool {
        return m_app->m_lastRunRoute == route && !handled.isEmpty()
            && handled.last().at(0).toString().startsWith(ShowControl::commandName(command));
    };
    int count = m_app->m_runCommands;

    // The keys: ⌘↩, ⌘⇧↩, Sub GO, Sub Back (Control ▸ GO is the same action).
    m_app->m_cueGoAction->trigger();
    QVERIFY(took(ShowControl::RouteKey, ShowControl::Go));
    QCOMPARE(m_app->m_runCommands, ++count);
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    m_app->m_cueBackAction->trigger();
    QVERIFY(took(ShowControl::RouteKey, ShowControl::Back));
    QCOMPARE(m_app->m_runCommands, ++count);
    m_app->m_cueSubGoAction->trigger();
    QVERIFY(took(ShowControl::RouteKey, ShowControl::SubGo));
    QCOMPARE(m_app->m_runCommands, ++count);
    m_app->m_cueSubBackAction->trigger();
    QVERIFY(took(ShowControl::RouteKey, ShowControl::SubBack));
    QCOMPARE(m_app->m_runCommands, ++count);

    // The Run tab: its buttons, and Space.
    {
        CueRunner runTab(doc, CueRunner::Full);
        runTab.go();
        QVERIFY(took(ShowControl::RouteRunTab, ShowControl::Go));
        QCOMPARE(m_app->m_runCommands, ++count);
        runTab.stop();
        QVERIFY(took(ShowControl::RouteRunTab, ShowControl::Stop));
        QCOMPARE(m_app->m_runCommands, ++count);
        // A stop is carried out on the next MasterTimer tick. A GO sent before
        // it lands finds the list still "running", plays its next cue, and is
        // then stopped by the late tick: on a loaded machine the list ended up
        // stopped. Wait the stop out, as a person would.
        QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 10000);
        runTab.load(QStringLiteral("2"));
        QVERIFY(took(ShowControl::RouteRunTab, ShowControl::Load));
        QCOMPARE(m_app->m_runCommands, ++count);
        QShortcut *space = nullptr;
        for (QShortcut *k : runTab.findChildren<QShortcut *>())
            if (k->key() == QKeySequence(Qt::Key_Space))
                space = k;
        QVERIFY(space != nullptr);
        emit space->activated();
        QVERIFY(took(ShowControl::RouteRunTab, ShowControl::Go));
        QCOMPARE(m_app->m_runCommands, ++count);
        QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 5000);
    }

    // A VC Cue Runner: the VC's route, quiet.
    {
        CueRunner vc(doc, CueRunner::Compact);
        m_app->hideViewNotice();
        vc.go();
        QVERIFY(took(ShowControl::RouteVcRunner, ShowControl::Go));
        QCOMPARE(m_app->m_runCommands, ++count);
        vc.back();
        QVERIFY(took(ShowControl::RouteVcRunner, ShowControl::Back));
        QCOMPARE(m_app->m_runCommands, ++count);
    }

    // The pad: a profile's chaser step on a running Show Tree list is a GO.
    ProgrammingManager *pm = ProgrammingManager::instance();
    QVERIFY(pm != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 5000);
    pm->slotButtonAction(QStringLiteral("chaser_step_forward"));
    QVERIFY(took(ShowControl::RoutePad, ShowControl::Go));
    QCOMPARE(m_app->m_runCommands, ++count);
    pm->slotButtonAction(QStringLiteral("chaser_step_back"));
    QVERIFY(took(ShowControl::RoutePad, ShowControl::Back));
    QCOMPARE(m_app->m_runCommands, ++count);

    // OSC (QLab): through ShowControl::run() to the same place.
    QByteArray datagram("/go");
    datagram.append(QByteArray(4 - datagram.size() % 4, '\0'));
    datagram.append(QByteArray(",\0\0\0", 4));
    QCOMPARE(sc->handleOsc(datagram), 1);
    QVERIFY(took(ShowControl::RouteOsc, ShowControl::Go));
    QCOMPARE(m_app->m_runCommands, ++count);

    // The footer's Next: status only -- a click shows the Run tab, no GO.
    m_app->slotUpdateStandby();
    QVERIFY(m_app->m_statusStandby != nullptr);
    m_app->m_statusStandby->click();
    QCOMPARE(m_app->m_runCommands, count);

    doc->setMode(Doc::Design);
}

void AppViews_Test::aWebGoIsRefusedUntilTheShowAllowsIt()
{
    Doc *doc = m_app->m_doc;
    ShowControl *sc = doc->showControl();
    ShowTreeController *st = doc->showTree();
    quint32 prod = st->addProduction("Remote GO");
    auto tidy = qScopeGuard([&]() {
        doc->operateSettings()->setRemoteGo(false);
        doc->masterTimer()->stopAllFunctions();
        st->removeProduction(prod);
        m_app->hideViewNotice();
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
        doc->setMode(Doc::Design);
    });
    const quint32 listId = st->addCueList(prod, "Main", ShowTree::ListMode::NonTracking);
    st->addCue(listId, -1, QStringLiteral("Cue 1"));
    st->addCue(listId, 0, QStringLiteral("Cue 2"));
    Chaser *list = qobject_cast<Chaser *>(doc->function(st->cueList(listId)->chaser));
    QVERIFY(list != nullptr);
    sc->setMainCueList(list->id());
    doc->setMode(Doc::Operate);
    m_app->hideViewNotice();

    QSignalSpy handled(sc, &ShowControl::commandHandled);
    ShowControl::Request r;
    r.command = ShowControl::Go;

    // Off, the default: refused, and nothing plays
    QVERIFY(!doc->operateSettings()->remoteGo());
    QVERIFY(!sc->run(r, ShowControl::RouteWeb));
    QVERIFY(!list->isRunning());
    QVERIFY(!handled.isEmpty());
    QVERIFY(!handled.last().at(1).toBool());
    QVERIFY(handled.last().at(0).toString().contains(QStringLiteral("remote GO is off")));
    // ...while the console's own routes are not asked
    QVERIFY(sc->run(r, ShowControl::RouteKey));
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 5000);
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);

    // On in Show Settings: the same GO plays
    doc->operateSettings()->setRemoteGo(true);
    QVERIFY(sc->run(r, ShowControl::RouteWeb));
    QCOMPARE(m_app->m_lastRunRoute, ShowControl::RouteWeb);
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 5000);

    /* The page's emergency stop needs no remote GO here, as the web
       server says (board Q256: only the page's PIN, which the page checks):
       with remote GO off the ALL OFF really runs. */
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    doc->operateSettings()->setRemoteGo(false);
    QVERIFY(sc->run(off, ShowControl::RouteWeb));
    QVERIFY(handled.last().at(1).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    // Every other page command still asks.
    QVERIFY(!sc->run(r, ShowControl::RouteWeb));
    QVERIFY(handled.last().at(0).toString().contains(QStringLiteral("remote GO is off")));
    QVERIFY(!list->isRunning());
}

void AppViews_Test::aGoThatFindsNothingHoldsNoPreview()
{
    Doc *doc = m_app->m_doc;
    ShowTreeController *st = doc->showTree();
    ShowControl *sc = doc->showControl();
    auto tidy = qScopeGuard([&]() { m_app->hideViewNotice(); });
    m_app->hideViewNotice();
    m_app->m_operateOffered = true;
    doc->setMode(Doc::Design);
    int taken = 0;
    QMetaObject::Connection c = connect(st, &ShowTreeController::stageTakenForRun, this,
                                        [&taken](QStringList *) { ++taken; });
    auto unhook = qScopeGuard([c]() { disconnect(c); });
    // A cue that isn't there: nothing played, nothing held, nothing said.
    ShowControl::Request r;
    r.command = ShowControl::Go;
    r.list = QStringLiteral("no such list");
    QVERIFY(!sc->run(r, ShowControl::RouteKey));
    QCOMPARE(m_app->m_lastRunRoute, ShowControl::RouteKey);
    QCOMPARE(taken, 0);
    QVERIFY(m_app->m_viewNotice->isHidden());
}

/* CHANGELOG 0.2.2, "Esc backs out": "plain Esc is no playback action (Stop
   all is ⌘⇧Esc)". */
void AppViews_Test::plainEscIsNoPlaybackAction()
{
    Doc *doc = m_app->m_doc;
    QList<quint32> fixtures, functions;
    auto tidy = qScopeGuard([&]() { takeAway(doc, fixtures, functions); });
    Chaser *list = nullptr;
    Scene *busk = litScene(doc, 210, 120, &fixtures, &functions);
    QVERIFY(busk != nullptr);
    QVERIFY(litScene(doc, 211, 200, &fixtures, &functions, &list) != nullptr);
    m_app->show();
    m_app->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(m_app));

    busk->start(doc->masterTimer(), FunctionParent::master());
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(busk->isRunning() && list->isRunning(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 211)), 200, 5000);
    auto bothRunning = [&]() { return busk->isRunning() && list->isRunning(); };

    // The main window.
    m_app->showContext("Virtual Console");
    QTest::keyClick(m_app, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY2(bothRunning(), "Esc in the main window stopped playback");

    // The Show Tree.
    m_app->showContext("Show Tree");
    ShowTreeView *tree = ShowTreeView::instance();
    QVERIFY(tree != nullptr);
    QTest::keyClick(tree->tree(), Qt::Key_Escape);
    QTest::keyClick(tree, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY2(bothRunning(), "Esc in the Show Tree stopped playback");

    // The Workbench (Build's).
    m_app->showContext("Build");
    ProgrammingManager *pm = ProgrammingManager::instance();
    QVERIFY(pm != nullptr && pm->workbench() != nullptr);
    QTest::keyClick(pm->workbench(), Qt::Key_Escape);
    QTest::keyClick(pm->workbench(), Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY2(bothRunning(), "Esc in the Workbench stopped playback");
    QCOMPARE(int(outputAt(doc, 211)), 200);

    // ⌘⇧Esc (Ctrl+Shift+Backspace on Windows and Linux): Stop all.
    QCOMPARE(m_app->m_controlPanicAction->shortcut(), App::stopAllShortcut());
    QTest::keySequence(m_app, App::stopAllShortcut());
    QTRY_VERIFY_WITH_TIMEOUT(!busk->isRunning() && !list->isRunning(), 5000);
}

/* CHANGELOG 0.2.2 (Q81): "it undoes the tab in front (Build, the Show
   Tree, Fixtures and the Lighting Studio -- the last two share the rig's
   history ...) and elsewhere reads "Nothing to undo on this tab", greyed.
   It no longer undoes a Show Tree step from another tab." */
void AppViews_Test::editUndoFollowsTheTabInFront()
{
    Doc *doc = m_app->m_doc;
    QAction *undo = m_app->menuBar()->findChild<QAction *>("editUndo");
    QVERIFY(undo != nullptr);
    QMenu *edit = nullptr;
    for (QAction *a : m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->menu()->actions().contains(undo))
            edit = a->menu();
    QVERIFY(edit != nullptr);
    auto undoOn = [&](const QString &tab) {
        m_app->showContext(tab);
        emit edit->aboutToShow();
        const QString text = undo->text();
        undo->trigger();
        return text;
    };

    // Build: the step on the shared history (a rename made through it).
    Scene *s = new Scene(doc);
    s->setName("Before");
    doc->addFunction(s);
    const quint32 sid = s->id();
    auto tidyScene = qScopeGuard([doc, sid]() { doc->deleteFunction(sid); });
    QVERIFY(doc->paletteOps()->functionStep("Rename Before", { sid }, [doc, sid](QList<quint32> &) {
        doc->function(sid)->setName("After");
        return true;
    }));
    QCOMPARE(doc->function(sid)->name(), QString("After"));
    const QString build = undoOn("Build");
    QVERIFY2(build.startsWith("Undo "), qPrintable(build));
    QCOMPARE(doc->function(sid)->name(), QString("Before"));

    // The rig's history: made on one tab, undone on the other, both ways.
    Fixture *fx = new Fixture(doc);
    fx->setName("Rig A");
    fx->setChannels(1);
    QVERIFY(doc->addFixture(fx));
    const quint32 fid = fx->id();
    auto tidyFixture = qScopeGuard([doc, fid]() { doc->deleteFixture(fid); });
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    for (const QString &tab : { QStringLiteral("Lighting Studio"), QStringLiteral("Fixtures") })
    {
        const QByteArray before = mon->rigSnapshot();
        doc->fixture(fid)->setName("Rig B");
        mon->pushRigUndo(before);
        const QString text = undoOn(tab);
        QVERIFY2(text.startsWith("Undo "), qPrintable(tab + ": " + text));
        QVERIFY(doc->fixture(fid) != nullptr);
        QCOMPARE(doc->fixture(fid)->name(), QString("Rig A"));
    }

    // Simple Desk and Run: nothing of theirs, and a Show Tree step stays.
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Stays"));
    for (const QString &tab : { QStringLiteral("Simple Desk"), QStringLiteral("Run") })
    {
        const QString text = undoOn(tab);
        QCOMPARE(text, App::menuTextWithKeys("Nothing to undo on this tab", QKeySequence::Undo));
        QVERIFY(!undo->isEnabled());
        QVERIFY2(st->production(prod) != nullptr, qPrintable(tab));
    }
    // ...and the Show Tree's own Undo takes it.
    undoOn("Show Tree");
    QTRY_VERIFY(st->production(prod) == nullptr);
}

/* CHANGELOG 0.2.2 (Q81): "renaming a fixture in the tree is now a step of
   it too". */
void AppViews_Test::renamingAFixtureInTheTreeIsARigStep()
{
    Doc *doc = m_app->m_doc;
    Fixture *fx = new Fixture(doc);
    fx->setName("Old name");
    fx->setChannels(1);
    fx->setUniverse(0);
    fx->setAddress(300);
    QVERIFY(doc->addFixture(fx));
    const quint32 fid = fx->id();
    auto tidy = qScopeGuard([doc, fid]() { doc->deleteFixture(fid); });

    m_app->showContext("Fixtures");
    FixtureManager *fm = FixtureManager::instance();
    QVERIFY(fm != nullptr);
    FixtureTreeWidget *tree = fm->m_fixtures_tree;
    QVERIFY(tree != nullptr);
    tree->updateTree();
    QTreeWidgetItem *row = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, PROP_ID).isValid() && (*it)->data(0, PROP_ID).toUInt() == fid
            && !(*it)->data(0, PROP_HEAD).isValid() && !(*it)->data(0, PROP_CHANNEL).isValid())
            row = *it;
    QVERIFY(row != nullptr);
    tree->setCurrentItem(row);

    // Return opens the name's editor; type, Return.
    QTest::keyClick(tree, Qt::Key_Return);
    QLineEdit *editor = nullptr;
    QTRY_VERIFY((editor = tree->viewport()->findChild<QLineEdit *>()) != nullptr);
    editor->selectAll();
    QTest::keyClicks(editor, "New name");
    QTest::keyClick(editor, Qt::Key_Return);
    QTRY_COMPARE(doc->fixture(fid)->name(), QString("New name"));

    QAction *undo = m_app->menuBar()->findChild<QAction *>("editUndo");
    QMenu *edit = nullptr;
    for (QAction *a : m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->menu()->actions().contains(undo))
            edit = a->menu();
    QVERIFY(edit != nullptr);
    emit edit->aboutToShow();
    QVERIFY2(undo->isEnabled() && undo->text().startsWith("Undo "), qPrintable(undo->text()));
    undo->trigger();
    QVERIFY(doc->fixture(fid) != nullptr);
    QCOMPARE(doc->fixture(fid)->name(), QString("Old name"));
}

/* CHANGELOG 0.2.2, Output rules: "On when not in use (full whenever nothing
   sets it, from the switch to Operate or when the show opens: Settings ▸
   Advanced ▸ Output)". */
void AppViews_Test::settingsAdvancedHoldsOnWhenNotInUse()
{
    Doc *doc = m_app->doc();
    OutputRuleKeeper *keeper = doc->outputRuleKeeper();
    QVERIFY(keeper != nullptr);
    auto tidy = qScopeGuard([doc, keeper]() {
        keeper->setOnAtOpenWhen(OutputRuleKeeper::OnAtOperate);
        doc->resetModified();
    });
    QCOMPARE(keeper->onAtOpenWhen(), OutputRuleKeeper::OnAtOperate);
    doc->resetModified();
    auto savedXml = [doc]() {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        doc->saveXML(&w);
        w.writeEndDocument();
        return QString::fromUtf8(buf.data());
    };
    QVERIFY(!savedXml().contains("OnAtOpen=\"ShowOpen\""));

    auto pick = [](AppSettings &dlg, OutputRuleKeeper::OnAtOpenWhen when) {
        QComboBox *combo = dlg.findChild<QComboBox *>(QStringLiteral("settingsOnAtOpenWhen"));
        if (combo == nullptr)
            return false;
        const int i = combo->findData(int(when));
        if (i < 0)
            return false;
        combo->setCurrentIndex(i);
        return true;
    };
    {
        AppSettings dlg(m_app);
        QTabWidget *tabs = dlg.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QVERIFY(tabs != nullptr);
        QVERIFY(tabs->widget(2)->findChild<QComboBox *>(QStringLiteral("settingsOnAtOpenWhen")) != nullptr);
        QVERIFY(pick(dlg, OutputRuleKeeper::OnAtShowOpen));
        dlg.reject();                   // Cancel leaves it
    }
    QCOMPARE(keeper->onAtOpenWhen(), OutputRuleKeeper::OnAtOperate);
    QVERIFY(!doc->isModified());
    {
        AppSettings dlg(m_app);
        QVERIFY(pick(dlg, OutputRuleKeeper::OnAtShowOpen));
        dlg.accept();
    }
    QCOMPARE(keeper->onAtOpenWhen(), OutputRuleKeeper::OnAtShowOpen);
    QVERIFY(keeper->onAtOpenArmed());   // on now, with no switch to Operate
    QVERIFY(doc->isModified());
    QVERIFY2(savedXml().contains("OnAtOpen=\"ShowOpen\""), "not saved with the show");
    {
        AppSettings dlg(m_app);
        QComboBox *combo = dlg.findChild<QComboBox *>(QStringLiteral("settingsOnAtOpenWhen"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->currentData().toInt(), int(OutputRuleKeeper::OnAtShowOpen));
    }
}

/* CHANGELOG 0.2.2: "Show Tree and Run have their own tab icons: a cue tree
   and a green GO button, instead of both using the chaser icon." */
void AppViews_Test::theShowTreeAndRunWearTheirOwnIcons()
{
    QTabWidget *tabs = m_app->m_tab;
    auto iconOf = [tabs](const QString &label) {
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabBar()->tabData(i).toString() == label)
                return tabs->tabIcon(i);
        return QIcon();
    };
    const QIcon tree = iconOf("Show Tree"), run = iconOf("Run");
    QCOMPARE(tree.name(), QStringLiteral("tab:showtree"));
    QCOMPARE(run.name(), QStringLiteral("tab:run"));
    foreach (QAction *a, m_app->m_windowJumps)
    {
        if (a->data().toString() == QLatin1String("Show Tree"))
            QCOMPARE(a->icon().name(), QStringLiteral("tab:showtree"));
        if (a->data().toString() == QLatin1String("Run"))
            QCOMPARE(a->icon().name(), QStringLiteral("tab:run"));
    }
    // Drawn, and not the chaser's picture.
    const QImage chaser = QIcon(":/chaser.png").pixmap(QSize(16, 16)).toImage()
                              .convertToFormat(QImage::Format_ARGB32);
    const QImage treeImg = tree.pixmap(QSize(16, 16)).toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage runImg = run.pixmap(QSize(16, 16)).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY(!treeImg.isNull() && !runImg.isNull());
    QVERIFY(treeImg != chaser && runImg != chaser && treeImg != runImg);
    int green = 0;
    for (int y = 0; y < runImg.height(); ++y)
        for (int x = 0; x < runImg.width(); ++x)
        {
            const QColor c = runImg.pixelColor(x, y);
            if (c.alpha() > 200 && c.green() > 140 && c.red() < 90 && c.blue() < 120)
                ++green;
        }
    QVERIFY2(green > runImg.width() * runImg.height() / 4, qPrintable(QString::number(green)));
}

void AppViews_Test::pagesSquashWithTheWindow()
{
    m_app->setWorkspaceView(App::ConsoleView);
    for (int i = 0; i < m_app->m_tab->count(); i++)
    {
        const QString tab = m_app->m_tab->tabText(i);
        if (tab == QLatin1String("Simple Desk"))
            continue;       // QLC+'s own page, as it is upstream
        m_app->m_tab->setCurrentIndex(i);
        QCoreApplication::processEvents();
        const int w = m_app->m_tab->widget(i)->minimumSizeHint().width();
        QVERIFY2(w <= 700, qPrintable(QString("%1 is at least %2 px wide").arg(tab).arg(w)));
    }
}

/****************************************************************************
 * Show only (Output Rules)
 ****************************************************************************/

QList<quint32> AppViews_Test::addShowOnlyLasers()
{
    Doc *doc = m_app->doc();
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(QStringLiteral("Test"), QStringLiteral("Show Laser"));
    if (def == nullptr)
    {
        def = new QLCFixtureDef();
        def->setManufacturer(QStringLiteral("Test"));
        def->setModel(QStringLiteral("Show Laser"));
        def->setType(QLCFixtureDef::Laser);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName(QStringLiteral("3ch"));
        const struct { const char *name; QLCChannel::Group group; } chans[] = {
            { "Laser On", QLCChannel::Intensity }, { "Pattern", QLCChannel::Effect },
            { "Colour", QLCChannel::Colour } };
        int i = 0;
        for (const auto &c : chans)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString::fromLatin1(c.name));
            ch->setGroup(c.group);
            def->addChannel(ch);
            mode->insertChannel(ch, i++);
        }
        def->addMode(mode);
        if (!doc->fixtureDefCache()->addFixtureDef(def))
        {
            delete def;
            return {};
        }
    }
    QList<quint32> ids;
    const char *names[] = { "Laser SL", "Laser SR" };
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString::fromLatin1(names[i]));
        f->setFixtureDefinition(def, def->modes().first());
        f->setUniverse(0);
        f->setAddress(quint32(400 + 3 * i));
        if (!doc->addFixture(f))
        {
            delete f;
            continue;
        }
        // Written as the show saves it: an older build reads nothing.
        f->setOutputRules(Fixture::stringToOutputRules(QStringLiteral("ShowOnly")));
        // On the stage, either side of centre (the grid is in metres).
        const QVector3D g = doc->monitorProperties()->gridSize();
        doc->monitorProperties()->setFixturePosition(f->id(), 0, 0,
            QVector3D(g.x() * 1000.0f * (i == 0 ? 0.35f : 0.65f), g.z() * 1000.0f * 0.4f, 5));
        ids << f->id();
    }
    doc->outputRuleKeeper()->rebuild();
    if (Monitor::instance() != nullptr)
        Monitor::instance()->updateView();
    for (int i = 0; i < 8; ++i)
    {
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }
    return ids;
}

void AppViews_Test::removeFixtures(const QList<quint32> &ids)
{
    for (quint32 id : ids)
        m_app->doc()->deleteFixture(id);
    m_app->doc()->outputRuleKeeper()->rebuild();
    QCoreApplication::processEvents();
}

void AppViews_Test::showOnlyShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QList<quint32> ids = addShowOnlyLasers();
    QCOMPARE(ids.size(), 2);
    const QSize was = m_app->size();
    const App::WorkspaceView view = m_app->workspaceView();
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->showContext(QStringLiteral("Lighting Studio"));
    auto settle = []() {
        for (int i = 0; i < 6; ++i)
        {
            QCoreApplication::processEvents();
            QTest::qWait(30);
        }
    };
    for (const QSize &size : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        m_app->resize(size);
        settle();
        QVERIFY(m_app->grab().save(QStringLiteral("%1/showonly-design-%2x%3.png")
                                   .arg(dir).arg(size.width()).arg(size.height())));
        QVERIFY(m_app->statusBar()->grab().save(QStringLiteral("%1/showonly-footer-%2.png")
                                               .arg(dir).arg(size.width())));
    }
    if (MonitorGraphicsView *gv = m_app->findChild<MonitorGraphicsView *>())
    {
        gv->resize(900, 600);
        settle();
        QVERIFY(gv->grab().save(QStringLiteral("%1/showonly-plot.png").arg(dir)));
    }
    m_app->resize(1280, 800);
    m_app->doc()->setMode(Doc::Operate);
    settle();
    QVERIFY(m_app->grab().save(QStringLiteral("%1/showonly-operate-1280x800.png").arg(dir)));
    m_app->doc()->setMode(Doc::Design);
    settle();
    {
        OutputRulesDialog dlg(m_app->doc(), ids, m_app);
        dlg.show();
        settle();
        QVERIFY(dlg.grab().save(QStringLiteral("%1/showonly-dialog.png").arg(dir)));
        // The dialog over the window, as it is seen, at three window sizes.
        const QPixmap d = dlg.grab();
        for (const QSize &size : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
        {
            m_app->resize(size);
            settle();
            QPixmap w = m_app->grab();
            {
                QPainter p(&w);
                p.drawPixmap((w.width() - d.width()) / 2, (w.height() - d.height()) / 2, d);
            }
            QVERIFY(w.save(QStringLiteral("%1/showonly-dialog-%2x%3.png")
                           .arg(dir).arg(size.width()).arg(size.height())));
        }
    }
    removeFixtures(ids);
    m_app->setWorkspaceView(view);
    m_app->resize(was);
    m_app->doc()->resetModified();
}

void AppViews_Test::showOnlyIsSaidAndBadged()
{
    const QList<quint32> ids = addShowOnlyLasers();
    QCOMPARE(ids.size(), 2);
    Doc *doc = m_app->doc();
    QCOMPARE(doc->mode(), Doc::Design);
    QLabel *chip = m_app->findChild<QLabel *>(QStringLiteral("footerShowOnly"));
    QVERIFY(chip != nullptr);
    QVERIFY(chip->isVisibleTo(m_app));
    QVERIFY2(chip->text().contains(QStringLiteral("Lasers held: Design")), qPrintable(chip->text()));
    QVERIFY(chip->toolTip().contains(QStringLiteral("Laser SL")));
    MonitorGraphicsView *plot = m_app->findChild<MonitorGraphicsView *>();
    QVERIFY(plot != nullptr);
    {
        QList<quint32> marked = plot->showOnlyHeldFixtures();
        std::sort(marked.begin(), marked.end());
        QList<quint32> want = ids;
        std::sort(want.begin(), want.end());
        QCOMPARE(marked, want);
    }
    // Locate is refused for it, saying why.
    QVERIFY(Monitor::locateRefusal(doc->fixture(ids.first())).isEmpty() == false);

    // Operate: the show drives them, nothing to say, no badge.
    doc->setMode(Doc::Operate);
    QCoreApplication::processEvents();
    QVERIFY(chip->isVisibleTo(m_app) == false);
    QVERIFY(plot->showOnlyHeldFixtures().isEmpty());
    // Blind holds them again, and says so.
    doc->inputOutputMap()->setOutputInhibited(true);
    QCoreApplication::processEvents();
    QVERIFY(chip->isVisibleTo(m_app));
    QVERIFY2(chip->text().contains(QStringLiteral("Lasers held: Blind")), qPrintable(chip->text()));
    doc->inputOutputMap()->setOutputInhibited(false);
    doc->setMode(Doc::Design);
    QCoreApplication::processEvents();

    // The dialog: the fifth rule, and unticking it lets them go.
    {
        OutputRulesDialog dlg(doc, ids, m_app);
        QCheckBox *cb = dlg.findChild<QCheckBox *>(QStringLiteral("outputRuleShowOnly"));
        QVERIFY(cb != nullptr);
        QCOMPARE(cb->checkState(), Qt::Checked);
        cb->setChecked(false);
        QVERIFY(dlg.apply());
    }
    QCOMPARE(doc->fixture(ids.first())->outputRules(), Fixture::OutputRules());
    doc->outputRuleKeeper()->rebuild();
    QCoreApplication::processEvents();
    QVERIFY(chip->isVisibleTo(m_app) == false);
    removeFixtures(ids);
    doc->resetModified();
}

void AppViews_Test::aControlSurfaceBlindFollowsTheConsolesRule()
{
    Doc *doc = m_app->doc();
    QVERIFY(m_app->m_pmjOverlay != nullptr);
    const ControlSurface::Role blind(ControlSurface::RoleType::Static, ControlSurface::Blind);
    doc->setMode(Doc::Operate);
    QCoreApplication::processEvents();
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, blind, 255);
    QVERIFY(doc->inputOutputMap()->outputInhibited() == false);   // the Blind view, not a mute
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, blind, 255);   // and back
    doc->setMode(Doc::Design);
    QCoreApplication::processEvents();
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, blind, 255);
    QVERIFY(doc->inputOutputMap()->outputInhibited());           // Design: a mute
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, blind, 255);
    QVERIFY(doc->inputOutputMap()->outputInhibited() == false);
}

void AppViews_Test::aPmjOverlayHearsOnlyItsOwnBoard()
{
    /* CS1 / F4: the PMJ overlay used to take any universe's MIDI ch 9 CC 0
       (the Grand Master) and every other PMJ channel, and then move its LEDs
       to that universe. Now only the universe whose input is patched with the
       PMJ Black 1 profile is the board. */
    // An earlier test may have loaded the stub already, under the name it gave it.
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    QLCIOPlugin *plug = cache->plugin(QStringLiteral("ArtNet"));
    if (plug == nullptr)
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    if (plug == nullptr)
    {
        QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
        stubDir.setFilter(QDir::Files);
        stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
        cache->load(stubDir);
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    }
    IOPluginStub *stub = static_cast<IOPluginStub *>(plug);
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    auto restore = qScopeGuard([this]() { m_app->clearDocument(); });
    m_app->clearDocument();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    while (iom->universesCount() < 3)
        iom->addUniverse();

    // Universe 1: some other controller (no PMJ profile), sending ch 9 CC 0.
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    const quint32 gmChannel = 32768;            // PMJ Master: MIDI ch 9, CC 0
    iom->setGrandMasterValue(200);
    m_app->m_pmjOverlay->slotInputValueChanged(0, gmChannel, 40, QString());
    QCoreApplication::processEvents();
    QCOMPARE(int(iom->grandMasterValue()), 200);

    // No universe is the PMJ yet: nowhere to hear it from either.
    m_app->m_pmjOverlay->slotInputValueChanged(1, gmChannel, 40, QString());
    QCOMPARE(int(iom->grandMasterValue()), 200);

    // Universe 2 is patched with the PMJ Black 1 profile: it is the board.
    QLCInputProfile *pmj = iom->profile(QStringLiteral("PMJ Black 1"));
    if (pmj == nullptr)
    {
        pmj = new QLCInputProfile();
        pmj->setManufacturer(QStringLiteral("PMJ"));
        pmj->setModel(QStringLiteral("Black 1"));
        QVERIFY(iom->addProfile(pmj));
    }
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 1, pmj->name()));
    QCoreApplication::processEvents();
    m_app->m_pmjOverlay->slotInputValueChanged(1, gmChannel, 80, QString());
    QVERIFY2(int(iom->grandMasterValue()) != 200, "the PMJ's own master fader did not move the Grand Master");

    // And it never moves to the other controller on its traffic.
    iom->setGrandMasterValue(200);
    m_app->m_pmjOverlay->slotInputValueChanged(0, gmChannel, 10, QString());
    m_app->m_pmjOverlay->slotInputValueChanged(2, gmChannel, 10, QString());
    QCOMPARE(int(iom->grandMasterValue()), 200);
    iom->setGrandMasterValue(255);
}

/* Q309, the one dispatch point: a PMJ control this computer's desk has
   (here its master fader) is the desk's alone; the overlay listened to the
   raw input and did its own job on it as well. */
void AppViews_Test::aPmjControlTheDeskHasIsNotAlsoThePmjs()
{
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    QLCIOPlugin *plug = cache->plugin(QStringLiteral("ArtNet"));
    if (plug == nullptr)
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    if (plug == nullptr)
    {
        QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
        stubDir.setFilter(QDir::Files);
        stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
        cache->load(stubDir);
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    }
    IOPluginStub *stub = static_cast<IOPluginStub *>(plug);
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    auto restore = qScopeGuard([this, iom]() {
        iom->setStaticControls({});
        iom->setInputPatch(1, KInputNone, QString(), QLCIOPlugin::invalidLine());
        iom->setGrandMasterValue(255);
        m_app->clearDocument();
    });
    m_app->clearDocument();
    while (iom->universesCount() < 2)
        iom->addUniverse();
    QLCInputProfile *pmj = iom->profile(QStringLiteral("PMJ Black 1"));
    if (pmj == nullptr)
    {
        pmj = new QLCInputProfile();
        pmj->setManufacturer(QStringLiteral("PMJ"));
        pmj->setModel(QStringLiteral("Black 1"));
        QVERIFY(iom->addProfile(pmj));
    }
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 1, pmj->name()));
    QCoreApplication::processEvents();
    const quint32 gmChannel = 32768;            // PMJ Master: MIDI ch 9, CC 0

    // The control case: the board's master, through the input map, moves the GM.
    iom->setGrandMasterValue(200);
    emit iom->inputValueChanged(1, gmChannel, 80);
    QCoreApplication::processEvents();
    QVERIFY2(int(iom->grandMasterValue()) != 200, "the PMJ's master fader did not move the Grand Master");

    // The same control is the desk's: the overlay does not hear it.
    iom->setGrandMasterValue(200);
    iom->setStaticControls({ (quint64(1) << 32) | gmChannel });
    emit iom->inputValueChanged(1, gmChannel, 40);
    QCoreApplication::processEvents();
    QVERIFY2(int(iom->grandMasterValue()) == 200, "a desk control also drove the PMJ overlay's role");
}

/* One lamp writer (CONTROL_SURFACES_EVAL §4): a free PMJ key the show
   learned (a GO, a hit, the stop on Favorites) is lit by the Control Map.
   The overlay repainted every key on each Grand Master change and sent the
   ones with no job of its own dark, blanking that lamp. */
void AppViews_Test::masterMoveKeepsShowLamps()
{
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    QLCIOPlugin *plug = cache->plugin(QStringLiteral("ArtNet"));
    if (plug == nullptr)
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    if (plug == nullptr)
    {
        QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
        stubDir.setFilter(QDir::Files);
        stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
        cache->load(stubDir);
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    }
    IOPluginStub *stub = static_cast<IOPluginStub *>(plug);
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    auto restore = qScopeGuard([this]() {
        InputOutputMap *map = m_app->doc()->inputOutputMap();
        map->setGrandMasterValue(255);
        // The patch outlives the show: the next PMJ test wants no board on universe 1.
        map->setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), true, 0);
        map->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine());
        m_app->clearDocument();
    });
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    ControlMap *cm = doc->controlMap();
    cm->setDimWhenReady(true);

    QLCInputProfile *pmj = iom->profile(QStringLiteral("PMJ Black 1"));
    if (pmj == nullptr)
    {
        pmj = new QLCInputProfile();
        pmj->setManufacturer(QStringLiteral("PMJ"));
        pmj->setModel(QStringLiteral("Black 1"));
        QVERIFY(iom->addProfile(pmj));
    }
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0, pmj->name()));
    QVERIFY(iom->setOutputPatch(0, stub->name(), QString(), 0, true));     // the feedback line
    QCoreApplication::processEvents();

    // A scene learned on Favorites, a key the PMJ gives no job.
    const quint32 favorites = 32944;
    const quint32 blackoutKey = 32918;      // O: the board's own
    Scene *s = new Scene(doc);
    s->setName(QStringLiteral("Warm"));
    QVERIFY(doc->addFunction(s));
    cm->setBinding({ 0, favorites, ControlMap::function(s->id()), ControlMap::Toggle });
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }
    auto lastSent = [stub](quint32 ch, int from) {
        int v = -1;
        for (int i = from; i < stub->m_feedback.size(); ++i)
            if (stub->m_feedback.at(i).first == ch)
                v = stub->m_feedback.at(i).second;
        return v;
    };
    const int ready = int(cm->lampValueOn(*cm->binding(ControlMap::function(s->id())), ControlMap::LampStandby));
    QVERIFY(ready > 0);
    QCOMPARE(lastSent(favorites, 0), ready);   // ready: dim, or the profile's lit

    // The Grand Master moves (the PMJ's Master, the footer, anything).
    const int before = stub->m_feedback.size();
    iom->setGrandMasterValue(100);
    iom->setGrandMasterValue(180);
    QCoreApplication::processEvents();
    QVERIFY2(lastSent(blackoutKey, before) >= 0, "the board stopped lighting its own keys");
    for (int i = before; i < stub->m_feedback.size(); ++i)
        if (stub->m_feedback.at(i).first == favorites)
            QVERIFY2(stub->m_feedback.at(i).second != 0, "a Grand Master move sent the show's lamp dark");
    QCOMPARE(lastSent(favorites, 0), ready);

    // Let go by the show: the board lights (here: darkens) it again.
    QVERIFY(cm->removeBinding(ControlMap::function(s->id())));
    const int after = stub->m_feedback.size();
    iom->setGrandMasterValue(120);
    QCoreApplication::processEvents();
    QCOMPARE(lastSent(favorites, after), 0);
}

namespace
{
/** The I/O stub as universe 1's input with the PMJ Black 1 profile, and its
 *  feedback line: the board as the overlay finds it. Null: no stub. */
IOPluginStub *patchPmj(App *app)
{
    IOPluginCache *cache = app->doc()->ioPluginCache();
    QLCIOPlugin *plug = cache->plugin(QStringLiteral("ArtNet"));
    if (plug == nullptr)
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    if (plug == nullptr)
    {
        QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
        stubDir.setFilter(QDir::Files);
        stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
        cache->load(stubDir);
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    }
    IOPluginStub *stub = static_cast<IOPluginStub *>(plug);
    if (stub == nullptr)
        return nullptr;
    InputOutputMap *iom = app->doc()->inputOutputMap();
    QLCInputProfile *pmj = iom->profile(QStringLiteral("PMJ Black 1"));
    if (pmj == nullptr)
    {
        pmj = new QLCInputProfile();
        pmj->setManufacturer(QStringLiteral("PMJ"));
        pmj->setModel(QStringLiteral("Black 1"));
        if (!iom->addProfile(pmj))
            return nullptr;
    }
    if (!iom->setInputPatch(0, stub->name(), QString(), 0, pmj->name())
        || !iom->setOutputPatch(0, stub->name(), QString(), 0, true))
        return nullptr;
    QCoreApplication::processEvents();
    return stub;
}

void unpatchPmj(App *app)
{
    InputOutputMap *map = app->doc()->inputOutputMap();
    map->setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), true, 0);
    map->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine());
}

/** A main cue list of @a cues cues in a new production; its chaser. */
Chaser *mainList(Doc *doc, int cues, quint32 *listId = nullptr)
{
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("PMJ Run"));
    const quint32 lid = st->addCueList(prod, QStringLiteral("Main"), ShowTree::ListMode::NonTracking);
    for (int i = 0; i < cues; ++i)
        st->addCue(lid, i - 1, QStringLiteral("Cue %1").arg(i + 1));
    Chaser *list = qobject_cast<Chaser *>(doc->function(st->cueList(lid)->chaser));
    if (list != nullptr)
        doc->showControl()->setMainCueList(list->id());
    if (listId != nullptr)
        *listId = lid;
    return list;
}

const ControlSurface::Role kPmjGo(ControlSurface::RoleType::Transport, ControlSurface::Go);
const ControlSurface::Role kPmjBack(ControlSurface::RoleType::Transport, ControlSurface::Back);
}

/* (a) of his 4612 test: the PMJ's Go did nothing -- registered, never wired.
   Now it is the Run tab's GO: the same function (CueRunner::runTabCommand(),
   RouteRunTab), the double-GO guard with it, the Run tab's list. */
void AppViews_Test::pmjGoRunsTheRunTabList()
{
    Doc *doc = m_app->doc();
    ShowControl *sc = doc->showControl();
    auto restore = qScopeGuard([this, doc, sc]() {
        sc->setGoGuardMs(0);
        sc->setGuardClock(ShowControl::GuardClock());
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        unpatchPmj(m_app);
        m_app->clearDocument();
        m_app->hideViewNotice();
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
    });
    m_app->clearDocument();
    QVERIFY(patchPmj(m_app) != nullptr);
    quint32 listId = ShowTree::invalidId();
    Chaser *list = mainList(doc, 3, &listId);
    QVERIFY(list != nullptr);
    doc->setMode(Doc::Operate);
    PMJOverlay *pmj = m_app->m_pmjOverlay;

    int count = m_app->m_runCommands;
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QCOMPARE(m_app->m_runCommands, ++count);
    QCOMPARE(m_app->m_lastRunRoute, ShowControl::RouteRunTab);
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    // Its release is not a second GO.
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);
    QCOMPARE(m_app->m_runCommands, count);

    // The double-GO guard: a second press inside the window is held.
    qint64 now = 1000000;
    sc->setGuardClock([&now]() { return now; });
    sc->setGoGuardMs(1000);
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 5000);
    QSignalSpy held(sc, &ShowControl::commandHeld);
    now += 300;
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QCOMPARE(held.count(), 1);
    QTest::qWait(50);
    QCOMPARE(list->currentStepIndex(), 1);
    sc->setGoGuardMs(0);
    sc->setGuardClock(ShowControl::GuardClock());

    // The list the Run tab plays, not only the main one.
    const quint32 prod = doc->showTree()->productionOfChaser(list->id());
    const quint32 other = doc->showTree()->addCueList(prod, QStringLiteral("Side"), ShowTree::ListMode::NonTracking);
    doc->showTree()->addCue(other, -1, QStringLiteral("Side 1"));
    Chaser *side = qobject_cast<Chaser *>(doc->function(doc->showTree()->cueList(other)->chaser));
    QVERIFY(side != nullptr);
    doc->controlMap()->setRunTabList(other);
    QCOMPARE(CueRunner::runTabChaser(doc), side);
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QTRY_VERIFY_WITH_TIMEOUT(side->isRunning(), 5000);
    QCOMPARE(list->currentStepIndex(), 1);
    doc->controlMap()->setRunTabList(ShowTree::invalidId());

    // A trigger the show learned on Go is Go's job then: the overlay does not GO too.
    Scene *warm = new Scene(doc);
    warm->setName(QStringLiteral("Warm"));
    QVERIFY(doc->addFunction(warm));
    doc->controlMap()->setBinding({ 0, 32896, ControlMap::function(warm->id()), ControlMap::Toggle });
    count = m_app->m_runCommands;
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QCOMPARE(m_app->m_runCommands, count);

    // In Design it does what the Run tab's GO does there.
    QVERIFY(doc->controlMap()->removeBinding(ControlMap::function(warm->id())));
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    doc->setMode(Doc::Design);
    CueRunner::runTabCommand(doc, ShowControl::Go);
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 5000);
    const bool runTabPlays = list->isRunning();
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QCOMPARE(m_app->m_lastRunRoute, ShowControl::RouteRunTab);
    QTRY_COMPARE_WITH_TIMEOUT(list->isRunning(), runTabPlays, 5000);
}

void AppViews_Test::pmjBackStepsBack()
{
    Doc *doc = m_app->doc();
    auto restore = qScopeGuard([this, doc]() {
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        unpatchPmj(m_app);
        m_app->clearDocument();
        m_app->hideViewNotice();
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
    });
    m_app->clearDocument();
    QVERIFY(patchPmj(m_app) != nullptr);
    Chaser *list = mainList(doc, 3);
    QVERIFY(list != nullptr);
    doc->setMode(Doc::Operate);
    PMJOverlay *pmj = m_app->m_pmjOverlay;

    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 5000);
    const int count = m_app->m_runCommands;
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjBack, 0);
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjBack, 127);
    QCOMPARE(m_app->m_runCommands, count + 1);
    QCOMPARE(m_app->m_lastRunRoute, ShowControl::RouteRunTab);
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 0, 5000);
    pmj->slotRoleActivated(PMJOverlay::deviceId, kPmjBack, 0);     // the release
    QCOMPARE(m_app->m_runCommands, count + 1);
}

/* (a) again: Go and Back never lit to say they were ready. Their lamps are
   the Run tab's buttons' rule (CueRunner::canGo() / canBack()) at the
   profile's own levels: 127 lit, 15 dim (sent doubled: the MIDI plugin
   halves it). */
void AppViews_Test::pmjGoLampLitWhenReady()
{
    Doc *doc = m_app->doc();
    auto restore = qScopeGuard([this, doc]() {
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        unpatchPmj(m_app);
        m_app->clearDocument();
        m_app->hideViewNotice();
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
    });
    m_app->clearDocument();
    IOPluginStub *stub = patchPmj(m_app);
    QVERIFY(stub != nullptr);
    const quint32 go = 32896, back = 32907, effects = 32946, groups = 32947;
    auto lastSent = [stub](quint32 ch) -> int {
        for (int i = stub->m_feedback.size() - 1; i >= 0; --i)
            if (stub->m_feedback.at(i).first == ch)
                return int(stub->m_feedback.at(i).second);
        return -1;
    };
    const int lit = 254, dim = 30;

    // No cue list: nothing to GO, ever -- dark.
    m_app->m_controlSurfaceEngine->refreshDevice(PMJOverlay::deviceId);
    QCOMPARE(lastSent(go), 0);
    QCOMPARE(lastSent(back), 0);
    // The page in use is lit: Effects until another is pressed.
    QCOMPARE(lastSent(effects), lit);
    QCOMPARE(lastSent(groups), 0);

    // A list with a cue to GO: Go lit, Back dim (nothing to go back to).
    Chaser *list = mainList(doc, 2);
    QVERIFY(list != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(lastSent(go), lit, 3000);
    QCOMPARE(lastSent(back), dim);

    // Running: Back lit too.
    doc->setMode(Doc::Operate);
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QTRY_COMPARE_WITH_TIMEOUT(lastSent(back), lit, 3000);
    QCOMPARE(lastSent(go), lit);

    // On the last cue nothing is left to GO: Go dim.
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 0);    // a press is the edge from 0
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, kPmjGo, 127);
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(lastSent(go), dim, 3000);
    QCOMPARE(lastSent(back), lit);

    // Stopped: Back dim again, Go lit (GO starts it over).
    list->stop(FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(lastSent(back), dim, 3000);
    QCOMPARE(lastSent(go), lit);

    // Another page pressed: its lamp is the lit one.
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId,
                                           ControlSurface::Role(ControlSurface::RoleType::Page, 1), 127);
    QCOMPARE(lastSent(groups), lit);
    QCOMPARE(lastSent(effects), 0);
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId,
                                           ControlSurface::Role(ControlSurface::RoleType::Page, 0), 127);
}

void AppViews_Test::aPmjFaderOrEncoderInOperateEditsNothing()
{
    /* CS1 / F5: the PMJ's faders and encoders edited palettes and live
       channels in Operate with no undo; the Encoder bank refuses there. Now
       they refuse too, in the same words, on the footer. */
    Doc *doc = m_app->doc();
    auto restore = qScopeGuard([this, doc]() {
        doc->setMode(Doc::Design);
        m_app->clearDocument();
    });
    m_app->clearDocument();
    Fixture *f = new Fixture(doc);
    f->setName(QStringLiteral("Dim"));
    f->setChannels(1);
    f->setUniverse(0);
    f->setAddress(0);
    QVERIFY(doc->addFixture(f));
    doc->programmer()->setProgrammerSelection({ f->id() });
    const ControlSurface::Role level1(ControlSurface::RoleType::Level, 1);
    const ControlSurface::Role enc1(ControlSurface::RoleType::Param, 1);
    auto value = [doc, f]() { return int(doc->programmerValues().value(f->id()).value(0, 255)); };

    // Design: fader 1 sets the selected fixture's intensity (the control case).
    doc->setMode(Doc::Design);
    m_app->statusBar()->clearMessage();
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, level1, 100);
    QCOMPARE(value(), 100);
    QVERIFY(!m_app->statusBar()->currentMessage().contains("Operate"));

    // Operate: nothing moves, and the footer says why, as the Encoder bank does.
    doc->setMode(Doc::Operate);
    QCoreApplication::processEvents();
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, level1, 30);
    QCOMPARE(value(), 100);
    QVERIFY2(m_app->statusBar()->currentMessage().contains("off in Operate -- switch to Design to edit"),
             qPrintable(m_app->statusBar()->currentMessage()));
    m_app->statusBar()->clearMessage();
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, enc1, 2);
    QVERIFY2(m_app->statusBar()->currentMessage().startsWith("Encoders are off in Operate -- switch to Design to edit"),
             qPrintable(m_app->statusBar()->currentMessage()));
    // Its Reset (the "N-Up" button) is an edit too.
    const ControlSurface::Role up1(ControlSurface::RoleType::Reset, 1);
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId, up1, 127);
    QCOMPARE(value(), 100);

    // The Grand Master is a master, not an edit: it works in Operate.
    doc->inputOutputMap()->setGrandMasterValue(255);
    m_app->m_pmjOverlay->slotRoleActivated(PMJOverlay::deviceId,
        ControlSurface::Role(ControlSurface::RoleType::Level, -1), 90);
    QCOMPARE(int(doc->inputOutputMap()->grandMasterValue()), 90);
    doc->inputOutputMap()->setGrandMasterValue(255);
}

void AppViews_Test::aSecondPmjBoardIsNamedNotIgnoredInSilence()
{
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    QLCIOPlugin *plug = cache->plugin(QStringLiteral("ArtNet"));
    if (plug == nullptr)
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    IOPluginStub *stub = static_cast<IOPluginStub *>(plug);
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    auto restore = qScopeGuard([this]() { m_app->clearDocument(); });
    m_app->clearDocument();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    while (iom->universesCount() < 3)
        iom->addUniverse();
    QLCInputProfile *pmj = iom->profile(QStringLiteral("PMJ Black 1"));
    if (pmj == nullptr)
    {
        pmj = new QLCInputProfile();
        pmj->setManufacturer(QStringLiteral("PMJ"));
        pmj->setModel(QStringLiteral("Black 1"));
        QVERIFY(iom->addProfile(pmj));
    }
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 0, pmj->name()));
    QVERIFY(iom->setInputPatch(2, stub->name(), QString(), 1, pmj->name()));
    QCoreApplication::processEvents();
    m_app->statusBar()->clearMessage();
    iom->setGrandMasterValue(200);
    m_app->m_pmjOverlay->slotInputValueChanged(2, 32768, 40, QString());
    QCOMPARE(int(iom->grandMasterValue()), 200);                 // the second board is not heard...
    const QString said = m_app->statusBar()->currentMessage();   // ...and it says so
    QVERIFY2(said.contains("second PMJ Black 1") && said.contains("universe 3") && said.contains("universe 2"),
             qPrintable(said));
    iom->setGrandMasterValue(255);
}

void AppViews_Test::aRefusedSurfacePressSaysSoOnTheFooter()
{
    /* The App's wiring of ControlMap::blocked to the footer, not only the signal. */
    Doc *doc = m_app->doc();
    auto restore = qScopeGuard([this, doc]() {
        doc->setMode(Doc::Design);
        m_app->clearDocument();
    });
    m_app->clearDocument();
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Show"));
    st->addCueList(prod, QStringLiteral("Act 1"));
    const quint32 other = st->addCueList(prod, QStringLiteral("Act 2"));
    ControlMap *cm = doc->controlMap();
    cm->setBinding({ 0, 20, ControlMap::cueList(other), ControlMap::Toggle });
    doc->setMode(Doc::Design);
    m_app->statusBar()->clearMessage();
    cm->handleInput(0, 20, 127);
    QVERIFY2(m_app->statusBar()->currentMessage().startsWith("Not fired: Cue list: Act 2"),
             qPrintable(m_app->statusBar()->currentMessage()));
    QVERIFY(m_app->statusBar()->currentMessage().contains("armed"));
    cm->handleInput(0, 20, 0);
}

/* Q313: the Grand Master follows its external fader at once, both ways: no
   pick-up, nothing waiting, no hint on the footer. */
void AppViews_Test::theGrandMasterFollowsItsFaderAtOnce()
{
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    VirtualConsole *vc = VirtualConsole::instance();
    const quint32 wasU = vc->m_properties.grandMasterInputUniverse();
    const quint32 wasC = vc->m_properties.grandMasterInputChannel();
    const uchar wasGm = iom->grandMasterValue();
    auto restore = qScopeGuard([=]() {
        vc->m_properties.setGrandMasterInputSource(wasU, wasC);
        iom->setGrandMasterValue(wasGm);
    });
    vc->m_properties.setGrandMasterInputSource(0, 117);
    iom->setGrandMasterValue(255);

    // The first value is far below: the GM goes there. Then up, then down.
    emit iom->inputValueChanged(0, 117, 100);
    QCOMPARE(int(iom->grandMasterValue()), 100);
    emit iom->inputValueChanged(0, 117, 220);
    QCOMPARE(int(iom->grandMasterValue()), 220);
    emit iom->inputValueChanged(0, 117, 5);
    QCOMPARE(int(iom->grandMasterValue()), 5);
    emit iom->inputValueChanged(0, 117, 250);
    QCOMPARE(int(iom->grandMasterValue()), 250);
    QCOMPARE(m_app->m_statusGrandMasterSlider->value(), 250);
    QVERIFY(!m_app->m_statusGrandMasterSlider->toolTip().contains(QStringLiteral("move it to")));
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
    const QStringList console = { "Connections", "Fixtures", "Lighting Studio",
                                  "Show Tree", "Shows", "Virtual Console", "Run" };
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
             QStringList({ "Connections 1", "Fixtures 2", "Lighting Studio 3", "Show Tree \u2014 cue lists, cues and hits 4",
                           "Timeline 5", "Virtual Console 6", "Run 7",
                           "|", "Functions", "Build", "Simple Desk D" }));

    m_app->setWorkspaceView(App::ClassicView);
    QCOMPARE(windowJumpsAsShown(m_app),
             QStringList({ "Fixtures 1", "Functions 2", "Shows 3", "Virtual Console 4",
                           "Simple Desk 5", "Inputs/Outputs 6",
                           "|", "Lighting Studio", "Build",
                           "Show Tree \u2014 cue lists, cues and hits", "Run" }));
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
                           "Build 5", "Show Tree \u2014 cue lists, cues and hits 6",
                           "Timeline 7", "Virtual Console 8",
                           "Simple Desk 9", "Run 0" }));

    // A tab in a window of its own keeps its number.
    m_app->setWorkspaceView(App::ConsoleView);
    QTabWidget *tabs = m_app->m_tab;
    m_app->slotDetachContext(tabs->indexOf(VirtualConsole::instance()));
    QStringList shown = windowJumpsAsShown(m_app);
    QCOMPARE(shown.value(5), QString("Virtual Console — In Its Own Window 6"));
    QCOMPARE(shown.value(6), QString("Run 7"));
    reattach(m_app, "Virtual Console");
    QCOMPARE(windowJumpsAsShown(m_app).value(5), QString("Virtual Console 6"));
    m_app->doc()->resetModified();
}

void AppViews_Test::aBrokenOutViewSurvivesAViewSwitch()
{
    // A tab the view hasn't got (indexOf() gave -1): nothing to break out.
    const int tabsBefore = m_app->m_tab->count();
    m_app->slotDetachContext(-1);
    m_app->slotDetachContext(tabsBefore);
    QCOMPARE(m_app->m_tab->count(), tabsBefore);

    QTabWidget *tabs = m_app->m_tab;
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    auto windowOf = [this](QWidget *w) -> DetachedContext * {
        foreach (DetachedContext *dw, m_app->findChildren<DetachedContext *>())
            if (dw->centralWidget() == w)
                return dw;
        return nullptr;
    };

    // The Virtual Console (in both views) and the Show Tree (qlcconsole
    // only) out in windows.
    QWidget *vc = VirtualConsole::instance();
    QWidget *build = ShowTreeView::instance();
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
    // The Show Tree has no tab in Classic: put back from the menu it isn't offered...
    QCOMPARE(m_app->detachedContexts(), QStringList({ "Virtual Console" }));
    // ...and closing its window puts it away, not on the bar.
    buildWin->close();
    QCOMPARE(tabs->indexOf(build), -1);
    QVERIFY(!build->isVisible());
    // The Virtual Console goes back to its place in Classic's order.
    vcWin->close();
    QCOMPARE(tabs->indexOf(vc), 3);

    // Out again, and back to qlcconsole: the Show Tree is a tab again, the VC stays out.
    m_app->slotDetachContext(tabs->indexOf(vc));
    vcWin = windowOf(vc);
    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(windowOf(vc), vcWin);
    QCOMPARE(tabs->indexOf(build), 3);
    vcWin->close();
    QCOMPARE(tabLabels(tabs), QStringList({ "Connections", "Fixtures", "Lighting Studio",
                                            "Show Tree", "Shows", "Virtual Console", "Run" }));
    m_app->doc()->resetModified();
}

/* What the operator reads on the bar (Q262), not the model's list. */
static QStringList barTexts(App *app)
{
    app->setTabLabelMode(App::TabTextOnly);
    QStringList out;
    QTabWidget *tabs = app->m_tab;
    for (int i = 0; i < tabs->count(); ++i)
        out << tabs->tabText(i);
    return out;
}

void AppViews_Test::theTabBarDefaultsPerView()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(barTexts(m_app), QStringList({ "Connections", "Fixtures", "Lighting Studio",
                                            "Show Tree", "Timeline", "Virtual Console", "Run" }));
    m_app->setWorkspaceView(App::EverythingView);
    QVERIFY(barTexts(m_app).contains("Timeline"));
    QVERIFY(!barTexts(m_app).contains("Shows"));
    m_app->setWorkspaceView(App::ClassicView);
    QVERIFY(barTexts(m_app).contains("Shows"));
    QVERIFY(!barTexts(m_app).contains("Timeline"));
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->setTabLabelMode(App::TabIconAndText);
    m_app->doc()->resetModified();
}

void AppViews_Test::aCustomTabSetRoundTripsThroughTheShow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("tabs.qxw");
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->setCustomTabs({ "Run", "Fixtures", "Virtual Console", "Connections" });
    // Connections leads and Run ends whatever the order asked for.
    const QStringList chosen = { "Connections", "Fixtures", "Virtual Console", "Run" };
    QCOMPARE(barTexts(m_app), chosen);
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->saveXML(path), QFile::NoError);

    m_app->clearDocument();             // a new show: the view's default
    QCOMPARE(barTexts(m_app).size(), 7);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(barTexts(m_app), chosen);

    // A show saved without the setting gets its view's default.
    m_app->setCustomTabs(QStringList());
    QCOMPARE(barTexts(m_app).size(), 7);
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    m_app->setCustomTabs({ "Fixtures", "Run" });
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(barTexts(m_app).size(), 7);

    // Classic stays stock whatever the show asks.
    m_app->setCustomTabs({ "Fixtures", "Run" });
    m_app->setWorkspaceView(App::ClassicView);
    QCOMPARE(barTexts(m_app), QStringList({ "Fixtures", "Functions", "Shows", "Virtual Console",
                                            "Simple Desk", "Inputs/Outputs" }));
    m_app->setCustomTabs(QStringList());
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->setTabLabelMode(App::TabIconAndText);
    m_app->doc()->resetModified();
}

void AppViews_Test::runAndConnectionsCantBeUnchecked()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    QAction *act = m_app->menuBar()->findChild<QAction *>("viewTabsAction");
    QVERIFY(act != nullptr);
    QVERIFY(act->isEnabled());
    QListWidget *list = nullptr;
    bool tried = false;
    QTimer::singleShot(300, m_app, [&]() {
        QDialog *dlg = m_app->findChild<QDialog *>("chooseTabsDialog");
        QVERIFY(dlg != nullptr);
        list = dlg->findChild<QListWidget *>("chooseTabsList");
        QVERIFY(list != nullptr);
        dlg->show();
        QVERIFY(QTest::qWaitForWindowExposed(dlg));
        auto row = [&](const QString &tab) -> QListWidgetItem * {
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toString() == tab)
                    return list->item(i);
            return nullptr;
        };
        // Every box works or there is none: Run and Connections have no box
        // (no tick that does nothing), can't be dragged, and sit at the ends.
        QListWidgetItem *conn = row("Connections"), *run = row("Run");
        QVERIFY(conn != nullptr && run != nullptr);
        QCOMPARE(list->row(conn), 0);
        QCOMPARE(list->row(run), list->count() - 1);
        foreach (QListWidgetItem *it, QList<QListWidgetItem *>({ conn, run }))
        {
            QVERIFY(!it->data(Qt::CheckStateRole).isValid());
            QVERIFY(!(it->flags() & Qt::ItemIsUserCheckable));
            QVERIFY(!(it->flags() & Qt::ItemIsDragEnabled));
            const QRect r = list->visualItemRect(it);
            QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
            QVERIFY(!it->data(Qt::CheckStateRole).isValid());
            tried = true;
        }
        // Every other row toggles from a click on its name, as a person clicks...
        QListWidgetItem *fixtures = row("Fixtures");
        QVERIFY(fixtures != nullptr);
        QCOMPARE(fixtures->checkState(), Qt::Checked);
        QRect r = list->visualItemRect(fixtures);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
        QCOMPARE(fixtures->checkState(), Qt::Unchecked);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
        QCOMPARE(fixtures->checkState(), Qt::Checked);
        // ...and from a click on its box, once (not twice, back where it was).
        QStyleOptionViewItem opt;
        opt.initFrom(list->viewport());
        opt.rect = r;
        opt.features = QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDisplay;
        const QRect box = list->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator,
                                                        &opt, list);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, box.center());
        QCOMPARE(fixtures->checkState(), Qt::Unchecked);
        dlg->accept();
    });
    act->trigger();
    QVERIFY(tried);
    QVERIFY(!barTexts(m_app).contains("Fixtures"));
    QVERIFY(barTexts(m_app).contains("Run"));
    QVERIFY(barTexts(m_app).contains("Connections"));
    // And not by the call either.
    m_app->setCustomTabs({ "Fixtures" });
    QVERIFY(barTexts(m_app).contains("Run") && barTexts(m_app).contains("Connections"));

    // Classic: offered for the features (Q293); its tab list is greyed.
    m_app->setCustomTabs(QStringList());
    m_app->setWorkspaceView(App::ClassicView);
    QVERIFY(act->isEnabled());
    QCOMPARE(act->toolTip(), QString("Turn features on or off for this show (Classic keeps QLC+'s own tabs)"));
    m_app->setWorkspaceView(App::ConsoleView);
    m_app->setTabLabelMode(App::TabIconAndText);
    m_app->doc()->resetModified();
}

/* Findability (board Q293): the feature shipped in 0.5.2 with tests for what
   it does but nothing testing that a person can find it. Walk the path a
   person clicks -- the menu bar's View menu, its Workspace submenu, "Tabs &
   Features…" in its place there -- and trigger the real menu action (not
   App::slotChooseTabs() by hand) to check it opens the real dialog, titled
   as the menu calls it. A future rename or move of the item breaks this. */
void AppViews_Test::tabsAndFeaturesIsFindableFromViewWorkspace()
{
    QMenu *view = nullptr;
    foreach (QAction *a, m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->text().remove('&') == "View")
            view = a->menu();
    QVERIFY2(view != nullptr, "the menu bar has a View menu");

    QMenu *workspace = nullptr;
    QAction *workspaceAction = nullptr;
    foreach (QAction *a, view->actions())
        if (a->menu() != nullptr && a->menu()->objectName() == QStringLiteral("viewWorkspaceMenu"))
        {
            workspace = a->menu();
            workspaceAction = a;
        }
    QVERIFY2(workspace != nullptr, "the View menu has a Workspace submenu");
    QCOMPARE(workspaceAction->text(), QString("Workspace"));   // the menu text as shown

    QAction *tabsAndFeatures = nullptr;
    foreach (QAction *a, workspace->actions())
        if (a->objectName() == QStringLiteral("viewTabsAction"))
            tabsAndFeatures = a;
    QVERIFY2(tabsAndFeatures != nullptr, "Tabs & Features… sits in the Workspace submenu");
    QCOMPARE(tabsAndFeatures->text(), QString("Tabs && Features…"));

    // Its place in the submenu: last, after the three view choices and the
    // separator that sets it apart from them.
    const QList<QAction *> items = workspace->actions();
    QCOMPARE(items.last(), tabsAndFeatures);
    QVERIFY2(items.size() >= 2 && items.at(items.size() - 2)->isSeparator(),
             "a separator sets Tabs & Features… apart from the view choices");

    // Triggering the menu item itself -- not calling slotChooseTabs() --
    // opens the real dialog.
    m_app->setWorkspaceView(App::ConsoleView);
    QVERIFY(tabsAndFeatures->isEnabled());
    bool opened = false;
    QTimer::singleShot(100, m_app, [&]() {
        QDialog *dlg = m_app->findChild<QDialog *>(QStringLiteral("chooseTabsDialog"));
        opened = (dlg != nullptr);
        if (dlg != nullptr)
        {
            QCOMPARE(dlg->windowTitle(), QString("Tabs & Features"));
            dlg->reject();
        }
    });
    tabsAndFeatures->trigger();
    QVERIFY2(opened, "the menu item opened the Tabs & Features dialog");
}

/* View ▸ Workspace ▸ Tabs & Features…, as a person uses it: tick or untick
   the features named (by their row), then OK. False when the page has no
   features list. */
static QMenu *rightClickFooter(App *app);

static bool chooseFeatures(App *app, const QMap<QString, bool> &want)
{
    QAction *act = app->menuBar()->findChild<QAction *>("viewTabsAction");
    if (act == nullptr || !act->isEnabled())
        return false;
    bool found = false;
    QTimer::singleShot(100, app, [&]() {
        QDialog *dlg = app->findChild<QDialog *>("chooseTabsDialog");
        if (dlg == nullptr)
            return;
        QListWidget *list = dlg->findChild<QListWidget *>("chooseFeaturesList");
        if (list == nullptr)
        {
            dlg->reject();
            return;
        }
        found = true;
        for (int i = 0; i < list->count(); ++i)
        {
            QListWidgetItem *it = list->item(i);
            if (want.contains(it->text()))
                it->setCheckState(want.value(it->text()) ? Qt::Checked : Qt::Unchecked);
        }
        dlg->accept();
    });
    act->trigger();
    return found;
}

/* UX-review shots, only where the pool gives QLC_SHOT_DIR. */
static void featureShot(QWidget *w, const QString &name)
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty() || w == nullptr)
        return;
    const QSize sizes[3] = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        w->window()->resize(qMin(sz.width(), 640), qMin(sz.height(), 560));
        QCoreApplication::processEvents();
        w->grab().save(QDir(dir).filePath(QStringLiteral("%1-%2x%3.png").arg(name).arg(sz.width()).arg(sz.height())));
    }
}

/* The same, with the question Turning Venues off asks while a venue is
   applied: @p answer is the button's object name ("venueOffClear",
   "venueOffKeep") or "cancel". @p asked is set when the question came, and
   @p askedText holds what it said. With no @p answer a question is a
   failure of the test (cancelled). */
static bool chooseFeaturesAnswering(App *app, const QMap<QString, bool> &want, const QString &answer,
                                    bool *asked, QString *askedText = nullptr)
{
    *asked = false;
    QTimer poll;
    poll.setInterval(20);
    QObject::connect(&poll, &QTimer::timeout, app, [&]() {
        QMessageBox *q = app->findChild<QMessageBox *>("venueOffPrompt");
        if (q == nullptr || *asked)
            return;
        *asked = true;
        featureShot(q, QStringLiteral("venue-off-prompt"));
        if (askedText != nullptr)
            *askedText = q->text() + QLatin1Char('\n') + q->informativeText();
        if (answer == QLatin1String("cancel"))
            q->button(QMessageBox::Cancel)->click();
        else if (QAbstractButton *b = q->findChild<QAbstractButton *>(answer))
            b->click();
        else
            q->button(QMessageBox::Cancel)->click();
    });
    poll.start();
    const bool ok = chooseFeatures(app, want);
    poll.stop();
    return ok;
}

static const QStringList kFeatureRows = { "Venues", "Power Estimate", "Grand Master", "Speed Masters",
                                          "Engine Load", "Show Review", "Control Surfaces", "Focus Calls" };

static QMap<QString, bool> allFeatures(bool on)
{
    QMap<QString, bool> out;
    for (const QString &f : kFeatureRows)
        out.insert(f, on);
    return out;
}

void AppViews_Test::eachFeatureSwitchHidesWhatItOwns()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    const bool was[4] = { m_app->footerChipShown(App::FooterLoad), m_app->footerChipShown(App::FooterPower),
                          m_app->footerChipShown(App::FooterGrandMaster),
                          m_app->footerChipShown(App::FooterSpeedMasters) };
    auto back = qScopeGuard([this, was]() {
        m_app->setFooterChipShown(App::FooterLoad, was[0]);
        m_app->setFooterChipShown(App::FooterPower, was[1]);
        m_app->setFooterChipShown(App::FooterGrandMaster, was[2]);
        m_app->setFooterChipShown(App::FooterSpeedMasters, was[3]);
        m_app->clearDocument();
        m_app->setTabLabelMode(App::TabIconAndText);
    });
    // Every chip shown on this computer: only the show's switch hides it.
    m_app->setFooterChipShown(App::FooterLoad, true);
    m_app->setFooterChipShown(App::FooterPower, true);
    m_app->setFooterChipShown(App::FooterGrandMaster, true);
    m_app->setFooterChipShown(App::FooterSpeedMasters, true);
    m_app->showContext(QStringLiteral("Lighting Studio"));
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    QComboBox *overlay = nullptr;
    QTRY_VERIFY((overlay = mon->findChild<QComboBox *>("studioOverlay")) != nullptr);
    Scene *scene = new Scene(doc);
    scene->setName("Look");
    QVERIFY(doc->addFunction(scene));
    doc->resetModified();

    // What a feature owns, as the person sees it.
    VenueChip *venueChip = m_app->m_venueChip;
    QAction *viewVenue = m_app->findChild<QAction *>(QStringLiteral("viewVenueAction"));
    QLabel *review = m_app->findChild<QLabel *>("footerReview");
    CueRunner *run = m_app->findChild<CueRunner *>("runTab");
    QVERIFY(venueChip != nullptr && viewVenue != nullptr && review != nullptr && run != nullptr);
    QToolButton *runFocus = run->findChild<QToolButton *>("crFocusCalls");
    QToolButton *runPad = run->findChild<QToolButton *>("crPadRun");
    QVERIFY(runFocus != nullptr && runPad != nullptr);
    // Whether the Lighting Studio's Focus menu offers @p name now.
    auto focusShown = [this, doc](const QString &name) {
        QMenu m;
        FocusMenu::fill(&m, doc, m_app, []() {});
        QAction *a = m.findChild<QAction *>(name);      // a submenu's own action too
        return a != nullptr && a->isVisible();
    };
    auto check = [&](bool on) {
        // Venues
        QCOMPARE(venueChip->isHidden(), !on);
        QCOMPARE(viewVenue->isVisible(), on);
        QCOMPARE(focusShown("focusVenues"), on);
        QCOMPARE(focusShown("focusReport"), on);
        // Grand Master, speed masters, engine load: the footer
        QCOMPARE(m_app->m_statusGrandMasterBox->isHidden(), !on);
        QCOMPARE(m_app->m_statusSpeedMasters->isHidden(), !on);
        QCOMPARE(m_app->m_statusLoadLabel->isHidden(), !on);
        // Power: the plot's overlay (the footer chip waits for an estimate)
        // and the Fixtures tab's Power folder.
        QCOMPARE(overlay->findText("Power") >= 0, on);
        {
            bool folder = false;
            FixtureManager *fm = FixtureManager::instance();
            QVERIFY(fm != nullptr);
            for (FixtureTreeWidget *t : fm->findChildren<FixtureTreeWidget *>())
                for (int i = 0; i < t->topLevelItemCount(); ++i)
                    folder = folder || t->topLevelItem(i)->text(0) == QStringLiteral("Power");
            QCOMPARE(folder, on);
        }
        // Show Review: its chip after a run
        doc->showReview()->run();
        QCOMPARE(review->isHidden(), !on);
        // Control surfaces: the triggers and the encoder bank; Learn Trigger…
        {
            ControlMapPanel panel(doc);
            panel.resize(800, 500);
            panel.show();
            QCOMPARE(panel.findChild<QLabel *>("controlMapEmpty")->isVisibleTo(&panel), on);
            QCOMPARE(panel.findChild<QWidget *>("encoderBankBox")->isVisibleTo(&panel), on);
            QCOMPARE(panel.findChild<QPushButton *>("controlMapRemoveAll")->isVisibleTo(&panel), on);
            QVERIFY(panel.findChild<QPushButton *>("controlMapEmergencyStop")->isVisibleTo(&panel));
            QMenu m;
            QCOMPARE(ControlLearnDialog::addTo(&m, doc, ControlMap::function(scene->id()), m_app) != nullptr, on);
        }
        QCOMPARE(runPad->isHidden(), !on);
        // Focus calls: the Focus menu and the Run tab
        QCOMPARE(focusShown("focusVenue"), on);
        QCOMPARE(focusShown("focusEditCalls"), on);
        QCOMPARE(runFocus->isHidden(), !on);
        // Quick focus has no switch.
        QVERIFY(focusShown("focusQuick"));
    };
    check(true);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(chooseFeatures(m_app, allFeatures(false)));
    QVERIFY(doc->isModified());
    check(false);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(chooseFeatures(m_app, allFeatures(true)));
    check(true);
    doc->resetModified();
}

/* The Tabs & Features dialog, driven as a person drives it, for EVERY
   feature: off, everything it owns put away at once and nothing else
   touched; on again, all back. Then the same with a venue applied (his
   real show), where Venues asks and the chip says why it stays. */
void AppViews_Test::everyFeatureSwitchRespondsThroughTheDialog()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    VenueCorrections *vc = doc->programmer()->venueCorrections();
    QTemporaryDir dir;
    const bool was[4] = { m_app->footerChipShown(App::FooterLoad), m_app->footerChipShown(App::FooterPower),
                          m_app->footerChipShown(App::FooterGrandMaster),
                          m_app->footerChipShown(App::FooterSpeedMasters) };
    auto back = qScopeGuard([this, was, vc]() {
        vc->reset();
        m_app->setFooterChipShown(App::FooterLoad, was[0]);
        m_app->setFooterChipShown(App::FooterPower, was[1]);
        m_app->setFooterChipShown(App::FooterGrandMaster, was[2]);
        m_app->setFooterChipShown(App::FooterSpeedMasters, was[3]);
        m_app->clearDocument();
        m_app->setTabLabelMode(App::TabIconAndText);
    });
    m_app->setFooterChipShown(App::FooterLoad, true);
    m_app->setFooterChipShown(App::FooterPower, true);
    m_app->setFooterChipShown(App::FooterGrandMaster, true);
    m_app->setFooterChipShown(App::FooterSpeedMasters, true);
    m_app->showContext(QStringLiteral("Lighting Studio"));
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    QComboBox *overlay = nullptr;
    QTRY_VERIFY((overlay = mon->findChild<QComboBox *>("studioOverlay")) != nullptr);
    Scene *scene = new Scene(doc);
    scene->setName("Look");
    QVERIFY(doc->addFunction(scene));
    VenueChip *venueChip = m_app->m_venueChip;
    QAction *viewVenue = m_app->findChild<QAction *>(QStringLiteral("viewVenueAction"));
    QLabel *review = m_app->findChild<QLabel *>("footerReview");
    CueRunner *run = m_app->findChild<CueRunner *>("runTab");
    QVERIFY(venueChip != nullptr && viewVenue != nullptr && review != nullptr && run != nullptr);
    QToolButton *runFocus = run->findChild<QToolButton *>("crFocusCalls");
    QToolButton *runPad = run->findChild<QToolButton *>("crPadRun");
    QVERIFY(runFocus != nullptr && runPad != nullptr);
    auto focusShown = [this, doc](const QString &name) {
        QMenu m;
        FocusMenu::fill(&m, doc, m_app, []() {});
        QAction *a = m.findChild<QAction *>(name);
        return a != nullptr && a->isVisible();
    };
    doc->resetModified();

    // Whether what @p row owns is shown now, as the person sees it.
    auto shown = [&](const QString &row) {
        if (row == "Venues")
            return !venueChip->isHidden() && viewVenue->isVisible() && focusShown("focusVenues");
        if (row == "Power Estimate")
        {
            if (overlay->findText("Power") < 0)
                return false;
            FixtureManager *fm = FixtureManager::instance();
            for (FixtureTreeWidget *t : fm->findChildren<FixtureTreeWidget *>())
                for (int i = 0; i < t->topLevelItemCount(); ++i)
                    if (t->topLevelItem(i)->text(0) == QStringLiteral("Power"))
                        return true;
            return false;
        }
        if (row == "Grand Master")
            return !m_app->m_statusGrandMasterBox->isHidden();
        if (row == "Speed Masters")
            return !m_app->m_statusSpeedMasters->isHidden();
        if (row == "Engine Load")
            return !m_app->m_statusLoadLabel->isHidden();
        if (row == "Show Review")
        {
            doc->showReview()->run();
            return !review->isHidden();
        }
        if (row == "Control Surfaces")
        {
            QMenu m;
            return !runPad->isHidden()
                   && ControlLearnDialog::addTo(&m, doc, ControlMap::function(scene->id()), m_app) != nullptr;
        }
        if (row == "Focus Calls")
            return focusShown("focusVenue") && focusShown("focusEditCalls") && !runFocus->isHidden();
        return false;
    };
    // Only @p off (if any) is away; every other feature still shown.
    auto onlyAway = [&](const QString &off, bool venueKept) {
        for (const QString &row : kFeatureRows)
        {
            bool want = row != off;
            if (row == "Venues" && venueKept)
                continue;   // checked on its own
            QVERIFY2(shown(row) == want, qPrintable(row + (want ? " should be shown" : " should be away")
                                                    + " with " + (off.isEmpty() ? "all on" : off + " off")));
        }
    };

    for (int applied = 0; applied < 2; ++applied)
    {
        if (applied == 1)
        {
            vc->setShowFile(dir.filePath("Tour.qlcc"), false);
            QVERIFY(vc->setActiveVenue(vc->addVenue("PMS")));
        }
        onlyAway(QString(), false);
        if (QTest::currentTestFailed())
            return;
        for (const QString &row : kFeatureRows)
        {
            bool asked = false;
            QVERIFY(chooseFeaturesAnswering(m_app, { { row, false } }, "venueOffKeep", &asked));
            // Only Venues asks, and only while a venue is applied.
            QCOMPARE(asked, row == "Venues" && applied == 1);
            QVERIFY2(m_app->doc()->isModified(), qPrintable(row));
            if (row == "Venues" && applied == 1)
            {
                // Kept: the chip stays and says why; the menus are away.
                QVERIFY(!venueChip->isHidden());
                QVERIFY(venueChip->text().contains("Venues off"));
                QVERIFY(venueChip->toolTip().contains("Venues is off for this show"));
                QVERIFY(!viewVenue->isVisible());
                QVERIFY(!focusShown("focusVenues"));
                onlyAway(row, true);
            }
            else
                onlyAway(row, false);
            if (QTest::currentTestFailed())
                return;
            doc->resetModified();
            QVERIFY(chooseFeaturesAnswering(m_app, { { row, true } }, "cancel", &asked));
            QVERIFY(!asked);
            QVERIFY2(doc->isModified(), qPrintable(row));
            onlyAway(QString(), false);
            if (QTest::currentTestFailed())
                return;
            doc->resetModified();
        }
    }
}

/* Turning Venues off while a venue is applied: the dialog says so, then asks. */
void AppViews_Test::turningVenuesOffAsksWhileAVenueIsApplied()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    VenueCorrections *vc = doc->programmer()->venueCorrections();
    QTemporaryDir dir;
    auto back = qScopeGuard([this, vc]() {
        vc->reset();
        m_app->clearDocument();
    });
    VenueChip *chip = m_app->m_venueChip;
    QAction *viewVenue = m_app->findChild<QAction *>(QStringLiteral("viewVenueAction"));
    QVERIFY(chip != nullptr && viewVenue != nullptr);
    vc->setShowFile(dir.filePath("Tour.qlcc"), false);
    const QString name = vc->addVenue("PMS");
    ShowFeatures *sf = ShowFeatures::instance();

    // The dialog's Venues row says the venue is applied; none applied, no note.
    auto noteShown = [this]() {
        bool note = false, seen = false;
        QTimer::singleShot(100, m_app, [&]() {
            QDialog *dlg = m_app->findChild<QDialog *>("chooseTabsDialog");
            if (dlg == nullptr)
                return;
            seen = true;
            QLabel *l = dlg->findChild<QLabel *>("chooseFeaturesVenueNote");
            note = l != nullptr && l->text().contains("PMS") && l->text().contains("applied");
            if (note)
                featureShot(dlg, QStringLiteral("features-dialog-venue-applied"));
            dlg->reject();
        });
        m_app->menuBar()->findChild<QAction *>("viewTabsAction")->trigger();
        return seen && note;
    };
    QVERIFY(!noteShown());
    QVERIFY(vc->setActiveVenue(name));
    QVERIFY(noteShown());

    bool asked = false;
    QString text;
    // Cancel: nothing changes.
    QVERIFY(chooseFeaturesAnswering(m_app, { { "Venues", false } }, "cancel", &asked, &text));
    QVERIFY(asked);
    QVERIFY2(text.contains("PMS is applied to the rig"), qPrintable(text));
    QVERIFY(sf->on(ShowFeatures::Venues));
    QVERIFY(vc->isActive());
    QVERIFY(!chip->isHidden());
    QVERIFY(viewVenue->isVisible());

    // Keep it applied: Venues off, the venue and its chip stay, saying why.
    QVERIFY(chooseFeaturesAnswering(m_app, { { "Venues", false } }, "venueOffKeep", &asked));
    QVERIFY(asked);
    QVERIFY(!sf->on(ShowFeatures::Venues));
    QVERIFY(vc->isActive());
    QVERIFY(!chip->isHidden());
    QCOMPARE(m_app->venueFooterText(), QString("Venue: PMS \302\267 Venues off"));
    QVERIFY(chip->toolTip().contains("Venue: PMS \342\200\224 applied; Venues is off for this show"));
    QVERIFY(!viewVenue->isVisible());
    // On again: back as it was, no question.
    QVERIFY(chooseFeaturesAnswering(m_app, { { "Venues", true } }, "cancel", &asked));
    QVERIFY(!asked);
    QCOMPARE(m_app->venueFooterText(), QString("Venue: PMS"));

    // Clear and turn off: the venue leaves the rig, the chip goes.
    QVERIFY(chooseFeaturesAnswering(m_app, { { "Venues", false } }, "venueOffClear", &asked));
    QVERIFY(asked);
    QVERIFY(!sf->on(ShowFeatures::Venues));
    QVERIFY(!vc->isActive());
    QTRY_VERIFY(chip->isHidden());
    QVERIFY(!viewVenue->isVisible());
    QVERIFY(vc->venueNames().contains(name));       // cleared, not deleted
}

void AppViews_Test::featuresAreSavedWithTheShow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("features.qxw"), plain = dir.filePath("plain.qxw");
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    auto back = qScopeGuard([this]() { m_app->clearDocument(); });
    QAction *viewVenue = m_app->findChild<QAction *>(QStringLiteral("viewVenueAction"));
    QVERIFY(viewVenue != nullptr);
    // Saved with everything on: nothing written (an older show reads the same).
    QCOMPARE(m_app->saveXML(plain), QFile::NoError);
    {
        QFile f(plain);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains("FeaturesOff"));
    }

    QVERIFY(chooseFeatures(m_app, { { "Venues", false }, { "Show Review", false } }));
    QVERIFY(!viewVenue->isVisible());
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray xml = f.readAll();
        QVERIFY2(xml.contains("FeaturesOff=\"Venues|Review\""), xml.left(600).constData());
    }

    m_app->clearDocument();             // a new show: every feature on
    QVERIFY(viewVenue->isVisible());
    QVERIFY(!m_app->m_venueChip->isHidden());
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(!viewVenue->isVisible());
    QVERIFY(m_app->m_venueChip->isHidden());
    m_app->doc()->showReview()->run();
    QVERIFY(m_app->findChild<QLabel *>("footerReview")->isHidden());
    QVERIFY(!m_app->doc()->isModified());

    // The show without the attribute opens with everything on, whatever was before.
    QCOMPARE(m_app->loadXML(plain), QFile::NoError);
    QVERIFY(viewVenue->isVisible());
    QVERIFY(!m_app->m_venueChip->isHidden());
    m_app->doc()->showReview()->run();
    QVERIFY(!m_app->findChild<QLabel *>("footerReview")->isHidden());
}

void AppViews_Test::theEmergencyStopSurvivesEverySwitchOff()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    ControlMap *cm = doc->controlMap();
    auto back = qScopeGuard([this, cm]() {
        cm->removeBinding(ControlMap::emergencyStop());
        m_app->clearDocument();
    });
    QVERIFY(chooseFeatures(m_app, allFeatures(false)));
    // The button under the Control Map, shown and working.
    ControlMapPanel panel(doc);
    panel.resize(800, 500);
    panel.show();
    QPushButton *stop = panel.findChild<QPushButton *>("controlMapEmergencyStop");
    QVERIFY(stop != nullptr);
    QVERIFY(stop->isVisibleTo(&panel));
    QVERIFY(stop->isEnabled());
    // A learned stop still fires ALL OFF: what plays stops.
    Fixture *lamp = new Fixture(doc);
    lamp->setName("Lamp");
    lamp->setChannels(1);
    lamp->setAddress(0);
    QVERIFY(doc->addFixture(lamp));
    Scene *look = new Scene(doc);
    look->setName("Playing");
    look->setValue(lamp->id(), 0, 255);
    QVERIFY(doc->addFunction(look));
    look->start(doc->masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(look->isRunning(), 3000);
    cm->setBinding({ 0, 188, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QSignalSpy fired(cm, &ControlMap::fired);
    cm->handleInput(0, 188, 127);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.first().first().toString(), QString("ALL OFF"));
    QTRY_VERIFY_WITH_TIMEOUT(!look->isRunning(), 3000);
    QVERIFY2(stop->text().startsWith("Emergency Stop: "), qPrintable(stop->text()));
}

/* Control surfaces phase F (CONTROL_SURFACES_EVAL §2, §4 #16, #17): one
   ALL OFF path. The screen's Stop All (its button, its menu item and its
   key), a VC Stop All button, the desk's stop, the pad's four together and
   the phone page all reach ShowControl::allOff(): the run handler sees
   ALL OFF from their route, and the emergency stop is struck (Show-only
   cut, every function stopped). Before, Stop All and the VC button stopped
   the MasterTimer on their own: no ALL OFF, no stop struck. */
void AppViews_Test::everyAllOffTakesTheOnePath()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    ShowControl *sc = doc->showControl();
    OutputRuleKeeper *rules = doc->outputRuleKeeper();
    ControlMap *cm = doc->controlMap();
    PadRun *pad = doc->programmer()->padRun();
    QVERIFY(rules != nullptr && pad != nullptr);
    QWidget host;
    VCButton *vcStop = new VCButton(&host, doc);
    vcStop->setAction(VCButton::StopAll);
    auto back = qScopeGuard([&]() {
        cm->removeBinding(ControlMap::emergencyStop());
        pad->stopEnded();
        rules->noteGo();
        doc->setMode(Doc::Design);
        m_app->clearDocument();
    });
    doc->setMode(Doc::Operate);                     // a VC button presses in Operate
    m_app->hideViewNotice();
    Fixture *lamp = new Fixture(doc);
    lamp->setName("Lamp");
    lamp->setChannels(1);
    lamp->setAddress(0);
    QVERIFY(doc->addFixture(lamp));
    Scene *look = new Scene(doc);
    look->setName("Playing");
    look->setValue(lamp->id(), 0, 255);
    QVERIFY(doc->addFunction(look));
    cm->setBinding({ 0, 189, ControlMap::emergencyStop(), ControlMap::StartOnly });

    QSignalSpy handled(sc, &ShowControl::commandHandled);
    struct Way { const char *name; ShowControl::Route route; std::function<void()> press; };
    int deskPresses = 0;
    const QList<Way> ways = {
        { "Stop All (screen)", ShowControl::RouteKey, [this]() { m_app->m_controlPanicAction->trigger(); } },
        { "VC Stop All button", ShowControl::RouteOther, [vcStop]() { vcStop->pressFunction(); } },
        { "desk stop", ShowControl::RouteControlMap, [cm, &deskPresses]() {
              cm->handleInput(0, 189, 0);       // a new press each time
              cm->handleInput(0, 189, 127);
              ++deskPresses; } },
        { "pad four", ShowControl::RoutePad, [pad]() { pad->emergencyStop(); } },
    };
    for (const Way &w : ways)
    {
        rules->noteGo();                            // the last stop's mark gone
        look->start(doc->masterTimer(), FunctionParent::master());
        QTRY_VERIFY_WITH_TIMEOUT(look->isRunning(), 3000);
        handled.clear();
        const int before = m_app->m_runCommands;
        w.press();
        QVERIFY2(m_app->m_runCommands == before + 1, w.name);
        QVERIFY2(m_app->m_lastRunRoute == w.route, w.name);
        QVERIFY2(!handled.isEmpty() && handled.last().at(0).toString().startsWith("ALL OFF")
                     && handled.last().at(1).toBool(), w.name);
        QVERIFY2(rules->emergencyStopped(), w.name);
        QTRY_VERIFY2_WITH_TIMEOUT(!look->isRunning(), w.name, 3000);
        pad->stopEnded();
    }
    QCOMPARE(deskPresses, 1);
    // A VC Stop All with a fade is a fade and stop, not the emergency stop.
    vcStop->setStopAllFadeOutTime(1000);
    rules->noteGo();
    const int before = m_app->m_runCommands;
    vcStop->pressFunction();
    QCOMPARE(m_app->m_runCommands, before);
    QVERIFY(!rules->emergencyStopped());
}

/* §2 mid-show trouble: Stop All was greyed while no function ran, so a level
   held with nothing running (a Simple Desk level, a held master) had no
   on-screen stop. Now never greyed: nothing running, it is enabled, and a
   press is the emergency stop. */
void AppViews_Test::stopAllIsNeverGreyed()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    auto back = qScopeGuard([this, doc]() {
        doc->outputRuleKeeper()->noteGo();
        m_app->clearDocument();
    });
    QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 3000);
    QVERIFY(m_app->m_controlPanicAction->isEnabled());
    QToolButton *button = m_app->findChild<QToolButton *>(QStringLiteral("showStopAll"));
    QVERIFY(button != nullptr);
    QVERIFY(button->isEnabled());
    // UX-review shots (the pool's QLC_SHOT_DIR): Stop All lit with nothing running.
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!dir.isEmpty())
    {
        QDir().mkpath(dir);
        const QSize was = m_app->size();
        m_app->setWorkspaceView(App::ConsoleView);
        m_app->show();
        for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            m_app->resize(size);
            for (int i = 0; i < 6; ++i)
            {
                QCoreApplication::processEvents();
                QTest::qWait(30);
            }
            QVERIFY(button->isVisible() && button->isEnabled());
            QVERIFY(m_app->grab().save(QStringLiteral("%1/stopall-idle-%2x%3.png")
                                       .arg(dir).arg(size.width()).arg(size.height())));
        }
        m_app->resize(was);
    }
    // A function started and stopped: still enabled after it.
    Fixture *lamp = new Fixture(doc);
    lamp->setName("Lamp");
    lamp->setChannels(1);
    lamp->setAddress(0);
    QVERIFY(doc->addFixture(lamp));
    Scene *look = new Scene(doc);
    look->setName("Brief");
    look->setValue(lamp->id(), 0, 255);
    QVERIFY(doc->addFunction(look));
    look->start(doc->masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(look->isRunning(), 3000);
    look->stop(FunctionParent::master());
    QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 3000);
    QCoreApplication::processEvents();
    QVERIFY(m_app->m_controlPanicAction->isEnabled());
    // Pressed with nothing running: the emergency stop all the same.
    m_app->m_controlPanicAction->trigger();
    QVERIFY(doc->outputRuleKeeper()->emergencyStopped());
}

/* §2 and CONTROL_SURFACES_EVAL's stopAllEnabledWithHeldLevel: what Stop All
   is lit for with nothing running. A Simple Desk channel and a VC Level
   slider up on ordinary dimmers (not Show only) -- no function runs, the
   light is up -- and Stop All takes both out. Before, the stop let go of
   Show-only channels only: both dimmers stayed at full. They stay out
   through later ticks, until their own fader is moved again. */
void AppViews_Test::stopAllTakesOutAHeldLevel()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    OutputRuleKeeper *rules = doc->outputRuleKeeper();
    auto back = qScopeGuard([this, doc, rules]() {
        rules->noteGo();
        doc->setMode(Doc::Design);
        m_app->clearDocument();
    });
    auto dimmer = [doc](const QString &name, quint32 address) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(1);
        f->setAddress(address);
        return doc->addFixture(f) ? f : nullptr;
    };
    Fixture *desk = dimmer("Desk lamp", 10);
    Fixture *fader = dimmer("Fader lamp", 11);
    QVERIFY(desk != nullptr && fader != nullptr);
    QVERIFY((doc->inputOutputMap()->universes().first()->channelCapabilities(10)
             & Universe::ShowOnlyOutput) == 0);
    SimpleDeskEngine eng(doc);
    QWidget host;
    VCSlider slider(&host, doc);
    slider.setSliderMode(VCSlider::Level);
    slider.addLevelChannel(fader->id(), 0);
    doc->setMode(Doc::Operate);
    m_app->hideViewNotice();
    eng.setValue(10, 255);
    slider.setSliderValue(200, false, true);        // pushed up in Operate
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 10)), 255, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 200, 5000);
    QCOMPARE(doc->masterTimer()->runningFunctions(), 0);   // nothing running
    QVERIFY(m_app->m_controlPanicAction->isEnabled());

    m_app->m_controlPanicAction->trigger();
    QVERIFY(rules->emergencyStopped());
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 10)), 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 0, 5000);
    QVERIFY(!eng.hasChannel(10));                   // the desk let go of it
    // Stays out: nothing moved since.
    QTest::qWait(200);
    QCOMPARE(int(outputAt(doc, 10)), 0);
    QCOMPARE(int(outputAt(doc, 11)), 0);
    // A hand on each, after the stop: lit again.
    eng.setValue(10, 120);
    slider.setSliderValue(90, false, true);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 10)), 120, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 90, 5000);

    // A Click & Go pick is a hand on it too (round 2): stopped again, a
    // level picked from its menu lights it, and so does a colour pick.
    // Without the stamp the pick moved the slider on screen and wrote
    // nothing: dark until someone dragged the fader.
    slider.setClickAndGoType(ClickAndGoWidget::Red);
    auto stopAgain = [&]() {
        rules->noteGo();
        m_app->m_controlPanicAction->trigger();
        QVERIFY(rules->emergencyStopped());
        QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 0, 5000);
        QTest::qWait(100);
        QCOMPARE(int(outputAt(doc, 11)), 0);
    };
    stopAgain();
    QVERIFY(QMetaObject::invokeMethod(&slider, "slotClickAndGoLevelChanged",
                                      Qt::DirectConnection, Q_ARG(uchar, uchar(150))));
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 150, 5000);
    stopAgain();
    QVERIFY(QMetaObject::invokeMethod(&slider, "slotClickAndGoLevelAndPresetChanged",
                                      Qt::DirectConnection, Q_ARG(uchar, uchar(170)),
                                      Q_ARG(QImage, QImage())));
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 170, 5000);
    stopAgain();
    QVERIFY(QMetaObject::invokeMethod(&slider, "slotClickAndGoColorChanged",
                                      Qt::DirectConnection, Q_ARG(QRgb, qRgb(255, 0, 0))));
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 11)), 128, 5000);
}

/* §4 #16: no Ctrl+Shift+Esc on Windows (it opens the Task Manager there,
   and Stop All never runs). The Mac keeps ⌘⇧Esc. The menu, the tooltip
   and the help say this computer's key. */
void AppViews_Test::theStopAllKeyIsSafeOnEveryPlatform()
{
    const QKeySequence taskManager(QStringLiteral("CTRL+SHIFT+ESC"));
    QCOMPARE(App::stopAllShortcut(true), taskManager);
    QVERIFY(App::stopAllShortcut(false) != taskManager);
    QVERIFY(!App::stopAllShortcut(false).isEmpty());
    QCOMPARE(App::stopAllShortcut(false), QKeySequence(QStringLiteral("CTRL+SHIFT+Backspace")));
    QCOMPARE(m_app->m_controlPanicAction->shortcut(), App::stopAllShortcut());
#if !defined(Q_OS_MACOS)
    QVERIFY(m_app->m_controlPanicAction->shortcut() != taskManager);
#endif
    QVERIFY2(m_app->m_controlPanicAction->toolTip().contains(
                 App::stopAllShortcut().toString(QKeySequence::NativeText)),
             qPrintable(m_app->m_controlPanicAction->toolTip()));
    // The Run tab's Stop names this computer's key too, never the Mac's
    // chord on Windows (read there as Ctrl+Shift+Esc: the Task Manager).
    {
        CueRunner run(m_app->doc(), CueRunner::Full);
        QToolButton *stop = run.findChild<QToolButton *>(QStringLiteral("crStop"));
        QVERIFY(stop != nullptr);
        QVERIFY2(stop->toolTip().contains(App::stopAllShortcut().toString(QKeySequence::NativeText)),
                 qPrintable(stop->toolTip()));
#if !defined(Q_OS_MACOS)
        QVERIFY2(!stop->toolTip().contains(QStringLiteral("Esc")), qPrintable(stop->toolTip()));
#endif
    }
    // No other action of the app's takes the key.
    int holders = 0;
    for (QAction *a : m_app->findChildren<QAction *>())
        if (a->shortcuts().contains(App::stopAllShortcut()))
            ++holders;
    QCOMPARE(holders, 1);
    // No text-editing key of this platform's is it (on X11 the Delete
    // Complete Line key is Ctrl+U, Qt 6.8's table).
    for (int k = QKeySequence::UnknownKey + 1; k <= QKeySequence::Cancel; ++k)
        QVERIFY2(!QKeySequence::keyBindings(QKeySequence::StandardKey(k)).contains(App::stopAllShortcut()),
                 qPrintable(QStringLiteral("standard key %1").arg(k)));

    /* The real path: the key typed through the window with a text field
       focused (a cue name, a level, a note) -- the field's ShortcutOverride
       sees it first, and one that took it would eat the e-stop. Each one
       fires Stop All, and the field keeps its text. */
    Doc *doc = m_app->doc();
    OutputRuleKeeper *rules = doc->outputRuleKeeper();
    QWidget *page = new QWidget(m_app);
    auto back = qScopeGuard([page, rules]() {
        delete page;
        rules->noteGo();
    });
    QLineEdit *line = new QLineEdit(QStringLiteral("SS-01.1 Opening"), page);
    QSpinBox *spin = new QSpinBox(page);
    spin->setRange(0, 255);
    spin->setValue(128);
    QPlainTextEdit *plain = new QPlainTextEdit(QStringLiteral("a note"), page);
    QTextEdit *rich = new QTextEdit(QStringLiteral("rich note"), page);
    QVBoxLayout *lay = new QVBoxLayout(page);
    for (QWidget *w : std::initializer_list<QWidget *>{ line, spin, plain, rich })
        lay->addWidget(w);
    page->setGeometry(0, 0, 300, 300);
    page->show();
    page->raise();
    m_app->show();
    m_app->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(m_app));
    const struct { const char *name; QWidget *field; } fields[] = {
        { "line edit", line }, { "spin box", spin }, { "plain text", plain }, { "rich text", rich } };
    for (const auto &f : fields)
    {
        rules->noteGo();
        QVERIFY(!rules->emergencyStopped());
        f.field->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY2_WITH_TIMEOUT(f.field->hasFocus() || f.field->isAncestorOf(QApplication::focusWidget()),
                                  f.name, 2000);
        const int before = m_app->m_runCommands;
        QTest::keySequence(QApplication::focusWidget(), App::stopAllShortcut());
        QVERIFY2(m_app->m_runCommands == before + 1, f.name);
        QVERIFY2(rules->emergencyStopped(), f.name);
    }
    QCOMPARE(line->text(), QStringLiteral("SS-01.1 Opening"));
    QCOMPARE(plain->toPlainText(), QStringLiteral("a note"));
}

/* Review of phase F part 2: Stop All lets go of what the Simple Desk and a
   VC Level slider hold -- but never of pan or tilt (coarse or fine). Let
   go of, the channel fell to its definition's default (0, one end of
   travel) and the head swung across the stage: the rule Stop ALL keeps
   for every head (FixtureRest::putAllAtRest). The dimmer goes out. */
void AppViews_Test::stopAllLeavesAHeldHeadWhereItIs()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    OutputRuleKeeper *rules = doc->outputRuleKeeper();
    auto back = qScopeGuard([this, doc, rules]() {
        rules->noteGo();
        doc->setMode(Doc::Design);
        m_app->clearDocument();
    });
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(QStringLiteral("Test"), QStringLiteral("Held Mover"));
    if (def == nullptr)
    {
        def = new QLCFixtureDef();
        def->setManufacturer(QStringLiteral("Test"));
        def->setModel(QStringLiteral("Held Mover"));
        def->setType(QLCFixtureDef::MovingHead);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName(QStringLiteral("5ch"));
        const struct { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; } chans[] = {
            { "Pan", QLCChannel::Pan, QLCChannel::MSB }, { "Pan fine", QLCChannel::Pan, QLCChannel::LSB },
            { "Tilt", QLCChannel::Tilt, QLCChannel::MSB }, { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB },
            { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB } };
        int i = 0;
        for (const auto &c : chans)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString::fromLatin1(c.name));
            ch->setGroup(c.group);
            ch->setControlByte(c.byte);
            def->addChannel(ch);
            mode->insertChannel(ch, i++);
        }
        def->addMode(mode);
        QVERIFY(doc->fixtureDefCache()->addFixtureDef(def));
    }
    auto mover = [doc, def](const QString &name, quint32 address) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setFixtureDefinition(def, def->modes().first());
        f->setUniverse(0);
        f->setAddress(address);
        return doc->addFixture(f) ? f : nullptr;
    };
    Fixture *deskHead = mover("Desk head", 20);      // 20..24
    Fixture *sliderHead = mover("Slider head", 30);  // 30..34
    QVERIFY(deskHead != nullptr && sliderHead != nullptr);
    QVERIFY((doc->inputOutputMap()->universes().first()->channelCapabilities(20)
             & Universe::ShowOnlyOutput) == 0);

    SimpleDeskEngine eng(doc);
    QWidget host;
    VCSlider slider(&host, doc);
    slider.setSliderMode(VCSlider::Level);
    for (quint32 ch = 0; ch < 5; ++ch)
        slider.addLevelChannel(sliderHead->id(), ch);
    doc->setMode(Doc::Operate);
    m_app->hideViewNotice();
    eng.setValue(20, 128);
    eng.setValue(21, 64);
    eng.setValue(22, 100);
    eng.setValue(23, 32);
    eng.setValue(24, 255);
    slider.setSliderValue(128, false, true);         // pushed up in Operate
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 24)), 255, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 20)), 128, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 30)), 128, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 34)), 128, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 32)), 128, 5000);
    int sliderHeld[4];
    for (int i = 0; i < 4; ++i)
        sliderHeld[i] = int(outputAt(doc, quint32(30 + i)));

    m_app->m_controlPanicAction->trigger();
    QVERIFY(rules->emergencyStopped());
    // The dimmers go out...
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 24)), 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 34)), 0, 5000);
    QVERIFY(!eng.hasChannel(24));
    // ...the heads stay where they were, through later ticks too.
    QTest::qWait(250);
    QCOMPARE(int(outputAt(doc, 20)), 128);
    QCOMPARE(int(outputAt(doc, 21)), 64);
    QCOMPARE(int(outputAt(doc, 22)), 100);
    QCOMPARE(int(outputAt(doc, 23)), 32);
    for (int i = 0; i < 4; ++i)
        QVERIFY2(int(outputAt(doc, quint32(30 + i))) == sliderHeld[i],
                 qPrintable(QStringLiteral("slider head channel %1: %2, was %3")
                            .arg(30 + i).arg(int(outputAt(doc, quint32(30 + i)))).arg(sliderHeld[i])));
    QCOMPARE(sliderHeld[0], 128);
    QVERIFY(eng.hasChannel(20) && eng.hasChannel(23));   // still the desk's
    // Moved again after the stop: the slider drives the head and the dimmer.
    slider.setSliderValue(90, false, true);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 30)), 90, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(int(outputAt(doc, 34)), 90, 5000);
}

/* §4 #16, decision 5: the desk's STOP is heard before any feature switch,
   mode, place or learn: Control Surfaces off, a learn listening, in Design
   and in Operate, in an otherwise empty show -- each press is ALL OFF. */
void AppViews_Test::theDeskStopGoesFirstPastEveryCheck()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    ControlMap *cm = doc->controlMap();
    OutputRuleKeeper *rules = doc->outputRuleKeeper();
    auto back = qScopeGuard([this, cm, rules, doc]() {
        cm->stopListening();
        cm->removeBinding(ControlMap::emergencyStop());
        rules->noteGo();
        doc->setMode(Doc::Design);
        QVERIFY(chooseFeatures(m_app, allFeatures(true)));
        m_app->clearDocument();
    });
    cm->setBinding({ 0, 190, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QVERIFY(chooseFeatures(m_app, allFeatures(false)));
    QVERIFY(!ShowFeatures::isOn(ShowFeatures::ControlSurfaces));
    QSignalSpy fired(cm, &ControlMap::fired);
    const Doc::Mode modes[] = { Doc::Design, Doc::Operate };
    for (Doc::Mode mode : modes)
    {
        doc->setMode(mode);
        for (int learning = 0; learning < 2; ++learning)
        {
            if (learning)
                cm->listen();
            rules->noteGo();
            const int before = m_app->m_runCommands;
            cm->handleInput(0, 190, 0);
            cm->handleInput(0, 190, 127);
            const QString what = QString("%1, %2").arg(mode == Doc::Operate ? "Operate" : "Design",
                                                       learning ? "a learn listening" : "no learn");
            QVERIFY2(m_app->m_runCommands == before + 1, qPrintable(what));
            QVERIFY2(m_app->m_lastRunRoute == ShowControl::RouteControlMap, qPrintable(what));
            QVERIFY2(rules->emergencyStopped(), qPrintable(what));
            cm->stopListening();
        }
    }
    QCOMPARE(fired.count(), 4);
}

void AppViews_Test::aHeldMasterIsSaidWithItsChipPutAway()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    InputOutputMap *iom = doc->inputOutputMap();
    SpeedMasters *sm = doc->speedMasters();
    const bool wasGM = m_app->footerChipShown(App::FooterGrandMaster);
    const bool wasSpeed = m_app->footerChipShown(App::FooterSpeedMasters);
    auto back = qScopeGuard([=]() {
        iom->setGrandMasterValue(255);
        if (sm->count() > 0)
            sm->setRate(0, 1.0);
        m_app->setFooterChipShown(App::FooterGrandMaster, wasGM);
        m_app->setFooterChipShown(App::FooterSpeedMasters, wasSpeed);
        m_app->clearDocument();
    });
    m_app->setFooterChipShown(App::FooterGrandMaster, true);
    m_app->setFooterChipShown(App::FooterSpeedMasters, true);
    QLabel *held = m_app->findChild<QLabel *>("footerMastersHeld");
    QVERIFY(held != nullptr);
    QVERIFY(held->isHidden());

    // The Grand Master at 30% with its feature off: said, with the value.
    iom->setGrandMasterValue(77);
    QTRY_COMPARE(m_app->m_statusGrandMasterSlider->value(), 77);
    QVERIFY(held->isHidden());                          // its own chip says it
    QVERIFY(chooseFeatures(m_app, { { "Grand Master", false }, { "Speed Masters", false } }));
    QVERIFY(m_app->m_statusGrandMasterBox->isHidden());
    QVERIFY(!held->isHidden());
    QVERIFY2(held->text().contains("GM 30%"), qPrintable(held->text()));
    iom->setGrandMasterValue(255);
    QTRY_VERIFY(held->isHidden());
    // The footer setting off says it the same.
    QVERIFY(chooseFeatures(m_app, allFeatures(true)));
    m_app->setFooterChipShown(App::FooterGrandMaster, false);
    iom->setGrandMasterValue(77);
    QTRY_VERIFY(!held->isHidden());
    QVERIFY(held->text().contains("GM 30%"));
    iom->setGrandMasterValue(255);
    QTRY_VERIFY(held->isHidden());

    // A speed master off its tempo, its chip put away.
    if (sm->count() < 1)
        sm->setCount(1);
    QVERIFY(chooseFeatures(m_app, { { "Speed Masters", false } }));
    sm->setRate(0, 0.5);
    QTRY_VERIFY(!held->isHidden());
    QVERIFY2(held->text().contains(QString::fromUtf8("\u00d70.50")), qPrintable(held->text()));
    sm->setRate(0, 1.0);
    QTRY_VERIFY(held->isHidden());
}

void AppViews_Test::anAppliedVenueIsSaidWithVenuesOff()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    VenueCorrections *vc = m_app->m_doc->programmer()->venueCorrections();
    QTemporaryDir dir;
    auto back = qScopeGuard([this, vc]() {
        vc->reset();
        m_app->clearDocument();
    });
    VenueChip *chip = m_app->m_venueChip;
    QVERIFY(chip != nullptr);
    QVERIFY(chooseFeatures(m_app, { { "Venues", false } }));
    QVERIFY(chip->isHidden());                          // no venue: put away
    vc->setShowFile(dir.filePath("Tour.qlcc"), false);
    QVERIFY(vc->setActiveVenue(vc->addVenue("Lincoln HS")));
    // On the rig: said whatever the switch.
    QTRY_VERIFY(!chip->isHidden());
    QCOMPARE(m_app->venueFooterText(), QString("Venue: Lincoln HS \302\267 Venues off"));
    QVERIFY(chip->toolTip().contains("Venues is off for this show"));
    QVERIFY(vc->setActiveVenue(QString()));
    QTRY_VERIFY(chip->isHidden());
}

void AppViews_Test::thePadButtonStaysWhileThePadHasAState()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    PadRun *pad = m_app->doc()->programmer()->padRun();
    auto back = qScopeGuard([this, pad]() {
        pad->stopEnded();
        pad->setEnabled(false);
        m_app->clearDocument();
    });
    CueRunner *run = m_app->findChild<CueRunner *>("runTab");
    QVERIFY(run != nullptr);
    QToolButton *btn = run->findChild<QToolButton *>("crPadRun");
    QVERIFY(btn != nullptr);
    QVERIFY(chooseFeatures(m_app, { { "Control Surfaces", false } }));
    QCOMPARE(pad->state(), PadRun::Tools);
    QVERIFY(btn->isHidden());
    // The pad running the show, then the emergency stop: Stopped, said on the button.
    pad->setEnabled(true);
    QVERIFY(!btn->isHidden());
    pad->emergencyStop();
    QTRY_COMPARE(pad->state(), PadRun::Stopped);
    QVERIFY(!btn->isHidden());
    QVERIFY2(btn->text().contains("STOP", Qt::CaseInsensitive), qPrintable(btn->text()));
    pad->stopEnded();
    pad->setEnabled(false);
    QTRY_VERIFY(btn->isHidden());
}

void AppViews_Test::classicCanStillTurnFeaturesBackOn()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    auto back = qScopeGuard([this]() {
        m_app->setWorkspaceView(App::ConsoleView);
        m_app->clearDocument();
        m_app->setTabLabelMode(App::TabIconAndText);
    });
    QVERIFY(chooseFeatures(m_app, { { "Venues", false } }));
    m_app->setWorkspaceView(App::ClassicView);
    QAction *act = m_app->menuBar()->findChild<QAction *>("viewTabsAction");
    QVERIFY(act != nullptr && act->isEnabled());
    // The page opens: the tabs greyed, the features usable.
    bool tabsGreyed = false;
    QTimer::singleShot(300, m_app, [&]() {
        QDialog *dlg = m_app->findChild<QDialog *>("chooseTabsDialog");
        if (dlg == nullptr)
            return;
        QListWidget *tabs = dlg->findChild<QListWidget *>("chooseTabsList");
        tabsGreyed = tabs != nullptr && !tabs->isEnabled();
        dlg->reject();
    });
    act->trigger();
    QVERIFY(tabsGreyed);
    QVERIFY(chooseFeatures(m_app, { { "Venues", true } }));
    QVERIFY(m_app->findChild<QAction *>(QStringLiteral("viewVenueAction"))->isVisible());
    QCOMPARE(barTexts(m_app), QStringList({ "Fixtures", "Functions", "Shows", "Virtual Console",
                                            "Simple Desk", "Inputs/Outputs" }));
    // The footer's right-click: only what Classic's footer shows.
    QMenu *menu = rightClickFooter(m_app);
    QVERIFY(menu != nullptr);
    QStringList names;
    for (QAction *a : menu->actions())
        if (a->objectName().startsWith("footerChip") && a->objectName() != "footerChipFeatures")
            names << a->objectName();
    menu->close();
    QCOMPARE(names, QStringList({ "footerChipGrandMaster" }));
}

void AppViews_Test::theFollowSpotAppearsOnceTheShowHasOne()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    Doc *doc = m_app->doc();
    m_app->slotRigOverview();
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr && mon->m_overviewDlg != nullptr);
    QPointer<QDockWidget> dock = mon->m_overviewDlg;
    auto back = qScopeGuard([this, dock]() {
        if (!dock.isNull())
            dock->close();
        m_app->clearDocument();
    });
    QComboBox *view = mon->m_overviewDlg->findChild<QComboBox *>("rigView");
    QVERIFY(view != nullptr);
    QVERIFY(view->findText("Angled") >= 0 || view->count() > 0);
    // No follow spot in the show: no operator views.
    QCOMPARE(view->findText("Operator"), -1);
    QCOMPARE(view->findText("Operator plan"), -1);

    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, doc);
    follow->setName("Follow");
    follow->setScriptPath(QStringLiteral("effectscripts/followspot.js"));
    QVERIFY(doc->addPalette(follow));
    // A palette alone is not a follow spot the show uses.
    QTest::qWait(400);
    QCOMPARE(view->findText("Operator"), -1);
    Scene *scene = new Scene(doc);
    scene->setName("Spot");
    scene->addPalette(follow->id());
    QVERIFY(doc->addFunction(scene));
    QTRY_VERIFY(view->findText("Operator") >= 0);       // once the burst of edits is over
    QVERIFY(view->findText("Operator plan") >= 0);
    view->setCurrentIndex(view->findText("Operator plan"));

    // Taken away: gone, and the view goes back to a plane.
    QVERIFY(doc->deleteFunction(scene->id()));
    QTRY_COMPARE(view->findText("Operator"), -1);
    QCOMPARE(view->findText("Operator plan"), -1);
    QVERIFY(view->currentText() != QStringLiteral("Operator plan"));
}

/* The footer's right-click menu, as a person opens it: a right-click on
   the footer away from any chip. */
static QMenu *rightClickFooter(App *app)
{
    QStatusBar *sb = app->statusBar();
    const QPoint at(sb->width() / 2, sb->height() / 2);
    QContextMenuEvent ev(QContextMenuEvent::Mouse, at, sb->mapToGlobal(at));
    QApplication::sendEvent(sb, &ev);
    for (QWidget *w : QApplication::topLevelWidgets())
        if (QMenu *m = qobject_cast<QMenu *>(w))
            if (m->objectName() == QStringLiteral("footerChipMenu") && m->isVisible())
                return m;
    return nullptr;
}

void AppViews_Test::rightClickingTheFooterTogglesAChip()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    const bool wasGM = m_app->footerChipShown(App::FooterGrandMaster);
    auto back = qScopeGuard([this, wasGM]() {
        m_app->setFooterChipShown(App::FooterGrandMaster, wasGM);
        m_app->clearDocument();
    });
    m_app->setFooterChipShown(App::FooterGrandMaster, true);
    QVERIFY(!m_app->m_statusGrandMasterBox->isHidden());

    QMenu *menu = rightClickFooter(m_app);
    QVERIFY(menu != nullptr);
    QAction *gm = menu->findChild<QAction *>("footerChipGrandMaster");
    if (gm == nullptr)
        for (QAction *a : menu->actions())
            if (a->objectName() == QStringLiteral("footerChipGrandMaster"))
                gm = a;
    QVERIFY(gm != nullptr);
    QVERIFY(gm->isCheckable() && gm->isChecked() && gm->isEnabled());
    gm->trigger();
    menu->close();
    QVERIFY(m_app->m_statusGrandMasterBox->isHidden());
    QCOMPARE(QSettings().value("workspace/showFooterGM").toBool(), false);
    {
        // Settings says the same, and setting it there is the footer's choice too.
        AppSettings dlg(m_app);
        QCheckBox *box = dlg.findChild<QCheckBox *>("settingsFooterGrandMaster");
        QVERIFY(box != nullptr);
        QVERIFY(!box->isChecked());
        QVERIFY(box->isEnabled());
        box->setChecked(true);
        dlg.accept();
    }
    QVERIFY(!m_app->m_statusGrandMasterBox->isHidden());
    menu = rightClickFooter(m_app);
    QVERIFY(menu != nullptr);
    for (QAction *a : menu->actions())
        if (a->objectName() == QStringLiteral("footerChipGrandMaster"))
            QVERIFY(a->isChecked());
    menu->close();

    // Off for the show: greyed in both, saying where it is turned on.
    QVERIFY(chooseFeatures(m_app, { { "Grand Master", false } }));
    QVERIFY(m_app->m_statusGrandMasterBox->isHidden());
    menu = rightClickFooter(m_app);
    QVERIFY(menu != nullptr);
    bool seen = false;
    for (QAction *a : menu->actions())
        if (a->objectName() == QStringLiteral("footerChipGrandMaster"))
        {
            seen = true;
            QVERIFY(!a->isEnabled());
            QVERIFY2(a->toolTip().contains("Tabs & Features"), qPrintable(a->toolTip()));
        }
    QVERIFY(seen);
    menu->close();
    {
        AppSettings dlg(m_app);
        QCheckBox *box = dlg.findChild<QCheckBox *>("settingsFooterGrandMaster");
        QVERIFY(box != nullptr && !box->isEnabled());
        dlg.reject();
    }
}

void AppViews_Test::tabsAndFeaturesShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    const QSize was = m_app->size();
    auto back = qScopeGuard([this, was]() {
        m_app->resize(was);
        m_app->clearDocument();
    });
    m_app->showContext(QStringLiteral("Show Tree"));
    auto settle = []() {
        for (int i = 0; i < 6; ++i)
        {
            QCoreApplication::processEvents();
            QTest::qWait(30);
        }
    };
    for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        m_app->resize(size);
        settle();
        const QString tag = QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
        // The page over the window, as it is seen.
        QPixmap page, tip;
        QPoint tipAt;
        QTimer::singleShot(200, m_app, [&]() {
            if (QDialog *dlg = m_app->findChild<QDialog *>("chooseTabsDialog"))
            {
                settle();
                page = dlg->grab();
                // A feature row's hover: what turning it off puts away.
                if (QListWidget *features = dlg->findChild<QListWidget *>("chooseFeaturesList"))
                {
                    QListWidgetItem *it = features->item(qMin(1, features->count() - 1));
                    const QRect r = features->visualItemRect(it);
                    const QPoint at = features->viewport()->mapToGlobal(r.center());
                    QToolTip::showText(at, it->toolTip(), features->viewport(), r);
                    settle();
                    for (QWidget *w : QApplication::topLevelWidgets())
                        if (w->inherits("QTipLabel") && w->isVisible())
                        {
                            tip = w->grab();
                            tipAt = features->viewport()->mapTo(dlg, r.center()) + QPoint(0, 12);
                        }
                    QToolTip::hideText();
                }
                dlg->reject();
            }
        });
        m_app->menuBar()->findChild<QAction *>("viewTabsAction")->trigger();
        QPixmap w = m_app->grab();
        const QPoint pageAt((w.width() - page.width()) / 2, qMax(0, (w.height() - page.height()) / 2));
        if (!page.isNull())
        {
            QPainter p(&w);
            p.drawPixmap(pageAt, page);
        }
        QVERIFY(w.save(QStringLiteral("%1/tabs-features-%2.png").arg(dir, tag)));
        // ...and a feature row's hover over it, whole, as it is seen.
        if (!tip.isNull())
        {
            QPainter p(&w);
            const QPoint at = pageAt + tipAt;
            p.drawPixmap(qMax(0, qMin(at.x(), w.width() - tip.width())),
                         qMin(at.y(), w.height() - tip.height()), tip);
            p.end();
            QVERIFY(w.save(QStringLiteral("%1/tabs-features-hover-%2.png").arg(dir, tag)));
        }
        // The footer's right-click, over the window's foot.
        QPixmap foot = m_app->grab();
        if (QMenu *menu = rightClickFooter(m_app))
        {
            settle();
            const QPixmap m = menu->grab();
            menu->close();
            QPainter p(&foot);
            const int y = foot.height() - m_app->statusBar()->height() - m.height();
            p.drawPixmap(foot.width() / 2, qMax(0, y), m);
        }
        QVERIFY(foot.save(QStringLiteral("%1/footer-menu-%2.png").arg(dir, tag)));
    }
}

void AppViews_Test::simpleDeskOpensAndClosesOnDemand()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ConsoleView);
    QTabWidget *tabs = m_app->m_tab;
    QVERIFY(!barTexts(m_app).contains("Simple Desk"));
    QAction *desk = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == "Simple Desk")
            desk = a;
    QVERIFY(desk != nullptr);
    QCOMPARE(desk->shortcut(), QKeySequence("CTRL+SHIFT+D"));
    desk->trigger();
    QVERIFY(barTexts(m_app).contains("Simple Desk"));
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(SimpleDesk::instance()));
    const int at = tabs->indexOf(SimpleDesk::instance());
    // Only it has a close button, and it closes it.
    QVERIFY(tabs->tabBar()->tabButton(at, QTabBar::RightSide) != nullptr);
    QVERIFY(tabs->tabBar()->tabButton(0, QTabBar::RightSide) == nullptr);
    {
        QWidget *x = tabs->tabBar()->tabButton(at, QTabBar::RightSide);
        QTest::mouseClick(x, Qt::LeftButton, Qt::NoModifier, x->rect().center());
    }
    QVERIFY(!barTexts(m_app).contains("Simple Desk"));
    QCOMPARE(barTexts(m_app).size(), 7);
    m_app->setTabLabelMode(App::TabIconAndText);
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
    QCOMPARE(m_app->m_viewNoticeButton->text(), QString("Switch to the qlcconsole View"));
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
    m_app->clearDocument();
}

void AppViews_Test::aShowTreeShowSaysSoInClassic()
{
    m_app->clearDocument();
    m_app->setWorkspaceView(App::ClassicView, false);
    QWidget *notice = m_app->m_statusShowTreeNotice;
    QPushButton *button = m_app->m_statusShowTreeButton;
    QVERIFY(notice != nullptr);
    QVERIFY(button != nullptr);
    QToolButton *close = notice->findChild<QToolButton *>("footerShowTreeNoticeClose");
    QVERIFY(close != nullptr);
    QLabel *text = notice->findChild<QLabel *>("footerShowTreeNoticeText");
    QVERIFY(text != nullptr);
    QCOMPARE(text->text(), QString("This show uses the Show Tree. Open the qlcconsole view to edit it."));
    QCOMPARE(button->text(), QString("Switch to the qlcconsole View"));

    // Classic stays stock QLC+: no Show Tree or Run tab.
    const QStringList classicTabs = App::workspaceViewTabs(App::ClassicView);
    QVERIFY(!classicTabs.contains("Show Tree"));
    QVERIFY(!classicTabs.contains("Run"));
    QVERIFY(!classicTabs.contains("Build"));

    // A plain QLC+ console: nothing to say.
    VirtualConsole *vc = VirtualConsole::instance();
    VCFrame *contents = vc->contents();
    Scene *plain = new Scene(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(plain));
    VCButton *button1 = new VCButton(contents, m_app->doc());
    vc->placeWidget(button1, contents);
    button1->setFunction(plain->id());
    m_app->updateShowTreeNotice();
    QVERIFY(notice->isHidden());
    QVERIFY(!vc->usesShowTree());

    // A button on a Show Tree cue list: said, in Classic only.
    ShowTreeController *st = m_app->doc()->showTree();
    const quint32 prod = st->addProduction("Spring Show");
    const quint32 list = st->addCueList(prod, "Act 1");
    st->addCue(list, -1, "Preset");
    st->addCue(list, 0, "Lights up");
    const quint32 chaserId = st->cueList(list)->chaser;
    QVERIFY(st->isGenerated(chaserId));
    VCButton *button2 = new VCButton(contents, m_app->doc());
    vc->placeWidget(button2, contents);
    button2->setFunction(chaserId);
    QVERIFY(vc->usesShowTree());
    m_app->updateShowTreeNotice();
    QVERIFY(!notice->isHidden());
    m_app->setWorkspaceView(App::EverythingView, false);
    QVERIFY(notice->isHidden());
    m_app->setWorkspaceView(App::ClassicView, false);
    QVERIFY(!notice->isHidden());

    // Saved and opened again: said on opening.
    QTemporaryDir dir;
    const QString path = dir.filePath("treeinclassic.qxw");
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    m_app->clearDocument();
    QVERIFY(notice->isHidden());            // a new show: qlcconsole, no content
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QVERIFY(!notice->isHidden());

    // ×: put away for this opening, through view changes ...
    close->click();
    QVERIFY(notice->isHidden());
    m_app->setWorkspaceView(App::ConsoleView, false);
    m_app->setWorkspaceView(App::ClassicView, false);
    QVERIFY(notice->isHidden());
    // ... and back the next time the show is opened.
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(!notice->isHidden());

    // Without the buttons, a Cue Runner alone says it too.
    st = m_app->doc()->showTree();
    foreach (VCWidget *w, vc->contents()->findChildren<VCButton *>())
        vc->removeWidget(w);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    m_app->updateShowTreeNotice();
    QVERIFY(notice->isHidden());
    VCCueRunner *runner = new VCCueRunner(vc->contents(), m_app->doc());
    vc->placeWidget(runner, vc->contents());
    quint32 loadedList = ShowTree::invalidId();
    for (const ShowTree::Production &p : st->productions())
        for (const ShowTree::CueList &l : p.lists)
            if (l.name == "Act 1")
                loadedList = l.id;
    QVERIFY(loadedList != ShowTree::invalidId());
    runner->setFollow(loadedList);
    m_app->updateShowTreeNotice();
    QVERIFY(!notice->isHidden());

    // It still plays in Classic: GO runs the list's chaser.
    Chaser *ch = runner->runner()->chaser();
    QVERIFY(ch != nullptr);
    QVERIFY(!ch->isRunning());
    runner->trigger(VCCueRunner::Go);
    QTRY_VERIFY(ch->isRunning());
    runner->trigger(VCCueRunner::Stop);
    QTRY_VERIFY(!ch->isRunning());
    m_app->hideViewNotice();               // GO in Design offers Operate

    // The button: the qlcconsole view, and the line goes.
    button->click();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QVERIFY(notice->isHidden());
    QVERIFY(m_app->tabInView("Show Tree"));

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

void AppViews_Test::theUniverseListMirrorsEachShowOpened()
{
    QTemporaryDir dir;
    InputOutputMap *io = m_app->doc()->inputOutputMap();
    auto build = [&](const QString &file, int universes, const QString &tag) {
        m_app->clearDocument();
        while (int(io->universesCount()) < universes)
            QVERIFY(io->addUniverse());
        while (int(io->universesCount()) > universes)
            QVERIFY(io->removeUniverse(int(io->universesCount()) - 1));
        for (int i = 0; i < universes; i++)
            io->setUniverseName(i, QStringLiteral("%1 %2").arg(tag).arg(i + 1));
        QCOMPARE(m_app->saveXML(dir.filePath(file)), QFile::NoError);
    };
    build(QStringLiteral("six.qxw"), 6, QStringLiteral("Six"));
    build(QStringLiteral("three.qxw"), 3, QStringLiteral("Three"));

    InputOutputManager *mgr = InputOutputManager::instance();
    QVERIFY(mgr != nullptr);
    QListWidget *list = mgr->m_list;

    auto open = [&](const QString &file, int universes, const QString &tag) {
        m_app->clearDocument();
        QCOMPARE(m_app->loadXML(dir.filePath(file)), QFile::NoError);
        QCOMPARE(int(io->universesCount()), universes);
        QCOMPARE(list->count(), universes);
        for (int i = 0; i < universes; i++)
        {
            QCOMPARE(list->item(i)->data(Qt::UserRole).toUInt(), quint32(i));
            QCOMPARE(list->item(i)->data(Qt::DisplayRole).toString(),
                     QStringLiteral("%1 %2").arg(tag).arg(i + 1));
        }
    };
    // Bigger, smaller, the same again: never a row more than the show has.
    for (int round = 0; round < 3; round++)
    {
        open(QStringLiteral("six.qxw"), 6, QStringLiteral("Six"));
        open(QStringLiteral("three.qxw"), 3, QStringLiteral("Three"));
    }

    // Renaming the last row renames the last universe, and only it.
    open(QStringLiteral("six.qxw"), 6, QStringLiteral("Six"));
    list->setCurrentRow(list->count() - 1);
    mgr->m_uniNameEdit->setText(QStringLiteral("Renamed"));
    QCOMPARE(io->getUniverseNameByIndex(5), QStringLiteral("Renamed"));
    QCOMPARE(list->item(5)->data(Qt::DisplayRole).toString(), QStringLiteral("Renamed"));
    for (int i = 0; i < 5; i++)
        QCOMPARE(io->getUniverseNameByIndex(i), QStringLiteral("Six %1").arg(i + 1));

    // A universe added and taken away keeps the list in step too.
    QVERIFY(io->addUniverse());
    QCOMPARE(list->count(), 7);
    QVERIFY(io->removeUniverse(6));
    QCOMPARE(list->count(), 6);

    m_app->clearDocument();
    m_app->doc()->resetModified();
}

/* 0.3.1 (k) 1: QLC+ 4 users save for QLC+ 4 from Save As, not only a
   .qlcc. The type list, the type it starts on, the extension it gives, and
   a real Save As to .qxw through the dialog. */
void AppViews_Test::saveAsOffersTheQlcPlus4Format()
{
    const QStringList filters = App::saveAsFilters();
    QCOMPARE(filters.count(), 3);
    QVERIFY(filters.at(0).contains("*.qlcc"));
    QVERIFY(filters.at(1).contains("QLC+ 4") && filters.at(1).contains("*.qxw"));
    QCOMPARE(App::saveAsDefaultFilter(QString()), filters.at(0));
    QCOMPARE(App::saveAsDefaultFilter("/shows/Spring.qlcc"), filters.at(0));
    QCOMPARE(App::saveAsDefaultFilter("/shows/Spring.QXW"), filters.at(1));
    QCOMPARE(App::saveAsFileName("/s/Spring", filters.at(1)), QString("/s/Spring.qxw"));
    QCOMPARE(App::saveAsFileName("/s/Spring", filters.at(0)), QString("/s/Spring.qlcc"));
    QCOMPARE(App::saveAsFileName("/s/Spring", filters.at(2)), QString("/s/Spring.qlcc"));
    QCOMPARE(App::saveAsFileName("/s/Spring.qlcc", filters.at(1)), QString("/s/Spring.qlcc"));
    QCOMPARE(App::saveAsFileName("/s/Spring.qxw", filters.at(0)), QString("/s/Spring.qxw"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    m_app->clearDocument();
    // A show opened from a .qxw: Save As starts on QLC+ 4's type.
    QCOMPARE(m_app->saveXML(dir.filePath("Spring.qxw")), QFile::NoError);
    QString startedOn;
    QStringList offered;
    QTimer::singleShot(0, [&]() {
        QFileDialog *dlg = qobject_cast<QFileDialog *>(modalOrFixtureEditor());
        if (dlg == nullptr)
            return;
        startedOn = dlg->selectedNameFilter();
        offered = dlg->nameFilters();
        dlg->selectFile(dir.filePath("Copy"));
        QMetaObject::invokeMethod(dlg, "accept");
    });
    QCOMPARE(m_app->slotFileSaveAs(), QFile::NoError);
    QCOMPARE(offered, filters);
    QCOMPARE(startedOn, filters.at(1));
    QVERIFY2(QFile::exists(dir.filePath("Copy.qxw")), qPrintable(m_app->fileName()));
    QCOMPARE(m_app->fileName(), dir.filePath("Copy.qxw"));
    // As QLC+ reads it: a <Workspace> with the <Engine> in it.
    {
        QFile f(dir.filePath("Copy.qxw"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QXmlStreamReader xml(&f);
        QVERIFY(xml.readNextStartElement());
        QCOMPARE(xml.name().toString(), QString("Workspace"));
    }
    // And it opens again.
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(dir.filePath("Copy.qxw")), QFile::NoError);
    m_app->clearDocument();
}

/* 0.3.1 (k) 2: QLC+ 4's Toggle Full Screen went with the slim toolbar
   (c91466e9b); it is back on View, Classic's toolbar, its keys and -f. */
void AppViews_Test::fullScreenIsBack()
{
    QAction *fs = m_app->findChild<QAction *>("viewFullScreen");
    QVERIFY(fs != nullptr);
    QVERIFY(fs->isCheckable());
    QVERIFY(fs->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_F11)));
#if defined(Q_OS_MACOS)
    QCOMPARE(fs->shortcuts().first(), QKeySequence(Qt::META | Qt::CTRL | Qt::Key_F));
#endif
    bool inView = false;
    for (QAction *menu : m_app->menuBar()->actions())
        if (menu->menu() != nullptr && menu->menu()->actions().contains(fs))
            inView = menu->text().remove('&') == QLatin1String("View");
    QVERIFY2(inView, "Full Screen is on the View menu");

    m_app->setWorkspaceView(App::ClassicView);
    QToolBar *classic = m_app->findChild<QToolBar *>("classicToolbar");
    QVERIFY(classic != nullptr && classic->actions().contains(fs));
    m_app->setWorkspaceView(App::ConsoleView);

    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app));
    QVERIFY(!(m_app->windowState() & Qt::WindowFullScreen));
    fs->trigger();
    QTRY_VERIFY(m_app->windowState() & Qt::WindowFullScreen);
    QVERIFY(fs->isChecked());
    fs->trigger();
    QTRY_VERIFY(!(m_app->windowState() & Qt::WindowFullScreen));
    QVERIFY(!fs->isChecked());
    // -f: as the action does, and checks it.
    m_app->slotControlFullScreen(false);
    QTRY_VERIFY(m_app->windowState() & Qt::WindowFullScreen);
    QVERIFY(fs->isChecked());
    m_app->slotControlFullScreen();
    QTRY_VERIFY(!(m_app->windowState() & Qt::WindowFullScreen));
    // -f resize: the first screen's size, by geometry.
    m_app->slotControlFullScreen(true);
    const QRect screen = QGuiApplication::screens().first()->geometry();
    if (screen.width() >= m_app->minimumSizeHint().width() && screen.height() >= m_app->minimumSizeHint().height())
        QCOMPARE(m_app->geometry(), screen);
    else    // a screen under the window's minimum (offscreen at QT_SCALE_FACTOR=2: 400x400): placed at its origin
        QCOMPARE(m_app->geometry().topLeft(), screen.topLeft());
    m_app->showNormal();
}

/* 0.3.1 (k) 5: QLC+ 4's Monitor -- the channels and their values -- from
   Classic's toolbar (where QLC+ had it) and View. */
void AppViews_Test::theDmxMonitorIsInClassic()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    Fixture *fx = new Fixture(doc);
    fx->setName("Dimmer Pack");
    fx->setChannels(4);
    fx->setUniverse(0);
    fx->setAddress(10);
    QVERIFY(doc->addFixture(fx));

    m_app->setWorkspaceView(App::ClassicView);
    QToolBar *classic = m_app->findChild<QToolBar *>("classicToolbar");
    QAction *act = m_app->findChild<QAction *>("viewDmxMonitor");
    QVERIFY(classic != nullptr && act != nullptr);
    QVERIFY(classic->actions().contains(act));
    // Where QLC+ had its Monitor: before the Address Tool.
    QVERIFY(classic->actions().indexOf(act) < classic->actions().indexOf(m_app->m_addressToolAction));

    act->trigger();
    DmxMonitorWindow *mon = m_app->findChild<DmxMonitorWindow *>("dmxMonitorWindow");
    QVERIFY(mon != nullptr);
    QVERIFY(mon->isVisible());
    QCOMPARE(mon->shownFixtures(), QList<quint32>() << fx->id());
    QVERIFY(doc->inputOutputMap()->universe(0)->monitor());
    // A fixture added while it is open shows up.
    Fixture *fx2 = new Fixture(doc);
    fx2->setChannels(2);
    fx2->setUniverse(1);
    fx2->setAddress(0);
    QVERIFY(doc->addFixture(fx2));
    QCOMPARE(mon->shownFixtures().count(), 2);
    // One universe at a time.
    mon->setUniverse(doc->inputOutputMap()->getUniverseID(1));
    QCOMPARE(mon->shownFixtures(), QList<quint32>() << fx2->id());
    mon->setUniverse(Universe::invalid());
    mon->setValueStyle(MonitorProperties::PercentageValues);
    mon->close();
    QVERIFY(mon->shownFixtures().isEmpty());

    m_app->setWorkspaceView(App::ConsoleView);
    m_app->clearDocument();
}

/* 0.3.1 (k) 4: a QLC+ 4 user can have new shows start in Classic. A
   setting of this computer, off by default; an opened show keeps its own. */
void AppViews_Test::aShowOpensLockedWhenTheUserAsks()
{
    const bool was = Monitor::startPlotLocked();
    const auto restore = qScopeGuard([was]() { Monitor::setStartPlotLocked(was); });

    // A show saved with its plot open.
    QTemporaryDir dir;
    const QString show = dir.filePath(QStringLiteral("open-plot.qlcc"));
    m_app->clearDocument();
    MonitorProperties *mp = m_app->doc()->monitorProperties();
    mp->setLayoutLocked(false);
    const quint32 lyr = mp->addLayer(QStringLiteral("Booms"));
    Q_UNUSED(lyr);
    QCOMPARE(m_app->saveXML(show), QFile::NoError);

    // Off (the default): it opens as saved.
    Monitor::setStartPlotLocked(false);
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    QVERIFY(!m_app->doc()->monitorProperties()->layoutLocked());
    QVERIFY(Monitor::instance() != nullptr);
    QVERIFY(!Monitor::instance()->findChild<MonitorGraphicsView *>()->layoutLocked());

    // On: the whole plot locked as it opens; the show is not "changed".
    Monitor::setStartPlotLocked(true);
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    mp = m_app->doc()->monitorProperties();
    QVERIFY(mp->layoutLocked());
    QVERIFY(!m_app->doc()->isModified());
    Monitor *mon = Monitor::instance();
    QVERIFY(mon->findChild<MonitorGraphicsView *>()->layoutLocked());
    // Each layer's own lock as saved, under the plot's.
    for (const MonitorProperties::MonitorLayer &l : mp->layers())
        if (l.name == QStringLiteral("Booms"))
            QVERIFY(!l.locked);
    m_app->clearDocument();
}

/* QLC+ 4's settings (LegacySettings): Settings ▸ General offers them when
   QLC+ 4 left some (here the test's own file, QLC_QLCPLUS_SETTINGS, set by
   ui/test/common/testsettings.cpp), asks, copies, and File ▸ Open Recent
   shows the shows at once. The start-up offer itself is skipped offscreen. */
void AppViews_Test::qlcPlus4SettingsAreBroughtOverFromSettings()
{
    const QString storePath = QString::fromLocal8Bit(qgetenv("QLC_QLCPLUS_SETTINGS"));
    QVERIFY(!storePath.isEmpty());
    QTemporaryDir dir;
    const QString show = dir.filePath("Old Show.qxw");
    {
        QFile f(show);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }
    const auto restore = qScopeGuard([storePath, show]() {
        QFile::remove(storePath);
        QSettings own;
        for (int i = 0; i < 10; i++)
            if (own.value(QString("workspace/recent%1").arg(i)).toString() == show)
                own.remove(QString("workspace/recent%1").arg(i));
        own.remove("midiplugin");
        own.remove("legacy");
    });

    {
        AppSettings dlg(m_app);     // no QLC+ 4 here: no group
        QVERIFY(dlg.findChild<QGroupBox *>("settingsQlcPlus4") == nullptr);
    }
    {
        QSettings store(storePath, QSettings::IniFormat);
        store.setValue("workspace/recent0", show);
        store.setValue("midiplugin/Input/nanoKONTROL2/mode", 1);
        store.setValue("workspace/geometry", QByteArray("left behind"));
    }

    AppSettings dlg(m_app);
    QVERIFY(dlg.findChild<QGroupBox *>("settingsQlcPlus4") != nullptr);
    QPushButton *bring = dlg.findChild<QPushButton *>("settingsBringOverQlcPlus");
    QVERIFY(bring != nullptr);
    QVERIFY(bring->isEnabled());

    answerNextBox(QMessageBox::No, nullptr);
    bring->click();
    QVERIFY(!QSettings().contains("midiplugin/Input/nanoKONTROL2/mode"));

    QString asked;
    answerNextBox(QMessageBox::Yes, &asked);
    bring->click();
    QVERIFY2(asked.contains("1 recent show"), qPrintable(asked));
    QVERIFY2(asked.contains("1 setting"), qPrintable(asked));
    QCOMPARE(QSettings().value("midiplugin/Input/nanoKONTROL2/mode").toInt(), 1);
    QVERIFY(!QSettings().contains("workspace/geometry")
            || QSettings().value("workspace/geometry").toByteArray() != QByteArray("left behind"));
    QStringList recent;
    foreach (QAction *a, m_app->m_fileOpenMenu->actions())
        recent << a->text();
    QVERIFY2(recent.contains(show), qPrintable(recent.join(", ")));
    QVERIFY(QSettings().contains("legacy/qlcplusSettingsOffered"));
    QVERIFY(!bring->isEnabled());           // nothing left to bring
}

void AppViews_Test::newFromTemplateOpensTheClassroomStageUntitled()
{
    // The menu: one entry per template, after New
    QMenu *menu = m_app->menuBar()->findChild<QMenu *>("fileNewFromTemplate");
    QVERIFY(menu != nullptr);
    const QList<App::ShowTemplate> templates = App::showTemplates();
    QVERIFY(!templates.isEmpty());
    QCOMPARE(menu->actions().size(), templates.size());
    QCOMPARE(menu->actions().first()->data().toString(), templates.first().resource);
    QVERIFY(QFile::exists(templates.first().resource));

    m_app->clearDocument();
    QVERIFY(m_app->newFromTemplate(templates.first().resource));
    Doc *doc = m_app->doc();
    QCOMPARE(doc->fixtures().count(), 68);      // docs/course/rig.md
    QVERIFY(!doc->fixtureGroups().isEmpty());
    // The stock colours include Flesh, #ffcdaa
    QLCPalette *flesh = nullptr;
    for (QLCPalette *p : doc->palettes())
        if (p->type() == QLCPalette::Color && p->name() == QLatin1String("Flesh"))
            flesh = p;
    QVERIFY(flesh != nullptr);
    QColor rgb, wauv;
    QVERIFY(QLCPalette::stringToColor(flesh->value().toString(), rgb, wauv));
    QCOMPARE(rgb, QColor(255, 205, 170));
    // Untitled and unchanged: Save asks where; nothing to save on close
    QVERIFY(m_app->fileName().isEmpty());
    QVERIFY(!doc->isModified());
    QVERIFY(doc->programmer()->venueCorrections()->showFile().isEmpty());
    // Never sends anything: no universe is patched to an output
    InputOutputMap *iom = doc->inputOutputMap();
    for (quint32 i = 0; i < iom->universesCount(); i++)
        QCOMPARE(iom->outputPatchesCount(iom->getUniverseID(i)), 0);

    // An edited show is asked about first: Cancel keeps it
    doc->setModified();
    QTimer::singleShot(0, this, []() {
        QMessageBox *box = qobject_cast<QMessageBox *>(modalOrFixtureEditor());
        if (box != nullptr)
            box->button(QMessageBox::Cancel)->click();
    });
    QVERIFY(!m_app->newFromTemplate(templates.first().resource));
    QVERIFY(doc->isModified());
    QCOMPARE(doc->fixtures().count(), 68);
    m_app->clearDocument();
}

void AppViews_Test::aPinKeepsTheShowInOperate()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    ShowSettingsDialog::setTestMode(true);
    auto cleanup = qScopeGuard([this]() {
        ShowSettingsDialog::setTestMode(false);
        if (m_app->doc()->mode() == Doc::Operate)
        {
            m_app->doc()->operateSettings()->setPin(QString());
            m_app->doc()->operateSettings()->setRemoteGo(false);
            m_app->slotModeDesign();
        }
        m_app->clearDocument();
    });

    QAction *settingsAction = m_app->findChild<QAction *>("fileShowSettings");
    QVERIFY(settingsAction != nullptr && settingsAction->isEnabled());

    // Show Settings: a PIN (a short one is asked again), and Open in Operate
    {
        ShowSettingsDialog dlg(doc, m_app);
        QPushButton *set = dlg.findChild<QPushButton *>("showSettingsSetPin");
        QPushButton *remove = dlg.findChild<QPushButton *>("showSettingsRemovePin");
        QCheckBox *openIn = dlg.findChild<QCheckBox *>("showSettingsOpenInOperate");
        QVERIFY(set && remove && openIn);
        QVERIFY(!remove->isEnabled());
        ShowSettingsDialog::testAnswers() = { "12", "2468", "1357" };   // too short; no match
        set->click();
        QVERIFY(!dlg.pinWillBeSet());
        ShowSettingsDialog::testAnswers() = { "2468", "2468" };
        set->click();
        QVERIFY(dlg.pinWillBeSet());
        QVERIFY(remove->isEnabled());
        openIn->setChecked(true);
        QCheckBox *remote = dlg.findChild<QCheckBox *>("showSettingsRemoteGo");
        QVERIFY(remote != nullptr && !remote->isChecked());
        remote->setChecked(true);
        // Nothing changes until OK
        QVERIFY(!doc->operateSettings()->hasPin());
        QVERIFY(!doc->operateSettings()->remoteGo());
        dlg.accept();
    }
    QVERIFY(doc->operateSettings()->remoteGo());
    QVERIFY(doc->operateSettings()->hasPin());
    QVERIFY(doc->operateSettings()->checkPin("2468"));
    QVERIFY(doc->operateSettings()->openInOperate());
    QVERIFY(doc->isModified());

    // Operate: locked. No settings, no Live Edit
    m_app->slotModeOperate();
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!settingsAction->isEnabled());
    QAction *liveEdit = nullptr;
    for (QAction *a : m_app->findChildren<QAction *>())
        if (a->text() == QObject::tr("Live Edit a Function"))
            liveEdit = a;
    QVERIFY(liveEdit != nullptr);
    QVERIFY(!liveEdit->isEnabled());

    // Wrong PIN, or Cancel: still in Operate. The PIN: Design
    ShowSettingsDialog::testAnswers() = { "1111" };
    m_app->slotModeDesign();
    QCOMPARE(doc->mode(), Doc::Operate);
    ShowSettingsDialog::testAnswers().clear();
    m_app->slotModeToggle();
    QCOMPARE(doc->mode(), Doc::Operate);
    ShowSettingsDialog::testAnswers() = { "2468" };
    m_app->slotModeToggle();
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(settingsAction->isEnabled());

    // Changing the PIN asks for the current one first
    {
        ShowSettingsDialog dlg(doc, m_app);
        ShowSettingsDialog::testAnswers() = { "0000" };
        dlg.findChild<QPushButton *>("showSettingsRemovePin")->click();
        QVERIFY(dlg.pinWillBeSet());
        ShowSettingsDialog::testAnswers() = { "2468" };
        dlg.findChild<QPushButton *>("showSettingsRemovePin")->click();
        QVERIFY(!dlg.pinWillBeSet());
        dlg.reject();       // Cancel: the show keeps its PIN
    }
    QVERIFY(doc->operateSettings()->hasPin());

    // Saved with the show; it opens in Operate, still locked
    QTemporaryDir dir;
    const QString path = dir.filePath("locked.qlcc");
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    m_app->clearDocument();
    QVERIFY(!doc->operateSettings()->hasPin());
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QTRY_COMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!doc->isModified());
    ShowSettingsDialog::testAnswers() = { "2468" };
    m_app->slotModeDesign();
    QCOMPARE(doc->mode(), Doc::Design);
}

/* Control ▸ Live Edit is on in Operate unless the show's PIN locks Design
   (getting-started.md, Show settings), and it follows the lock while the
   show stays in Operate: a locked show loaded from web access in Operate
   used to leave Live Edit on (its state was set on a mode change only). */
void AppViews_Test::liveEditFollowsTheLockInOperate()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    auto cleanup = qScopeGuard([this]() {
        m_app->doc()->operateSettings()->setPin(QString());
        if (m_app->doc()->mode() == Doc::Operate)
            m_app->slotModeDesign();
        m_app->doc()->setLoadedFromWeb(false);
        m_app->clearDocument();
    });
    QAction *liveEdit = m_app->m_liveEditAction;
    QAction *vcLiveEdit = m_app->m_liveEditVirtualConsoleAction;
    QAction *capture = m_app->m_captureLiveEditsAction;
    QVERIFY(liveEdit != nullptr && vcLiveEdit != nullptr && capture != nullptr);

    // Design: off. Operate, no PIN: on.
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(!liveEdit->isEnabled());
    m_app->slotModeOperate();
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(liveEdit->isEnabled());
    QVERIFY(vcLiveEdit->isEnabled());
    QVERIFY(capture->isEnabled());

    // The lock comes on while the show stays in Operate: off, and a capture
    // running stops. It goes: on again.
    capture->setChecked(true);
    doc->operateSettings()->setPin("2468");
    QVERIFY(doc->operateSettings()->locksDesign());
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!liveEdit->isEnabled());
    QVERIFY(!vcLiveEdit->isEnabled());
    QVERIFY(!capture->isEnabled());
    QVERIFY(!capture->isChecked());
    doc->operateSettings()->setLockDesign(false);
    QVERIFY(liveEdit->isEnabled());
    doc->operateSettings()->setLockDesign(true);
    QVERIFY(!liveEdit->isEnabled());

    // The real path: a locked show sent from web access while an unlocked
    // one runs in Operate.
    QTemporaryDir dir;
    const QString path = dir.filePath("locked.qlcc");
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    doc->operateSettings()->setPin(QString());
    QVERIFY(liveEdit->isEnabled());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString xml = QString::fromUtf8(file.readAll());
    m_app->slotLoadDocFromMemory(xml);
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(doc->operateSettings()->locksDesign());
    QVERIFY(!liveEdit->isEnabled());
}

/* Changelog: Open in Operate (File ▸ Show Settings) with no PIN: the saved
   show loads in Operate and Design is one click away, no prompt; a show
   without it loads in Design and stays there. */
void AppViews_Test::openInOperateWithoutAPinLoadsInOperateAndLeavesFreely()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    ShowSettingsDialog::setTestMode(true);
    auto cleanup = qScopeGuard([this]() {
        ShowSettingsDialog::setTestMode(false);
        ShowSettingsDialog::testAnswers().clear();
        if (m_app->doc()->mode() == Doc::Operate)
            m_app->slotModeDesign();
        m_app->clearDocument();
    });
    QTemporaryDir dir;

    // Without it: saved, loaded, in Design.
    QCOMPARE(m_app->saveXML(dir.filePath("plain.qlcc")), QFile::NoError);
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(dir.filePath("plain.qlcc")), QFile::NoError);
    QTest::qWait(50);
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(!doc->operateSettings()->openInOperate());

    // Ticked in Show Settings (no PIN): saved with the show.
    {
        ShowSettingsDialog dlg(doc, m_app);
        QCheckBox *openIn = dlg.findChild<QCheckBox *>("showSettingsOpenInOperate");
        QVERIFY(openIn != nullptr);
        openIn->setChecked(true);
        dlg.accept();
    }
    QVERIFY(doc->operateSettings()->openInOperate());
    QVERIFY(!doc->operateSettings()->hasPin());
    QCOMPARE(m_app->saveXML(dir.filePath("open.qlcc")), QFile::NoError);
    m_app->clearDocument();
    QVERIFY(!doc->operateSettings()->openInOperate());
    QCOMPARE(m_app->loadXML(dir.filePath("open.qlcc")), QFile::NoError);
    QTRY_COMPARE(doc->mode(), Doc::Operate);
    QVERIFY(doc->operateSettings()->openInOperate());

    // No PIN: Design is not asked for anything.
    ShowSettingsDialog::testAnswers().clear();
    m_app->slotModeDesign();
    QCOMPARE(doc->mode(), Doc::Design);
}

void AppViews_Test::reviewChipCountsAndOpensThePanel()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    auto back = qScopeGuard([this]() {
        if (ShowReviewPanel::shared() != nullptr)
            ShowReviewPanel::shared()->close();
        m_app->clearDocument();
    });
    QLabel *chip = m_app->findChild<QLabel *>("footerReview");
    QVERIFY(chip != nullptr);
    QVERIFY(chip->isHidden());                      // not checked yet
    doc->showReview()->run();
    QVERIFY(!chip->isHidden());
    QCOMPARE(chip->text(), QStringLiteral("Reviewed"));

    // Two fixtures on the same channels: a blocker, counted a moment after the edit.
    Fixture *a = new Fixture(doc);
    a->setName("Lamp 1");
    a->setChannels(1);
    a->setAddress(0);
    QVERIFY(doc->addFixture(a));
    Fixture *b = new Fixture(doc);
    b->setName("Lamp 2");
    b->setChannels(1);
    b->setAddress(4);
    QVERIFY(doc->addFixture(b));
    b->setAddress(0);
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QStringLiteral("1 ⛔")), 5000);

    QTest::mouseClick(chip, Qt::LeftButton);
    QVERIFY(ShowReviewPanel::shared() != nullptr);
    QVERIFY(ShowReviewPanel::shared()->isVisible());
    QVERIFY(ShowReviewPanel::shared()->selectCheck("PAT-01"));
    QVERIFY(ShowReviewPanel::shared()->acceptSelected("Doubled on purpose", "Ava"));
    QCOMPARE(chip->text(), QStringLiteral("Reviewed"));

    // Fix on a cue's finding goes to the cue in the Show Tree.
    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Show"), "Act 1");
    const quint32 cue = st->addCue(list, -1, "Empty");
    QVERIFY(m_app->reviewJump(nullptr, QStringLiteral("cue"), QVariantList() << cue));
    QVERIFY(ShowTreeView::instance() != nullptr);
    QCOMPARE(ShowTreeView::instance()->currentId(), cue);
}

void AppViews_Test::teachingModeShowsItselfAndThePractice()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    auto back = qScopeGuard([this]() {
        m_app->doc()->teachingSettings()->setEnabled(false);
        m_app->setWorkspaceView(App::ConsoleView);
        m_app->clearDocument();
    });
    QLabel *chip = m_app->findChild<QLabel *>("footerTeaching");
    QVERIFY(chip != nullptr);

    auto practiceItems = [doc, this]() {
        QMenu menu;
        FocusMenu::fill(&menu, doc, m_app, nullptr);
        int n = 0;
        for (QAction *a : menu.actions())
            if (a->objectName() == QLatin1String("focusPractice"))
                n++;
        return n;
    };

    // Not a teaching show: nothing about it anywhere
    QVERIFY(chip->isHidden());
    QVERIFY(!m_app->windowTitle().contains("Teaching"));
    QCOMPARE(practiceItems(), 0);

    doc->teachingSettings()->setEnabled(true);
    QVERIFY(doc->isModified());
    for (App::WorkspaceView v : { App::ConsoleView, App::EverythingView, App::ClassicView })
    {
        m_app->setWorkspaceView(v);
        QVERIFY2(!chip->isHidden(), qPrintable(QString("view %1").arg(int(v))));
        QCOMPARE(chip->text(), QString("TEACHING"));
    }
    QVERIFY(m_app->windowTitle().contains("(Teaching)"));
    QCOMPARE(practiceItems(), 1);

    // Off again: gone
    doc->teachingSettings()->setEnabled(false);
    QVERIFY(chip->isHidden());
    QVERIFY(!m_app->windowTitle().contains("Teaching"));
    QCOMPARE(practiceItems(), 0);
}

void AppViews_Test::teachingModeIsGuardedByThePin()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    ShowSettingsDialog::setTestMode(true);
    auto back = qScopeGuard([this]() {
        ShowSettingsDialog::setTestMode(false);
        m_app->doc()->teachingSettings()->setEnabled(false);
        m_app->clearDocument();
    });

    // No PIN: ticking and unticking just works
    {
        ShowSettingsDialog dlg(doc, m_app);
        QCheckBox *teaching = dlg.findChild<QCheckBox *>("showSettingsTeaching");
        QCheckBox *allow = dlg.findChild<QCheckBox *>("showSettingsAllowOutput");
        QVERIFY(teaching && allow);
        QVERIFY(!allow->isEnabled());           // only in a teaching show
        teaching->setChecked(true);
        QVERIFY(allow->isEnabled());
        QVERIFY(!doc->teachingSettings()->enabled());       // nothing until OK
        dlg.accept();
    }
    QVERIFY(doc->teachingSettings()->enabled());
    QVERIFY(doc->teachingSettings()->holdsOutput());
    QVERIFY(doc->operateSettings()->setPin("2468"));
    doc->operateSettings()->setLockDesign(false);       // the PIN is for Teaching
    doc->resetModified();

    // Off needs the PIN: wrong, or none, leaves it on and the dialog open
    {
        ShowSettingsDialog dlg(doc, m_app);
        QCheckBox *teaching = dlg.findChild<QCheckBox *>("showSettingsTeaching");
        teaching->setChecked(false);
        ShowSettingsDialog::testAnswers() = { "1111" };
        dlg.accept();
        QVERIFY(dlg.result() != QDialog::Accepted);
        QVERIFY(doc->teachingSettings()->enabled());
        ShowSettingsDialog::testAnswers().clear();
        dlg.accept();
        QVERIFY(doc->teachingSettings()->enabled());
        QVERIFY(!doc->isModified());
        ShowSettingsDialog::testAnswers() = { "2468" };
        dlg.accept();
        QCOMPARE(dlg.result(), int(QDialog::Accepted));
    }
    QVERIFY(!doc->teachingSettings()->enabled());
    QVERIFY(doc->isModified());
    // A show setting, not an edit: Undo has no step for it
    QVERIFY(!doc->inputOutputMap()->teachingHold());

    // Letting real output through also asks
    doc->teachingSettings()->setEnabled(true);
    {
        ShowSettingsDialog dlg(doc, m_app);
        dlg.findChild<QCheckBox *>("showSettingsAllowOutput")->setChecked(true);
        ShowSettingsDialog::testAnswers() = { "9999" };
        dlg.accept();
        QVERIFY(!doc->teachingSettings()->allowOutput());
        ShowSettingsDialog::testAnswers() = { "2468" };
        dlg.accept();
    }
    QVERIFY(doc->teachingSettings()->allowOutput());
    QVERIFY(!doc->inputOutputMap()->teachingHold());

    // Q203: letting show control through (or sending it in Blind) asks too;
    // holding more does not
    QVERIFY(!doc->teachingSettings()->allowShowControl());
    {
        ShowSettingsDialog dlg(doc, m_app);
        dlg.findChild<QCheckBox *>("showSettingsAllowShowControl")->setChecked(true);
        ShowSettingsDialog::testAnswers() = { "9999" };
        dlg.accept();
        QVERIFY(!doc->teachingSettings()->allowShowControl());
        ShowSettingsDialog::testAnswers() = { "2468" };
        dlg.accept();
    }
    QVERIFY(doc->teachingSettings()->allowShowControl());
    {
        ShowSettingsDialog dlg(doc, m_app);
        dlg.findChild<QCheckBox *>("showSettingsHoldBlindControl")->setChecked(true);
        ShowSettingsDialog::testAnswers().clear();
        dlg.accept();                                   // holding more: no PIN
        QCOMPARE(dlg.result(), int(QDialog::Accepted));
    }
    QVERIFY(doc->teachingSettings()->holdShowControlInBlind());
    {
        ShowSettingsDialog dlg(doc, m_app);
        dlg.findChild<QCheckBox *>("showSettingsHoldBlindControl")->setChecked(false);
        ShowSettingsDialog::testAnswers() = { "9999" };
        dlg.accept();                                   // sending in Blind again: PIN
        QVERIFY(doc->teachingSettings()->holdShowControlInBlind());
        ShowSettingsDialog::testAnswers() = { "2468" };
        dlg.accept();
    }
    QVERIFY(!doc->teachingSettings()->holdShowControlInBlind());

    // The lock-Design box is only for a show with a PIN, and off keeps the
    // PIN for Teaching alone: a student can still leave Operate
    QVERIFY(doc->operateSettings()->hasPin());
    QVERIFY(!doc->operateSettings()->locksDesign());
}

void AppViews_Test::teachingOperateWarnsQuietlyThenAsksWhenItLooksReal()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    auto back = qScopeGuard([this]() {
        if (m_app->doc()->mode() == Doc::Operate)
            m_app->slotModeDesign();
        m_app->doc()->teachingSettings()->setEnabled(false);
        m_app->clearDocument();
        ShowSettingsDialog::setTestMode(false);
        NetworkRoute::setResolverForTest(nullptr, nullptr);
    });

    // A classroom show: nothing patched. Operate is a lesson (1.5): a quiet
    // line, the Not ready entry, no dialog.
    doc->teachingSettings()->setEnabled(true);
    doc->resetModified();
    bool boxed = false;
    QTimer probe;
    probe.setInterval(10);
    connect(&probe, &QTimer::timeout, this, [&boxed]() {
        if (qobject_cast<QMessageBox *>(modalOrFixtureEditor()) != nullptr)
            boxed = true;
    });
    probe.start();
    m_app->slotModeOperate();
    QTest::qWait(60);
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!boxed);
    auto statusKeys = []() {
        QStringList keys;
        for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
            keys << e.key;
        return keys;
    };
    QVERIFY(statusKeys().contains("teaching.operate"));
    QLabel *noticeText = m_app->findChild<QLabel *>("viewNoticeText");
    QVERIFY(noticeText != nullptr);
    QVERIFY(noticeText->text().contains("Teaching mode"));
    QVERIFY(noticeText->text().contains("held"));
    m_app->slotModeDesign();
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(!statusKeys().contains("teaching.operate"));

    // Real output let through and patched: it looks like a real show
    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = doc->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        cache->load(stubDir);
    IOPluginStub *stub = static_cast<IOPluginStub *>(cache->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
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
        "  <Output Plugin=\"ArtNet\" UID=\"10.4.12.7\" Line=\"0\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n"));
    xml.readNextStartElement();
    InputOutputMap *iom = doc->inputOutputMap();
    QVERIFY(iom->loadXML(xml));

    // Held while Teaching holds it: the footer says why, and no dialog
    QVERIFY(iom->outputPatch(0, 0)->isHeld());
    QLabel *rig = m_app->findChild<QLabel *>("footerRigNetwork");
    QVERIFY(rig != nullptr && !rig->isHidden());
    QVERIFY(rig->text().contains("Teaching mode holds real output"));
    boxed = false;
    m_app->slotModeOperate();
    QTest::qWait(60);
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!boxed);
    m_app->slotModeDesign();

    doc->teachingSettings()->setAllowOutput(true);
    QVERIFY(iom->outputPatch(0, 0)->isPatched());
    QVERIFY(!rig->text().contains("Teaching"));         // only the usual "rig network not set"

    // A start nobody is there to answer for (-p, Open in Operate) never
    // shows the dialog
    boxed = false;
    m_app->startInOperate();
    QTest::qWait(60);
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!boxed);
    m_app->slotModeDesign();

    // Q204: the web page's toggle never puts the question on the console
    // (it would look real here: output let through and patched)
    boxed = false;
    m_app->slotModeToggleFromWeb();
    QTest::qWait(60);
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!boxed);
    m_app->slotModeToggleFromWeb();
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(!boxed);

    // Cancel: stays in Design. Operate in Teaching Mode: goes, and asks once.
    probe.stop();
    QString asked;
    answerNextBox(QMessageBox::Cancel, &asked);
    m_app->slotModeOperate();
    QVERIFY(asked.contains("Teaching mode"));
    QCOMPARE(doc->mode(), Doc::Design);
    answerNextBox(QMessageBox::No, nullptr);
    m_app->slotModeOperate();
    QCOMPARE(doc->mode(), Doc::Operate);
    m_app->slotModeDesign();
    m_app->slotModeOperate();           // not asked again (nobody answers: it would hang)
    QCOMPARE(doc->mode(), Doc::Operate);
    m_app->slotModeDesign();

    // "Turn Off Teaching Mode…" asks for the PIN: wrong stays in Design with
    // Teaching on; right turns it off and goes to Operate
    doc->teachingSettings()->setEnabled(false);
    doc->teachingSettings()->setEnabled(true);          // asked afresh
    QVERIFY(doc->teachingSettings()->allowOutput());
    ShowSettingsDialog::setTestMode(true);
    QVERIFY(doc->operateSettings()->setPin("2468"));
    doc->operateSettings()->setLockDesign(false);
    ShowSettingsDialog::testAnswers() = { "0000" };
    answerNextBox(QMessageBox::Yes, nullptr);
    m_app->slotModeOperate();
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(doc->teachingSettings()->enabled());
    ShowSettingsDialog::testAnswers() = { "2468" };
    answerNextBox(QMessageBox::Yes, nullptr);
    m_app->slotModeOperate();
    QCOMPARE(doc->mode(), Doc::Operate);
    QVERIFY(!doc->teachingSettings()->enabled());
}

void AppViews_Test::aStudentHandoutIsACopy()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    QTemporaryDir dir;
    auto back = qScopeGuard([this]() {
        m_app->doc()->teachingSettings()->setEnabled(false);
        m_app->clearDocument();
    });

    // The teacher's own show: a PIN that locks Design, opens in Operate, no Teaching
    doc->operateSettings()->setPin("1111");
    doc->operateSettings()->setOpenInOperate(true);
    const QString master = dir.filePath("Master.qlcc");
    QCOMPARE(m_app->saveXML(master), QFile::NoError);
    doc->resetModified();

    HandoutOptions options;
    options.path = dir.filePath("Handout.qlcc");
    options.pinHash = ShowPin::hash("9753");
    options.lesson = "3.6";
    options.link = "https://example.org/lesson-06/";
    QString why;
    QVERIFY2(m_app->writeStudentHandout(options, &why), qPrintable(why));
    QVERIFY(QFileInfo(options.path).exists());

    // The open show is as it was: its file, its settings, not modified
    QCOMPARE(m_app->fileName(), master);
    QVERIFY(!doc->isModified());
    QVERIFY(!doc->teachingSettings()->enabled());
    QVERIFY(!doc->inputOutputMap()->teachingHold());
    QVERIFY(doc->operateSettings()->checkPin("1111"));
    QVERIFY(doc->operateSettings()->openInOperate());
    QVERIFY(doc->operateSettings()->locksDesign());

    // The copy: Teaching on, output held, the new PIN for Teaching only
    QCOMPARE(m_app->loadXML(options.path), QFile::NoError);
    doc = m_app->doc();
    QVERIFY(doc->teachingSettings()->enabled());
    QVERIFY(doc->teachingSettings()->holdsOutput());
    QCOMPARE(doc->teachingSettings()->lesson(), QString("3.6"));
    QVERIFY(!doc->teachingSettings()->handedOut().isEmpty());
    QVERIFY(doc->operateSettings()->checkPin("9753"));
    QVERIFY(!doc->operateSettings()->checkPin("1111"));
    QVERIFY(!doc->operateSettings()->openInOperate());
    QVERIFY(!doc->operateSettings()->locksDesign());
    QCOMPARE(doc->mode(), Doc::Design);

    // Save and autosave carry the flag
    const QString again = dir.filePath("Again.qlcc");
    QCOMPARE(m_app->saveXML(again), QFile::NoError);
    const QString auto1 = dir.filePath("auto.qlcc");
    QCOMPARE(m_app->saveXML(auto1, true), QFile::NoError);
    for (const QString &f : { again, auto1 })
    {
        QFile file(f);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY2(file.readAll().contains("<Teaching Enabled=\"1\""), qPrintable(f));
    }
}

void AppViews_Test::aHandoutResetsToItself()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    QTemporaryDir dir;
    auto back = qScopeGuard([this]() {
        ShowSettingsDialog::setTestMode(false);
        m_app->doc()->teachingSettings()->setEnabled(false);
        m_app->clearDocument();
    });

    // The teacher's show has two fixtures' worth of work in it: a named group
    QVERIFY(doc->addFixtureGroup(new FixtureGroup(doc)));
    doc->fixtureGroups().first()->setName("Front wash");
    const QString master = dir.filePath("Master.qlcc");
    QCOMPARE(m_app->saveXML(master), QFile::NoError);

    HandoutOptions options;
    options.path = dir.filePath("Handout.qlcc");
    options.pinHash = ShowPin::hash("9753");
    QString why;
    QVERIFY2(m_app->writeStudentHandout(options, &why), qPrintable(why));
    QVERIFY(!doc->teachingSettings()->hasHandoutSnapshot());     // the master has none

    // The student's copy carries its handout state
    QCOMPARE(m_app->loadXML(options.path), QFile::NoError);
    doc = m_app->doc();
    QVERIFY(doc->teachingSettings()->enabled());
    QVERIFY(doc->teachingSettings()->hasHandoutSnapshot());
    QCOMPARE(doc->fixtureGroups().size(), 1);

    // The student works on it, and saves
    doc->fixtureGroups().first()->setName("My changes");
    QVERIFY(doc->addFixtureGroup(new FixtureGroup(doc)));
    const QString work = dir.filePath("Student.qlcc");
    QCOMPARE(m_app->saveXML(work), QFile::NoError);
    m_app->setFileName(work);
    QCOMPARE(doc->fixtureGroups().size(), 2);

    // Reset puts it back: the handout's groups, still a handout, file name kept,
    // and marked changed so a save writes it
    m_app->resetToHandout();
    doc = m_app->doc();
    QCOMPARE(doc->fixtureGroups().size(), 1);
    QCOMPARE(doc->fixtureGroups().first()->name(), QString("Front wash"));
    QVERIFY(doc->teachingSettings()->enabled());
    QVERIFY(doc->operateSettings()->checkPin("9753"));
    QCOMPARE(m_app->fileName(), work);
    QVERIFY(doc->isModified());
    QVERIFY(doc->teachingSettings()->hasHandoutSnapshot());     // and can be reset again

    // The saved file still carries it
    QCOMPARE(m_app->saveXML(work), QFile::NoError);
    QCOMPARE(m_app->loadXML(work), QFile::NoError);
    QVERIFY(m_app->doc()->teachingSettings()->hasHandoutSnapshot());

    // The dialog: the button is there and on for a handout; the PIN is asked
    ShowSettingsDialog::setTestMode(true);
    {
        ShowSettingsDialog dlg(m_app->doc());
        QPushButton *reset = dlg.findChild<QPushButton *>("showSettingsReset");
        QVERIFY(reset != nullptr);
        QVERIFY(reset->isEnabled());
        ShowSettingsDialog::testAnswers() = { "0000" };         // wrong PIN: nothing
        reset->click();
        QVERIFY(!dlg.resetRequested());
        ShowSettingsDialog::testAnswers() = { "9753" };
        reset->click();
        QVERIFY(dlg.resetRequested());
    }

    // A show that is not a handout has nothing to go back to
    m_app->clearDocument();
    {
        ShowSettingsDialog dlg(m_app->doc());
        QPushButton *reset = dlg.findChild<QPushButton *>("showSettingsReset");
        QVERIFY(reset != nullptr);
        QVERIFY(!reset->isEnabled());
    }
}

void AppViews_Test::teachingAsksBeforePatchChanges()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    ShowSettingsDialog::setTestMode(true);
    auto back = qScopeGuard([this]() {
        ShowSettingsDialog::setTestMode(false);
        m_app->doc()->teachingSettings()->setEnabled(false);
    });
    int &asked = ShowSettingsDialog::patchQuestions();
    asked = 0;

    // Not a Teaching show: no question
    QVERIFY(ShowSettingsDialog::confirmPatchChange(doc, nullptr, "Add fixtures"));
    QCOMPARE(asked, 0);

    // Teaching: asked; "no" or no answer stops it, "yes" goes on
    doc->teachingSettings()->setEnabled(true);
    ShowSettingsDialog::testAnswers() = { "no" };
    QVERIFY(!ShowSettingsDialog::confirmPatchChange(doc, nullptr, "Add fixtures"));
    ShowSettingsDialog::testAnswers().clear();
    QVERIFY(!ShowSettingsDialog::confirmPatchChange(doc, nullptr, "Add fixtures"));
    ShowSettingsDialog::testAnswers() = { "yes" };
    QVERIFY(ShowSettingsDialog::confirmPatchChange(doc, nullptr, "Add fixtures"));
    QCOMPARE(asked, 3);

    // The Patch dialog asks on Apply, and a "no" leaves the patch alone
    QTemporaryDir dir;
    QVERIFY(doc->addFixtureGroup(new FixtureGroup(doc)));
    PatchInventoryDialog dlg(doc, QList<quint32>());
    ShowSettingsDialog::testAnswers() = { "no" };
    asked = 0;
    dlg.accept();
    QCOMPARE(asked, 1);
    QVERIFY(dlg.result() != QDialog::Accepted);
    ShowSettingsDialog::testAnswers() = { "yes" };
    dlg.accept();
    QCOMPARE(asked, 2);

    doc->teachingSettings()->setEnabled(false);
    asked = 0;
    PatchInventoryDialog plain(doc, QList<quint32>());
    plain.accept();
    QCOMPARE(asked, 0);
}

void AppViews_Test::theMenusShowTheirKeys()
{
    m_app->clearDocument();
    QCOMPARE(m_app->doc()->mode(), Doc::Design);

    QAction *saveAs = m_app->findChild<QAction *>("fileSaveAs");
    QAction *quit = m_app->findChild<QAction *>("fileQuit");
    QAction *settings = m_app->findChild<QAction *>("appSettings");
    QVERIFY(saveAs && quit && settings);
    QCOMPARE(saveAs->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    QCOMPARE(saveAs->shortcutContext(), Qt::ApplicationShortcut);
    QCOMPARE(quit->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Q));
    QCOMPARE(settings->text(), QString("&Settings…"));
    QCOMPARE(settings->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Comma));
    // macOS's application-menu title for it, which Qt takes from its own
    // strings, is Settings… too
    QCOMPARE(QCoreApplication::translate("MAC_APPLICATION_MENU", "Preferences..."), QString("Settings…"));
    QCOMPARE(QCoreApplication::translate("MAC_APPLICATION_MENU", "Quit %1"), QString("Quit %1"));

    // Nothing else holds those keys, the Virtual Console's design keys
    // included (two shortcuts on one key fire neither)
    m_app->showContext("Virtual Console");
    for (QAction *a : m_app->findChildren<QAction *>())
    {
        if (a == saveAs || a == quit)
            continue;
        for (const QKeySequence &k : a->shortcuts())
        {
            QVERIFY2(k != saveAs->shortcut(), qPrintable(a->text()));
            QVERIFY2(k != quit->shortcut(), qPrintable(a->text()));
        }
    }

    // Undo / Redo: the keys in the text, no binding of their own
    QAction *undo = m_app->menuBar()->findChild<QAction *>("editUndo");
    QAction *redo = m_app->menuBar()->findChild<QAction *>("editRedo");
    QVERIFY(undo && redo);
    QVERIFY(undo->shortcut().isEmpty());
    QVERIFY(redo->shortcut().isEmpty());
    const QString z = QKeySequence(QKeySequence::Undo).toString(QKeySequence::NativeText);
    QVERIFY2(undo->text().contains(z), qPrintable(undo->text()));
    // The platform's own Redo keys, the ones the tabs bind (⇧⌘Z on a Mac)
    const QString redoKey = QKeySequence(QKeySequence::Redo).toString(QKeySequence::NativeText);
    QVERIFY(!redoKey.isEmpty());
    QVERIFY2(redo->text().contains(redoKey), qPrintable(redo->text()));
#if defined(Q_OS_WIN)
    QVERIFY2(redo->text().contains("Ctrl+Y"), qPrintable(redo->text()));
#endif
    // The menu, opened, keeps them as it names the step
    QMenu *edit = qobject_cast<QMenu *>(undo->parent());
    QVERIFY(edit != nullptr);
    emit edit->aboutToShow();
    QVERIFY2(undo->text().contains(z), qPrintable(undo->text()));
    QVERIFY(undo->shortcut().isEmpty());
}

void AppViews_Test::newShowsCanStartInClassic()
{
    const QVariant was = QSettings().value(App::newShowsClassicSettingKey());
    const auto restore = qScopeGuard([was]() {
        if (was.isValid())
            QSettings().setValue(App::newShowsClassicSettingKey(), was);
        else
            QSettings().remove(App::newShowsClassicSettingKey());
    });
    QSettings().remove(App::newShowsClassicSettingKey());
    QVERIFY(!App::newShowsClassic());
    QCOMPARE(App::newShowView(), App::ConsoleView);

    {
        AppSettings dlg(m_app);
        QCheckBox *box = dlg.findChild<QCheckBox *>("settingsNewShowsClassic");
        QVERIFY(box != nullptr);
        QVERIFY(!box->isChecked());
        box->setChecked(true);
        dlg.accept();
    }
    QVERIFY(App::newShowsClassic());
    m_app->clearDocument();
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QVERIFY(!m_app->doc()->isModified());

    // A show saved in the qlcconsole view opens in it all the same.
    QTemporaryDir dir;
    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(m_app->saveXML(dir.filePath("console.qlcc")), QFile::NoError);
    m_app->clearDocument();
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QCOMPARE(m_app->loadXML(dir.filePath("console.qlcc")), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);

    {
        AppSettings dlg(m_app);
        QCheckBox *box = dlg.findChild<QCheckBox *>("settingsNewShowsClassic");
        QVERIFY(box->isChecked());
        box->setChecked(false);
        dlg.accept();
    }
    QVERIFY(!App::newShowsClassic());
    m_app->clearDocument();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
}

/* 0.3.1 (k) 6: a .qxw is for QLC+ 4, which reads past what it doesn't know.
   Saving one that holds such things says what, once, after the save is
   done (the file is there, whole), with Save as .qlcc… -- never a dialog
   in the way. A plain show says nothing. */
void AppViews_Test::aQxwSaveSaysWhatQlcPlus4Leaves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    m_app->clearDocument();
    m_app->hideViewNotice();
    Doc *doc = m_app->doc();
    Fixture *fx = new Fixture(doc);
    fx->setChannels(1);
    fx->setUniverse(0);
    fx->setAddress(0);
    QVERIFY(doc->addFixture(fx));

    // Nothing QLC+ 4 would miss: no word.
    const QString plain = dir.filePath("Plain.qxw");
    QCOMPARE(m_app->saveXML(plain), QFile::NoError);
    QVERIFY2(m_app->lastStockLosses().isEmpty(), qPrintable(m_app->lastStockLosses().join(" | ")));
    QVERIFY(m_app->m_viewNotice->isHidden());

    // A Show Tree and a venue house fixture: said, after the file is saved.
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    st->addCue(st->addCueList(prod, "Act 1"), -1, "One");
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    const QString show = dir.filePath("Spring.qxw");
    venues->setShowFile(show, false);
    Fixture *house = new Fixture(doc);
    house->setName("House Wash");
    house->setChannels(1);
    house->setUniverse(0);
    house->setAddress(20);
    QVERIFY(doc->addFixture(house));
    house->setOwner(RigOwner::House);
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));
    venues->moveShowHouseIn();
    QVERIFY(doc->isVenueFixture(house->id()));

    QElapsedTimer t;
    t.start();
    QCOMPARE(m_app->saveXML(show), QFile::NoError);
    QVERIFY(QFile::exists(show));
    QVERIFY(!doc->isModified());
    QVERIFY(modalOrFixtureEditor() == nullptr);
    const QStringList losses = m_app->lastStockLosses();
    QVERIFY2(losses.count() == 2, qPrintable(losses.join(" | ")));
    QVERIFY(losses.at(0).contains("Show Tree"));
    QVERIFY2(losses.at(0).contains("1 cue list") && losses.at(0).contains("1 cue"), qPrintable(losses.at(0)));
    QVERIFY(losses.at(1).contains("house fixture"));
    QVERIFY(!m_app->m_viewNotice->isHidden());
    const QString said = m_app->m_viewNoticeText->text();
    QVERIFY2(said.contains("QLC+ 4") && said.contains("Show Tree") && said.contains(".qlcc"), qPrintable(said));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QString("Save as .qlcc…"));
    QVERIFY(!m_app->m_viewNoticeButton->isHidden());

    // Once a file: the next save of it says nothing.
    m_app->hideViewNotice();
    QCOMPARE(m_app->saveXML(show), QFile::NoError);
    QVERIFY(m_app->m_viewNotice->isHidden());
    // A .qlcc keeps it all: nothing to say.
    QCOMPARE(m_app->saveXML(dir.filePath("Spring.qlcc")), QFile::NoError);
    QVERIFY(m_app->m_viewNotice->isHidden());

    // The button: Save As, starting on .qlcc.
    QCOMPARE(m_app->saveXML(dir.filePath("Spring2.qxw")), QFile::NoError);
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QString startedOn;
    QTimer::singleShot(0, [&]() {
        if (QFileDialog *dlg = qobject_cast<QFileDialog *>(modalOrFixtureEditor()))
        {
            startedOn = dlg->selectedNameFilter();
            dlg->reject();
        }
    });
    m_app->m_viewNoticeButton->click();
    QCOMPARE(startedOn, App::saveAsFilters().at(0));
    m_app->hideViewNotice();

    // "Don't ask again for this show": that file stays quiet, others don't.
    const QString quiet = dir.filePath("Quiet.qxw");
    QCOMPARE(m_app->saveXML(quiet), QFile::NoError);
    QVERIFY(!m_app->m_viewNotice->isHidden());
    QVERIFY(!m_app->m_viewNoticeButton2->isHidden());
    QCOMPARE(m_app->m_viewNoticeButton2->text(), QString("Don't ask again for this show"));
    QVERIFY(!App::stockNoticeOff(quiet));
    m_app->m_viewNoticeButton2->click();
    QVERIFY(m_app->m_viewNotice->isHidden());
    QVERIFY(App::stockNoticeOff(quiet));
    m_app->m_stockNoticeShown.clear();      // a new session
    QCOMPARE(m_app->saveXML(quiet), QFile::NoError);
    QVERIFY(m_app->m_viewNotice->isHidden());
    QCOMPARE(m_app->saveXML(dir.filePath("Loud.qxw")), QFile::NoError);
    QVERIFY(!m_app->m_viewNotice->isHidden());
    App::setStockNoticeOff(quiet, false);
    QVERIFY(!App::stockNoticeOff(quiet));
    m_app->hideViewNotice();
    m_app->clearDocument();
}

void AppViews_Test::unsavedChangesListsWhatChanged()
{
    Doc *doc = m_app->m_doc;
    ShowTreeController *st = doc->showTree();
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    QList<quint32> fixtures, functions, groups, palettes;
    quint32 prod = ShowTree::invalidId();
    const int oldTheme = m_app->theme();
    const QSize oldSize = m_app->size();
    auto tidy = qScopeGuard([&]() {
        if (m_app->m_unsavedDialog != nullptr)
            m_app->m_unsavedDialog->close();
        doc->setMode(Doc::Design);
        if (prod != ShowTree::invalidId())
            st->removeProduction(prod);
        takeAway(doc, fixtures, functions);
        for (quint32 g : groups)
            doc->deleteFixtureGroup(g);
        for (quint32 p : palettes)
            doc->deletePalette(p);
        m_app->setTheme(oldTheme);
        m_app->resize(oldSize);
        doc->resetModified();
        QCoreApplication::processEvents();
    });
    QVERIFY(litScene(doc, 240, 1, &fixtures, &functions) != nullptr);
    QVERIFY(litScene(doc, 241, 1, &fixtures, &functions) != nullptr);
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName("Stage");
    doc->addFixtureGroup(g);
    g->assignFixture(fixtures.first());
    groups << g->id();
    FixtureGroup *g2 = new FixtureGroup(doc);
    g2->setName("Back");
    doc->addFixtureGroup(g2);
    g2->assignFixture(fixtures.last());
    groups << g2->id();
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    palettes << full->id();
    QLCPalette *red = new QLCPalette(QLCPalette::Color, doc);
    red->setName("Red");
    red->setValue(QColor(Qt::red));
    doc->addPalette(red);
    palettes << red->id();
    prod = st->addProduction("Unsaved");
    const quint32 listId = st->addCueList(prod, "Act 1", ShowTree::ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "Verse");
    const quint32 c2 = st->addCue(listId, 0, "Chorus");
    const quint32 c3 = st->addCue(listId, 1, "Bridge");
    ShowTree::Target t;
    t.groups << g->id();
    const quint32 l2 = st->addLayer(c2, t);
    const quint32 l3 = st->addLayer(c3, t);
    st->setCurrentProduction(prod);
    m_app->show();
    m_app->setTheme(App::ThemeVSCodeDark);
    m_app->showContext(QStringLiteral("Show Tree"));
    QTRY_VERIFY_WITH_TIMEOUT(view->selectNode(c1), 3000);

    // Saved: nothing to list.
    doc->resetModified();
    ChangeJournal *journal = doc->changeJournal();
    QVERIFY(journal->entries().isEmpty());
    QVERIFY(!m_app->m_statusDirtyLabel->text().contains(QStringLiteral("Unsaved")));

    // Three different edits, each on the cue it is selected on.
    QVERIFY(view->selectNode(c1));
    QVERIFY(st->renameNode(c1, "Verse 1"));
    QVERIFY(view->selectNode(c2));
    QVERIFY(st->setLayerLook(c2, l2, st->lookForPalette(red->id())));
    QVERIFY(st->setLayerLook(c2, l2, st->lookForPalette(full->id())));
    QVERIFY(view->selectNode(c3));
    QVERIFY(st->addToLayer(c3, l3, QList<quint32>() << g2->id(), QList<quint32>()));

    QCOMPARE(m_app->m_statusDirtyLabel->text(), QString::fromUtf8("\xe2\x97\x8f Unsaved changes"));
    QVERIFY(m_app->m_statusDirtyLabel->cursor().shape() == Qt::PointingHandCursor);

    const QList<ChangeJournal::Entry> es = journal->entries();
    QVERIFY(es.count() >= 3);
    // Newest first: the target (Bridge), the colour and dimmer (Chorus, grouped), the rename (Verse).
    QCOMPARE(es.at(0).kind, ChangeJournal::Target);
    QVERIFY2(es.at(0).title.contains("Bridge"), qPrintable(es.at(0).title));
    QCOMPARE(es.at(0).subject.jump, ChangeJournal::ToCue);
    QCOMPARE(es.at(0).subject.id, c3);
    QCOMPARE(es.at(0).subject.id2, ShowTree::invalidId());
    QCOMPARE(es.at(1).kind, ChangeJournal::Look);
    QVERIFY2(es.at(1).title.contains("Chorus"), qPrintable(es.at(1).title));
    QCOMPARE(es.at(1).edits, 2);
    QCOMPARE(es.at(1).subject.id, c2);
    QCOMPARE(es.at(2).kind, ChangeJournal::Cue);
    QVERIFY2(es.at(2).title.contains("Verse 1"), qPrintable(es.at(2).title));
    QCOMPARE(es.at(2).what, QStringLiteral("Rename"));
    QCOMPARE(es.at(2).subject.id, c1);
    QCOMPARE(journal->stepCount(), 4);
    QVERIFY(es.at(0).when > 0);

    // The footer's tooltip says how many; a click opens the list.
    QVERIFY2(journal->tooltip().contains("unsaved change"), qPrintable(journal->tooltip()));
    QTest::mouseClick(m_app->m_statusDirtyLabel, Qt::LeftButton);
    UnsavedChangesDialog *dlg = qobject_cast<UnsavedChangesDialog *>(m_app->m_unsavedDialog.data());
    QVERIFY(dlg != nullptr);
    QStringList rows = dlg->rows();
    QVERIFY(rows.count() >= 3);
    QVERIFY2(rows.at(1).contains("Chorus") && rows.at(1).endsWith("|2"), qPrintable(rows.at(1)));

    // A row takes you to its thing.
    m_app->showContext(QStringLiteral("Fixtures"));
    dlg->activateRow(0);
    QCOMPARE(m_app->m_tab->tabBar()->tabData(m_app->m_tab->currentIndex()).toString(), QStringLiteral("Show Tree"));
    QCOMPARE(view->currentCueId(), c3);
    dlg->activateRow(2);
    QCOMPARE(view->currentCueId(), c1);

    const QByteArray shotDir = qgetenv("QLC_SHOT_DIR");
    auto shots = [&](const QString &state) {
        if (shotDir.isEmpty())
            return;
        QDir().mkpath(QString::fromLocal8Bit(shotDir));
        const QList<QPair<QSize, QString> > sizes = { { QSize(1024, 640), "1024x640" },
                                                      { QSize(1280, 800), "1280x800" },
                                                      { QSize(1920, 1080), "1920x1080" } };
        for (const auto &sz : sizes)
        {
            m_app->resize(sz.first);
            for (int i = 0; i < 5; ++i)
                QCoreApplication::processEvents();
            QPixmap shot = m_app->grab();
            if (state == QLatin1String("list") && m_app->m_unsavedDialog != nullptr)
            {
                dlg->move(m_app->mapToGlobal(QPoint(shot.width() - dlg->width() - 20,
                                                    shot.height() - dlg->height() - 40)));
                for (int i = 0; i < 5; ++i)
                    QCoreApplication::processEvents();
                QPainter p(&shot);
                p.drawPixmap(m_app->mapFromGlobal(dlg->pos()), dlg->grab());
            }
            else if (state == QLatin1String("hover"))
            {
                QToolTip::showText(m_app->m_statusDirtyLabel->mapToGlobal(QPoint(0, -30)), journal->tooltip(),
                                   m_app->m_statusDirtyLabel);
                QCoreApplication::processEvents();
                QPainter p(&shot);
                for (QWidget *w : QApplication::topLevelWidgets())
                    if (w->inherits("QTipLabel") && w->isVisible())
                        p.drawPixmap(m_app->mapFromGlobal(w->pos()), w->grab());
            }
            const QString base = QString::fromLocal8Bit(shotDir) + QStringLiteral("/%1-%2-%3")
                                     .arg(qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after")), state, sz.second);
            shot.save(base + QStringLiteral(".png"));
        }
    };
    shots("hover");
    shots("list");
    QToolTip::hideText();

    // Undo takes its entry away (the newest step: the target); the rest stay.
    QVERIFY(st->undoShared());
    QCOMPARE(journal->stepCount(), 3);
    QCOMPARE(journal->entries().first().subject.id, c2);
    QTest::mouseClick(dlg->findChild<QPushButton *>("unsavedUndo"), Qt::LeftButton);
    QCOMPARE(journal->stepCount(), 2);
    QCOMPARE(journal->entries().first().subject.id, c2);
    QCOMPARE(journal->entries().first().edits, 1);

    // A fixture added is listed by area; a change nobody tells the journal about is counted only.
    Fixture *fx = new Fixture(doc);
    fx->setName("Extra");
    fx->setChannels(1);
    fx->setUniverse(0);
    fx->setAddress(300);
    QVERIFY(doc->addFixture(fx));
    fixtures << fx->id();
    QCOMPARE(journal->entries().first().kind, ChangeJournal::Fixture);
    QVERIFY(journal->entries().first().title.contains("Extra"));
    QCOMPARE(journal->entries().first().subject.jump, ChangeJournal::ToFixture);
    // (A timer elsewhere may mark the show modified meanwhile; that is "other" too.)
    const int otherBefore = journal->otherCount();
    doc->setModified();
    QCOMPARE(journal->otherCount(), otherBefore + 1);
    QVERIFY(!journal->entries().last().itemised);
    QVERIFY(journal->summary().contains("area"));

    // Save clears the list.
    doc->resetModified();
    QVERIFY(journal->entries().isEmpty());
    QCOMPARE(journal->stepCount(), 0);
    QCOMPARE(journal->otherCount(), 0);
    QVERIFY(dlg->rows().isEmpty() || !dlg->summaryText().isEmpty());
    // ...and an Undo past the save is not a change since it.
    QVERIFY(st->undoShared());
    QCOMPARE(journal->stepCount(), 0);
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

/* Q236: the Workbench opened in Operate is Blind; Play Live is one click and
   the footer says so (one status source); a second click, Design or leaving
   it is Blind again. QLC_SHOT_DIR + QLC_SHOT_PREFIX: pictures at three sizes. */
void AppViews_Test::workbenchPlayLiveIsSaidInTheFooter()
{
    Doc *doc = m_app->doc();
    m_app->setWorkspaceView(App::ConsoleView);
    Scene *scene = new Scene(doc);
    scene->setName("01-01.01-Wash");
    QVERIFY(doc->addFunction(scene));
    const quint32 sid = scene->id();
    auto tidy = qScopeGuard([this, doc, sid]() {
        if (ShowTreeView::instance() != nullptr)
            ShowTreeView::instance()->closeWorkbench();
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        doc->deleteFunction(sid);
        doc->resetModified();
    });
    const QByteArray shotDir = qgetenv("QLC_SHOT_DIR");
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));
    m_app->show();
    doc->setMode(Doc::Operate);
    m_app->hideViewNotice();
    m_app->m_operateOffered = true;
    m_app->showContext("Show Tree");
    QVERIFY(ProgrammingManager::openFunction(sid));
    Workbench *wb = ProgrammingManager::instance()->workbench();
    QVERIFY(wb != nullptr && wb->isVisible());
    auto shots = [&](const QString &state) {
        if (shotDir.isEmpty())
            return;
        QDir().mkpath(QString::fromLocal8Bit(shotDir));
        for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
        {
            m_app->resize(sz);
            for (int i = 0; i < 5; ++i)
                QCoreApplication::processEvents();
            const QString base = QString::fromLocal8Bit(shotDir)
                + QStringLiteral("/%1-workbench-operate-%2-%3").arg(prefix, state).arg(sz.width());
            QVERIFY(m_app->grab().save(base + QStringLiteral(".png")));
        }
    };
    auto chipText = [this]() { return m_app->stageIndicatorText(nullptr, nullptr); };

    // Blind on opening: the stage keeps the show; the footer does not say Workbench.
    QVERIFY(!wb->playsLive());
    QVERIFY(!scene->isRunning());
    QVERIFY(!chipText().contains("Workbench"));
    shots("blind");

    // One click: live, and the footer says so.
    QToolButton *play = wb->findChild<QToolButton *>("workbenchPlay");
    QVERIFY(play != nullptr && play->isVisible() && play->isEnabled());
    play->click();
    QVERIFY(wb->playsLive());
    QVERIFY2(chipText().contains("Workbench: 01-01.01-Wash"), qPrintable(chipText()));
    QVERIFY(m_app->m_statusStage->text().contains("LIVE"));
    shots("live");

    // A second click: Blind again, the footer back.
    play->click();
    QVERIFY(!wb->playsLive());
    QVERIFY(!chipText().contains("Workbench"));

    // Leaving (Design) is Blind too.
    play->click();
    QVERIFY(wb->playsLive());
    doc->setMode(Doc::Design);
    QVERIFY(!wb->playsLive());
    QVERIFY(!chipText().contains("Workbench"));
}

/* Q150: the venue chip is in the footer, app-wide: "Set Venue…" in the
   qlcconsole and Everything views, in Classic only once the show has
   venues; View ▸ Venue… and the Lighting Studio's Focus ▸ Venues ▸ open
   the same menu as the chip. */
void AppViews_Test::theVenueChipIsInTheFooter()
{
    VenueChip *chip = m_app->findChild<VenueChip *>(QStringLiteral("footerVenue"));
    QVERIFY(chip != nullptr);
    QCOMPARE(chip, m_app->m_venueChip);
    VenueCorrections *vc = m_app->m_doc->programmer()->venueCorrections();
    QTemporaryDir dir;
    auto back = qScopeGuard([this, vc]() {
        vc->reset();
        m_app->setWorkspaceView(App::ConsoleView);
        m_app->updateVenueFooter();
    });

    m_app->setWorkspaceView(App::ConsoleView);
    QCOMPARE(chip->text(), QString("Set Venue\u2026"));
    QVERIFY(!chip->isHidden());
    QCOMPARE(m_app->venueFooterText(), QString("Set Venue\u2026"));
    m_app->setWorkspaceView(App::EverythingView);
    QVERIFY(!chip->isHidden());
    // Classic: QLC+'s footer, until the show has a venue.
    m_app->setWorkspaceView(App::ClassicView);
    QVERIFY(chip->isHidden());
    QCOMPARE(m_app->venueFooterText(), QString());
    vc->setShowFile(dir.filePath("Tour.qlcc"), false);
    QVERIFY(vc->setActiveVenue(vc->addVenue("Lincoln HS")));
    QVERIFY(!chip->isHidden());
    QCOMPARE(m_app->venueFooterText(), QString("Venue: Lincoln HS"));
    m_app->setWorkspaceView(App::ConsoleView);

    // View ▸ Venue…: the chip's menu, opened (not modal).
    QAction *viewVenue = m_app->findChild<QAction *>(QStringLiteral("viewVenueAction"));
    QVERIFY(viewVenue != nullptr);
    viewVenue->trigger();
    QPointer<QMenu> menu = chip->menu();
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->isVisible());
    QStringList chipItems;
    for (QAction *a : menu->actions())
    {
        if (!a->isSeparator())
        {
            chipItems << a->objectName();
        }
    }
    QCOMPARE(chipItems, QStringList({ "venueSwitch", "venueNew", "venueRename", "venueRig", "venueFocus",
                                      "venueReport" }));
    menu->close();

    // Focus ▸ Venues ▸ in the Lighting Studio: the same menu, the same entries.
    QWidget studio;
    QMenu focus;
    FocusMenu::fill(&focus, m_app->m_doc, &studio, []() {});
    QMenu *venues = nullptr;
    for (QAction *a : focus.actions())
    {
        if (a->objectName() == QLatin1String("focusVenues"))
        {
            venues = a->menu();
        }
    }
    QVERIFY(venues != nullptr);
    QStringList focusItems;
    for (QAction *a : venues->actions())
    {
        if (!a->isSeparator())
        {
            focusItems << a->objectName();
        }
    }
    QCOMPARE(focusItems, chipItems);

    // Switched there: the footer's chip follows.
    vc->addVenue("Kennedy");
    FocusMenu::fill(&focus, m_app->m_doc, &studio, []() {});
    for (QAction *a : focus.actions())
    {
        if (a->objectName() == QLatin1String("focusVenues"))
        {
            venues = a->menu();
        }
    }
    QMenu *sw = venues->findChild<QMenu *>(QStringLiteral("venueSwitchMenu"));
    QVERIFY(sw != nullptr);
    for (QAction *a : sw->actions())
    {
        if (a->text() == QLatin1String("Kennedy"))
        {
            a->trigger();
        }
    }
    QCOMPARE(vc->activeVenue(), QString("Kennedy"));
    QCOMPARE(chip->text(), QString("Venue: Kennedy"));
}

// Changelog (Teaching, output hold): the "Run this show for real?" question is
// never asked of a show that opens itself in Operate, from a file or the web.
void AppViews_Test::aLoadedTeachingShowOpensInOperateWithoutAsking()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    QTemporaryDir dir;
    auto back = qScopeGuard([this]() {
        if (m_app->doc()->mode() == Doc::Operate)
            m_app->slotModeDesign();
        m_app->doc()->teachingSettings()->setEnabled(false);
        m_app->clearDocument();
        NetworkRoute::setResolverForTest(nullptr, nullptr);
    });

    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = doc->ioPluginCache();
    // an earlier test may have loaded it already, under the name it gave it
    QLCIOPlugin *plug = cache->plugin(QStringLiteral("ArtNet"));
    if (plug == nullptr)
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    if (plug == nullptr)
    {
        cache->load(stubDir);
        plug = cache->plugin(QStringLiteral("I/O Plugin Stub"));
    }
    IOPluginStub *stub = static_cast<IOPluginStub *>(plug);
    QVERIFY2(stub != nullptr, "the I/O plugin stub did not load");
    stub->m_nameOverride = QStringLiteral("ArtNet");
    stub->m_lineNames = QStringList() << QStringLiteral("10.4.12.7");
    NetworkRoute::LocalAddress school;
    school.iface = "en0";
    school.ip = QHostAddress("10.4.12.7");
    school.prefix = 16;
    NetworkRoute::setResolverForTest(
        [school]() { return QList<NetworkRoute::LocalAddress>() << school; },
        [](const QHostAddress &) { return QHostAddress("10.4.12.7"); });

    // A student copy that looks real: output let through, patched, opens in Operate
    doc->teachingSettings()->setEnabled(true);
    doc->teachingSettings()->setAllowOutput(true);
    doc->operateSettings()->setOpenInOperate(true);
    QXmlStreamReader xml(QStringLiteral(
        "<InputOutputMap>\n"
        " <Universe Name=\"U01\" ID=\"0\">\n"
        "  <Output Plugin=\"ArtNet\" UID=\"10.4.12.7\" Line=\"0\"/>\n"
        " </Universe>\n"
        "</InputOutputMap>\n"));
    xml.readNextStartElement();
    QVERIFY(doc->inputOutputMap()->loadXML(xml));
    QVERIFY(doc->inputOutputMap()->transmitGateStatus().networkOutputs > 0);

    const QString path = dir.filePath(QStringLiteral("student") + KExtWorkspaceConsole);
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString saved = QString::fromUtf8(f.readAll());
    f.close();
    QVERIFY(saved.contains(QLatin1String("Teaching")));

    // Open: the show opens itself in Operate, and nobody is asked
    m_app->clearDocument();
    bool boxed = false;
    QTimer probe;
    probe.setInterval(10);
    connect(&probe, &QTimer::timeout, this, [&boxed]() {
        if (qobject_cast<QMessageBox *>(modalOrFixtureEditor()) != nullptr)
            boxed = true;
    });
    probe.start();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QTRY_COMPARE_WITH_TIMEOUT(m_app->doc()->mode(), Doc::Operate, 3000);
    QTest::qWait(60);
    QVERIFY(m_app->doc()->teachingSettings()->enabled());
    QVERIFY(m_app->doc()->teachingSettings()->allowOutput());
    QVERIFY(!boxed);
    m_app->slotModeDesign();

    // The same show sent to the web page's load: no dialog either
    m_app->clearDocument();
    m_app->slotLoadDocFromMemory(saved);
    QTest::qWait(60);
    QVERIFY(!boxed);
}

/* The footer's right side is one line: selection, mode, stage chip, the Follow chip and
   the autosave label share a vertical centre and a height, and the gaps are equal. */
void AppViews_Test::theFooterRightSideSharesOneLine()
{
    const QSize oldSize = m_app->size();
    m_app->m_doc->setMode(Doc::Design);
    m_app->hideViewNotice();
    m_app->show();
    m_app->m_statusSelectionLabel->setText("Selected: Lanterns (6)");
    m_app->m_statusSelectionLabel->show();
    m_app->m_statusFollowLabel->setText(QString::fromUtf8("\xe2\x97\x8e Follow \xc2\xb7 Lanterns"));
    m_app->m_statusFollowLabel->show();
    const QList<QWidget *> items = { m_app->m_statusSelectionLabel, m_app->m_statusFollowLabel,
                                     m_app->m_statusModeChipLabel, m_app->m_statusStage,
                                     m_app->m_statusDirtyLabel };
    const QByteArray shotDir = qgetenv("QLC_SHOT_DIR");
    for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        m_app->resize(sz);
        for (int i = 0; i < 5; ++i)
            QCoreApplication::processEvents();
        if (!shotDir.isEmpty())
            m_app->statusBar()->grab().save(QString::fromLocal8Bit(shotDir)
                + QStringLiteral("/after-follow-%1-footer.png").arg(sz.width()));
        QList<QRect> r;
        for (QWidget *w : items)
        {
            if (!w->isVisibleTo(m_app->statusBar()))
                continue;
            r << QRect(w->mapTo(m_app->statusBar(), QPoint(0, 0)), w->size());
        }
        QVERIFY2(r.size() >= 3, qPrintable(QString::number(r.size())));
        for (const QRect &q : r)
        {
            QVERIFY2(qAbs(q.center().y() - r.first().center().y()) <= 1, qPrintable(QString::number(sz.width())));
            QCOMPARE(q.height(), r.first().height());
        }
        const int gap = r[1].left() - r[0].right();
        for (int i = 1; i < r.size(); ++i)
            QVERIFY2(qAbs((r[i].left() - r[i - 1].right()) - gap) <= 1, qPrintable(QString::number(sz.width())));
    }
    m_app->resize(oldSize);
}

/* The footer's stage chip (CHANGELOG [Unreleased]): one item that always says what lights
   the stage -- a cue list running (LIVE), Blind (the stage holds what it had), the Build
   look, nothing, and the holds (Blackout, Teaching). Driven from the engine's signals:
   a list started, stopped, Blind toggled. With QLC_SHOT_DIR set it also saves the footer
   at the three sizes (QLC_SHOT_PREFIX before|after) for the visual check. */
void AppViews_Test::theFooterSaysWhatLightsTheStage()
{
    Doc *doc = m_app->m_doc;
    ShowTreeController *st = doc->showTree();
    QList<quint32> fixtures, functions;
    QList<quint32> groups, palettes;
    quint32 prod = ShowTree::invalidId();
    const int oldTheme = m_app->theme();
    const QSize oldSize = m_app->size();
    auto tidy = qScopeGuard([&]() {
        doc->teachingSettings()->setEnabled(false);
        doc->inputOutputMap()->setBlackout(false);
        doc->inputOutputMap()->setOutputInhibited(false);
        m_app->setBlindView(false);
        st->stopPreview();
        doc->setMode(Doc::Design);
        doc->masterTimer()->stopAllFunctions();
        if (prod != ShowTree::invalidId())
            st->removeProduction(prod);
        takeAway(doc, fixtures, functions);
        for (quint32 g : groups)
            doc->deleteFixtureGroup(g);
        for (quint32 p : palettes)
            doc->deletePalette(p);
        m_app->hideViewNotice();
        m_app->setTheme(oldTheme);
        m_app->resize(oldSize);
        QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, 5000);
        if (const QTimer *sweep = st->findChild<QTimer *>(QString(), Qt::FindDirectChildrenOnly))
            QTRY_VERIFY_WITH_TIMEOUT(!sweep->isActive(), 5000);
        QCoreApplication::processEvents();
    });
    QVERIFY(litScene(doc, 240, 1, &fixtures, &functions) != nullptr);
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName("Stage");
    doc->addFixtureGroup(g);
    g->assignFixture(fixtures.first());
    groups << g->id();
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    palettes << full->id();
    prod = st->addProduction("Chip");
    const quint32 listId = st->addCueList(prod, "Act 1", ShowTree::ListMode::NonTracking);
    const quint32 c1 = st->addCue(listId, -1, "Verse");
    const quint32 list2 = st->addCueList(prod, "Act 2", ShowTree::ListMode::NonTracking);
    const quint32 c2 = st->addCue(list2, -1, "Chorus");
    ShowTree::Target t;
    t.groups << g->id();
    st->setLayerLook(c1, st->addLayer(c1, t), st->lookForPalette(full->id()));
    st->setLayerLook(c2, st->addLayer(c2, t), st->lookForPalette(full->id()));
    st->setCurrentProduction(prod);
    Chaser *list = qobject_cast<Chaser *>(doc->function(st->cueList(listId)->chaser));
    QVERIFY(list != nullptr);
    doc->showControl()->setMainCueList(list->id());
    doc->setMode(Doc::Design);
    m_app->hideViewNotice();
    m_app->m_operateOffered = true;

    QToolButton *chip = m_app->findChild<QToolButton *>(QStringLiteral("footerStage"));
    QVERIFY(chip != nullptr);
    QCOMPARE(chip, m_app->m_statusStage);

    // The sizes the visual check looks at (the window's minimum, a laptop, wide), dark theme.
    const QByteArray shotDir = qgetenv("QLC_SHOT_DIR");
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));
    m_app->setTheme(qEnvironmentVariableIsSet("QLC_SHOT_LIGHT") ? App::ThemeDefault : App::ThemeVSCodeDark);
    m_app->show();
    auto shots = [&](const QString &state) {
        if (shotDir.isEmpty())
            return;
        QDir().mkpath(QString::fromLocal8Bit(shotDir));
        const QList<QPair<QSize, QString> > sizes = { { QSize(1024, 640), "1024x640" },
                                                      { QSize(1280, 800), "1280x800" },
                                                      { QSize(1920, 1080), "1920x1080" } };
        for (const auto &sz : sizes)
        {
            m_app->resize(sz.first);
            for (int i = 0; i < 5; ++i)
                QCoreApplication::processEvents();
            const QString base = QString::fromLocal8Bit(shotDir) + QStringLiteral("/%1-%2-%3")
                                     .arg(prefix, state, sz.second);
            m_app->grab().save(base + QStringLiteral(".png"));
            m_app->statusBar()->grab().save(base + QStringLiteral("-footer.png"));
        }
    };
    // Nothing running, nothing built.
    QVERIFY(!chip->isHidden());
    QVERIFY(m_app->m_stage == App::Stage::NoList);
    QVERIFY(chip->text() == QStringLiteral("Stage: no cue list"));
    shots("nolist");

    // The Workbench / Show Tree preview is what is on: the Build look.
    st->previewCue(c1);
    QVERIFY(m_app->m_stage == App::Stage::BuildLook);
    QVERIFY2(chip->text().startsWith(QStringLiteral("Stage: 1 Verse")), qPrintable(chip->text()));
    QVERIFY(chip->toolTip().startsWith(QStringLiteral("Stage: 1 Verse (Show Tree)")) || chip->text().endsWith(")"));
    QVERIFY(chip->toolTip().contains("selected in the Show Tree"));
    shots("buildlook");
    // The Show Tree page, toolbar and all, with that cue selected (the visual check).
    if (!shotDir.isEmpty())
    {
        if (ShowTreeView *view = ShowTreeView::instance())
            view->selectNode(c1);
        m_app->showContext(QStringLiteral("Show Tree"));
        shots("showtree");
        m_app->showContext(QStringLiteral("Run"));
    }
    st->stopPreview();
    QVERIFY(m_app->m_stage == App::Stage::NoList);

    // A cue list starts for real: LIVE, with the list and the cue.
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_stage == App::Stage::Live, 5000);
    QCOMPARE(chip->text(), QString::fromUtf8("\xe2\x97\x8f LIVE \xc2\xb7 Act 1 \xc2\xb7 1 Verse"));
    QVERIFY(chip->toolTip().contains("lights the stage"));
    shots("live");

    // The Show Tree's on-rig cue is what the chip names, not the Run tab's last cue: pick
    // another cue and play it over the running list (the list goes on underneath)...
    st->previewCue(c2);
    st->resumePreview();
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains("Chorus"), 5000);
    // (the text may be shortened to the room; the tooltip keeps all of it)
    const QString whole = chip->text() + QStringLiteral("\n") + chip->toolTip();
    QVERIFY2(whole.contains(QString::fromUtf8("(Show Tree) \xc2\xb7 Act 1 running")) && !chip->text().contains("Verse"),
             qPrintable(whole));
    shots("ontherig-over-run");
    // The chip is never drawn cut at the sizes the visual check looks at: what shows is a
    // whole form of the line (it shortens longest-first before it elides), inside the chip.
    for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        m_app->resize(sz);
        for (int i = 0; i < 5; ++i)
            QCoreApplication::processEvents();
        m_app->updateStageIndicator();
        QFont bold = chip->font();
        bold.setBold(true);
        const QString at = QStringLiteral("%1 wide: \"%2\" in %3 px").arg(sz.width()).arg(chip->text()).arg(chip->width());
        QVERIFY2(chip->text().startsWith(QStringLiteral("Stage: ")), qPrintable(at));
        QVERIFY2(!chip->text().contains(QString::fromUtf8("\xe2\x80\xa6")), qPrintable(at + " is elided"));
        QVERIFY2(chip->text().endsWith(QLatin1Char(')')) || chip->text().endsWith(QStringLiteral("Chorus"))
                     || chip->text().endsWith(QStringLiteral("running")), qPrintable(at + " is cut"));
        QVERIFY2(QFontMetrics(bold).horizontalAdvance(chip->text()) <= chip->width(), qPrintable(at + " is wider than the chip"));
    }
    m_app->resize(1280, 800);
    // ...the list stops: still the on-rig cue, now alone...
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    st->previewCue(c2);
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains("Chorus") && !chip->text().contains("running"),
                              qPrintable(chip->text()), 5000);
    // ...and picking a different cue moves the chip with it.
    st->previewCue(c1);
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains("Verse") && !chip->text().contains("Chorus"),
                              qPrintable(chip->text()), 5000);
    shots("ontherig");
    st->stopPreview();
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_stage == App::Stage::NoList, 5000);
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_stage == App::Stage::Live, 5000);

    // GO takes the stage while a cue is selected in the Show Tree: the preview is held,
    // and the chip says so (what the old toolbar's Held line did).
    st->previewCue(c2);
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(int(st->previewState()) == int(ShowTreeController::PreviewState::Held), 5000);
    m_app->resize(1280, 800);
    QCoreApplication::processEvents();
    m_app->updateStageIndicator();
    // (this one-cue list may have run out on that GO: LIVE, or "no cue list", either way held)
    QVERIFY2((m_app->m_stage == App::Stage::Live || m_app->m_stage == App::Stage::NoList)
             && (chip->text() + chip->toolTip()).contains(QStringLiteral("Show Tree preview held")),
             qPrintable(chip->text()));
    QVERIFY(chip->toolTip().contains("Chorus") && chip->toolTip().contains("Play"));
    shots("gohold");
    st->resumePreview();            // Play: the chip names the on-rig cue again
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains("Chorus") && !chip->toolTip().contains("Show Tree preview held"),
                              qPrintable(chip->text()), 5000);
    st->stopPreview();
    QTRY_VERIFY2_WITH_TIMEOUT(!chip->text().contains("held") && !chip->text().contains("Show Tree)"),
                              qPrintable(chip->text()), 5000);
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_stage == App::Stage::Live, 5000);

    // Blind (Operate's Blind view): the stage holds the cue while the screens show the edit.
    doc->setMode(Doc::Operate);
    m_app->setBlindView(true);
    QVERIFY(m_app->m_stage == App::Stage::BlindView);
    m_app->resize(1280, 800);
    QCoreApplication::processEvents();
    m_app->updateStageIndicator();
    QCOMPARE(chip->text(), QString::fromUtf8("BLIND \xc2\xb7 stage holds 1 Verse"));
    shots("blind");
    m_app->setBlindView(false);
    QVERIFY(m_app->m_stage == App::Stage::Live);
    shots("blindoff");

    // The list stops: back to nothing.
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_stage == App::Stage::NoList, 5000);
    m_app->setBlindView(true);
    QVERIFY(chip->text() == QString::fromUtf8("BLIND \xc2\xb7 stage: no cue list"));
    m_app->setBlindView(false);
    doc->setMode(Doc::Design);

    // Design's Blind mutes the rig.
    doc->inputOutputMap()->setOutputInhibited(true);
    QVERIFY(m_app->m_stage == App::Stage::BlindMuted);
    QVERIFY(chip->text() == QString::fromUtf8("BLIND \xc2\xb7 rig muted"));
    shots("muted");
    doc->inputOutputMap()->setOutputInhibited(false);

    // Blackout and Teaching's output hold say so too.
    doc->inputOutputMap()->setBlackout(true);
    QVERIFY(m_app->m_stage == App::Stage::Blackout);
    shots("blackout");
    doc->inputOutputMap()->setBlackout(false);
    doc->teachingSettings()->setEnabled(true);
    QVERIFY(m_app->m_stage == App::Stage::Held);
    QVERIFY(chip->text().startsWith(QStringLiteral("HELD")));
    shots("held");
    doc->teachingSettings()->setEnabled(false);
    QVERIFY(m_app->m_stage == App::Stage::NoList);

    // A long cue name shortens to the room, the tooltip keeps it whole.
    m_app->resize(1024, 640);
    QVERIFY(st->renameNode(c1, QString(120, 'x')));
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_stage == App::Stage::Live, 5000);
    QVERIFY(chip->text().endsWith(QString::fromUtf8("\xe2\x80\xa6")));
    QVERIFY(chip->sizeHint().width() <= m_app->width() / 3 + 16);
    QVERIFY(chip->toolTip().contains(QString(120, 'x')));
}

/* Dump DMX opens a dialog: closed as soon as it shows, and counted. */
struct DumpWatch
{
    explicit DumpWatch(App *app) : dumped(app->m_dumpDmxAction, &QAction::triggered)
    {
        QObject::connect(&closer, &QTimer::timeout, []() {
            if (QWidget *modal = modalOrFixtureEditor())
                modal->close();
        });
        closer.start(50);
    }
    QSignalSpy dumped;
    QTimer closer;
};

/* The key as a person types it: to the focused widget, through the main
   window's shortcut map. */
static void typeKey(QWidget *w, Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    w->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(w->hasFocus());
    QTest::keyClick(QApplication::focusWidget(), key, mods);
    QCoreApplication::processEvents();
}

static QWidget *showFront(App *app, const QString &context)
{
    app->show();
    app->activateWindow();
    if (!QTest::qWaitForWindowActive(app))
        return nullptr;
    app->showContext(context);
    QCoreApplication::processEvents();
    return app->m_tab->currentWidget();
}

/* Before / after pictures of the edit keys (WFKEYS_SHOTS=dir,
   WFKEYS_PREFIX=before|after): the window at three sizes, dark. Nothing
   without the variable. */
static void wfShots(App *app, const QString &name)
{
    const QString dir = qEnvironmentVariable("WFKEYS_SHOTS");
    if (dir.isEmpty())
        return;
    QTest::qWait(100);
    const QString prefix = qEnvironmentVariable("WFKEYS_PREFIX", QStringLiteral("after"));
    const int was = app->theme();
    const QSize size = app->size();
    app->setTheme(App::ThemeVSCodeDark);
    for (const QSize &s : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        app->resize(s);
        QCoreApplication::processEvents();
        app->grab().save(QStringLiteral("%1/%2-%3-%4x%5.png").arg(dir, prefix, name)
                         .arg(s.width()).arg(s.height()));
    }
    app->setTheme(was);
    app->resize(size);
    QCoreApplication::processEvents();
}

void AppViews_Test::ctrlDOnTheShowTreeDuplicates()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction(QStringLiteral("P")), QStringLiteral("Main"));
    const quint32 cue = st->addCue(list, -1, QStringLiteral("One"));
    QVERIFY(showFront(m_app, QStringLiteral("Show Tree")) != nullptr);
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    QTreeWidget *tree = view->findChild<QTreeWidget *>(QStringLiteral("stTree"));
    QVERIFY(tree != nullptr);
    QVERIFY(view->selectNode(cue));
    QCOMPARE(st->cueList(list)->cues.count(), 1);

    DumpWatch dump(m_app);
    typeKey(tree, Qt::Key_D, Qt::ControlModifier);
    wfShots(m_app, QStringLiteral("showtree-cmd-d"));
    QTRY_COMPARE(st->cueList(list)->cues.count(), 2);
    QCOMPARE(dump.dumped.count(), 0);

    // Nothing selected: Ctrl+D does nothing (Dump DMX is Alt+Ctrl+D).
    tree->clearSelection();
    typeKey(tree, Qt::Key_D, Qt::ControlModifier);
    QCOMPARE(st->cueList(list)->cues.count(), 2);
    QCOMPARE(dump.dumped.count(), 0);
    typeKey(tree, Qt::Key_D, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(st->cueList(list)->cues.count(), 2);
    QCOMPARE(dump.dumped.count(), 1);
    m_app->clearDocument();
}

void AppViews_Test::ctrlDInBuildListsDuplicates()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    QLCPalette *pal = new QLCPalette(QLCPalette::Dimmer, doc);
    pal->setName(QStringLiteral("Full"));
    pal->setValue(255);
    QVERIFY(doc->addPalette(pal));
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("Opening"));
    QVERIFY(doc->addFunction(scene));
    EFX *efx = new EFX(doc);
    efx->setName(QStringLiteral("Circle"));
    QVERIFY(doc->addFunction(efx));
    QVERIFY(showFront(m_app, QStringLiteral("Build")) != nullptr);
    ProgrammingManager *pm = ProgrammingManager::instance();
    QVERIFY(pm != nullptr);
    DumpWatch dump(m_app);

    // Palettes.
    pm->setSection(ProgrammingManager::PalettesSection);
    pm->m_paletteTree->updateTree();
    pm->revealPalette(pal->id());
    QCoreApplication::processEvents();
    QVERIFY(!pm->m_paletteTree->selectedItems().isEmpty());
    const int palettes = doc->palettes().count();
    typeKey(pm->m_paletteTree, Qt::Key_D, Qt::ControlModifier);
    wfShots(m_app, QStringLiteral("build-palettes-cmd-d"));
    QTRY_COMPARE(doc->palettes().count(), palettes + 1);
    QCOMPARE(dump.dumped.count(), 0);

    // Scenes (the outline).
    pm->setSection(ProgrammingManager::ScenesSection);
    pm->m_outline->rebuild();
    QVERIFY(pm->m_outline->selectRow({ ProgrammingOutline::SceneRow, Function::invalidId(), scene->id() }));
    const int functions = doc->functions().count();
    typeKey(pm->m_outline, Qt::Key_D, Qt::ControlModifier);
    QTRY_COMPARE(doc->functions().count(), functions + 1);
    QCOMPARE(dump.dumped.count(), 0);

    // Effects: Ctrl+D, then Ctrl+C / Ctrl+V copy the effect, not a scene.
    pm->setSection(ProgrammingManager::EffectsSection);
    pm->rebuildEffectsNow();
    pm->selectEffectRow(false, efx->id());
    QVERIFY(!pm->m_effectsList->selectedItems().isEmpty());
    typeKey(pm->m_effectsList, Qt::Key_D, Qt::ControlModifier);
    QTRY_COMPARE(doc->functions().count(), functions + 2);
    QCOMPARE(dump.dumped.count(), 0);
    auto efxCount = [doc]() {
        int n = 0;
        foreach (Function *f, doc->functions())
            n += f->type() == Function::EFXType ? 1 : 0;
        return n;
    };
    QCOMPARE(efxCount(), 2);
    pm->rebuildEffectsNow();
    pm->selectEffectRow(false, efx->id());
    typeKey(pm->m_effectsList, Qt::Key_C, Qt::ControlModifier);
    typeKey(pm->m_effectsList, Qt::Key_V, Qt::ControlModifier);
    QTRY_COMPARE(efxCount(), 3);

    // Nothing selected: Ctrl+D does nothing; Alt+Ctrl+D is Dump DMX.
    pm->m_effectsList->clearSelection();
    typeKey(pm->m_effectsList, Qt::Key_D, Qt::ControlModifier);
    QCOMPARE(dump.dumped.count(), 0);
    QCOMPARE(efxCount(), 3);
    typeKey(pm->m_effectsList, Qt::Key_D, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(dump.dumped.count(), 1);
    QCOMPARE(efxCount(), 3);
    m_app->clearDocument();
}

void AppViews_Test::editKeysInTheFixturesTab()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    Fixture *fx = new Fixture(doc);
    fx->setName(QStringLiteral("Front"));
    fx->setChannels(1);
    fx->setAddress(0);
    fx->setUniverse(0);
    QVERIFY(doc->addFixture(fx));
    FixtureGroup *grp = new FixtureGroup(doc);
    grp->setName(QStringLiteral("Wash"));
    grp->setSize(QSize(2, 1));
    QVERIFY(doc->addFixtureGroup(grp));
    grp->assignFixture(fx->id());
    QVERIFY(showFront(m_app, QStringLiteral("Fixtures")) != nullptr);
    FixtureManager *fm = FixtureManager::instance();
    QVERIFY(fm != nullptr);
    FixtureTreeWidget *tree = fm->m_fixtures_tree;
    tree->updateTree();
    QVERIFY(fm->selectFixture(fx->id()));
    tree->currentItem()->setSelected(true);
    DumpWatch dump(m_app);

    // Ctrl+D: a new fixture like it, unpatched (the Inventory), named on.
    typeKey(tree, Qt::Key_D, Qt::ControlModifier);
    wfShots(m_app, QStringLiteral("fixtures-cmd-d"));
    QTRY_COMPARE(doc->fixtures().count(), 2);
    QCOMPARE(dump.dumped.count(), 0);
    Fixture *copy = nullptr;
    foreach (Fixture *f, doc->fixtures())
        if (f->id() != fx->id())
            copy = f;
    QVERIFY(copy != nullptr);
    QVERIFY(!copy->isPatched());
    QVERIFY(fx->isPatched());
    QCOMPARE(copy->channels(), fx->channels());
    QVERIFY(copy->name() != fx->name());
    QVERIFY(copy->name().startsWith(QStringLiteral("Front")));

    // Ctrl+C, Ctrl+V: another.
    QVERIFY(fm->selectFixture(fx->id()));
    tree->currentItem()->setSelected(true);
    typeKey(tree, Qt::Key_C, Qt::ControlModifier);
    typeKey(tree, Qt::Key_V, Qt::ControlModifier);
    QTRY_COMPARE(doc->fixtures().count(), 3);

    // A group: a copy of it, the same members.
    tree->clearSelection();
    QTreeWidgetItem *grpRow = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
        if ((*it)->data(0, PROP_GROUP).isValid()
            && (*it)->data(0, PROP_GROUP).toUInt() == grp->id())
            grpRow = *it;
    QVERIFY(grpRow != nullptr);
    grpRow->setSelected(true);
    tree->setCurrentItem(grpRow, 0, QItemSelectionModel::NoUpdate);
    typeKey(tree, Qt::Key_D, Qt::ControlModifier);
    QTRY_COMPARE(doc->fixtureGroups().count(), 2);
    FixtureGroup *gcopy = doc->fixtureGroups().last() == grp ? doc->fixtureGroups().first()
                                                             : doc->fixtureGroups().last();
    QCOMPARE(gcopy->fixtureList(), grp->fixtureList());
    QVERIFY(gcopy->name() != grp->name());
    QCOMPARE(doc->fixtures().count(), 3);

    // Esc: the selection goes.
    QVERIFY(!tree->selectedItems().isEmpty());
    typeKey(tree, Qt::Key_Escape);
    QVERIFY(tree->selectedItems().isEmpty());
    QCOMPARE(dump.dumped.count(), 0);
    m_app->clearDocument();
}

static Fixture *plotFixture(Doc *doc, MonitorGraphicsView *gv, const QString &name, float xMm)
{
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setChannels(1);
    fx->setAddress(quint32(xMm / 1000.0f));
    if (!doc->addFixture(fx))
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(xMm, 3000.0f, 0));
    gv->addFixture(fx->id(), QPointF(xMm, 3000.0f));
    return fx;
}

void AppViews_Test::editKeysOnThePlot()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    QVERIFY(showFront(m_app, QStringLiteral("Lighting Studio")) != nullptr);
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    Fixture *a = plotFixture(doc, gv, QStringLiteral("Left"), 2000.0f);
    Fixture *b = plotFixture(doc, gv, QStringLiteral("Right"), 5000.0f);
    QVERIFY(a != nullptr && b != nullptr);
    mon->findChild<MonitorLayersPanel *>()->reload();
    gv->scene()->clearSelection();
    gv->fixtureItemForId(a->id())->setSelected(true);
    gv->fixtureItemForId(b->id())->setSelected(true);

    // Esc: the selection goes; nothing else does.
    typeKey(gv, Qt::Key_Escape);
    QVERIFY(gv->scene()->selectedItems().isEmpty());
    QVERIFY(gv->fixtureItemForId(a->id()) != nullptr);

    // Backspace: both off the plot, still in the show; one undo step.
    gv->fixtureItemForId(a->id())->setSelected(true);
    gv->fixtureItemForId(b->id())->setSelected(true);
    typeKey(gv, Qt::Key_Backspace);
    wfShots(m_app, QStringLiteral("plot-backspace"));
    QTRY_VERIFY(gv->fixtureItemForId(a->id()) == nullptr);
    QVERIFY(gv->fixtureItemForId(b->id()) == nullptr);
    QVERIFY(doc->fixture(a->id()) != nullptr && doc->fixture(b->id()) != nullptr);
    QVERIFY(!doc->monitorProperties()->containsFixture(a->id()));
    QVERIFY(mon->canUndo());
    mon->undo();
    QTRY_VERIFY(gv->fixtureItemForId(a->id()) != nullptr);
    QVERIFY(gv->fixtureItemForId(b->id()) != nullptr);

    // F2: the selected item's row in Plot layers, renaming.
    gv->scene()->clearSelection();
    gv->fixtureItemForId(a->id())->setSelected(true);
    typeKey(gv, Qt::Key_F2);
    QLineEdit *editor = qobject_cast<QLineEdit *>(QApplication::focusWidget());
    QVERIFY(editor != nullptr);
    QVERIFY(mon->findChild<MonitorLayersPanel *>()->m_tree->isAncestorOf(editor));
    QCOMPARE(editor->text(), QStringLiteral("Left"));
    QTest::keyClick(editor, Qt::Key_Escape);
    QCoreApplication::processEvents();

    // Cmd/Ctrl+F: Plot layers' search.
    typeKey(gv, Qt::Key_F, Qt::ControlModifier);
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(mon->findChild<MonitorLayersPanel *>()->m_search));
    m_app->clearDocument();
}

void AppViews_Test::editKeysInPlotLayers()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    QVERIFY(showFront(m_app, QStringLiteral("Lighting Studio")) != nullptr);
    Monitor *mon = Monitor::instance();
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    MonitorProperties *props = doc->monitorProperties();
    StageTarget *t = props->addStageTarget();
    t->setName(QStringLiteral("Soloist"));
    t->setKind(StageTarget::Kind::Person);
    t->setPosition(QVector3D(3.0f, 3.0f, 0.0f));
    gv->updateTargets();
    MonitorLayersPanel *panel = mon->findChild<MonitorLayersPanel *>();
    QVERIFY(panel != nullptr);
    panel->setVisible(true);
    panel->reload();
    // Picked on the plot: the list follows (as a click on its row does the
    // other way).
    gv->scene()->clearSelection();
    gv->m_targetItems.value(t->id())->setSelected(true);
    QCoreApplication::processEvents();
    QVERIFY(!panel->selectedObjects().isEmpty());
    QVERIFY(!gv->scene()->selectedItems().isEmpty());
    const int before = props->stageTargets().size();
    DumpWatch dump(m_app);

    // Cmd/Ctrl+D: as on the plot.
    typeKey(panel->m_tree, Qt::Key_D, Qt::ControlModifier);
    QTRY_COMPARE(props->stageTargets().size(), before + 1);
    QCOMPARE(dump.dumped.count(), 0);

    // Esc: the list and the plot let go.
    panel->reload();
    // Picked on the plot: the list follows (as a click on its row does the
    // other way).
    gv->scene()->clearSelection();
    gv->m_targetItems.value(t->id())->setSelected(true);
    QCoreApplication::processEvents();
    QVERIFY(!panel->selectedObjects().isEmpty());
    typeKey(panel->m_tree, Qt::Key_Escape);
    wfShots(m_app, QStringLiteral("plotlayers-esc"));
    QVERIFY(panel->m_tree->selectedItems().isEmpty());
    QVERIFY(gv->scene()->selectedItems().isEmpty());

    // Cmd/Ctrl+F: the search.
    typeKey(panel->m_tree, Qt::Key_F, Qt::ControlModifier);
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(panel->m_search));

    // Backspace: off the plot (a target asks first: yes). Selected on the
    // plot, the list follows.
    gv->scene()->clearSelection();
    gv->m_targetItems.value(t->id())->setSelected(true);
    QCoreApplication::processEvents();
    QVERIFY(!panel->selectedObjects().isEmpty());
    dump.closer.stop();
    QTimer yes;
    connect(&yes, &QTimer::timeout, [&]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(modalOrFixtureEditor()))
        {
            for (QAbstractButton *btn : box->buttons())
                if (box->buttonRole(btn) == QMessageBox::AcceptRole
                    || box->buttonRole(btn) == QMessageBox::YesRole
                    || box->buttonRole(btn) == QMessageBox::DestructiveRole)
                {
                    btn->click();
                    return;
                }
            box->accept();
        }
    });
    yes.start(50);
    typeKey(panel->m_tree, Qt::Key_Backspace);
    QTRY_COMPARE(props->stageTargets().size(), before);
    yes.stop();
    m_app->clearDocument();
}

void AppViews_Test::escBacksOutThenClearsTheSelection()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    QLCPalette *pal = new QLCPalette(QLCPalette::Dimmer, doc);
    pal->setName(QStringLiteral("Full"));
    pal->setValue(255);
    QVERIFY(doc->addPalette(pal));
    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction(QStringLiteral("P")), QStringLiteral("Main"));
    const quint32 cue = st->addCue(list, -1, QStringLiteral("One"));
    QVERIFY(showFront(m_app, QStringLiteral("Show Tree")) != nullptr);
    ShowTreeView *view = ShowTreeView::instance();
    QTreeWidget *tree = view->findChild<QTreeWidget *>(QStringLiteral("stTree"));
    QVERIFY(view->selectNode(cue));
    view->editPalette(pal->id());
    QVERIFY(view->editedPalette() == pal->id());

    // The first Esc closes the Look Editor; the cue stays selected.
    typeKey(tree, Qt::Key_Escape);
    QVERIFY(view->editedPalette() != pal->id());
    QVERIFY(!tree->selectedItems().isEmpty());
    // The next lets go of the selection.
    typeKey(tree, Qt::Key_Escape);
    QVERIFY(tree->selectedItems().isEmpty());

    // Build's palette list: the same.
    QVERIFY(showFront(m_app, QStringLiteral("Build")) != nullptr);
    ProgrammingManager *pm = ProgrammingManager::instance();
    pm->setSection(ProgrammingManager::PalettesSection);
    pm->m_paletteTree->updateTree();
    pm->revealPalette(pal->id());
    QCoreApplication::processEvents();
    QVERIFY(!pm->m_paletteTree->selectedItems().isEmpty());
    pm->m_workbench->editPalette(pal->id());
    typeKey(pm->m_paletteTree, Qt::Key_Escape);
    QVERIFY(!pm->m_paletteTree->selectedItems().isEmpty());   // backed out first
    for (int i = 0; i < 4 && !pm->m_paletteTree->selectedItems().isEmpty(); ++i)
        typeKey(pm->m_paletteTree, Qt::Key_Escape);
    QVERIFY(pm->m_paletteTree->selectedItems().isEmpty());
    m_app->clearDocument();
}

void AppViews_Test::ctrlDOnThePlotDuplicates()
{
    m_app->clearDocument();
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    m_app->show();
    m_app->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(m_app));
    m_app->showContext(QStringLiteral("Lighting Studio"));
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    MonitorProperties *props = doc->monitorProperties();
    StageTarget *t = props->addStageTarget();
    t->setName(QStringLiteral("Soloist"));
    t->setKind(StageTarget::Kind::Person);
    t->setPosition(QVector3D(3.0f, 3.0f, 0.0f));
    gv->updateTargets();
    QVERIFY(gv->m_targetItems.value(t->id()) != nullptr);
    gv->scene()->clearSelection();
    gv->m_targetItems.value(t->id())->setSelected(true);
    const int before = props->stageTargets().size();

    // Dump DMX opens a dialog: close it if it ever does, and count it.
    QSignalSpy dumped(m_app->m_dumpDmxAction, &QAction::triggered);
    QTimer closer;
    connect(&closer, &QTimer::timeout, [&]() {
        if (QWidget *modal = modalOrFixtureEditor())
            modal->close();
    });
    closer.start(50);

    // The key as a person types it: to the focused plot, through the
    // main window's shortcut map.
    gv->viewport()->setFocus();
    QTRY_VERIFY(gv->hasFocus() || gv->viewport()->hasFocus());
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_D, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(props->stageTargets().size(), before + 1);
    QCOMPARE(dumped.count(), 0);
    bool named = false;
    foreach (StageTarget *o, props->stageTargets())
        named = named || o->name() == QStringLiteral("Soloist 2");
    QVERIFY(named);

    // Nothing selected: Ctrl+D does nothing; Alt+Ctrl+D is Dump DMX.
    gv->scene()->clearSelection();
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_D, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(props->stageTargets().size(), before + 1);
    QCOMPARE(dumped.count(), 0);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_D, Qt::ControlModifier | Qt::AltModifier);
    QCoreApplication::processEvents();
    QCOMPARE(props->stageTargets().size(), before + 1);
    QCOMPARE(dumped.count(), 1);
    closer.stop();
    m_app->clearDocument();
}

/* A notice above the tabs (showViewNotice) pushes the page down: at each
   window size the bar ends where the tabs begin, and no button or box of the
   page on show is cut off by the edge of the widget holding it. */
void AppViews_Test::theViewNoticePushesThePageDown()
{
    m_app->clearDocument();
    m_app->show();
    QTabWidget *tabs = m_app->findChild<QTabWidget *>("MainTabs");
    QVERIFY(tabs != nullptr);
    const QSize sizes[] = { m_app->minimumSizeHint(), QSize(1280, 800), QSize(1920, 1080) };
    for (const QString &label : { QString("Connections"), QString("Show Tree"), QString("Run") })
    {
        int at = -1;
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabBar()->tabData(i).toString() == label) // the text may be short or an icon (tab fit)
                at = i;
        QVERIFY2(at >= 0, qPrintable(label));
        tabs->setCurrentIndex(at);
        for (const QSize &sz : sizes)
        {
            m_app->resize(sz);
            m_app->showViewNotice(QStringLiteral("Show control held: Teaching (cue 1 not sent)"));
            QTest::qWait(60);
            QVERIFY(m_app->m_viewNotice->isVisible());
            const QRect bar(m_app->m_viewNotice->mapTo(m_app, QPoint(0, 0)), m_app->m_viewNotice->size());
            const QRect tab(tabs->mapTo(m_app, QPoint(0, 0)), tabs->size());
            QVERIFY2(bar.bottom() < tab.top(), qPrintable(QString("%1 %2x%3: bar %4-%5, tabs from %6")
                         .arg(label).arg(sz.width()).arg(sz.height()).arg(bar.top()).arg(bar.bottom()).arg(tab.top())));
            QWidget *page = tabs->currentWidget();
            // (at the window's minimum, pages are squeezed by their own design: only the bar is checked)
            const auto buttons = sz.width() >= 1280 ? page->findChildren<QAbstractButton *>() : QList<QAbstractButton *>();
            for (QAbstractButton *b : buttons)
            {
                if (!b->isVisible() || b->parentWidget() == nullptr || b->width() < 4)
                    continue;
                QVERIFY2(b->y() >= 0 && b->parentWidget()->rect().adjusted(0, -2, 0, 0).contains(b->geometry().topLeft()),
                         qPrintable(QString("%1 %2x%3: %4 \"%5\" starts above its parent (y=%6)")
                                        .arg(label).arg(sz.width()).arg(sz.height())
                                        .arg(b->metaObject()->className(), b->text()).arg(b->y())));
            }
            m_app->hideViewNotice();
        }
    }
}
