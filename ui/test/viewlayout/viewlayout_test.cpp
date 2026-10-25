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
#include <QtMath>

#include "viewlayout_test.h"

#define private public
#define protected public
#include "app.h"
#include "functionmanager.h"
#include "functionstreewidget.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "sceneeditor.h"
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

void ViewLayout_Test::initTestCase()
{
    QVERIFY(m_dir.isValid());

    // Seeded once App has named the settings (its constructor does), before
    // init() builds the tabs that read them.
    m_app = new App();
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

QTEST_MAIN(ViewLayout_Test)
