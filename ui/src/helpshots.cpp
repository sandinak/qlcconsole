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
#include <QTextBrowser>
#include <QTableWidget>
#include <QDir>

#include "programmingmanager.h"
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "truss.h"
#include "stageplatform.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "patchinventorydialog.h"
#include "functionstreewidget.h"
#include "scenegrouplooks.h"
#include "effectbrowser.h"
#include "lookeditor.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "focuscallsdialog.h"
#include "venuerigdialog.h"
#include "venuecorrections.h"
#include "focusreport.h"
#include "inputoutputmap.h"
#include "focuscontrolmap.h"
#include "focusmodepanel.h"
#include "refinesession.h"
#include "fixturegroup.h"
#include "refinepanel.h"
#include "stagetarget.h"
#include "focuscalls.h"
#include "gamepadrole.h"
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
                                  "speedMastersFooter", "footerStandby", "footerMode" })
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

    // The stage marks: centre mark and line, curtain line, number line.
    list << Shot{ QStringLiteral("lighting-studio-stage-marks"), [=]() {
        window();
        Frame f;
        Monitor *mon = studio(MonitorGraphicsView::PovTop);
        if (mon == NULL)
            return f;
        /* A stage with an apron built on in front of it, and the FOH
           electric out over the audience, for this picture only: the demo
           is a bare floor. */
        const float u = mon->m_props->gridUnitMetres();
        const QVector3D grid = mon->m_props->gridSize();
        StagePlatform *deck = mon->m_props->addPlatform();
        deck->setName(QStringLiteral("Deck"));
        deck->setOriginX(1.0f * u);
        deck->setOriginY(0.5f * u);
        deck->setWidth((grid.x() - 2.0f) * u);
        deck->setDepth((grid.z() - 3.0f) * u);
        deck->setHeight(0.9f);
        deck->setColor(QColor(40, 44, 52));
        mon->setProperty("helpShotDeck", deck->id());
        // The apron: from the curtain line (found 1.2 m up from the deck's
        // front) out to a unit and a half short of the grid's front.
        mon->m_props->setApronDepth(1.2f + 1.5f * u);
        if (StagePlatform *apron = mon->m_props->apronPlatform())
        {
            apron->setFrontCurve(0.5f * u);
            mon->setProperty("helpShotApron", apron->id());
        }
        Truss *foh = mon->m_props->addTruss();
        foh->setName(QStringLiteral("FOH"));
        foh->setType(Truss::Horizontal);
        foh->setOrigin(QVector3D(3.0f * u, (grid.z() - 0.4f) * u, 6.0f));
        foh->setLength((grid.x() - 6.0f) * u);
        mon->setProperty("helpShotFoh", foh->id());
        mon->m_graphicsView->updateTrusses();
        mon->m_graphicsView->updatePlatforms();
        mon->m_props->setStageMarks(MonitorProperties::CentreMarkFlag | MonitorProperties::CentreLineFlag
                                    | MonitorProperties::CurtainLineFlag
                                    | MonitorProperties::NumberLineFlag);
        mon->m_graphicsView->updateStageMarks();
        settle();
        f.grab = mon->m_graphicsView;
        return f;
    }, [=]() {
        Monitor *mon = Monitor::instance();
        if (mon == NULL)
            return;
        mon->m_props->setStageMarks(MonitorProperties::NoStageMarks);
        mon->m_props->setCurtainLineAuto();
        foreach (const char *name, QList<const char *>() << "helpShotDeck" << "helpShotApron")
        {
            if (mon->property(name).isValid())
                mon->m_props->removePlatform(mon->property(name).toUInt());
            mon->setProperty(name, QVariant());
        }
        if (mon->property("helpShotFoh").isValid())
            mon->m_props->removeTruss(mon->property("helpShotFoh").toUInt());
        mon->setProperty("helpShotFoh", QVariant());
        mon->m_graphicsView->updateTrusses();
        mon->m_graphicsView->updatePlatforms();
    } };

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

    /* Focus calls and Refine (FOCUS_REFINE_DESIGN.md): the demo's Movers and
       Pars, to the Centre target, and a cue of Movers to refine. */
    auto demoCalls = [doc]() {
        QList<FocusCall> calls;
        quint32 centre = 0xFFFFFFFF;
        if (MonitorProperties *props = doc->monitorProperties())
            for (const StageTarget *t : props->stageTargets())
                centre = t->id();
        for (const FixtureGroup *g : doc->fixtureGroups())
        {
            FocusCall c;
            c.groups << g->id();
            if (g->name() == QLatin1String("Movers"))
            {
                c.target.kind = FocusSession::Reference::Target;
                c.target.id = centre;
                c.note = QStringLiteral("Cut off the proscenium");
            }
            calls << c;
        }
        doc->programmer()->focusCalls()->setCalls(calls);
    };
    // An Xbox pad's every control, as a connected one's profile names them.
    auto xboxMap = [](FocusControlMap *map) {
        QMap<int, QString> all;
        for (int r = GamepadRole::A; r <= GamepadRole::RightTrigger; ++r)
            if (r != GamepadRole::Guide)
                all.insert(r, QString());
        map->setControls(all);
    };
    list << Shot{ QStringLiteral("focus-calls-dialog"), [=]() {
        demoCalls();
        Frame f;
        FocusCallsDialog *d = new FocusCallsDialog(doc, app);
        d->setObjectName(QStringLiteral("helpShotFocusCalls"));
        d->resize(820, 520);
        d->show();
        settle();
        f.grab = d;
        Callout target = mark(named(d, "fcTarget"), 3);
        target.place = Right;
        Callout venue = mark(named(d, "fcVenue"), 4);
        venue.place = Right;
        f.callouts << mark(named(d, "fcCalls"), 1) << mark(named(d, "fcGroups"), 2)
                   << target << venue << mark(named(d, "fcStart"), 5);
        return f;
    }, [=]() {
        if (QWidget *d = app->findChild<QWidget *>(QStringLiteral("helpShotFocusCalls")))
            d->deleteLater();
        settle();
    } };

    list << Shot{ QStringLiteral("focus-calls-panel"), [=]() {
        demoCalls();
        Frame f;
        FocusSession::Options o;
        o.calls = doc->programmer()->focusCalls()->sessionCalls();
        o.others = FocusSession::OthersDim;
        QString why;
        FocusModePanel *panel = FocusModePanel::startWith(doc, app, o, &why,
                                                          int(FocusSession::KeyboardInput));
        if (panel == NULL)
        {
            fprintf(stderr, "[HELPSHOT] focus calls did not start: %s\n", qPrintable(why));
            return f;
        }
        panel->controlMap()->setMode(FocusControlMap::ControllerMap);
        xboxMap(panel->controlMap());
        panel->resize(700, 720);
        settle();
        f.grab = panel;
        f.callouts << mark(named(panel, "focusCall"), 1) << mark(named(panel, "focusScope"), 2)
                   << mark(panel->controlMap(), 3);
        return f;
    }, [=]() {
        doc->programmer()->focusSession()->stop();
        settle();
    } };

    list << Shot{ QStringLiteral("refine-panel"), [=]() {
        Frame f;
        ShowTreeController *st = doc->showTree();
        quint32 movers = 0xFFFFFFFF;
        for (const FixtureGroup *g : doc->fixtureGroups())
            if (g->name() == QLatin1String("Movers"))
                movers = g->id();
        const quint32 prod = st->addProduction(QStringLiteral("Spring Show"));
        const quint32 cueList = st->addCueList(prod, QStringLiteral("Main"));
        const quint32 cue = st->addCue(cueList, -1, QStringLiteral("Ballad"));
        ShowTree::Target t;
        t.groups << movers;
        const quint32 layer = st->addLayer(cue, t);
        ShowTree::Look pos;
        pos.key = ShowTree::KeyPosition;
        pos.palettes << st->createInlinePalette(QLCPalette::PanTilt, { 270, 135 }, QStringLiteral("Centre"));
        st->setLayerLook(cue, layer, pos);
        RefinePanel *panel = RefinePanel::open(doc, app, cue, layer);
        if (panel == NULL)
            return f;
        RefineSession *r = doc->programmer()->refineSession();
        r->setFlashEnabled(false);
        r->pickFixture(r->layerFixtures().value(2));
        r->nudgePosition(3.0f, -2.0f);
        xboxMap(panel->controlMap());
        panel->resize(700, 720);
        settle();
        f.grab = panel;
        f.callouts << mark(named(panel, "refineLayer"), 1) << mark(named(panel, "refineScope"), 2)
                   << mark(named(panel, "refinePending"), 3) << mark(panel->controlMap(), 4);
        return f;
    }, [=]() {
        doc->programmer()->refineSession()->stop();
        settle();
    } };

    /* Venues (VENUES.md): the demo's Pars as Lincoln HS's house rig, the
       fourth universe a House slot, a range of the first one not ours. */
    auto groupNamed = [doc](const QString &name) -> FixtureGroup * {
        for (FixtureGroup *g : doc->fixtureGroups())
            if (g->name() == name)
                return g;
        return nullptr;
    };
    auto endVenue = [doc, groupNamed]() {
        VenueCorrections *v = doc->programmer()->venueCorrections();
        if (FixtureGroup *pars = groupNamed(QStringLiteral("Pars")))
            pars->setOwner(RigOwner::Unset);
        if (doc->inputOutputMap()->universesCount() > 3)
            doc->inputOutputMap()->setUniverseSlotOwner(3, RigOwner::Unset);
        v->setCheckedBy(QString());
        v->setActiveVenue(QString());
        for (const QString &name : v->venueNames())
            v->removeVenue(name);
        doc->resetModified();
    };
    list << Shot{ QStringLiteral("venue-rig-page"), [=]() {
        Frame f;
        VenueCorrections *v = doc->programmer()->venueCorrections();
        const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
        v->setActiveVenue(venue);
        if (FixtureGroup *pars = groupNamed(QStringLiteral("Pars")))
            pars->setOwner(RigOwner::House);
        if (doc->inputOutputMap()->universesCount() > 3)
            doc->inputOutputMap()->setUniverseSlotOwner(3, RigOwner::House);
        VenueRigDialog *d = new VenueRigDialog(doc, venue, app);
        d->setObjectName(QStringLiteral("helpShotVenueRig"));
        if (QTableWidgetItem *it = d->universeTable()->item(0, VenueRigDialog::NotOursCol))
            it->setText(QStringLiteral("401-512"));
        d->resize(820, 640);
        d->show();
        settle();
        f.grab = d;
        QWidget *clashes = named(d, "vrClashes");
        if (clashes == nullptr || !clashes->isVisible())
            clashes = named(d, "vrNoClashes");
        f.callouts << mark(named(d, "vrHouse"), 1) << mark(named(d, "vrUniverses"), 2)
                   << mark(named(d, "vrKeepPatch"), 3) << mark(clashes, 4);
        for (Callout &c : f.callouts)
            c.place = Left;
        f.pad = QMargins(40, 0, 0, 0);
        return f;
    }, [=]() {
        if (QWidget *d = app->findChild<QWidget *>(QStringLiteral("helpShotVenueRig")))
            delete d;
        endVenue();
        settle();
    } };

    /* The focus report of a venue: two movers corrected and checked, one
       dead, as the web page (the PDF is the same page). */
    list << Shot{ QStringLiteral("focus-report"), [=]() {
        demoCalls();
        Frame f;
        VenueCorrections *v = doc->programmer()->venueCorrections();
        MonitorProperties *props = doc->monitorProperties();
        const QString venue = v->addVenue(QStringLiteral("Lincoln HS"));
        v->setActiveVenue(venue);
        v->setCheckedBy(QStringLiteral("Sam"));
        QList<quint32> movers;
        if (FixtureGroup *g = groupNamed(QStringLiteral("Movers")))
            movers = g->fixtureList();
        for (int i = 0; i < movers.size() && i < 3; ++i)
        {
            FixtureRigProps rp = props->fixtureRigProps(movers.at(i));
            rp.panOffsetDeg = (i == 0) ? 1.3f : -0.8f * i;
            rp.tiltOffsetDeg = (i == 0) ? -0.4f : 0.5f;
            props->setFixtureRigProps(movers.at(i), rp);
            v->record(movers.at(i));
        }
        v->markChecked(movers.mid(0, 6));
        if (movers.size() > 6)
            v->setDead(movers.at(6), true);
        QTextBrowser *page = new QTextBrowser(app);
        page->setObjectName(QStringLiteral("helpShotFocusReport"));
        page->setWindowFlags(Qt::Window);
        page->setHtml(FocusReport::html(doc, venue, QStringLiteral("Spring Show")));
        page->resize(960, 350);
        page->show();
        settle();
        f.grab = page;
        return f;
    }, [=]() {
        if (QWidget *d = app->findChild<QWidget *>(QStringLiteral("helpShotFocusReport")))
            delete d;
        endVenue();
        settle();
    } };

    /* Scramble practice, focused back: six of eight movers put back on
       their marks and saved, one close, one not touched. */
    list << Shot{ QStringLiteral("practice-score"), [=]() {
        demoCalls();
        Frame f;
        VenueCorrections *v = doc->programmer()->venueCorrections();
        FocusCallsDialog calls(doc, app);
        QString why;
        if (!calls.startPractice(2.0f, 6.0f, 7, &why))
        {
            fprintf(stderr, "[HELPSHOT] practice did not start: %s\n", qPrintable(why));
            return f;
        }
        FocusSession::Options o;
        o.calls = doc->programmer()->focusCalls()->sessionCalls();
        o.others = FocusSession::OthersDim;
        FocusModePanel *panel = FocusModePanel::startWith(doc, app, o, &why,
                                                          int(FocusSession::KeyboardInput));
        FocusSession *s = doc->programmer()->focusSession();
        if (panel != nullptr)
        {
            const VenueCorrections::Venue pv = v->venue(v->activeVenue());
            MonitorProperties *props = doc->monitorProperties();
            for (int i = 0; i < s->count(); ++i)
            {
                const quint32 fid = s->currentFixture();
                if (pv.practice.truth.contains(fid) && i < s->count() - 1)
                {
                    const RigCalibration truth = pv.practice.truth.value(fid);
                    const FixtureRigProps rp = props->fixtureRigProps(fid);
                    // The last but one lands a little off; the rest on the mark.
                    const float miss = (i == s->count() - 2) ? 1.6f : 0.3f;
                    s->nudge(truth.panOffsetDeg - rp.panOffsetDeg + miss,
                             truth.tiltOffsetDeg - rp.tiltOffsetDeg - 0.2f);
                    s->save();
                }
                s->next();
            }
            QPointer<FocusModePanel> left(panel);
            s->stop();
            settle();
            if (!left.isNull())
                delete left.data();
        }
        QDialog *d = FocusCallsDialog::showPracticeResult(doc, app);
        if (d == nullptr)
            return f;
        d->setObjectName(QStringLiteral("helpShotPractice"));
        d->show();
        settle();
        f.grab = d;
        d->resize(620, d->sizeHint().height());
        settle();
        f.callouts << mark(named(d, "practiceResultText"), 1) << mark(named(d, "practiceResultReport"), 2);
        for (Callout &c : f.callouts)
            c.place = Left;
        f.pad = QMargins(40, 0, 0, 0);
        return f;
    }, [=]() {
        if (QWidget *d = app->findChild<QWidget *>(QStringLiteral("helpShotPractice")))
            delete d;
        doc->programmer()->focusSession()->stop();
        endVenue();
        settle();
    } };

    /* Focus mode's save lamp: this light and another not saved. */
    list << Shot{ QStringLiteral("focus-mode-save-status"), [=]() {
        demoCalls();
        Frame f;
        FocusSession::Options o;
        o.calls = doc->programmer()->focusCalls()->sessionCalls();
        o.others = FocusSession::OthersDim;
        QString why;
        FocusModePanel *panel = FocusModePanel::startWith(doc, app, o, &why,
                                                          int(FocusSession::KeyboardInput));
        if (panel == nullptr)
            return f;
        FocusSession *s = doc->programmer()->focusSession();
        s->setFlashEnabled(false);
        s->nudge(1.5f, 0.0f);
        s->next();
        s->nudge(0.0f, -1.0f);
        panel->resize(700, 720);
        settle();
        QWidget *lamp = named(panel, "focusStatusLamp");
        QWidget *words = named(panel, "focusSaveStatus");
        f.grab = panel;
        if (lamp != nullptr && words != nullptr)
            f.crop = QRect(lamp->mapTo(panel, QPoint(0, 0)), lamp->size())
                     | QRect(words->mapTo(panel, QPoint(0, 0)), words->size());
        f.pad = QMargins(12, 12, 12, 12);
        return f;
    }, [=]() {
        doc->programmer()->focusSession()->stop();
        doc->resetModified();
        settle();
    } };

    return list;
}
