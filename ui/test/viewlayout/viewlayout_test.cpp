/*
  Q Light Controller Plus
  viewlayout_test.cpp

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
#include <QFile>
#include <QLayout>
#include <QSettings>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QWindow>
#include <QtMath>

#include "viewlayout_test.h"

#define private public
#define protected public
#include "app.h"
#include "appsettings.h"
#include "functionmanager.h"
#include "functionstreewidget.h"
#include "sceneeditor.h"
#undef protected
#undef private
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QCheckBox>
#include <QSlider>
#include "apputil.h"
#include "horizontalpan.h"
#include "fixtureconsole.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "showtreecontroller.h"
#include "chasereditor.h"
#include "efxeditor.h"
#include "chaser.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

/* Allowance for a frame or a layout margin at a page's edge. */
static const int kSlack = 12;

/* The function manager's splitter as a session that collapsed the editor
   (or saved it while the pane was put away) leaves it. */
static QByteArray collapsedEditorState()
{
    QSplitter sp(Qt::Horizontal);
    sp.addWidget(new QWidget);
    sp.addWidget(new QWidget);
    sp.resize(1000, 600);
    sp.setSizes({ 1000, 0 });
    return sp.saveState();
}

/* The same, as a session that dragged the tree away (pane 0 at no width). */
static QByteArray collapsedTreeState()
{
    QSplitter sp(Qt::Horizontal);
    sp.addWidget(new QWidget);
    sp.addWidget(new QWidget);
    sp.resize(1000, 600);
    sp.setSizes({ 0, 1000 });
    return sp.saveState();
}

/* A real session's saved window and splitter states (a 2560-wide screen):
   the tabs are built from these, as they are on the user's Mac. */
static void seedRealisticSettings()
{
    static const struct { const char *key; const char *state; } kSaved[] = {
        { "workspace/geometry", "AdnQywADAAAAAAAAAAAAHwAACN0AAAWfAAAAAAAAAD8AAAjdAAAFnwAAAAAAAAAACgAAAAAAAAAAPwAACN0AAAWf" },
        { "monitor/vsplitter2", "AAAA/wAAAAEAAAADAAABBAAAAVQAAAC0AP////8BAAAAAQA=" },
        { "programming/buildSplitter", "AAAA/wAAAAEAAAACAAABXQAAA/EA/////wEAAAABAA==" },
        { "programming/canvasSplitter", "AAAA/wAAAAEAAAACAAABRAAAAaIA/////wEAAAACAA==" },
        { "programming/workbenchSplitter", "AAAA/wAAAAEAAAACAAABuwAAAsgA/////wEAAAABAA==" },
        { "simpledesk/splitter", "AAAA/wAAAAEAAAACAAABxAAAAYwB/////wEAAAACAA==" },
        { "inputmanager/splitter", "AAAA/wAAAAEAAAACAAAA9gAAAUgB/////wEAAAABAA==" },
        { "showmanager/hsplitter", "AAAA/wAAAAEAAAACAAAAXwAAADIB/////wEAAAACAA==" },
        { "showmanager/vsplitter", "AAAA/wAAAAEAAAACAAAAwgAAADIB/////wEAAAABAA==" },
        { "effectdesigner/split", "AAAA/wAAAAEAAAACAAACGQAAAtMA/////wEAAAABAA==" },
        { "fixturemanager/splitterstate_pwr", "AAAA/wAAAAEAAAACAAACVgAAA20B/////wEAAAABAA==" },
    };
    QSettings settings;
    for (const auto &s : kSaved)
        settings.setValue(QLatin1String(s.key), QByteArray::fromBase64(s.state));
    settings.setValue("monitor/layerspanel3", true);
}

void ViewLayout_Test::initTestCase()
{
    QVERIFY(m_dir.isValid());

    // Seeded once App has named the settings (its constructor does), before
    // init() builds the tabs that read them.
    m_app = new App();
    seedRealisticSettings();
    QSettings().setValue("functionmanager/splitter", collapsedEditorState());
    m_app->init();
    m_app->resize(1280, 800);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app));

    // A course show (a Show Tree with layer scenes it made), copied: the
    // test never writes to the repository's copy.
    const QString show = m_dir.filePath("show.qlcc");
    QVERIFY(QFile::copy(QStringLiteral(COURSE_SHOW), show));
    QCOMPARE(m_app->loadXML(show), QFile::NoError);

    // And one of each plain kind, to open.
    Doc *doc = m_app->doc();
    Scene *scene = new Scene(doc);
    scene->setName("Plain scene");
    QVERIFY(doc->addFunction(scene));
    Chaser *chaser = new Chaser(doc);
    chaser->setName("Plain chaser");
    QVERIFY(doc->addFunction(chaser));
    EFX *efx = new EFX(doc);
    efx->setName("Plain EFX");
    QVERIFY(doc->addFunction(efx));
}

void ViewLayout_Test::cleanupTestCase()
{
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

static QString describe(const QWidget *w)
{
    QString name = QString::fromLatin1(w->metaObject()->className());
    if (!w->objectName().isEmpty())
        name += QStringLiteral(" '%1'").arg(w->objectName());
    return name;
}

/* The bounding box of a widget's visible children, in its own coordinates. */
static QRect childrenBox(const QWidget *w)
{
    QRect box;
    for (QObject *o : w->children())
    {
        QWidget *c = qobject_cast<QWidget *>(o);
        if (c == nullptr || c->isWindow() || !c->isVisible())
            continue;
        box = box.united(c->geometry());
    }
    return box;
}

/* Whatever doesn't fill its space, below and including page; empty if
   nothing. */
static QStringList layoutProblems(QWidget *page)
{
    QStringList bad;

    // The page itself: its content reaches its right and bottom edges.
    const QRect box = childrenBox(page);
    if (!box.isEmpty())
    {
        if (box.right() < page->width() - 1 - kSlack)
            bad << QStringLiteral("%1 ends %2 px short of the right edge")
                   .arg(describe(page)).arg(page->width() - 1 - box.right());
        if (box.bottom() < page->height() - 1 - kSlack)
            bad << QStringLiteral("%1 ends %2 px short of the bottom edge")
                   .arg(describe(page)).arg(page->height() - 1 - box.bottom());
    }

    for (QSplitter *sp : page->findChildren<QSplitter *>())
    {
        if (!sp->isVisible() || sp->window() != page->window())
            continue;
        const bool horz = sp->orientation() == Qt::Horizontal;
        for (int i = 0; i < sp->count(); ++i)
        {
            QWidget *pane = sp->widget(i);
            if (pane == nullptr || !pane->isVisible())
                continue;
            const int along = horz ? pane->width() : pane->height();
            if (along <= 0)
                bad << QStringLiteral("%1: pane %2 (%3) is shown at no %4")
                       .arg(describe(sp)).arg(i).arg(describe(pane))
                       .arg(horz ? "width" : "height");

            // A bare container pane holds something that fills it, not a
            // strip at one side of an empty space.
            const QByteArray cls(pane->metaObject()->className());
            if ((cls == "QWidget" || cls == "QFrame") && along > 0)
            {
                const QRect inner = childrenBox(pane);
                if (inner.isEmpty())
                    bad << QStringLiteral("%1: pane %2 is shown empty").arg(describe(sp)).arg(i);
                else if (inner.width() < pane->width() / 2 || inner.height() < pane->height() / 2)
                    bad << QStringLiteral("%1: pane %2 (%3x%4) holds only %5x%6")
                           .arg(describe(sp)).arg(i).arg(pane->width()).arg(pane->height())
                           .arg(inner.width()).arg(inner.height());
            }
        }
    }
    return bad;
}

void ViewLayout_Test::everyTabFillsTheWindow()
{
    const QList<QSize> sizes = { QSize(1280, 800), QSize(1920, 1080), QSize(2560, 1440) };
    const QList<App::WorkspaceView> views = { App::ClassicView, App::ConsoleView, App::EverythingView };
    QTabWidget *tabs = m_app->m_tab;
    QStringList bad;

    for (App::WorkspaceView view : views)
    {
        m_app->setWorkspaceView(view, false);
        for (const QSize &size : sizes)
        {
            m_app->resize(size);
            for (int i = 0; i < tabs->count(); ++i)
            {
                tabs->setCurrentIndex(i);
                QCoreApplication::processEvents();
                QWidget *page = tabs->widget(i);
                const QString where = QStringLiteral("%1, %2 at %3x%4")
                        .arg(App::workspaceViewName(view), tabs->tabText(i))
                        .arg(size.width()).arg(size.height());

                // The window's central area itself, then the page in it.
                QWidget *central = m_app->centralWidget();
                if (central->geometry().right() < m_app->width() - 1 - kSlack)
                    bad << where + QStringLiteral(": the central area stops short of the window");
                if (page->width() < tabs->width() - 2 * kSlack)
                    bad << where + QStringLiteral(": the page is narrower than the tabs");
                for (const QString &p : layoutProblems(page))
                    bad << where + QStringLiteral(": ") + p;
            }
        }
    }
    m_app->resize(1280, 800);
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
}

/* Whatever in the main window doesn't fill it; empty if all does. */
static QStringList mainWindowProblems(App *app, const QString &where)
{
    QStringList bad;
    QCoreApplication::processEvents();
    if (app->windowHandle() != nullptr && app->windowHandle()->size() != app->size())
        bad << where + QStringLiteral(": the window is %1x%2 but its widget %3x%4")
               .arg(app->windowHandle()->width()).arg(app->windowHandle()->height())
               .arg(app->width()).arg(app->height());
    QWidget *central = app->centralWidget();
    if (central->geometry().right() < app->width() - 1 - kSlack)
        bad << where + QStringLiteral(": the central area is %1 px wide in a %2 px window")
               .arg(central->width()).arg(app->width());
    QTabWidget *tabs = app->m_tab;
    if (tabs->width() < central->width() - kSlack)
        bad << where + QStringLiteral(": the tabs are %1 px wide in a %2 px central area")
               .arg(tabs->width()).arg(central->width());
    if (QWidget *page = tabs->currentWidget())
    {
        if (page->width() < tabs->width() - 2 * kSlack)
            bad << where + QStringLiteral(": %1 is %2 px wide in %3 px of tabs")
                   .arg(describe(page)).arg(page->width()).arg(tabs->width());
        for (const QString &p : layoutProblems(page))
            bad << where + QStringLiteral(": ") + p;
    }
    // Nothing else of the window's own sits on it: a page put away (parked)
    // or back from a window of its own, shown where it was left.
    for (QObject *o : app->children())
    {
        QWidget *c = qobject_cast<QWidget *>(o);
        if (c == nullptr || c->isWindow() || !c->isVisible() || c == central
            || c == app->menuBar() || c == app->statusBar())
            continue;
        bad << where + QStringLiteral(": %1 is shown loose on the window at %2,%3 %4x%5")
               .arg(describe(c)).arg(c->x()).arg(c->y()).arg(c->width()).arg(c->height());
    }
    return bad;
}

void ViewLayout_Test::theWindowFollowsResizesAroundADetach()
{
    QTabWidget *tabs = m_app->m_tab;
    const QList<QSize> sizes = { QSize(900, 700), QSize(1280, 800), QSize(2000, 1200), QSize(1280, 800) };
    QStringList bad;
    auto indexOf = [tabs](const QString &label) {
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabBar()->tabData(i).toString() == label)
                return i;
        return -1;
    };
    // As a drag of the window's corner arrives (through the platform
    // window) and as a program resizes it, turn about.
    auto resizeTo = [this](const QSize &size, int step) {
        if (step % 2 == 0 && m_app->windowHandle() != nullptr)
            m_app->windowHandle()->resize(size);
        else
            m_app->resize(size);
        QCoreApplication::processEvents();
    };

    for (App::WorkspaceView view : { App::ConsoleView, App::EverythingView })
    {
        m_app->setWorkspaceView(view, false);
        m_app->resize(1280, 800);
        QCoreApplication::processEvents();
        // Each tab once: Everything adds the QLC+ tabs to qlcconsole's.
        QStringList labels;
        for (int i = 0; i < tabs->count(); ++i)
            labels << tabs->tabBar()->tabData(i).toString();
        if (view == App::EverythingView)
            for (const QString &done : App::workspaceViewTabs(App::ConsoleView))
                labels.removeAll(done);

        for (const QString &label : labels)
        {
            const QString at = App::workspaceViewName(view) + QStringLiteral(", ") + label;
            const int idx = indexOf(label);
            QVERIFY2(idx >= 0, qPrintable(at));
            tabs->setCurrentIndex(idx);
            QCoreApplication::processEvents();
            QWidget *page = tabs->widget(idx);

            m_app->slotDetachContext(idx);
            QCoreApplication::processEvents();
            DetachedContext *window = qobject_cast<DetachedContext *>(page->window());
            QVERIFY2(window != nullptr && window != static_cast<QWidget *>(m_app), qPrintable(at));

            int step = 0;
            for (const QSize &size : sizes)
            {
                resizeTo(size, step++);
                bad << mainWindowProblems(m_app, QStringLiteral("%1 in its own window, main window at %2x%3")
                                          .arg(at).arg(size.width()).arg(size.height()));
                // Another tab between sizes, as the user goes on working.
                if (tabs->count() > 1)
                {
                    tabs->setCurrentIndex((tabs->currentIndex() + 1) % tabs->count());
                    bad << mainWindowProblems(m_app, QStringLiteral("%1 in its own window, %2 at %3x%4")
                                              .arg(at, tabs->tabBar()->tabData(tabs->currentIndex()).toString())
                                              .arg(size.width()).arg(size.height()));
                }
            }

            // Its own window follows its own resizes too.
            window->resize(1100, 700);
            QCoreApplication::processEvents();
            if (window->centralWidget() != page || page->width() < window->width() - kSlack)
                bad << at + QStringLiteral(": its window is %1 px wide, the page %2 px")
                       .arg(window->width()).arg(page->width());

            // Put back by closing its window: back on the bar, and the main
            // window still follows.
            window->close();
            QCoreApplication::processEvents();
            const int back = indexOf(label);
            if (back < 0 || page->window() != static_cast<QWidget *>(m_app))
            {
                bad << at + QStringLiteral(": not back on the bar after its window closed");
                continue;
            }
            tabs->setCurrentIndex(back);
            for (const QSize &size : { QSize(2000, 1200), QSize(900, 700) })
            {
                resizeTo(size, step++);
                bad << mainWindowProblems(m_app, QStringLiteral("%1 back on the bar, at %2x%3")
                                          .arg(at).arg(size.width()).arg(size.height()));
            }
        }

        // Two at once, the first put back while the second is still out.
        if (tabs->count() >= 3)
        {
            const QString first = tabs->tabBar()->tabData(0).toString();
            const QString second = tabs->tabBar()->tabData(1).toString();
            m_app->slotDetachContext(indexOf(first));
            m_app->slotDetachContext(indexOf(second));
            QCoreApplication::processEvents();
            int step = 0;
            for (const QSize &size : sizes)
            {
                resizeTo(size, step++);
                bad << mainWindowProblems(m_app, QStringLiteral("%1: %2 and %3 out, at %4x%5")
                                          .arg(App::workspaceViewName(view), first, second)
                                          .arg(size.width()).arg(size.height()));
            }
            m_app->reattachContext(first);
            resizeTo(QSize(2000, 1200), 0);
            bad << mainWindowProblems(m_app, QStringLiteral("%1: %2 back, %3 out")
                                      .arg(App::workspaceViewName(view), first, second));
            m_app->reattachAllContexts();
            QCoreApplication::processEvents();
            QVERIFY(m_app->detachedContexts().isEmpty());
        }
    }

    m_app->setWorkspaceView(App::ConsoleView, false);
    m_app->resize(1280, 800);
    QCoreApplication::processEvents();
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
}

/* Select one function in the Functions tab's tree, as a click does. */
static void pick(FunctionManager *fm, quint32 fid)
{
    QTreeWidgetItem *item = fm->m_tree->functionItem(fm->m_doc->function(fid));
    QVERIFY(item != nullptr);
    fm->m_tree->clearSelection();
    fm->m_tree->setCurrentItem(item);
    QCoreApplication::processEvents();
}

/* The editor that's open, shown beside the tree at a usable width; null if
   none is. */
static QWidget *openEditor(FunctionManager *fm)
{
    QWidget *ed = fm->m_scene_editor != nullptr ? fm->m_scene_editor : fm->m_editor;
    if (ed == nullptr || !ed->isVisible())
        return nullptr;
    QWidget *pane = fm->m_hsplitter->widget(1);
    // At least a quarter of the tab each (a first run has half and half):
    // from a collapsed state the pane came back only as wide as the
    // editor's minimum, or the banner's -- the "sidebar".
    const int least = fm->width() / 4;
    if (!pane->isAncestorOf(ed) || pane->width() < least || fm->m_tree->width() < least)
        return nullptr;
    return ed;
}

void ViewLayout_Test::classicFunctionsOpensWhatIsPicked()
{
    m_app->setWorkspaceView(App::ClassicView, false);
    m_app->showContext("Functions");
    QCoreApplication::processEvents();
    FunctionManager *fm = FunctionManager::instance();
    QVERIFY(fm != nullptr);
    QVERIFY(fm->isVisible());

    const struct { const char *name; const char *editor; } kinds[] = {
        { "Plain scene", "SceneEditor" }, { "Plain chaser", "ChaserEditor" }, { "Plain EFX", "EFXEditor" } };
    for (const auto &k : kinds)
    {
        quint32 fid = Function::invalidId();
        for (Function *f : m_app->doc()->functions())
            if (f->name() == QLatin1String(k.name))
                fid = f->id();
        QVERIFY(fid != Function::invalidId());
        pick(fm, fid);
        QWidget *ed = openEditor(fm);
        QVERIFY2(ed != nullptr, k.name);
        QVERIFY2(ed->inherits(k.editor), qPrintable(describe(ed)));
        QVERIFY(ed->isEnabled());
        const QStringList bad = layoutProblems(fm);
        QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
    }

    // Two picked: no editor, as in QLC+.
    fm->m_tree->selectAll();
    QCoreApplication::processEvents();
    QVERIFY(fm->m_editor == nullptr && fm->m_scene_editor == nullptr);
    QVERIFY(!fm->m_hsplitter->widget(1)->isVisible());
    fm->m_tree->clearSelection();
}

/* CHANGELOG [0.2.2]: in Classic QLC+ the editor opens beside the tree "even
   from a saved splitter state that had a pane collapsed" -- here the tree.
   The state is restored as FunctionManager's constructor restores
   functionmanager/splitter; picking a function must bring the tree back. */
void ViewLayout_Test::classicFunctionsUncollapsesTheTree()
{
    m_app->setWorkspaceView(App::ClassicView, false);
    m_app->showContext("Functions");
    QCoreApplication::processEvents();
    FunctionManager *fm = FunctionManager::instance();
    QVERIFY(fm != nullptr);
    auto idOf = [this](const char *name) {
        for (Function *f : m_app->doc()->functions())
            if (f->name() == QLatin1String(name))
                return f->id();
        return Function::invalidId();
    };
    const quint32 scene = idOf("Plain scene"), chaser = idOf("Plain chaser");
    QVERIFY(scene != Function::invalidId() && chaser != Function::invalidId());

    // An editor open, then the tree dragged away to nothing (as the saved
    // functionmanager/splitter state restores it).
    pick(fm, scene);
    QVERIFY(openEditor(fm) != nullptr);
    QVERIFY(fm->m_hsplitter->restoreState(collapsedTreeState()));
    QCoreApplication::processEvents();
    QCOMPARE(fm->m_hsplitter->sizes().value(0), 0);

    pick(fm, chaser);
    QWidget *ed = openEditor(fm);       // both panes at least a quarter wide
    QVERIFY2(ed != nullptr, qPrintable(QString("tree %1, pane %2 of %3")
                                       .arg(fm->m_tree->width())
                                       .arg(fm->m_hsplitter->widget(1)->width()).arg(fm->width())));
    QVERIFY(ed->inherits("ChaserEditor"));
    QVERIFY(fm->m_tree->width() >= fm->width() / 4);
    const QStringList bad = layoutProblems(fm);
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
    fm->editFunction(nullptr);
    fm->m_tree->clearSelection();
}

void ViewLayout_Test::aShowTreeSceneOpensGreyedUnderItsBanner()
{
    // Classic QLC+ lists the tree's own functions only when asked to.
    FunctionManager::instance()->setShowTreeFunctionsShown(true);
    ShowTreeController *st = m_app->doc()->showTree();
    quint32 made = Function::invalidId();
    for (Function *f : m_app->doc()->functions())
        if (f->type() == Function::SceneType && st->isGenerated(f->id()))
            made = f->id();
    QVERIFY2(made != Function::invalidId(), "the course show has a layer scene the tree made");
    // Listed, as an Eos import's are (the tree's own are hidden).
    m_app->doc()->function(made)->setVisible(true);
    FunctionManager::instance()->m_tree->updateTree();

    for (App::WorkspaceView view : { App::ClassicView, App::EverythingView })
    {
        m_app->setWorkspaceView(view, false);
        m_app->showContext("Functions");
        QCoreApplication::processEvents();
        FunctionManager *fm = FunctionManager::instance();
        pick(fm, made);
        if (view != App::ClassicView)
        {
            // Elsewhere a click selects (to drag); a double-click opens.
            QVERIFY(fm->m_scene_editor == nullptr);
            fm->slotTreeItemDoubleClicked(fm->m_tree->currentItem());
            QCoreApplication::processEvents();
        }
        QWidget *ed = openEditor(fm);
        QVERIFY2(ed != nullptr, qPrintable(App::workspaceViewName(view)));
        QVERIFY(ed->inherits("SceneEditor"));
        QVERIFY(!ed->isEnabled());
        QWidget *banner = fm->m_lockedBanner;
        QVERIFY(banner != nullptr && banner->isVisible());
        QCOMPARE(banner->objectName(), QString("stLockedBanner"));
        QVERIFY(banner->geometry().bottom() <= ed->geometry().top());
        const QStringList bad = layoutProblems(fm);
        QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
        fm->editFunction(nullptr);
        QCoreApplication::processEvents();
        QVERIFY(fm->m_lockedBanner == nullptr);
    }
    FunctionManager::instance()->setShowTreeFunctionsShown(false);
}

/* WCAG relative luminance and contrast. */
static double luminance(const QColor &c)
{
    auto lin = [](double s) { return s <= 0.03928 ? s / 12.92 : qPow((s + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

static double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

void ViewLayout_Test::theEditedRowReadsInEveryTheme()
{
    m_app->setWorkspaceView(App::ClassicView, false);
    m_app->showContext("Functions");
    FunctionManager *fm = FunctionManager::instance();
    quint32 fid = Function::invalidId();
    for (Function *f : m_app->doc()->functions())
        if (f->name() == QLatin1String("Plain scene"))
            fid = f->id();
    QTreeWidgetItem *item = fm->m_tree->functionItem(m_app->doc()->function(fid));
    QVERIFY(item != nullptr);
    const QVariant own = item->data(0, Qt::ForegroundRole);

    const int was = m_app->theme();
    const int themes[] = { App::ThemeDefault, App::ThemeTan, App::ThemeBlue,
                           App::ThemeQLCOriginal, App::ThemeRedShift, App::ThemeVSCodeDark };
    for (int theme : themes)
    {
        m_app->setTheme(theme);
        QCoreApplication::processEvents();
        fm->editFunction(m_app->doc()->function(fid));
        // What is drawn: the row's own colours, else the list's.
        const QPalette pal = fm->m_tree->palette();
        const QColor bg = item->background(0).style() != Qt::NoBrush
                ? item->background(0).color() : pal.color(QPalette::Base);
        const QColor fg = item->foreground(0).style() != Qt::NoBrush
                ? item->foreground(0).color() : pal.color(QPalette::Text);
        QVERIFY2(contrast(fg, bg) >= 4.5,
                 qPrintable(QString("theme %1: %2 on %3, %4").arg(theme).arg(fg.name(), bg.name())
                            .arg(contrast(fg, bg))));
        fm->editFunction(nullptr);
        // Closed: the row's own colour back.
        QCOMPARE(item->data(0, Qt::ForegroundRole), own);
    }
    m_app->setTheme(was);
}

static quint32 idOfFunction(Doc *doc, const QString &name)
{
    for (Function *f : doc->functions())
        if (f->name() == name)
            return f->id();
    return Function::invalidId();
}

void ViewLayout_Test::classicHidesTheShowTreesOwnFunctions()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    FunctionManager *fm = FunctionManager::instance();
    fm->setShowTreeFunctionsShown(false);

    // The course show's tree made layer scenes (hidden) and a cue list's
    // chaser (visible, as QLC+ would list it).
    QList<quint32> made, madeVisible;
    for (Function *f : doc->functions())
        if (st->isGenerated(f->id()))
        {
            made << f->id();
            if (f->isVisible())
                madeVisible << f->id();
        }
    QVERIFY2(!made.isEmpty(), "the course show's tree made functions");
    QVERIFY2(!madeVisible.isEmpty(), "and one of them (a cue list's chaser) is visible");
    const quint32 plain = idOfFunction(doc, "Plain scene");
    QVERIFY(plain != Function::invalidId());

    auto listed = [&](quint32 fid) {
        return fm->m_tree->functionItem(doc->function(fid)) != nullptr;
    };

    m_app->setWorkspaceView(App::ClassicView, false);
    m_app->showContext("Functions");
    QCoreApplication::processEvents();
    QCOMPARE(fm->m_tree->showTreeFunctions(), FunctionsTreeWidget::ShowTreeHidden);
    for (quint32 fid : made)
        QVERIFY2(!listed(fid), qPrintable(doc->function(fid)->name()));
    QVERIFY(listed(plain));

    // An edit in the tree bakes again: still none listed.
    QVERIFY(!st->productions().isEmpty());
    st->bake();
    emit st->changed();
    QTRY_VERIFY(!fm->m_refreshTimer->isActive());
    for (Function *f : doc->functions())
        if (st->isGenerated(f->id()))
            QVERIFY2(!listed(f->id()), qPrintable(f->name()));

    // The toggle: every one of them, the hidden layer scenes too.
    fm->setShowTreeFunctionsShown(true);
    QVERIFY(fm->m_showTreeFunctionsAction->isChecked());
    QCOMPARE(fm->m_tree->showTreeFunctions(), FunctionsTreeWidget::ShowTreeAll);
    for (quint32 fid : made)
        QVERIFY2(listed(fid), qPrintable(doc->function(fid)->name()));
    QVERIFY(listed(plain));
    fm->setShowTreeFunctionsShown(false);
    QVERIFY(!fm->m_showTreeFunctionsAction->isChecked());

    // Elsewhere, what is visible, as QLC+ lists it.
    m_app->setWorkspaceView(App::EverythingView, false);
    m_app->showContext("Functions");
    QCoreApplication::processEvents();
    QCOMPARE(fm->m_tree->showTreeFunctions(), FunctionsTreeWidget::ShowTreeStock);
    for (quint32 fid : made)
        QCOMPARE(listed(fid), doc->function(fid)->isVisible());
    QVERIFY(listed(plain));
    m_app->setWorkspaceView(App::ClassicView, false);
}

/* A plain dimmer pack of @a channels channels, patched where universe 0 has
   room, and a scene with it, its values set. */
static quint32 manyChannelScene(Doc *doc, const QString &name, int channels)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name + QStringLiteral(" pack"));
    fxi->setChannels(channels);
    // The first free run of addresses in universe 0.
    bool added = false;
    for (quint32 addr = 0; !added && addr + channels <= 512; addr += 16)
    {
        fxi->setUniverse(0);
        fxi->setAddress(addr);
        added = doc->addFixture(fxi);
    }
    if (!added)
    {
        delete fxi;
        return Function::invalidId();
    }
    Scene *scene = new Scene(doc);
    scene->setName(name);
    scene->addFixture(fxi->id());
    for (int c = 0; c < channels; ++c)
        scene->setValue(fxi->id(), c, 128);
    if (!doc->addFunction(scene))
        return Function::invalidId();
    return scene->id();
}

static SceneEditor *openSceneEditor(App *app, quint32 fid)
{
    app->setWorkspaceView(App::ClassicView, false);
    app->showContext("Functions");
    QCoreApplication::processEvents();
    FunctionManager *fm = FunctionManager::instance();
    fm->editFunction(nullptr);
    // The last editor goes with deleteLater(): gone before the next opens.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    pick(fm, fid);
    QTest::qWait(20);
    return qobject_cast<SceneEditor *>(fm->m_scene_editor);
}

void ViewLayout_Test::sceneEditorHasGeneralOverTheSliders()
{
    Doc *doc = m_app->doc();
    m_app->resize(1280, 800);
    quint32 fid = idOfFunction(doc, "Wide scene");
    if (fid == Function::invalidId())
        fid = manyChannelScene(doc, "Wide scene", 96);
    QVERIFY(fid != Function::invalidId());

    // Two saved states no split should come back as: the sliders' pane at
    // no height, and a horizontal split.
    QSplitter bad(Qt::Vertical);
    bad.addWidget(new QWidget);
    bad.addWidget(new QWidget);
    bad.resize(400, 600);
    bad.setSizes({ 600, 0 });
    QSplitter sideways(Qt::Horizontal);
    sideways.addWidget(new QWidget);
    sideways.addWidget(new QWidget);
    const QList<QVariant> states = { QVariant(), QVariant(bad.saveState()),
                                     QVariant(sideways.saveState()), QVariant(QByteArray("junk")) };
    for (const QVariant &state : states)
    {
        if (state.isValid())
            QSettings().setValue("sceneeditor/splitter", state);
        else
            QSettings().remove("sceneeditor/splitter");
        SceneEditor *ed = openSceneEditor(m_app, fid);
        QVERIFY(ed != nullptr);
        QVERIFY(ed->isVisible());

        QSplitter *sp = ed->m_splitter;
        QVERIFY(sp != nullptr);
        QCOMPARE(sp->objectName(), QString("sceneEditorSplitter"));
        QCOMPARE(sp->orientation(), Qt::Vertical);
        QCOMPARE(sp->count(), 2);
        QCOMPARE(sp->widget(0), ed->General);
        QCOMPARE(sp->widget(1), static_cast<QWidget *>(ed->m_tab));
        // General is not a tab any more; the fixture is.
        QCOMPARE(ed->m_tab->indexOf(ed->General), -1);
        QVERIFY(ed->m_tab->count() >= 1);
        QVERIFY(ed->General->isAncestorOf(ed->m_tree));
        QVERIFY(ed->General->isAncestorOf(ed->m_channelGroupsTree));

        // Both shown, General above the sliders, each a usable height.
        QVERIFY(ed->General->isVisible() && ed->m_tab->isVisible());
        const QList<int> sizes = sp->sizes();
        QVERIFY2(sizes.value(0) >= 60 && sizes.value(1) >= 120,
                 qPrintable(QString("state %1: %2 / %3 of %4 (editor %5, Functions %6)")
                            .arg(states.indexOf(state)).arg(sizes.value(0)).arg(sizes.value(1))
                            .arg(sp->height()).arg(ed->height())
                            .arg(FunctionManager::instance()->height())));
        QVERIFY2(sizes.value(1) > sizes.value(0),
                 qPrintable(QString("state %1: %2 / %3").arg(states.indexOf(state))
                            .arg(sizes.value(0)).arg(sizes.value(1))));
        QVERIFY(ed->General->geometry().bottom() < ed->m_tab->geometry().top());

        // The lists scroll up and down; the sliders sideways.
        QVERIFY(ed->m_tree->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
        QVERIFY(ed->m_channelGroupsTree->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
        QScrollArea *area = qobject_cast<QScrollArea *>(ed->m_tab->currentWidget());
        QVERIFY(area != nullptr);
        QVERIFY(HorizontalPan::isEnabled(area));
        QVERIFY(area->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
        QVERIFY2(area->horizontalScrollBar()->maximum() > 0,
                 qPrintable(QString("96 channels in %1 px").arg(area->viewport()->width())));

        const QStringList problems = layoutProblems(FunctionManager::instance());
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
        FunctionManager::instance()->editFunction(nullptr);
        QCoreApplication::processEvents();
    }

    // A scene with nothing in it: no sliders' pane, General has it all.
    const quint32 plain = idOfFunction(doc, "Plain scene");
    SceneEditor *ed = openSceneEditor(m_app, plain);
    QVERIFY(ed != nullptr);
    QCOMPARE(ed->m_tab->count(), 0);
    QVERIFY(ed->m_tab->isHidden());
    QVERIFY(ed->General->isVisible());
    QVERIFY(ed->General->height() > ed->height() / 2);
    FunctionManager::instance()->editFunction(nullptr);
    QSettings().remove("sceneeditor/splitter");
}

void ViewLayout_Test::sceneEditorListsAScenesLooks()
{
    Doc *doc = m_app->doc();
    QVERIFY(!doc->fixtureGroups().isEmpty());
    QVERIFY(!doc->palettes().isEmpty());
    FixtureGroup *grp = doc->fixtureGroups().first();
    QLCPalette *pal = doc->palettes().first();

    Scene *scene = new Scene(doc);
    scene->setName("Looks scene");
    scene->addFixtureGroup(grp->id());
    scene->addPalette(pal->id());
    QVERIFY(doc->addFunction(scene));

    SceneEditor *ed = openSceneEditor(m_app, scene->id());
    QVERIFY(ed != nullptr);
    QTreeWidget *looks = ed->findChild<QTreeWidget *>("sceneLooksTree");
    QVERIFY(looks != nullptr);
    QVERIFY(looks->isVisible());
    QVERIFY(ed->General->isAncestorOf(looks));
    QStringList texts;
    for (QTreeWidgetItemIterator it(looks); *it; ++it)
        texts << (*it)->text(0);
    QVERIFY2(texts.contains(grp->name()), qPrintable(texts.join(", ")));
    QVERIFY2(texts.join("\n").contains(pal->name()), qPrintable(texts.join(", ")));
    FunctionManager::instance()->editFunction(nullptr);

    // A plain scene has no such list.
    ed = openSceneEditor(m_app, idOfFunction(doc, "Plain scene"));
    QVERIFY(ed != nullptr);
    QVERIFY(ed->findChild<QTreeWidget *>("sceneLooksTree") == nullptr);
    FunctionManager::instance()->editFunction(nullptr);
}

void ViewLayout_Test::sliderRowsScrollSideways()
{
    Doc *doc = m_app->doc();
    m_app->resize(1280, 800);
    quint32 fid = idOfFunction(doc, "Wide scene");
    if (fid == Function::invalidId())
        fid = manyChannelScene(doc, "Wide scene", 96);
    SceneEditor *ed = openSceneEditor(m_app, fid);
    QVERIFY(ed != nullptr);
    QScrollArea *area = qobject_cast<QScrollArea *>(ed->m_tab->currentWidget());
    QVERIFY(area != nullptr);
    QScrollBar *bar = area->horizontalScrollBar();
    QVERIFY(bar->maximum() > 0);
    bar->setValue(0);

    // Shift + a wheel turn over a fader: the row moves, the fader doesn't.
    QSlider *fader = area->widget()->findChild<QSlider *>();
    QVERIFY(fader != nullptr);
    const int level = fader->value();
    const QPointF at(fader->width() / 2.0, fader->height() / 2.0);
    QWheelEvent shiftWheel(at, fader->mapToGlobal(at), QPoint(), QPoint(0, -120),
                           Qt::NoButton, Qt::ShiftModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(fader, &shiftWheel);
    QVERIFY2(bar->value() > 0, "Shift + wheel scrolled the row");
    QCOMPARE(fader->value(), level);
    const int afterShift = bar->value();

    // A sideways swipe, no Shift: the same.
    QWheelEvent swipe(at, fader->mapToGlobal(at), QPoint(-40, 0), QPoint(-120, 0),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(fader, &swipe);
    QVERIFY(bar->value() > afterShift);
    QCOMPARE(fader->value(), level);

    // A plain wheel turn is still the fader's.
    QWheelEvent plain(at, fader->mapToGlobal(at), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    const int beforePlain = bar->value();
    QApplication::sendEvent(fader, &plain);
    QCOMPARE(bar->value(), beforePlain);

    // A drag on the space between the faders pans (to the left: the row
    // moves with the pointer, so it shows what was to the left).
    bar->setValue(bar->maximum() / 2);
    const int start = bar->value();
    FixtureConsole *console = qobject_cast<FixtureConsole *>(area->widget());
    QVERIFY(console != nullptr);
    // Its own background: below the last of its children, or at its edge.
    QPoint spot(2, console->height() - 2);
    QVERIFY2(console->childAt(spot) == nullptr, "a spot of the console with no control on it");
    const QPoint g0 = console->mapToGlobal(spot);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(spot), QPointF(g0), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(console, &press);
    const QPoint g1 = g0 + QPoint(150, 0);
    QMouseEvent move(QEvent::MouseMove, QPointF(spot + QPoint(150, 0)), QPointF(g1), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(console, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(spot + QPoint(150, 0)), QPointF(g1),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(console, &release);
    QCOMPARE(bar->value(), qMax(0, start - 150));
    QVERIFY(QApplication::overrideCursor() == nullptr);
    FunctionManager::instance()->editFunction(nullptr);
}

void ViewLayout_Test::alwaysShowScrollBarsSetting()
{
    const QString base = AppUtil::styleName();
    QVERIFY(!base.isEmpty());
    QVERIFY(!m_app->alwaysShowScrollBars());

    // Through Settings ▸ Appearance, as a user sets it.
    {
        AppSettings dlg(m_app, m_app);
        QCheckBox *box = dlg.findChild<QCheckBox *>("settingsAlwaysShowScrollBars");
        QVERIFY(box != nullptr);
        QVERIFY(!box->isChecked());
        box->setChecked(true);
        dlg.accept();
    }
    QVERIFY(m_app->alwaysShowScrollBars());
    QVERIFY(QSettings().value("workspace/alwaysShowScrollBars").toBool());
    QCOMPARE(QApplication::style()->styleHint(QStyle::SH_ScrollBar_Transient), 0);
    QScrollArea probe;
    QCOMPARE(probe.style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, probe.horizontalScrollBar()), 0);
    // Still the same style underneath (the theme's tweaks key on it).
    QCOMPARE(AppUtil::styleName(), base);
    QCoreApplication::processEvents();

    m_app->setAlwaysShowScrollBars(false);
    QVERIFY(!m_app->alwaysShowScrollBars());
    QVERIFY(!QSettings().value("workspace/alwaysShowScrollBars").toBool());
    QCOMPARE(AppUtil::styleName(), base);
    QSettings().remove("workspace/alwaysShowScrollBars");
}

QTEST_MAIN(ViewLayout_Test)
