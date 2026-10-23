/*
  Q Light Controller Plus
  helpshots.cpp

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

#include <QCoreApplication>
#include <QApplication>
#include <QTreeWidgetItemIterator>
#include <QDialogButtonBox>
#include <QTreeWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QDockWidget>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QAction>
#include <QToolBar>
#include <QTabBar>
#include <QComboBox>
#include <QDialog>
#include <QPainterPath>
#include <QPainter>
#include <QSettings>
#include <QTemporaryDir>
#include <QWidget>
#include <QTimer>
#include <QDir>

#include "programmingmanager.h"
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "truss.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "patchinventorydialog.h"
#include "functionstreewidget.h"
#include "scenegrouplooks.h"
#include "effectbrowser.h"
#include "lookeditor.h"
#include "helpshots.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "app.h"
#include "doc.h"

namespace
{
const QColor kCallout(255, 122, 0);     // reads on the dark theme and on white

void settle()
{
    // Two passes: the first lays out what the setup changed, the second
    // paints what that layout revealed (lazily built panels, scroll ranges).
    // Widgets replaced by deleteLater() go too -- an event loop would have
    // removed them, and a grab would show them next to their replacement.
    for (int i = 0; i < 2; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
}

QWidget *named(const QObject *root, const char *name)
{
    QWidget *w = root ? root->findChild<QWidget *>(QLatin1String(name)) : NULL;
    if (w == NULL)
        fprintf(stderr, "[HELPSHOT] no control named %s\n", name);
    return w;
}

HelpShots::Callout mark(QWidget *w, int number)
{
    HelpShots::Callout c;
    c.widget = w;
    c.number = number;
    return c;
}

/** @a item's row, in @a frame's grab coordinates. */
HelpShots::Callout markRow(QTreeWidget *tree, QTreeWidgetItem *item,
                           QWidget *grab, int number)
{
    HelpShots::Callout c;
    c.number = number;
    if (tree != NULL && item != NULL && grab != NULL)
    {
        QRect r = tree->visualItemRect(item);
        r.setLeft(0);
        r.setRight(tree->viewport()->width() - 1);
        c.rect = QRect(tree->viewport()->mapTo(grab, r.topLeft()), r.size());
    }
    return c;
}

/** The part of @a w that's on show, in @a grab's coordinates: clipped by
    every parent on the way up, as a control in a scroll area or under a
    splitter can be bigger than what you see of it. Empty when hidden. */
QRect seenRect(QWidget *w, QWidget *grab)
{
    if (w == NULL || grab == NULL || !w->isVisible())
        return QRect();
    QRect r = w->rect();
    for (QWidget *c = w; c != grab; c = c->parentWidget())
    {
        if (c->parentWidget() == NULL)
            return QRect();             // not inside the grab
        r = r.translated(c->pos()).intersected(c->parentWidget()->rect());
    }
    return r;
}

const int kWidth = 1280;
const int kHeight = 780;
}

HelpShots::HelpShots(App *app, const QString &outDir, const QString &only)
    : QObject(app)
    , m_app(app)
    , m_outDir(outDir)
    , m_only(only)
    , m_failed(0)
{
}

void HelpShots::isolateSettings()
{
    if (qEnvironmentVariableIsEmpty("QLC_HELP_SHOTS"))
        return;
    // Kept for the whole run; removed with it.
    static QTemporaryDir dir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
}

void HelpShots::runIfRequested(App *app)
{
    const QString outDir = QString::fromLocal8Bit(qgetenv("QLC_HELP_SHOTS"));
    if (outDir.isEmpty() || app == NULL)
        return;
    QDir().mkpath(outDir);

    HelpShots *hs = new HelpShots(app, outDir,
                                  QString::fromLocal8Bit(qgetenv("QLC_HELP_SHOTS_ONLY")));
    for (const Shot &s : hs->shots())
        if (hs->m_only.isEmpty() || s.name.contains(hs->m_only, Qt::CaseInsensitive))
            hs->m_queue << s;

    // The pictures are all in one theme, the one that reads best printed
    // small: Blue. (The settings are this run's own; see isolateSettings().)
    app->setTheme(App::ThemeBlue);

    // Past startup, so the window has its size and the show is in.
    QTimer::singleShot(700, hs, &HelpShots::next);
}

void HelpShots::next()
{
    if (m_queue.isEmpty())
    {
        fprintf(stderr, "[HELPSHOT] done, %d failed\n", m_failed);
        // The shots change the show (selections, opened looks); none of that
        // is the user's, so quit without the save prompt.
        m_app->doc()->resetModified();
        QCoreApplication::exit(m_failed == 0 ? 0 : 1);
        return;
    }
    const Shot shot = m_queue.takeFirst();
    take(shot);
    QTimer::singleShot(50, this, &HelpShots::next);
}

void HelpShots::take(const Shot &shot)
{
    Frame frame = shot.setup();
    settle();
    if (!frame.saved)
        save(shot.name, frame);
    if (shot.cleanup)
        shot.cleanup();
    settle();
}

bool HelpShots::save(const QString &name, const Frame &frame)
{
    if (frame.grab.isNull())
    {
        fprintf(stderr, "[HELPSHOT] %s: nothing to grab\n", qPrintable(name));
        ++m_failed;
        return false;
    }
    bool ok = true;
    for (const Callout &c : frame.callouts)
        if (c.widget.isNull() ? c.rect.isEmpty() : seenRect(c.widget, frame.grab).isEmpty())
        {
            fprintf(stderr, "[HELPSHOT] %s: callout %d has no control on show\n",
                    qPrintable(name), c.number);
            ++m_failed;
            ok = false;
        }

    const QImage img = annotate(frame.grab->grab().toImage(), frame);
    const QString path = m_outDir + QLatin1Char('/') + name + QStringLiteral(".png");
    if (img.save(path))
        fprintf(stderr, "[HELPSHOT] %s\n", qPrintable(path));
    else
    {
        fprintf(stderr, "[HELPSHOT] %s: can't write\n", qPrintable(path));
        ++m_failed;
        ok = false;
    }
    return ok;
}

QImage HelpShots::annotate(const QImage &image, const Frame &frame)
{
    const qreal dpr = image.devicePixelRatio();
    const QMargins pad = frame.pad;

    // The grab, on a page wide enough for the padding, in the window colour.
    QImage out(QSize(image.width() + (pad.left() + pad.right()) * dpr,
                     image.height() + (pad.top() + pad.bottom()) * dpr),
               QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(dpr);
    out.fill(QApplication::palette().color(QPalette::Window));

    QPainter p(&out);
    p.drawImage(QPoint(pad.left(), pad.top()), image);
    p.setRenderHint(QPainter::Antialiasing);

    // Bigger marks on a bigger picture: the help shrinks a whole window to
    // fit its page, and the numbers must still read there.
    const QRect bounds(QPoint(0, 0), out.size() / dpr);
    const qreal scale = qBound(1.0, bounds.width() / 900.0, 1.6);
    const int badge = qRound(22 * scale);
    QFont f = QApplication::font();
    f.setBold(true);
    f.setPixelSize(qRound(14 * scale));
    p.setFont(f);

    for (const Callout &c : frame.callouts)
    {
        QRect r = c.rect;
        if (!c.widget.isNull() && !frame.grab.isNull())
            r = seenRect(c.widget, frame.grab);
        if (r.isEmpty())
            continue;
        r.translate(pad.left(), pad.top());
        r = r.adjusted(-3, -3, 3, 3).intersected(bounds.adjusted(1, 1, -1, -1));

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kCallout, 2.5 * scale));
        p.drawRoundedRect(QRectF(r), 5, 5);

        // Where the number goes: on a big control, its top-left corner; on a
        // short one, outside it so it hides none of its text: to its left, or
        // where the shot asks (above, right); at its corner when there's no
        // room to its left.
        QRect b(0, 0, badge, badge);
        if (c.place == Above)
            b.moveCenter(QPoint(r.center().x(), r.top() - badge / 2 - 2));
        else if (c.place == Left)
            b.moveCenter(QPoint(r.left() - badge / 2 - 3, r.center().y()));
        else if (r.height() >= 3 * badge || c.place == Corner)
            b.moveCenter(r.topLeft());
        else if (c.place == Right)
            b.moveCenter(QPoint(r.right() + badge / 2 + 3, r.center().y()));
        else if (r.left() - badge - 3 >= bounds.left())
            b.moveCenter(QPoint(r.left() - badge / 2 - 3, r.center().y()));
        else
            b.moveCenter(r.topLeft());
        b.moveLeft(qBound(bounds.left() + 1, b.left(), bounds.right() - badge - 1));
        b.moveTop(qBound(bounds.top() + 1, b.top(), bounds.bottom() - badge - 1));

        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(kCallout);
        p.drawEllipse(b);
        p.setPen(Qt::white);
        p.drawText(b, Qt::AlignCenter, QString::number(c.number));
    }
    p.end();

    if (!frame.crop.isEmpty())
    {
        const QRect crop = frame.crop.translated(pad.left(), pad.top());
        QImage cropped = out.copy(QRect(crop.topLeft() * dpr, crop.size() * dpr)
                                  .intersected(out.rect()));
        cropped.setDevicePixelRatio(dpr);
        return cropped;
    }
    return out;
}

QList<HelpShots::Shot> HelpShots::shots()
{
    App *app = m_app;
    Doc *doc = app->doc();
    ProgrammingManager *pm = app->findChild<ProgrammingManager *>();

    auto function = [doc](const QString &name) -> Function * {
        for (Function *f : doc->functions())
            if (f != NULL && f->name().contains(name, Qt::CaseInsensitive))
                return f;
        fprintf(stderr, "[HELPSHOT] no function %s\n", qPrintable(name));
        return NULL;
    };
    auto palette = [doc](const QString &name) -> QLCPalette * {
        for (QLCPalette *p : doc->palettes())
            if (p != NULL && p->name() == name)
                return p;
        fprintf(stderr, "[HELPSHOT] no palette %s\n", qPrintable(name));
        return NULL;
    };
    // The main window at the size every picture of it is taken at.
    auto window = [app]() {
        app->showNormal();
        app->resize(kWidth, kHeight);
    };
    // Brings forward the tab that holds @a w.
    auto showTab = [app](QWidget *w) {
        while (w != NULL && app->m_tab->indexOf(w) < 0)
            w = w->parentWidget();
        if (w != NULL)
            app->m_tab->setCurrentWidget(w);
        settle();
    };
    // The look editor has the page's whole height now: nothing to size.
    auto editorHeight = [](int) { settle(); };
    // Loads a scene in the Build tab's Workbench (Scenes section, selected
    // in the navigator), and selects the look whose row contains @a look
    // (the scene's own row when empty).
    auto openScene = [pm, function, showTab](const QString &scene, const QString &look) {
        if (pm == NULL)
            return;
        showTab(pm);
        pm->setSection(ProgrammingManager::ScenesSection);
        // A narrow navigator: the Workbench wants the width.
        if (QSplitter *split = qobject_cast<QSplitter *>(named(pm, "programmingSplitter")))
            split->setSizes(QList<int>() << 330 << split->width() - 330);
        Function *f = function(scene);
        if (f == NULL)
            return;
        pm->m_outline->rebuild();
        pm->m_workbench->clear();       // build it afresh
        pm->showFunction(f->id());
        settle();
        if (look.isEmpty())
            return;
        ProgrammingOutline *rows = pm->m_workbench->rows();
        QTreeWidgetItem *sceneRow = rows->sceneItem(f->id());
        if (sceneRow == NULL)
            return;
        sceneRow->setExpanded(true);
        QTreeWidgetItem *hit = NULL;
        for (QTreeWidgetItemIterator it(sceneRow); *it != NULL && hit == NULL; ++it)
            if ((*it)->text(0).contains(look, Qt::CaseInsensitive)
                && (*it)->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::PaletteRow)
                hit = *it;
        if (hit != NULL)
            rows->selectRow(ProgrammingOutline::rowOf(hit));
        else
            fprintf(stderr, "[HELPSHOT] no look %s\n", qPrintable(look));
        settle();
    };

    QList<Shot> list;

    // Getting started: the whole window, what's where.
    list << Shot{ QStringLiteral("getting-started-main-window"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QString());
        Frame f;
        f.grab = app;
        // The top bar: the tab's tools, then the show controls.
        f.callouts << mark(app->m_toolbar, 1)
                   << mark(app->m_tab->currentWidget(), 2)
                   << mark(app->m_tab->tabBar(), 3)
                   << mark(app->statusBar(), 4);
        return f;
    }, nullptr };

    // Build: the section switch, the navigator, the Workbench and its editor pane.
    list << Shot{ QStringLiteral("build-tab-layout"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QStringLiteral("White"));
        editorHeight(250);
        Frame f;
        f.grab = app;
        f.callouts << mark(named(pm, "buildSection2"), 1)
                   << mark(named(pm, "programmingOutline"), 2)
                   << mark(named(pm, "workbenchHeader"), 3)
                   << mark(named(pm, "workbenchRows"), 4)
                   << mark(named(pm, "workbenchAddBar"), 5)
                   << mark(named(pm, "lookEditor"), 6)
                   << mark(named(pm, "programmingFixturesGroups"), 7);
        return f;
    }, nullptr };

    // The look editor on an Effect look: what's picked, its Spread, its master.
    list << Shot{ QStringLiteral("programming-tab-look-editor-effect"), [=]() {
        window();
        // Wider than the other pictures: the effect's rows want the room, and
        // at 1280 its Spread row and labels were cut off at the pane's edges.
        app->resize(kWidth + 500, kHeight);
        openScene(QStringLiteral("05 Stage Shape"), QStringLiteral("Line"));
        editorHeight(600);
        // Scrolled down to the Spread row and the speed master under it.
        for (const char *name : { "effectSpeedMaster", "effectSpread" })
        {
            QWidget *row = named(pm, name);
            for (QWidget *w = row; w != NULL; w = w->parentWidget())
                if (QScrollArea *sa = qobject_cast<QScrollArea *>(w))
                {
                    sa->ensureWidgetVisible(row, 0, 8);
                    sa->horizontalScrollBar()->setValue(0);   // its labels, on the left
                    break;
                }
        }
        Frame f;
        f.grab = named(pm, "lookEditor");
        f.callouts << mark(named(pm, "effectChange"), 1)
                   << mark(named(pm, "effectSpread"), 2);
        Callout master = mark(named(pm, "effectSpeedMaster"), 3);
        master.place = Right;       // its label is on its left
        f.callouts << master;
        return f;
    }, nullptr };

    // The effect browser, opened from that look.
    list << Shot{ QStringLiteral("programming-tab-effect-browser"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QStringLiteral("Line"));
        LookEditor *le = pm ? pm->m_workbench->lookEditor() : NULL;
        Frame f;
        if (le == NULL)
            return f;
        le->openEffectPicker();
        settle();
        QWidget *dlg = named(le, "effectPicker");
        if (dlg == NULL)
            return f;
        dlg->resize(900, 600);
        f.grab = dlg;
        f.pad = QMargins(30, 0, 0, 0);      // numbers left of the search and tabs
        if (EffectBrowser *eb = dlg->findChild<EffectBrowser *>())
        {
            Callout list = mark(eb->list(), 3);
            list.place = Left;      // its corner is under the tabs' number
            f.callouts << mark(eb->search(), 1) << mark(eb->tabs(), 2) << list;
        }
        f.callouts << mark(named(dlg, "effectPickerInfo"), 4)
                   << mark(named(dlg, "effectPickerDone"), 5);
        return f;
    }, [=]() {
        if (pm != NULL && pm->m_workbench->lookEditor() != NULL)
            pm->m_workbench->lookEditor()->closeEffectPicker(false);
    } };

    // The footer, on its own.
    list << Shot{ QStringLiteral("footer"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QString());
        Frame f;
        f.grab = app->statusBar();
        f.pad = QMargins(4, 30, 4, 4);      // the numbers go above the chips
        int n = 0;
        for (const char *name : { "footerReadiness", "footerGrandMaster",
                                  "speedMastersFooter", "footerStandby",
                                  "footerTimecode", "footerLoad", "footerMode" })
        {
            Callout c = mark(named(app, name), ++n);
            c.place = Above;
            f.callouts << c;
        }
        return f;
    }, nullptr };

    // Connections, by universe, with the first universe's output showing.
    list << Shot{ QStringLiteral("connections-by-universe"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QString());   // something to send
        Frame f;
        QCheckBox *byUni = qobject_cast<QCheckBox *>(named(app, "connectionsByUniverse"));
        QTreeWidget *tree = qobject_cast<QTreeWidget *>(named(app, "connectionsTree"));
        if (byUni == NULL || tree == NULL)
            return f;
        showTab(tree);
        byUni->setChecked(true);
        settle();
        if (tree->topLevelItemCount() > 0)
        {
            tree->setCurrentItem(tree->topLevelItem(0));
            tree->topLevelItem(0)->setSelected(true);
        }
        f.grab = app;
        Callout by = mark(byUni, 1);
        by.place = Corner;          // to its right is the next tick box
        f.callouts << by
                   << mark(tree, 2)
                   << mark(named(app, "connectionsOutput"), 3);
        return f;
    }, nullptr };

    // The Patch dialog, proposing addresses for every fixture in the show.
    list << Shot{ QStringLiteral("patch-dialog"), [=]() {
        window();
        QList<quint32> ids;
        for (Fixture *fx : doc->fixtures())
            ids << fx->id();
        PatchInventoryDialog *dlg = new PatchInventoryDialog(doc, ids, app);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setObjectName(QStringLiteral("helpShotPatch"));
        dlg->resize(1100, 680);
        dlg->show();
        Frame f;
        f.grab = dlg;
        f.pad = QMargins(30, 0, 0, 0);
        for (const char *name : { "placeHow", "addressOrder" })
        {
            Callout c = mark(named(dlg, name), f.callouts.size() + 1);
            c.place = Above;        // their labels are on their left
            f.callouts << c;
        }
        f.callouts << mark(named(dlg, "patchTable"), 3)
                   << mark(named(dlg, "patchSummary"), 4)
                   << mark(named(dlg, "patchApply"), 5);
        return f;
    }, [=]() {
        if (QWidget *dlg = app->findChild<QWidget *>(QStringLiteral("helpShotPatch")))
            dlg->close();
    } };

    // The Fixtures tab: its group folders open, @a group selected (its
    // head-layout grid on the right), or nothing selected when empty.
    auto fixturesTab = [=](const QString &group) -> FixtureManager * {
        FixtureManager *fm = app->findChild<FixtureManager *>();
        if (fm == NULL)
        {
            fprintf(stderr, "[HELPSHOT] no Fixtures tab\n");
            return NULL;
        }
        showTab(fm);
        QTreeWidget *tree = fm->m_fixtures_tree;
        tree->clearSelection();
        QTreeWidgetItem *hit = NULL;
        for (QTreeWidgetItemIterator it(tree); *it != NULL; ++it)
            if ((*it)->data(0, PROP_GROUP).isValid())
            {
                for (QTreeWidgetItem *up = (*it)->parent(); up != NULL; up = up->parent())
                    up->setExpanded(true);
                if (!group.isEmpty() && (*it)->text(0) == group)
                    hit = *it;
            }
        if (hit != NULL)
            tree->setCurrentItem(hit);
        else if (!group.isEmpty())
            fprintf(stderr, "[HELPSHOT] no group %s\n", qPrintable(group));
        fm->slotSelectionChanged();
        settle();
        return fm;
    };
    // The Lighting Studio tab in @a pov, its Plot layers panel open.
    auto studio = [=](MonitorGraphicsView::ViewPOV pov) -> Monitor * {
        Monitor *mon = Monitor::instance();
        if (mon == NULL || mon->m_povCombo == NULL)
        {
            fprintf(stderr, "[HELPSHOT] no Lighting Studio\n");
            return NULL;
        }
        showTab(mon);
        if (mon->m_layersAction != NULL)
            mon->m_layersAction->setChecked(true);
        const int idx = mon->m_povCombo->findData(int(pov));
        if (idx >= 0)
            mon->m_povCombo->setCurrentIndex(idx);
        mon->slotPOVChanged(mon->m_povCombo->currentIndex());
        settle();
        return mon;
    };

    // The Fixtures tab: the group folders.
    list << Shot{ QStringLiteral("fixture-manager-groups"), [=]() {
        window();
        Frame f;
        if (fixturesTab(QString()) != NULL)
            f.grab = app;
        return f;
    }, nullptr };

    // A group selected: its head-layout grid.
    list << Shot{ QStringLiteral("fixture-manager-grid-editor"), [=]() {
        window();
        Frame f;
        if (fixturesTab(QStringLiteral("Pars")) != NULL)
            f.grab = app;
        return f;
    }, [=]() { fixturesTab(QString()); } };

    // Connections: the Devices view, the signal followed outward.
    list << Shot{ QStringLiteral("connections-devices-tab"), [=]() {
        window();
        Frame f;
        QCheckBox *byUni = qobject_cast<QCheckBox *>(named(app, "connectionsByUniverse"));
        QTreeWidget *tree = qobject_cast<QTreeWidget *>(named(app, "connectionsTree"));
        if (byUni == NULL || tree == NULL)
            return f;
        showTab(tree);
        byUni->setChecked(false);
        settle();
        tree->expandAll();
        settle();
        f.grab = app;
        return f;
    }, nullptr };

    // The Lighting Studio from above, and from the front.
    list << Shot{ QStringLiteral("lighting-studio-top-view"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QString());   // the rig lit
        Frame f;
        if (studio(MonitorGraphicsView::PovTop) != NULL)
            f.grab = app;
        return f;
    }, nullptr };

    list << Shot{ QStringLiteral("lighting-studio-front-view-badges"), [=]() {
        window();
        Frame f;
        if (studio(MonitorGraphicsView::PovFront) != NULL)
            f.grab = app;
        return f;
    }, [=]() { studio(MonitorGraphicsView::PovTop); } };

    // The Rig Overview, over the studio.
    list << Shot{ QStringLiteral("lighting-studio-rig-overview-3d"), [=]() {
        window();
        Frame f;
        Monitor *mon = studio(MonitorGraphicsView::PovTop);
        if (mon == NULL)
            return f;
        mon->showStageOverview();
        settle();
        if (mon->m_overviewDlg == NULL)
            return f;
        // The software renderer: offscreen there is no graphics card to ask.
        if (QCheckBox *gpu = mon->m_overviewDlg->findChild<QCheckBox *>(QStringLiteral("rigGpu")))
            gpu->setChecked(false);
        mon->m_overviewDlg->setFloating(true);   // a picture of the panel alone
        mon->m_overviewDlg->resize(1100, 760);
        settle();
        f.grab = mon->m_overviewDlg;
        return f;
    }, [=]() {
        Monitor *mon = Monitor::instance();
        if (mon != NULL && mon->m_overviewDlg != NULL)
            mon->m_overviewDlg->close();
    } };

    /* A truss's own editor, then with one of its fixtures picked. The editor
       is modal: each picture is taken from inside its event loop. */
    auto trussEditor = [=](const QString &name, bool pickFixture) {
        Frame done;
        done.saved = true;
        Monitor *mon = studio(MonitorGraphicsView::PovTop);
        const QList<Truss *> trusses = mon != NULL ? mon->m_props->trusses() : QList<Truss *>();
        if (trusses.isEmpty())
        {
            fprintf(stderr, "[HELPSHOT] %s: no truss\n", qPrintable(name));
            ++m_failed;
            return done;
        }
        QTimer::singleShot(0, this, [=]() {
            QWidget *dlg = QApplication::activeModalWidget();
            Frame f;
            if (dlg != NULL)
            {
                dlg->resize(1500, 780);
                settle();
                if (pickFixture)
                    for (QTreeWidget *tree : dlg->findChildren<QTreeWidget *>())
                        for (QTreeWidgetItemIterator it(tree); *it != NULL; ++it)
                            if ((*it)->text(0).startsWith(QStringLiteral("Mover 1")))
                            {
                                tree->setCurrentItem(*it);
                                break;
                            }
                settle();
                f.grab = dlg;
            }
            save(name, f);
            if (QDialog *d = qobject_cast<QDialog *>(dlg))
                d->reject();
        });
        Truss *truss = trusses.first();
        for (Truss *t : trusses)
            if (t->name() == QLatin1String("US Truss"))
                truss = t;
        mon->slotEditTruss(truss->id());
        return done;
    };
    list << Shot{ QStringLiteral("lighting-studio-truss-editor"), [=]() {
        window();
        return trussEditor(QStringLiteral("lighting-studio-truss-editor"), false);
    }, nullptr };
    list << Shot{ QStringLiteral("lighting-studio-fixture-mount-properties"), [=]() {
        window();
        return trussEditor(QStringLiteral("lighting-studio-fixture-mount-properties"), true);
    }, nullptr };

    // The look editor on a Colour look.
    list << Shot{ QStringLiteral("programming-tab-look-editor-color"), [=]() {
        window();
        openScene(QStringLiteral("05 Stage Shape"), QStringLiteral("White"));
        Frame f;
        f.grab = app;
        return f;
    }, nullptr };

    // The stack checker: a second movement effect covers the first. Last,
    // as it changes the show (and puts it back).
    list << Shot{ QStringLiteral("effects-stack-checker"), [=]() {
        window();
        Frame f;
        Scene *scene = qobject_cast<Scene *>(function(QStringLiteral("06 Stage Shape")));
        QLCPalette *line = palette(QStringLiteral("Line"));
        if (scene == NULL || line == NULL)
            return f;
        if (!scene->palettes().contains(line->id()))
            scene->addPalette(line->id());
        openScene(scene->name(), QString());
        ProgrammingOutline *outline = pm ? pm->m_workbench->rows() : NULL;
        if (outline == NULL)
            return f;
        outline->rebuild();
        QTreeWidgetItem *sceneRow = outline->sceneItem(scene->id());
        outline->setSubtreeExpanded(sceneRow, true);
        QTreeWidgetItem *covered = NULL;
        for (QTreeWidgetItemIterator it(sceneRow); *it != NULL && covered == NULL; ++it)
            if ((*it)->text(0).contains(QStringLiteral("Circle")))
                covered = *it;
        if (covered != NULL)
            outline->scrollToItem(covered);
        settle();
        f.grab = outline;
        f.callouts << markRow(outline, covered, outline, 1)
                   << markRow(outline, sceneRow, outline, 2);
        return f;
    }, [=]() {
        // Put the show back as it was.
        Scene *scene = qobject_cast<Scene *>(function(QStringLiteral("06 Stage Shape")));
        QLCPalette *line = palette(QStringLiteral("Line"));
        if (scene != NULL && line != NULL)
            scene->removePalette(line->id());
        doc->resetModified();
    } };

    return list;
}
