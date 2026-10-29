/*
  Q Light Controller Plus
  courseshow_test.cpp

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
#include <QTreeWidgetItemIterator>
#include <QApplication>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QDataStream>
#include <QListWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QSplitter>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QMimeData>
#include <QTabBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QColorDialog>
#include <QComboBox>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QLabel>
#include <QTimer>
#include <QCheckBox>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QtTest>

#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#include "fixturemanager.h"
#include "mainbar.h"
#include "showtreeview.h"
#include "addfixture.h"
#undef protected
#undef private
#include "structurestudioview.h"
#include "monitorlayerspanel.h"
#include "fixturegroupsource.h"
#include "functionstreewidget.h"
#include "showtreecontroller.h"
#include "shutterautoopen.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "showembedding.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "cuerunner.h"
#include "outputrulesdialog.h"
#include "focuscallsdialog.h"
#include "monitorgraphicsview.h"
#include "focusmodepanel.h"
#include "venuecorrections.h"
#include "programmercontroller.h"
#include "focussession.h"
#include "focuscalls.h"
#include "refinepanel.h"
#include "refinesession.h"
#include "eosimportwizard.h"
#include "eosimporter.h"
#include "qxwimporter.h"
#include "virtualconsole.h"
#include "vcframe.h"
#include "vcslider.h"
#include "helpshots.h"
#include "showstatus.h"
#include "universe.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "paletteops.h"
#include "doc.h"

#include "courseshow_test.h"
#include "../common/dragpos.h"

using namespace ShowTree;

namespace
{
const QString kCourse = QStringLiteral(COURSE_DIR);
const QString kShows = kCourse + QStringLiteral("/shows");
const QString kClassroom = kShows + QStringLiteral("/ShowChoir-Classroom.qlcc");
const QString kL1Start = kShows + QStringLiteral("/lesson-1-1-start.qlcc");
const QString kL1Finish = kShows + QStringLiteral("/lesson-1-1-finish.qlcc");
const QString kL2Start = kShows + QStringLiteral("/lesson-1-2-start.qlcc");
const QString kL2Finish = kShows + QStringLiteral("/lesson-1-2-finish.qlcc");
const QString kL3Start = kShows + QStringLiteral("/lesson-1-3-start.qlcc");
const QString kL3Finish = kShows + QStringLiteral("/lesson-1-3-finish.qlcc");
const QString kL4Start = kShows + QStringLiteral("/lesson-1-4-start.qlcc");
const QString kL4Finish = kShows + QStringLiteral("/lesson-1-4-finish.qlcc");
const QString kL5Start = kShows + QStringLiteral("/lesson-1-5-start.qlcc");

/* Level 2's show files: lesson-2-N-start / -finish, each lesson starting
   from the one before's finish (2.1 from Lesson 1.4's). */
/* How many of Level 2's lessons are written (and have show files). */
const int kLevelTwoLessons = 3;

QString levelTwoStart(int n)
{
    return kShows + QStringLiteral("/lesson-2-%1-start.qlcc").arg(n);
}
QString levelTwoFinish(int n)
{
    return kShows + QStringLiteral("/lesson-2-%1-finish.qlcc").arg(n);
}

/* The base palettes every lesson uses -- typed, as the course teaches. */
const QStringList kPaletteCommands = {
    "palette colour \"White\" #ffffff",
    "palette colour \"Warm White\" #ffc982",
    "palette colour \"Red\" #ff0000",
    "palette colour \"Amber\" #ffb000",
    "palette colour \"Yellow\" #ffff00",
    "palette colour \"Green\" #00ff00",
    "palette colour \"Cyan\" #00ffff",
    "palette colour \"Blue\" #0000ff",
    "palette colour \"Lavender\" #b48cff",
    "palette colour \"Magenta\" #ff00ff",
    "palette dimmer \"Full\" 100",
    "palette dimmer \"75%\" 75",
    "palette dimmer \"Half\" 50",
    "palette dimmer \"25%\" 25",
    "palette dimmer \"10%\" 10",
    "palette dimmer \"Out\" 0",
    // A spot's shutter (the AT-Spots' is closed until a look opens it).
    "palette shutter \"Open\"",
};

/* What the lessons name: if one of these changes, a handout is wrong. */
const QStringList kGroups = { "Steps", "US", "CS", "DS", "StepTops",
                              "Trusses", "Trusses SR", "Trusses SL", "Truss-Spots",
                              "TrussW-RGB", "TrussW-W", "TrussC-RGB", "TrussC-W", "USTs",
                              "AT-Spots", "AT-W-ALL", "Lanterns" };
const QStringList kTargets = { "Soloist DS", "Center", "Riser Line" };

/* Lesson 1, as the handout's "type it" box has it. */
const QStringList kLessonOneTyped = {
    "production \"Spring Show\"",
    "list \"01-Everybody Scream\"",
    "cue + \"Pose\"",
    "TrussC-RGB colour Blue dimmer Full",
    "cue + \"Verse\"",
    "StepTops colour \"Warm White\" dimmer 75",
    "cue + \"Chorus\"",
    "Steps colour Magenta dimmer Full",
};

/* What Lesson 1 ends with: each cue's own layers and their looks. */
const QStringList kLessonOneResult = {
    "Spring Show (current) / 01-Everybody Scream (Tracking)",
    "1.0.0 Pose: TrussC-RGB [Colour Blue, Dimmer Full]",
    "1.0.1 Verse: StepTops [Colour Warm White, Dimmer 75%]",
    "1.0.2 Chorus: Steps [Colour Magenta, Dimmer Full]",
};

void settle(int ms = 0)
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
    if (ms > 0)
        QTest::qWait(ms);
}

/* How long a check waits for the rig to show a change. The engine writes
   the universes on its own timer thread, a frame or more after the UI
   asks -- and much later when the machine is busy (parallel gates). So a
   check of what the rig sends polls until it's there (waitUntil, QTRY_*
   with kRigWait); it never reads once after a fixed sleep. */
const int kRigWait = 5000;

/* Waits (up to @a ms) until @a ok holds, as the rig shows a change a
   frame or two (or, on a loaded machine, many) after it's made. */
template <typename F>
bool waitUntil(F ok, int ms = kRigWait)
{
    QElapsedTimer t;
    t.start();
    while (!ok())
    {
        if (t.elapsed() > ms)
            return false;
        QTest::qWait(20);
    }
    return true;
}

/* A Show Tree widget by its object name, wherever it is now: on the view,
   or in its header (+ Production, Undo, the hint at the top right), which
   the main window's top bar (MainBar) borrows while the Show Tree is the
   tab on show. */
template <typename T>
T *stFind(ShowTreeView *view, const QString &name)
{
    if (T *w = view->findChild<T *>(name))
        return w;
    QWidget *tools = MainBar::tabTools(view);
    if (tools == nullptr)
        return nullptr;
    if (tools->objectName() == name)
        return qobject_cast<T *>(tools);
    return tools->findChild<T *>(name);
}

/* Answers the next QInputDialog with @a text (OK), as a student types it. */
void answerNextInputDialog(const QString &text)
{
    QTimer *t = new QTimer();
    t->setInterval(20);
    QObject::connect(t, &QTimer::timeout, [t, text]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QInputDialog *d = qobject_cast<QInputDialog *>(w))
                if (d->isVisible())
                {
                    d->setTextValue(text);
                    d->accept();
                    t->stop();
                    t->deleteLater();
                    return;
                }
    });
    t->start();
}

QMimeData *idsMime(const QString &type, const QList<quint32> &ids)
{
    QMimeData *mime = new QMimeData();
    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    for (quint32 id : ids)
        ds << id;
    mime->setData(type, data);
    return mime;
}

/* Drag over @a w at @a at and let go, as the mouse does: returns what the
   hint at the top right said while over it. */
QString dragAndDrop(QWidget *w, const QPoint &at, QMimeData *mime, QLabel *hint,
                    Qt::KeyboardModifiers keys = Qt::NoModifier)
{
    QDragEnterEvent enter(dragPos(at), Qt::CopyAction, mime, Qt::LeftButton, keys);
    QCoreApplication::sendEvent(w, &enter);
    QDragMoveEvent move(dragPos(at), Qt::CopyAction, mime, Qt::LeftButton, keys);
    QCoreApplication::sendEvent(w, &move);
    const QString said = hint != nullptr ? hint->text() : QString();
    QDropEvent drop(at, Qt::CopyAction, mime, Qt::LeftButton, keys);
    QCoreApplication::sendEvent(w, &drop);
    delete mime;
    settle();
    return said;
}

FixtureGroup *groupNamed(Doc *doc, const QString &name)
{
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name() == name)
            return g;
    return nullptr;
}

QLCPalette *paletteNamed(Doc *doc, const QString &name)
{
    for (QLCPalette *p : doc->palettes())
        if (p->name() == name)
            return p;
    return nullptr;
}

/* The Library row named @a text under All (a group in its folder, a
   palette under its type): what a student finds and drags. */
QTreeWidgetItem *libraryRow(QTreeWidget *lib, const QString &text)
{
    QTreeWidgetItem *all = nullptr;
    for (int i = 0; i < lib->topLevelItemCount(); ++i)
        if (lib->topLevelItem(i)->text(0).startsWith(QLatin1String("All")))
            all = lib->topLevelItem(i);
    if (all == nullptr)
        return nullptr;
    QList<QTreeWidgetItem *> todo = { all };
    while (!todo.isEmpty())
    {
        QTreeWidgetItem *it = todo.takeFirst();
        // A group shows how many fixtures it holds: "Steps (12)".
        if (it != all && (it->text(0) == text || it->text(0).startsWith(text + QStringLiteral(" ("))))
            return it;
        for (int i = 0; i < it->childCount(); ++i)
            todo << it->child(i);
    }
    return nullptr;
}

/* Each cue's own layers and looks, one line per cue, for comparing. */
QStringList describe(Doc *doc)
{
    QStringList out;
    const ShowTreeController *st = doc->showTree();
    auto paletteName = [doc](quint32 id) {
        const QLCPalette *p = doc->palette(id);
        return p != nullptr ? p->name() : QStringLiteral("?");
    };
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
        {
            if (l.looks || l.isSubCues())
                continue;
            out << QStringLiteral("%1%2 / %3 (%4)").arg(p.name,
                                                         st->currentProduction() == p.id ? " (current)" : "",
                                                         l.name,
                                                       l.mode == ListMode::Tracking ? "Tracking" : "Non-tracking");
            for (const Cue &c : l.cues)
            {
                QStringList layers;
                for (const Layer &ly : c.layers)
                {
                    QStringList groups;
                    for (quint32 g : ly.target.groups)
                        groups << (doc->fixtureGroup(g) ? doc->fixtureGroup(g)->name() : QStringLiteral("?"));
                    if (!ly.target.sets.isEmpty())     // a group set (Level 2)
                        groups << st->targetLabel(ly.target);
                    QStringList looks;
                    for (const Look &lk : ly.looks)
                    {
                        QStringList names;
                        for (quint32 pid : lk.palettes)
                            names << paletteName(pid);
                        looks << QStringLiteral("%1 %2").arg(keyLabel(lk.key), names.join(" + "));
                    }
                    layers << QStringLiteral("%1 [%2]").arg(groups.join(" + "), looks.join(", "));
                }
                out << QStringLiteral("%1 %2: %3").arg(c.number, c.name, layers.join("; "));
            }
        }
    return out;
}

/* What @a fixture sends now (its first @a n channels), as the rig shows it. */
QByteArray sends(Doc *doc, const QString &fixture, int n = 3)
{
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == fixture)
        {
            QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
            const QByteArray v = us.at(int(fx->universe()))->preGMValues().mid(int(fx->address()), n);
            doc->inputOutputMap()->releaseUniverses(false);
            return v;
        }
    return QByteArray();
}

/* ---- Helpers for Lessons 1.2-1.5: each is what a student does with the
   mouse, and returns what the top right said while (or after) doing it. */

/* The docked picker (Q61): ⋯ → Level… opened it in the Show Tree; type
   @a value and Apply, as a student does. */
bool pickDockedLevel(ShowTreeView *view, int value)
{
    QSpinBox *spin = view->findChild<QSpinBox *>(QStringLiteral("stPickerLevel"));
    QPushButton *apply = view->findChild<QPushButton *>(QStringLiteral("stPickerApply"));
    if (spin == nullptr || apply == nullptr || !view->dockedPickerOpen())
        return false;
    spin->setValue(value);
    apply->click();
    return !view->dockedPickerOpen();
}

/* The docked picker's colour (⋯ → Colour…): pick @a c and Apply. */
bool pickDockedColour(ShowTreeView *view, const QColor &c)
{
    QColorDialog *picker = view->findChild<QColorDialog *>(QStringLiteral("stPickerColour"));
    QPushButton *apply = view->findChild<QPushButton *>(QStringLiteral("stPickerApply"));
    if (picker == nullptr || apply == nullptr || !view->dockedPickerOpen())
        return false;
    picker->setCurrentColor(c);
    apply->click();
    return !view->dockedPickerOpen();
}

QLabel *hintOf(ShowTreeView *view)
{
    return stFind<QLabel>(view, "stDropHint");
}

/* + Cue, then its name typed in the Name box and Return. */
bool newCueNamed(ShowTreeView *view, const QString &name)
{
    stFind<QToolButton>(view, "stNewCue")->click();
    settle();
    QLineEdit *box = stFind<QLineEdit>(view, "stCueName");
    if (view->currentKind() != ShowTreeView::CueNode || box == nullptr)
        return false;
    box->setText(name);
    QTest::keyClick(box, Qt::Key_Return);
    settle();
    return view->m_st->cue(view->currentCueId()) != nullptr
           && view->m_st->cue(view->currentCueId())->name == name;
}

/* The selected cue's page: a drop on the dashed area. */
QString dropOnZone(ShowTreeView *view, QMimeData *mime)
{
    QWidget *page = stFind<QWidget>(view, "stCuePage");
    QLabel *zone = stFind<QLabel>(view, "stCueDropZone");
    return dragAndDrop(page, zone->mapTo(page, zone->rect().center()), mime, hintOf(view));
}

QString dropGroupOnZone(Doc *doc, ShowTreeView *view, const QString &group)
{
    if (libraryRow(view->library(), group) == nullptr)
        return QStringLiteral("no Library row ") + group;
    return dropOnZone(view, idsMime(FixtureGroupSource::fixtureGroupMimeType(), { groupNamed(doc, group)->id() }));
}

QString dropPaletteOnZone(Doc *doc, ShowTreeView *view, const QString &palette)
{
    if (libraryRow(view->library(), palette) == nullptr)
        return QStringLiteral("no Library row ") + palette;
    return dropOnZone(view, idsMime(FunctionsTreeWidget::paletteDragMimeType(), { paletteNamed(doc, palette)->id() }));
}

/* The cue page's row that starts with @a start (a layer, or "⤳ …" grey). */
QListWidgetItem *cueRow(ShowTreeView *view, const QString &start)
{
    QListWidget *rows = view->cueLayerList();
    for (int i = 0; i < rows->count(); ++i)
        if (rows->item(i)->text().startsWith(start))
            return rows->item(i);
    // A child layer's row: "   ↳ US", indented under its parent.
    for (int i = 0; i < rows->count(); ++i)
    {
        QString t = rows->item(i)->text().trimmed();
        if (t.startsWith(QStringLiteral("↳ ")))
            t = t.mid(2);
        if (t.startsWith(start))
            return rows->item(i);
    }
    return nullptr;
}

QString dropOnRow(ShowTreeView *view, const QString &start, QMimeData *mime)
{
    QListWidgetItem *it = cueRow(view, start);
    if (it == nullptr)
    {
        delete mime;
        return QStringLiteral("no row ") + start + QStringLiteral(" in ") + view->cueLayerItems().join(" | ");
    }
    QListWidget *rows = view->cueLayerList();
    return dragAndDrop(rows->viewport(), rows->visualItemRect(it).center(), mime, hintOf(view));
}

QString dropPaletteOnRow(Doc *doc, ShowTreeView *view, const QString &start, const QString &palette)
{
    if (libraryRow(view->library(), palette) == nullptr)
        return QStringLiteral("no Library row ") + palette;
    return dropOnRow(view, start, idsMime(FunctionsTreeWidget::paletteDragMimeType(),
                                          { paletteNamed(doc, palette)->id() }));
}

StageTarget *targetNamed(Doc *doc, const QString &name)
{
    for (StageTarget *t : doc->monitorProperties()->stageTargets())
        if (t->name() == name)
            return t;
    return nullptr;
}

QString dropTargetOnRow(Doc *doc, ShowTreeView *view, const QString &start, const QString &target)
{
    StageTarget *t = targetNamed(doc, target);
    if (t == nullptr || libraryRow(view->library(), target) == nullptr)
        return QStringLiteral("no target ") + target;
    return dropOnRow(view, start, idsMime(ShowTreeView::targetMimeType(), { t->id() }));
}

/* Click a row on the cue page: its layer's page opens. */
bool clickCueRow(ShowTreeView *view, const QString &start)
{
    QListWidgetItem *it = cueRow(view, start);
    if (it == nullptr)
        return false;
    QListWidget *rows = view->cueLayerList();
    QTest::mouseClick(rows->viewport(), Qt::LeftButton, Qt::NoModifier, rows->visualItemRect(it).center());
    settle(50);
    return view->currentKind() == ShowTreeView::LayerNode
           || view->currentKind() == ShowTreeView::TrackedLayerNode;
}

/* On a layer's page, the text of its @a key row (Colour, Dimmer…). */
QString rowText(ShowTreeView *view, const QString &key)
{
    QLabel *l = stFind<QLabel>(view, QStringLiteral("stRow:%1").arg(key));
    return l == nullptr ? QString() : l->text();
}

/* On a layer's page: the ⋯ button of the @a key row. */
QToolButton *moreButton(ShowTreeView *view, const QString &key)
{
    QLabel *row = stFind<QLabel>(view, QStringLiteral("stRow:%1").arg(key));
    if (row == nullptr)
        return nullptr;
    const int y = row->mapToGlobal(row->rect().center()).y();
    QWidget *host = row->parentWidget();
    while (host != nullptr && host->findChildren<QToolButton *>().isEmpty())
        host = host->parentWidget();
    if (host == nullptr)
        return nullptr;
    for (QToolButton *b : host->findChildren<QToolButton *>())
        if (b->text() == QStringLiteral("⋯") && b->isVisible()
            && qAbs(b->mapToGlobal(b->rect().center()).y() - y) < b->height())
            return b;
    return nullptr;
}

/* On a layer's page: the ⋯ of the @a key row, then the menu entry @a menu
   (and in its submenu @a item, when given). Returns whether it was found. */
bool rowMenu(ShowTreeView *view, const QString &key, const QString &menu, const QString &item = QString())
{
    QToolButton *more = moreButton(view, key);
    if (more == nullptr)
        return false;
    bool chose = false;
    QTimer::singleShot(100, [&chose, menu, item]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMenu *m = qobject_cast<QMenu *>(w))
                if (m->isVisible())
                    for (QAction *a : m->actions())
                    {
                        if (a->text() != menu)
                            continue;
                        if (item.isEmpty())
                        {
                            chose = true;
                            m->close();
                            a->trigger();
                            return;
                        }
                        if (a->menu() != nullptr)
                            for (QAction *p : a->menu()->actions())
                                if (p->text() == item)
                                {
                                    chose = true;
                                    m->close();
                                    p->trigger();
                                    return;
                                }
                    }
    });
    more->click();
    settle(100);
    return chose;
}

/* Each cue's looks of its own (cue looks), one line per cue that has any. */
QStringList cueLooks(Doc *doc)
{
    QStringList out;
    const ShowTreeController *st = doc->showTree();
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
            {
                if (c.looks.isEmpty())
                    continue;
                QStringList looks;
                for (const Look &lk : c.looks)
                {
                    QStringList names;
                    for (quint32 pid : lk.palettes)
                        names << (doc->palette(pid) ? doc->palette(pid)->name() : QStringLiteral("?"));
                    looks << QStringLiteral("%1 %2").arg(keyLabel(lk.key), names.join(" + "));
                }
                out << QStringLiteral("%1 cue looks [%2]").arg(c.number, looks.join(", "));
            }
    return out;
}

/* The cue numbered @a number in the first list. */
quint32 cueNumbered(Doc *doc, const QString &number)
{
    const ShowTreeController *st = doc->showTree();
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                if (c.number == number)
                    return c.id;
    return ShowTree::invalidId();
}

/* The pan and tilt (coarse) @a fixture sends now. */
QPair<int, int> panTilt(Doc *doc, const QString &fixture)
{
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == fixture)
        {
            const QByteArray v = sends(doc, fixture, int(fx->channels()));
            int pan = -1, tilt = -1;
            for (quint32 c = 0; c < fx->channels(); ++c)
            {
                const QString n = fx->channel(c)->name();
                if (pan < 0 && n.startsWith(QLatin1String("Pan")) && !n.contains(QLatin1String("fine"), Qt::CaseInsensitive))
                    pan = uchar(v.at(int(c)));
                if (tilt < 0 && n.startsWith(QLatin1String("Tilt")) && !n.contains(QLatin1String("fine"), Qt::CaseInsensitive))
                    tilt = uchar(v.at(int(c)));
            }
            return qMakePair(pan, tilt);
        }
    return qMakePair(-1, -1);
}

/* The channel named @a name of @a fixture, as sent now (-1: none). */
int channelValue(Doc *doc, const QString &fixture, const QString &name)
{
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == fixture)
        {
            const QByteArray v = sends(doc, fixture, int(fx->channels()));
            for (quint32 c = 0; c < fx->channels(); ++c)
                if (fx->channel(c)->name() == name)
                    return uchar(v.at(int(c)));
        }
    return -1;
}

/* What @a fixture's channel @a name sends after the universe's own steps
   (auto-open shutters among them), not what the looks put in. */
int outputValue(Doc *doc, const QString &fixture, const QString &name)
{
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == fixture)
            for (quint32 c = 0; c < fx->channels(); ++c)
                if (fx->channel(c)->name() == name)
                {
                    QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
                    const QByteArray *post = us.at(int(fx->universe()))->postGMValues();
                    const int a = int(fx->address() + c);
                    const int v = a < post->size() ? uchar(post->at(a)) : 0;
                    doc->inputOutputMap()->releaseUniverses(false);
                    return v;
                }
    return -1;
}

}

void CourseShow_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    // The definitions that ship with the app, as a student's install has
    // them (a test program isn't inside an app bundle to find them).
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    m_app->showNormal();
    m_app->resize(1280, 800);
}

void CourseShow_Test::cleanupTestCase()
{
    if (Monitor *mon = Monitor::instance())
        if (mon->m_overviewDlg != nullptr)
            mon->m_overviewDlg->close();
    settle();
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

bool CourseShow_Test::open(const QString &path)
{
    // As File -> Open does: the show before goes first.
    m_app->doc()->resetModified();
    m_app->clearDocument();
    const bool ok = m_app->loadXML(path) == QFile::NoError;
    m_app->doc()->resetModified();
    if (Monitor::instance() != nullptr)
        Monitor::instance()->updateView();
    settle(50);
    return ok;
}

ShowTreeView *CourseShow_Test::showTree() const
{
    // Brought forward, as a student clicks its tab: the preview runs only
    // while it is the tab on show.
    ShowTreeView *view = ShowTreeView::instance();
    QWidget *w = view;
    while (w != nullptr && m_app->m_tab->indexOf(w) < 0)
        w = w->parentWidget();
    if (w != nullptr)
        m_app->m_tab->setCurrentWidget(w);
    settle();
    return view;
}

/* Lesson 1, done with the mouse as the handout says: the buttons, the
   dialogs, the Name box, the drags from the Library onto the cue page. Each
   step checks what the handout's "you should see" line says. */
static bool lessonOneByMouse(Doc *doc, ShowTreeView *view)
{
    ShowTreeController *st = doc->showTree();
    QLabel *hint = stFind<QLabel>(view, "stDropHint");
    QWidget *page = stFind<QWidget>(view, "stCuePage");
    QLabel *zone = stFind<QLabel>(view, "stCueDropZone");
    QLineEdit *name = stFind<QLineEdit>(view, "stCueName");
    if (hint == nullptr || page == nullptr || zone == nullptr || name == nullptr)
        { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

    // Step 1: + Production, "Spring Show".
    answerNextInputDialog(QStringLiteral("Spring Show"));
    stFind<QToolButton>(view, "stNewProduction")->click();
    settle();
    if (st->productions().count() != 1 || st->productions().first().name != "Spring Show"
        || view->currentKind() != ShowTreeView::ProductionNode)
        { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

    // Step 2: + Cue list, "01-Everybody Scream" -- Tracking.
    answerNextInputDialog(QStringLiteral("01-Everybody Scream"));
    stFind<QToolButton>(view, "stNewList")->click();
    settle();
    if (view->currentKind() != ShowTreeView::ListNode
        || st->productions().first().lists.count() != 1
        || st->productions().first().lists.first().mode != ListMode::Tracking)
        { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

    // Steps 3-8: three cues, each a group dropped on the dashed area and a
    // colour and a level dropped on its row.
    struct Step { QString cue, number, group, colour, level; };
    const QList<Step> steps = {
        { "Pose", "1.0.0", "TrussC-RGB", "Blue", "Full" },
        { "Verse", "1.0.1", "StepTops", "Warm White", "75%" },
        { "Chorus", "1.0.2", "Steps", "Magenta", "Full" },
    };
    for (const Step &s : steps)
    {
        stFind<QToolButton>(view, "stNewCue")->click();
        settle();
        const Cue *cue = st->cue(view->currentCueId());
        if (view->currentKind() != ShowTreeView::CueNode || cue == nullptr || cue->number != s.number)
        {
            qWarning() << "new cue" << (cue ? cue->number : QString("none"));
            { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }
        }
        // The Name box has the keyboard: type, then Return.
        name->setText(s.cue);
        QTest::keyClick(name, Qt::Key_Return);
        settle();
        if (st->cue(view->currentCueId())->name != s.cue)
            { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

        // The group from the Library onto the dashed area: a new layer.
        if (libraryRow(view->library(), s.group) == nullptr)
        {
            qWarning() << "no Library row" << s.group;
            { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }
        }
        const QPoint onZone = zone->mapTo(page, zone->rect().center());
        const QString said = dragAndDrop(page, onZone,
                                         idsMime(FixtureGroupSource::fixtureGroupMimeType(),
                                                 { groupNamed(doc, s.group)->id() }), hint);
        // The handout quotes the hint the top right shows while dragging.
        if (said != QStringLiteral("a new layer on %1 in %2 %3").arg(s.group, s.number, s.cue))
        {
            qWarning() << "drop hint:" << said;
            return false;
        }
        const Cue *c = st->cue(view->currentCueId());
        if (c->layers.count() != 1)
            { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

        // The colour, then the level, onto that layer's row.
        QListWidget *rows = view->cueLayerList();
        auto rowAt = [rows](const QString &start) {
            for (int i = 0; i < rows->count(); ++i)
                if (rows->item(i)->text().startsWith(start))
                    return rows->visualItemRect(rows->item(i)).center();
            return QPoint(-1, -1);
        };
        for (const QString &pal : { s.colour, s.level })
        {
            if (libraryRow(view->library(), pal) == nullptr)
            {
                qWarning() << "no Library row" << pal;
                { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }
            }
            const QPoint at = rowAt(s.group);
            if (at.x() < 0)
            {
                qWarning() << "no row" << s.group << view->cueLayerItems();
                { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }
            }
            const QString hinted = dragAndDrop(rows->viewport(), at,
                                               idsMime(FunctionsTreeWidget::paletteDragMimeType(),
                                                       { paletteNamed(doc, pal)->id() }), hint);
            if (!hinted.startsWith(QStringLiteral("%1 → layer %2 in %3 %4").arg(pal, s.group, s.number, s.cue)))
            {
                qWarning() << "palette hint:" << hinted;
                return false;
            }
        }
    }
    // No step to make it the show GO plays: the first production in a show
    // is current by itself (and clicking its Current button would unset it,
    // so the handout doesn't send students there).
    if (st->currentProduction() != st->productions().first().id)
        { qWarning() << "the new production isn't current"; return false; }

    if (describe(doc) != kLessonOneResult)
        qWarning() << describe(doc);
    return describe(doc) == kLessonOneResult;
}

void CourseShow_Test::buildTheShows()
{
    const QString rig = qEnvironmentVariable("COURSE_RIG");
    if (rig.isEmpty())
        QSKIP("set COURSE_RIG to the rig derive-classroom-rig.py wrote");
    QVERIFY(open(rig));
    Doc *doc = m_app->doc();
    ShowTreeView *view = showTree();
    QVERIFY(view != nullptr);

    for (const QString &cmd : kPaletteCommands)
        QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    // Aim palettes for the stage targets, as a drop of the target makes them.
    for (StageTarget *t : doc->monitorProperties()->stageTargets())
    {
        QLCPalette *p = new QLCPalette(QLCPalette::Aim, doc);
        p->setName(QStringLiteral("Aim %1").arg(t->name()));
        p->setStageTargetId(t->id());
        QVERIFY(doc->addPalette(p));
    }
    QCOMPARE(m_app->saveXML(kClassroom), QFile::NoError);
    QCOMPARE(m_app->saveXML(kL1Start), QFile::NoError);

    QVERIFY(open(kL1Start));
    QVERIFY(lessonOneByMouse(m_app->doc(), showTree()));
    QCOMPARE(m_app->saveXML(kL1Finish), QFile::NoError);
    m_app->doc()->resetModified();
}

void CourseShow_Test::theTeachingShowIsSafeAndComplete()
{
    QStringList shows = { kClassroom, kL1Start, kL1Finish, kL2Start, kL2Finish, kL3Start, kL3Finish,
                          kL4Start, kL4Finish, kL5Start };
    // Level 2's, as many as are written.
    for (int n = 1; n <= kLevelTwoLessons; ++n)
        shows << levelTwoStart(n) << levelTwoFinish(n);
    for (const QString &path : shows)
    {
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QVERIFY2(open(path), qPrintable(path));
        Doc *doc = m_app->doc();

        // It can't send DMX: no universe has an output (or an input).
        InputOutputMap *io = doc->inputOutputMap();
        for (quint32 u = 0; u < io->universesCount(); ++u)
        {
            QVERIFY2(io->outputPatchesCount(u) == 0, qPrintable(QString("%1: universe %2").arg(path).arg(u + 1)));
            QVERIFY(io->inputPatch(u) == nullptr);
        }

        // Every fixture has its own definition -- none stands in as a
        // generic dimmer -- though this computer has only the app's own:
        // the rest came out of the show file.
        QVERIFY(doc->fixtures().count() >= 60);
        for (Fixture *fx : doc->fixtures())
        {
            QVERIFY2(!fx->definitionUnavailable(), qPrintable(fx->name()));
            QVERIFY2(fx->fixtureDef() != nullptr
                     && fx->fixtureDef()->manufacturer() != KXMLFixtureGeneric, qPrintable(fx->name()));
            QVERIFY2(!fx->name().contains('#'), qPrintable(fx->name()));     // plain names
            // Nor does any fixture talk to the network by itself (a WLED
            // lantern at its own address would).
            QVERIFY2(fx->connection() == Fixture::Dmx && fx->ipAddress().isEmpty(), qPrintable(fx->name()));
        }

        for (const QString &g : kGroups)
            QVERIFY2(groupNamed(doc, g) != nullptr, qPrintable(g));
        for (const QString &cmd : kPaletteCommands)
        {
            const QString pal = cmd.section('"', 1, 1);
            QVERIFY2(paletteNamed(doc, pal) != nullptr, qPrintable(pal));
        }
        QStringList targets;
        for (StageTarget *t : doc->monitorProperties()->stageTargets())
            targets << t->name();
        for (const QString &t : kTargets)
        {
            QVERIFY2(targets.contains(t), qPrintable(t));
            QVERIFY(paletteNamed(doc, "Aim " + t) != nullptr);
        }
    }
    // What the footer's Not ready list says about it (the handout explains).
    settle(300);
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        qInfo().noquote() << "[READINESS]" << e.summary << "|" << e.items.join("; ").left(200);
    // The classroom show and Lesson 1's start have no show in them yet.
    QVERIFY(open(kClassroom));
    QVERIFY(m_app->doc()->showTree()->productions().isEmpty());
    QVERIFY(open(kL1Start));
    QVERIFY(m_app->doc()->showTree()->productions().isEmpty());
}

void CourseShow_Test::lessonOneWithTheMouse()
{
    QVERIFY(open(kL1Start));
    QVERIFY(lessonOneByMouse(m_app->doc(), showTree()));
    const QStringList built = describe(m_app->doc());
    QCOMPARE(built, kLessonOneResult);
    // The finish file is what the lesson makes.
    QVERIFY(open(kL1Finish));
    QCOMPARE(describe(m_app->doc()), built);
}

void CourseShow_Test::lessonOneSetInThisCue()
{
    QVERIFY(open(kL1Finish));
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    ShowTreeView *view = showTree();
    const CueList &list = st->productions().first().lists.first();
    const quint32 chorus = list.cues.at(2).id;
    QVERIFY(view->selectNode(chorus));
    settle(100);

    // Click the grey TrussC-RGB row on the chorus's page: its page opens.
    QListWidget *rows = view->cueLayerList();
    QListWidgetItem *grey = nullptr;
    for (int i = 0; i < rows->count(); ++i)
        if (rows->item(i)->text().startsWith(QStringLiteral("⤳ TrussC-RGB")))
            grey = rows->item(i);
    QVERIFY2(grey != nullptr, qPrintable(view->cueLayerItems().join(" | ")));
    QTest::mouseClick(rows->viewport(), Qt::LeftButton, Qt::NoModifier, rows->visualItemRect(grey).center());
    settle(100);
    QCOMPARE(view->currentKind(), ShowTreeView::TrackedLayerNode);

    // Its Colour row: Blue, grey, from 1.0.0. ⋯ → Set in this cue → Magenta.
    QLabel *colour = stFind<QLabel>(view, QStringLiteral("stRow:%1").arg(KeyColour));
    QVERIFY(colour != nullptr);
    QVERIFY2(colour->text().contains("Blue"), qPrintable(colour->text()));
    QToolButton *more = nullptr;
    const int y = colour->mapToGlobal(colour->rect().center()).y();
    for (QToolButton *b : colour->parentWidget()->findChildren<QToolButton *>())
        if (b->text() == QStringLiteral("⋯") && b->isVisible()
            && qAbs(b->mapToGlobal(b->rect().center()).y() - y) < b->height())
            more = b;
    QVERIFY(more != nullptr);
    bool chose = false;
    QTimer::singleShot(100, [&chose]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMenu *m = qobject_cast<QMenu *>(w))
                if (m->isVisible())
                    for (QAction *a : m->actions())
                        if (a->menu() != nullptr && a->text() == QStringLiteral("Set in this cue"))
                            for (QAction *p : a->menu()->actions())
                                if (p->text() == QStringLiteral("Magenta"))
                                {
                                    p->trigger();
                                    chose = true;
                                    m->close();
                                    return;
                                }
    });
    more->click();
    settle(100);
    QVERIFY(chose);
    // The chorus now has TrussC-RGB as its own layer, magenta; its level
    // still comes from the Pose (Full), so the cyc is magenta at full.
    const QStringList d = describe(doc);
    QCOMPARE(d.last(), QStringLiteral("1.0.2 Chorus: Steps [Colour Magenta, Dimmer Full]; "
                                      "TrussC-RGB [Colour Magenta]"));
    QVERIFY(view->selectNode(chorus));
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\xff\x00\xff", 3), kRigWait);
    doc->resetModified();
}

void CourseShow_Test::lessonOneTyped()
{
    QVERIFY(open(kL1Start));
    ShowTreeView *view = showTree();
    for (const QString &cmd : kLessonOneTyped)
        QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    QCOMPARE(describe(m_app->doc()), kLessonOneResult);
    // Why every layer gets a level too (the teacher's demo): the lesson's
    // groups are LED fixtures with no dimmer channel, so a colour alone
    // already shows at full -- but a light with a dimmer (the AT-W-ALL front washes,
    // Lesson 1.3) stays dark with a colour alone.
    for (const QString &g : { QStringLiteral("TrussC-RGB"), QStringLiteral("StepTops"),
                              QStringLiteral("Steps"), QStringLiteral("AT-W-ALL") })
    {
        QVERIFY(view->runCommand(QStringLiteral("cue + \"Probe %1\" block").arg(g)));
        QVERIFY(view->runCommand(QStringLiteral("\"%1\" colour Blue").arg(g)));
        QVERIFY(view->selectNode(view->currentCueId()));
        const QString name = g == "TrussC-RGB" ? "SR Truss Cyc 1" : g == "StepTops" ? "US Step 1 Top SR"
                             : g == "Steps" ? "US Step 1" : "SR AT Wash 1";
        Fixture *fx = nullptr;
        for (Fixture *f : m_app->doc()->fixtures())
            if (f->name() == name)
                fx = f;
        QVERIFY(fx != nullptr);
        // Wait for the probe's blue to reach the rig, then read it all.
        Doc *doc = m_app->doc();
        const bool blue = waitUntil([doc, fx, name]() {
            /* All of blue: its Blue at full AND its Red and Green out. Blue
               alone at 255 was true already mid-fade from the magenta
               before, and the read below then caught red still up. */
            const QByteArray now = sends(doc, name, int(fx->channels()));
            bool sawBlue = false, others = true;
            for (quint32 c = 0; c < fx->channels(); ++c)
            {
                const QString n = fx->channel(c)->name();
                if (n.contains(QLatin1String("Blue")))
                    sawBlue = sawBlue || uchar(now.at(int(c))) == 255;
                else if (n.contains(QLatin1String("Red")) || n.contains(QLatin1String("Green")))
                    others = others && uchar(now.at(int(c))) == 0;
            }
            if (sawBlue)
                return others;
            return now.left(3) == QByteArray("\x00\x00\xff", 3);
        });
        const QByteArray v = sends(doc, name, int(fx->channels()));
        QVERIFY2(blue, qPrintable(name + " " + v.toHex(' ')));
        const quint32 dim = fx->masterIntensityChannel();
        if (g == "AT-W-ALL")
        {
            // The definition's defaults (fixed 2026-10-19): facing forward,
            // a little below horizontal -- at the stage, not the floor.
            int pan = -1, tilt = -1;
            for (quint32 c = 0; c < fx->channels(); ++c)
            {
                if (fx->channel(c)->name() == QLatin1String("Pan"))
                    pan = uchar(v.at(int(c)));
                if (fx->channel(c)->name() == QLatin1String("Tilt"))
                    tilt = uchar(v.at(int(c)));
            }
            QCOMPARE(pan, 128);
            QCOMPARE(tilt, 32);
            const QString probe = qEnvironmentVariable("COURSE_PROBE");
            if (!probe.isEmpty())
            {
                // White and full, and a look at it on the Rig Overview.
                QVERIFY(view->runCommand("AT-W-ALL colour White dimmer Full"));
                QVERIFY(view->selectNode(view->currentCueId()));
                settle(400);
                Monitor *mon = Monitor::instance();
                mon->toggleStageOverview();
                settle(400);
                mon->m_overviewDlg->resize(1100, 700);
                settle(300);
                mon->m_overviewView->repaint();
                mon->m_overviewDlg->grab().save(probe);
                // And aimed at Center, as Lesson 1.3 will.
                showTree();
                QVERIFY(view->runCommand("AT-W-ALL aim Center"));
                QVERIFY(view->selectNode(view->currentCueId()));
                settle(600);
                mon->m_overviewView->repaint();
                QString aimed = probe;
                aimed.insert(aimed.lastIndexOf('.'), QStringLiteral("-aimed"));
                mon->m_overviewDlg->grab().save(aimed);
                mon->m_overviewDlg->close();
                settle();
                showTree();
            }
            // Its own Dimmer channel stays at 0: blue, but dark.
            Q_UNUSED(dim)
            bool dark = false;
            for (quint32 c = 0; c < fx->channels(); ++c)
                if (fx->channel(c)->name() == QLatin1String("Dimmer"))
                    dark = uchar(v.at(int(c))) == 0;
            QVERIFY(dark);
        }
        else
        {
            QCOMPARE(dim, QLCChannel::invalid());
            QCOMPARE(v.left(3), QByteArray("\x00\x00\xff", 3));
        }
    }
    // A typed level (75) reused the palette that plays it: nothing new.
    QVERIFY(paletteNamed(m_app->doc(), "Dimmer 75%") == nullptr);
    // One that isn't there is named as the show's levels are: "30%".
    QVERIFY(view->runCommand("Steps dimmer 30"));
    QVERIFY(paletteNamed(m_app->doc(), "30%") != nullptr);
    m_app->doc()->resetModified();
}

void CourseShow_Test::lessonOnePlaysInTheRunTab()
{
    QVERIFY(open(kL1Finish));
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    // The production made in class is the one GO plays.
    QCOMPARE(st->productions().count(), 1);
    QCOMPARE(st->currentProduction(), st->productions().first().id);

    CueRunner *run = m_app->findChild<CueRunner *>();
    QVERIFY(run != nullptr);
    m_app->m_tab->setCurrentWidget(run);
    settle(100);
    QVERIFY2(run->nextText().contains("1.0.0"), qPrintable(run->nextText()));

    // GO, three times: Pose, Verse, Chorus -- what each cue lights.
    struct Expect { QString cue, fixture; QByteArray rgb; };
    const QList<Expect> expect = {
        { "1.0.0", "SR Truss Cyc 1", QByteArray("\x00\x00\xff", 3) },
        { "1.0.1", "US Step 1 Top SR", QByteArray() },     // lit (warm white, 75 %)
        { "1.0.2", "US Step 1", QByteArray("\xff\x00\xff", 3) },
    };
    for (const Expect &e : expect)
    {
        run->findChild<QToolButton *>("crGo")->click();
        QTRY_VERIFY2_WITH_TIMEOUT(run->nowText().contains(e.cue), qPrintable(run->nowText()), kRigWait);
        const auto lit = [doc, &e]() {
            const QByteArray v = sends(doc, e.fixture);
            return e.rgb.isEmpty() ? v.count(char(0)) < v.size() : v == e.rgb;
        };
        QVERIFY2(waitUntil(lit), qPrintable(e.fixture + " " + sends(doc, e.fixture).toHex(' ')));
    }
    // Tracking: in the chorus the cyc is still blue (from 1.0.0).
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), kRigWait);
    run->findChild<QToolButton *>("crStop")->click();
    settle(100);
}

/* The Rig Overview of cue @a cueId (selected in the tree, as Design shows
   it), saved to @a path. Leaves the Rig Overview open. */
static bool rigShot(ShowTreeView *view, quint32 cueId, const QString &path, int plane = -1, int room = -1)
{
    Monitor *mon = Monitor::instance();
    if (mon == nullptr)
        return false;
    if (mon->m_overviewDlg == nullptr || !mon->m_overviewDlg->isVisible())
    {
        mon->toggleStageOverview();
        settle(300);
    }
    if (mon->m_overviewDlg == nullptr)
        return false;
    mon->m_overviewDlg->resize(1100, 700);
    QList<QComboBox *> combos = mon->m_overviewDlg->findChildren<QComboBox *>();
    // View (Top, Front, Back, Side, 45°) is the first; Room the second.
    // The default is 45° by its name: a view added to the list (Back) once
    // moved it, and the pictures came out from the side.
    if (combos.count() >= 2)
    {
        combos.at(0)->setCurrentIndex(plane >= 0 ? plane : combos.at(0)->findText(QStringLiteral("45°")));
        combos.at(1)->setCurrentIndex(room >= 0 ? room : 1);
    }
    if (cueId != ShowTree::invalidId())
        view->selectNode(cueId);
    settle(600);
    mon->m_overviewView->repaint();
    QDir().mkpath(QFileInfo(path).absolutePath());
    const bool ok = mon->m_overviewDlg->grab().save(path);
    qInfo().noquote() << "[COURSESHOT]" << path << (ok ? "" : "CAN'T WRITE");
    return ok;
}

static void closeRig()
{
    if (Monitor *mon = Monitor::instance())
        if (mon->m_overviewDlg != nullptr)
        {
            QList<QComboBox *> combos = mon->m_overviewDlg->findChildren<QComboBox *>();
            if (combos.count() >= 2)
            {
                combos.at(0)->setCurrentIndex(combos.at(0)->findText(QStringLiteral("45°")));
                combos.at(1)->setCurrentIndex(1);
            }
            mon->m_overviewDlg->close();
        }
    settle();
}

#define LESSON_STEP(cond, what) \
    do { if (!(cond)) { qWarning().noquote() << "lesson step failed:" << (what) << "line" << __LINE__ \
                                             << describe(doc).join(" || "); return false; } } while (0)

/* Lesson 1.2 with the mouse, as its handout says: the verse made cooler and
   dimmer in place (⋯ → Set to, ⋯ → Level…), the picked level kept (Make
   reusable), and a new cue whose colour and level are cue looks. */
static bool lessonTwoByMouse(Doc *doc, ShowTreeView *view)
{
    ShowTreeController *st = doc->showTree();
    QLabel *hint = hintOf(view);

    // Steps 1-2: the verse, its StepTops row: the layer's page.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.1")), "select 1.0.1");
    settle(50);
    LESSON_STEP(clickCueRow(view, "StepTops"), "click StepTops");
    LESSON_STEP(rowText(view, KeyColour).startsWith("Warm White"), rowText(view, KeyColour));

    // Step 3: Colour ⋯ → Set to → Lavender. The chorus changes too (it tracks).
    LESSON_STEP(rowMenu(view, KeyColour, "Set to", "Lavender"), "Set to Lavender");
    qInfo().noquote() << "[L2] after Set to:" << hint->text() << "|" << rowText(view, KeyColour);
    LESSON_STEP(rowText(view, KeyColour).startsWith("Lavender"), rowText(view, KeyColour));

    // Step 4: Dimmer ⋯ → Level… 40.
    LESSON_STEP(rowMenu(view, KeyDimmer, "Level…"), "Level…");
    // It opens docked in the window, never behind it (Q61).
    LESSON_STEP(view->dockedPickerOpen(), "the Level picker, docked");
    LESSON_STEP(pickDockedLevel(view, 40), "Level 40, Apply");
    settle(50);
    qInfo().noquote() << "[L2] after Level:" << hint->text() << "|" << rowText(view, KeyDimmer);
    LESSON_STEP(rowText(view, KeyDimmer).startsWith("40%"), rowText(view, KeyDimmer));

    // Step 5: Make reusable: 40% joins the palettes.
    QLabel *reuse = stFind<QLabel>(view, QStringLiteral("stReuse:%1").arg(KeyDimmer));
    LESSON_STEP(reuse != nullptr && reuse->isVisible(), "Make reusable link");
    emit reuse->linkActivated(QStringLiteral("reuse"));
    settle(50);
    qInfo().noquote() << "[L2] after Make reusable:" << hint->text() << "|" << rowText(view, KeyDimmer);
    LESSON_STEP(paletteNamed(doc, "40%") != nullptr && !st->isInlinePalette(paletteNamed(doc, "40%")->id()),
                "40% is a palette");
    {
        // In the Library with the levels: Palettes → Dimmer → 40%.
        QTreeWidgetItem *row = libraryRow(view->library(), "40%");
        QStringList path;
        for (QTreeWidgetItem *p = row; p != nullptr; p = p->parent())
            path.prepend(p->text(0));
        qInfo().noquote() << "[L2] 40% in the Library:" << path.join(" → ") << "| path" << paletteNamed(doc, "40%")->path();
        LESSON_STEP(row != nullptr, "no 40% in the Library");
    }
    LESSON_STEP(rowText(view, KeyDimmer) == QStringLiteral("40%"), rowText(view, KeyDimmer));

    // Undo three times: the warm verse is back; Redo three times: the cool one.
    for (int i = 0; i < 3; ++i)
    {
        stFind<QToolButton>(view, "stUndo")->click();
        settle(50);
    }
    {
        const QStringList d = describe(doc);
        LESSON_STEP(d.value(2) == QStringLiteral("1.0.1 Verse: StepTops [Colour Warm White, Dimmer 75%]"), d.value(2));
    }
    for (int i = 0; i < 3; ++i)
    {
        stFind<QToolButton>(view, "stRedo")->click();
        settle(50);
    }
    {
        const QStringList d = describe(doc);
        LESSON_STEP(d.value(2) == QStringLiteral("1.0.1 Verse: StepTops [Colour Lavender, Dimmer 40%]"), d.value(2));
        LESSON_STEP(!st->isInlinePalette(paletteNamed(doc, "40%")->id()), "40% a one-off again after Redo");
    }

    // Steps 6-8: the chorus, then + Cue "Scream": TrussW-RGB and Lanterns,
    // then Red and Full dropped on the dashed area: cue looks.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.2")), "select 1.0.2");
    settle(50);
    LESSON_STEP(newCueNamed(view, "Scream"), "new cue Scream");
    LESSON_STEP(st->cue(view->currentCueId())->number == "1.0.3", st->cue(view->currentCueId())->number);
    for (const QString &g : { QStringLiteral("TrussW-RGB"), QStringLiteral("Lanterns") })
    {
        const QString said = dropGroupOnZone(doc, view, g);
        qInfo().noquote() << "[L2] drop" << g << ":" << said;
        LESSON_STEP(said == QStringLiteral("a new layer on %1 in 1.0.3 Scream").arg(g), said);
    }
    for (const QString &pal : { QStringLiteral("Red"), QStringLiteral("Full") })
    {
        const QString said = dropPaletteOnZone(doc, view, pal);
        qInfo().noquote() << "[L2] drop" << pal << ":" << said << "| after:" << hint->text();
        LESSON_STEP(said == QStringLiteral("%1 → cue look on 1.0.3 Scream: reaches 2 of 2 layers").arg(pal), said);
    }
    qInfo().noquote() << "[L2] rows:" << view->cueLayerItems().join(" | ");
    QLabel *looks = stFind<QLabel>(view, "stCueLooks");
    qInfo().noquote() << "[L2] cue looks:" << (looks ? looks->text() : QString());
    return true;
}

/* Lesson 1.3 with the mouse: front light on the verse (AT-W-ALL Warm White
   Full), aimed at Center; a solo cue with the AT-Spots aimed at Soloist DS. */
static bool lessonThreeByMouse(Doc *doc, ShowTreeView *view, const QString &shots = QString())
{
    ShowTreeController *st = doc->showTree();
    QLabel *hint = hintOf(view);
    // With COURSE_SHOTS: the Rig Overview at each "you should see".
    auto shot = [view, &shots](const QString &name) {
        if (shots.isEmpty())
            return;
        const quint32 cue = view->currentCueId();
        rigShot(view, cue, shots + QLatin1Char('/') + name + QStringLiteral(".png"));
        closeRig();
        view->selectNode(cue);
        settle();
    };

    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.1")), "select 1.0.1");
    settle(50);
    {
        const QString said = dropGroupOnZone(doc, view, "AT-W-ALL");
        LESSON_STEP(said == QStringLiteral("a new layer on AT-W-ALL in 1.0.1 Verse"), said);
    }
    for (const QString &pal : { QStringLiteral("Warm White"), QStringLiteral("Full") })
    {
        const QString said = dropPaletteOnRow(doc, view, "AT-W-ALL", pal);
        LESSON_STEP(said.startsWith(QStringLiteral("%1 → layer AT-W-ALL in 1.0.1 Verse").arg(pal)), said);
    }
    settle(300);
    qInfo() << "[L3] bare AT wash pan/tilt" << panTilt(doc, "SR AT Wash 1") << panTilt(doc, "SL AT Wash 1")
            << "dimmer" << channelValue(doc, "SR AT Wash 1", "Dimmer");
    LESSON_STEP(waitUntil([doc]() { return panTilt(doc, "SR AT Wash 1") == qMakePair(128, 32); }), "bare wash pan/tilt");
    shot(QStringLiteral("rig-verse-front"));
    {
        const QString said = dropTargetOnRow(doc, view, "AT-W-ALL", "Center");
        qInfo().noquote() << "[L3] drop Center:" << said << "| after:" << hint->text();
        LESSON_STEP(said.startsWith(QStringLiteral("Aim at Center → layer AT-W-ALL in 1.0.1 Verse")), said);
    }
    settle(300);
    qInfo() << "[L3] aimed AT wash pan/tilt" << panTilt(doc, "SR AT Wash 1") << panTilt(doc, "SL AT Wash 1");
    LESSON_STEP(waitUntil([doc]() { return panTilt(doc, "SR AT Wash 1") != qMakePair(128, 32); }), "the wash didn't move");
    shot(QStringLiteral("rig-verse-center"));

    // The solo: after the scream.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.3")), "select 1.0.3");
    settle(50);
    LESSON_STEP(newCueNamed(view, "Solo"), "new cue Solo");
    LESSON_STEP(st->cue(view->currentCueId())->number == "1.0.4", st->cue(view->currentCueId())->number);
    // Mode: Block -- the solo starts from a dark stage.
    {
        QComboBox *mode = stFind<QComboBox>(view, "stCueMode");
        LESSON_STEP(mode != nullptr, "Mode box");
        const int block = mode->findText(QStringLiteral("Block"), Qt::MatchStartsWith);
        LESSON_STEP(block >= 0, "no Block in Mode");
        mode->setCurrentIndex(block);
        emit mode->activated(block);
        settle(50);
        qInfo().noquote() << "[L3] mode items" << mode->itemText(0) << mode->itemText(1) << mode->itemText(2)
                          << "| after Block:" << hint->text() << "| rows:" << view->cueLayerItems().join(" | ");
        LESSON_STEP(st->cue(view->currentCueId())->mode == CueMode::Block, "not Block");
        // The tree says so (⊘ block) and the stage is dark until the spots come.
        bool badged = false;
        for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
            for (int col = 0; col < (*it)->columnCount(); ++col)
                if ((*it)->text(0).contains(QStringLiteral("Solo"))
                    && (*it)->text(col) == QStringLiteral("B"))
                    badged = true;
        LESSON_STEP(badged, "no ⊘ block badge on the Solo");
        const auto dark = [doc]() {
            for (const char *name : { "SR Truss Wash 1", "SR Truss Cyc 1" })
                if (sends(doc, name).count(char(0)) != sends(doc, name).size())
                    return false;
            return true;
        };
        LESSON_STEP(waitUntil(dark), "the Block cue's stage is not dark: wash "
                    + QString::fromLatin1(sends(doc, "SR Truss Wash 1").toHex(' ')));
        LESSON_STEP(view->cueLayerItems().isEmpty(), view->cueLayerItems().join(" | "));
    }
    {
        const QString said = dropGroupOnZone(doc, view, "AT-Spots");
        LESSON_STEP(said == QStringLiteral("a new layer on AT-Spots in 1.0.4 Solo"), said);
    }
    for (const QString &pal : { QStringLiteral("White"), QStringLiteral("Full") })
    {
        const QString said = dropPaletteOnRow(doc, view, "AT-Spots", pal);
        LESSON_STEP(said.startsWith(QStringLiteral("%1 → layer AT-Spots in 1.0.4 Solo").arg(pal)), said);
    }
    settle(300);
    // White at Full lights the floor: the spots' shutter rests closed, and
    // auto-open (the show's setting, on by default) opens it for a level.
    LESSON_STEP(waitUntil([doc]() { return channelValue(doc, "SR AT Spot", "Master Dimmer") == 255; }), "spot dimmer");
    qInfo() << "[L3] AT spot pan/tilt" << panTilt(doc, "SR AT Spot") << panTilt(doc, "SL AT Spot")
            << "shutter" << outputValue(doc, "SR AT Spot", "Strobe/Shutter");
    LESSON_STEP(doc->shutterAutoOpen()->isEnabled(), "auto-open is off in the lesson show");
    LESSON_STEP(waitUntil([doc]() { return outputValue(doc, "SR AT Spot", "Strobe/Shutter") >= 8; }), "spot shutter still closed");
    // It's auto-open that lights them: off, the shutter stays closed (the
    // handout's Stuck? row); on again, open.
    doc->shutterAutoOpen()->setEnabled(false);
    LESSON_STEP(waitUntil([doc]() { return outputValue(doc, "SR AT Spot", "Strobe/Shutter") < 8; }), "auto-open off, shutter still open");
    doc->shutterAutoOpen()->setEnabled(true);
    LESSON_STEP(waitUntil([doc]() { return outputValue(doc, "SR AT Spot", "Strobe/Shutter") >= 8; }), "auto-open on again, shutter closed");
    LESSON_STEP(waitUntil([doc]() { return panTilt(doc, "SR AT Spot") == qMakePair(0, 0); }),
                "the spot has a position already");
    shot(QStringLiteral("rig-solo-floor"));
    {
        const QString said = dropTargetOnRow(doc, view, "AT-Spots", "Soloist DS");
        qInfo().noquote() << "[L3] drop Soloist DS:" << said;
        LESSON_STEP(said.startsWith(QStringLiteral("Aim at Soloist DS → layer AT-Spots in 1.0.4 Solo")), said);
    }
    settle(300);
    qInfo() << "[L3] aimed AT spot pan/tilt" << panTilt(doc, "SR AT Spot") << panTilt(doc, "SL AT Spot");
    LESSON_STEP(waitUntil([doc]() { return panTilt(doc, "SR AT Spot") != qMakePair(0, 0); }), "the spot didn't move");
    shot(QStringLiteral("rig-solo"));
    qInfo().noquote() << "[L3] solo rows:" << view->cueLayerItems().join(" | ");
    return true;
}

/* Types @a seconds into a time box on the cue page, as a student does:
   select what's there, type, Return. */
static void typeSeconds(QDoubleSpinBox *box, const QString &seconds)
{
    box->setFocus();
    box->selectAll();
    QTest::keyClicks(box, seconds);
    QTest::keyClick(box, Qt::Key_Return);
    settle();
}

/* Lesson 1.4 with the mouse: fades and a hold, typed on each cue's page. */
static bool lessonFourByMouse(Doc *doc, ShowTreeView *view)
{
    ShowTreeController *st = doc->showTree();
    struct Timing { QString cue; QString fadeIn, fadeOut, hold; };
    const QList<Timing> timings = {
        { "1.0.0", "3", QString(), QString() },     // Pose: the cyc breathes in
        { "1.0.1", "2", QString(), QString() },     // Verse
        // Chorus 1.0.2 stays a snap (0).
        { "1.0.3", QString(), "3", "2" },           // Scream: snap in, holds 2 s, melts away
        { "1.0.4", "3", QString(), QString() },     // Solo: fades up as the scream goes
    };
    for (const Timing &t : timings)
    {
        LESSON_STEP(view->selectNode(cueNumbered(doc, t.cue)), "select " + t.cue);
        settle(50);
        if (!t.fadeIn.isEmpty())
            typeSeconds(view->m_cueFadeIn, t.fadeIn);
        if (!t.fadeOut.isEmpty())
            typeSeconds(view->m_cueFadeOut, t.fadeOut);
        if (!t.hold.isEmpty())
            typeSeconds(view->m_cueHold, t.hold);
        const Cue *c = st->cue(cueNumbered(doc, t.cue));
        qInfo().noquote() << "[L4]" << t.cue << c->fadeIn << c->fadeOut << c->hold << "|" << hintOf(view)->text();
        LESSON_STEP(t.fadeIn.isEmpty() || c->fadeIn == uint(t.fadeIn.toInt() * 1000), "fade in " + t.cue);
        LESSON_STEP(t.fadeOut.isEmpty() || c->fadeOut == uint(t.fadeOut.toInt() * 1000), "fade out " + t.cue);
        LESSON_STEP(t.hold.isEmpty() || c->hold == uint(t.hold.toInt() * 1000), "hold " + t.cue);
    }
    // The tree says each cue's timing.
    QStringList rows;
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        if ((*it)->text(0).startsWith(QLatin1String("1.0.")))
            rows << (*it)->text(0) + " / " + (*it)->text(1);
    qInfo().noquote() << "[L4] tree:" << rows.join(" | ");
    return true;
}

void CourseShow_Test::buildLessonShows()
{
    if (qEnvironmentVariableIsEmpty("COURSE_BUILD"))
        QSKIP("set COURSE_BUILD=1 to make Lessons 1.2-1.5's show files");
    QVERIFY(QFile::exists(kL1Finish));
    QVERIFY(QFile::remove(kL2Start) || !QFile::exists(kL2Start));
    QVERIFY(QFile::copy(kL1Finish, kL2Start));

    QVERIFY(open(kL2Start));
    QVERIFY(lessonTwoByMouse(m_app->doc(), showTree()));
    QCOMPARE(m_app->saveXML(kL2Finish), QFile::NoError);
    m_app->doc()->resetModified();

    QVERIFY(QFile::remove(kL3Start) || !QFile::exists(kL3Start));
    QVERIFY(QFile::copy(kL2Finish, kL3Start));
    QVERIFY(open(kL3Start));
    QVERIFY(lessonThreeByMouse(m_app->doc(), showTree()));
    QCOMPARE(m_app->saveXML(kL3Finish), QFile::NoError);
    m_app->doc()->resetModified();

    QVERIFY(QFile::remove(kL4Start) || !QFile::exists(kL4Start));
    QVERIFY(QFile::copy(kL3Finish, kL4Start));
    QVERIFY(open(kL4Start));
    QVERIFY(lessonFourByMouse(m_app->doc(), showTree()));
    QCOMPARE(m_app->saveXML(kL4Finish), QFile::NoError);
    m_app->doc()->resetModified();

    // Lesson 1.5 runs the show: it starts from 1.4's finish and saves nothing.
    QVERIFY(QFile::remove(kL5Start) || !QFile::exists(kL5Start));
    QVERIFY(QFile::copy(kL4Finish, kL5Start));
}

/* The Run tab, brought forward as a student clicks its tab. */
static CueRunner *runTab(App *app)
{
    CueRunner *run = app->findChild<CueRunner *>(QStringLiteral("cueRunner"));
    if (run == nullptr)
        run = app->findChild<CueRunner *>();
    if (run != nullptr)
        app->m_tab->setCurrentWidget(run);
    settle(100);
    return run;
}

/* The Run tab's row for cue @a number. */
static QTreeWidgetItem *runRow(CueRunner *run, const QString &number)
{
    for (QTreeWidgetItemIterator it(run->cueTree()); *it; ++it)
        if ((*it)->text(1) == number)
            return *it;
    return nullptr;
}

static void press(CueRunner *run, const char *button)
{
    run->findChild<QToolButton *>(QLatin1String(button))->click();
}

void CourseShow_Test::lessonFourPlaysTheFades()
{
    QVERIFY(open(kL4Finish));
    Doc *doc = m_app->doc();
    {
        // In the Show Tree, a click on a cue shows it at once: fades are
        // for GO (the handout says so).
        ShowTreeView *view = showTree();
        QVERIFY(view->selectNode(cueNumbered(doc, "1.0.0")));
        QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), kRigWait);
        QVERIFY(view->selectNode(doc->showTree()->productions().first().id));
        settle(300);
    }
    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    QStringList timing;
    for (QTreeWidgetItemIterator it(run->cueTree()); *it; ++it)
        timing << (*it)->text(1) + " " + (*it)->text(3);
    qInfo().noquote() << "[L4] Run tab timing:" << timing.join(" | ");

    // GO: the Pose fades in over 3 s -- seen part way (not a snap), then
    // all there. Polled: on a busy machine the fade's frames come late.
    press(run, "crGo");
    const auto partWay = [doc]() {
        const uchar b = uchar(sends(doc, "SR Truss Cyc 1").at(2));
        return b > 20 && b < 235;
    };
    QVERIFY2(waitUntil(partWay), qPrintable(sends(doc, "SR Truss Cyc 1").toHex(' ')));
    qInfo() << "[L4] cyc blue part way" << uchar(sends(doc, "SR Truss Cyc 1").at(2));
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), kRigWait);

    // GO, GO: Verse (2 s), Chorus (a snap).
    press(run, "crGo");
    QTest::qWait(2600);
    press(run, "crGo");
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "US Step 1"), QByteArray("\xff\x00\xff", 3), kRigWait);

    // GO: the Scream snaps in, holds 2 s, then the Solo comes by itself.
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.3"), kRigWait);
    qInfo().noquote() << "[L4] scream now:" << run->nowText() << "| next:" << run->nextText();
    QTRY_VERIFY_WITH_TIMEOUT(uchar(sends(doc, "SR Truss Wash 1").at(0)) > 200, kRigWait);
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.4"), kRigWait);
    qInfo().noquote() << "[L4] after the hold:" << run->nowText() << "| wash" << sends(doc, "SR Truss Wash 1").toHex(' ')
                      << "| spot dimmer" << channelValue(doc, "SR AT Spot", "Master Dimmer");
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Wash 1", 3), QByteArray("\x00\x00\x00", 3), 6000);
    QTRY_COMPARE_WITH_TIMEOUT(channelValue(doc, "SR AT Spot", "Master Dimmer"), 255, 6000);
    press(run, "crStop");
    settle(200);
    doc->resetModified();
}

void CourseShow_Test::lessonFiveRunsTheShow()
{
    QVERIFY(open(kL5Start));
    Doc *doc = m_app->doc();
    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    // Show time: Operate first, as the handout says.
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Operate);
    QTRY_COMPARE_WITH_TIMEOUT(run->modeText(), QStringLiteral("LIVE"), kRigWait);
    qInfo().noquote() << "[L5] mode:" << run->modeText() << "| header:" << run->headerText()
                      << "| next:" << run->nextText();
    QVERIFY2(run->nextText().contains("1.0.0"), qPrintable(run->nextText()));

    // "GO" twice: Pose, Verse.
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.0"), kRigWait);
    QTest::qWait(3300);
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.1"), kRigWait);
    QTest::qWait(2300);
    // Back: the Pose again.
    press(run, "crBack");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.0"), kRigWait);
    QTest::qWait(500);
    qInfo().noquote() << "[L5] after Back:" << run->nowText() << "| next:" << run->nextText();
    QVERIFY2(run->nowText().contains("1.0.0"), qPrintable(run->nowText()));

    // "Standby cue 1.0.2": click its row -- LOAD.
    QTreeWidgetItem *chorus = runRow(run, "1.0.2");
    QVERIFY(chorus != nullptr);
    QTest::mouseClick(run->cueTree()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      run->cueTree()->visualItemRect(chorus).center());
    settle(100);
    qInfo().noquote() << "[L5] after LOAD:" << run->nextText() << "| state:" << chorus->text(0);
    QVERIFY2(run->nextText().contains("1.0.2"), qPrintable(run->nextText()));
    QVERIFY2(chorus->text(0).contains("STANDBY"), qPrintable(chorus->text(0)));
    // "GO": the chorus, though the verse was next.
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.2"), kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "US Step 1"), QByteArray("\xff\x00\xff", 3), kRigWait);

    // Double-click the Solo: jump there now.
    QTreeWidgetItem *solo = runRow(run, "1.0.4");
    QVERIFY(solo != nullptr);
    // A double-click, as the mouse sends it: a click, then the second press.
    const QPoint at = run->cueTree()->visualItemRect(solo).center();
    QTest::mouseClick(run->cueTree()->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QTest::mouseDClick(run->cueTree()->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    settle(50);
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.4"), kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(channelValue(doc, "SR AT Spot", "Master Dimmer"), 255, 6000);
    qInfo().noquote() << "[L5] after the jump:" << run->nowText() << "| next:" << run->nextText();

    // Stop: the stage goes dark -- within kRigWait, and it takes two ticks
    // (~45 ms), also at a load of 80. It once stayed at full under a loaded
    // gate: the universes then got their frames through this (GUI) thread,
    // so the engine stopped the cue but the rig held its last frame for as
    // long as this thread was away (MasterTimer_Test::
    // aStopReachesTheUniversesWithTheGuiThreadBusy). If it ever comes back,
    // say what still plays, what Stop said and whether the engine ticked.
    QStringList said;
    const QMetaObject::Connection sc = connect(doc->showControl(), &ShowControl::commandHandled, this,
                                               [&said](const QString &w, bool done) {
                                                   said << QStringLiteral("%1 (%2)").arg(w, done ? QStringLiteral("done") : QStringLiteral("refused"));
                                               });
    static QAtomicInt ticks;
    ticks.storeRelaxed(0);
    const QMetaObject::Connection tc = connect(doc->masterTimer(), &MasterTimer::tickReady, doc->masterTimer(),
                                               []() { ticks.ref(); }, Qt::DirectConnection);
    press(run, "crStop");
    QElapsedTimer sinceStop;
    sinceStop.start();
    while (sinceStop.elapsed() < kRigWait && channelValue(doc, "SR AT Spot", "Master Dimmer") != 0)
        QTest::qWait(20);
    disconnect(sc);
    disconnect(tc);
    QStringList running;
    for (Function *f : doc->functions())
        if (f != nullptr && f->isRunning())
            running << f->name();
    QVERIFY2(channelValue(doc, "SR AT Spot", "Master Dimmer") == 0,
             qPrintable(QStringLiteral("spot dimmer %1, %2 ms and %3 ticks after Stop; Stop said: %4; still running: %5")
                            .arg(channelValue(doc, "SR AT Spot", "Master Dimmer"))
                            .arg(sinceStop.elapsed()).arg(ticks.loadRelaxed())
                            .arg(said.isEmpty() ? QStringLiteral("nothing") : said.join(QStringLiteral("; ")))
                            .arg(running.isEmpty() ? QStringLiteral("nothing") : running.join(QStringLiteral(", ")))));
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "US Step 1"), QByteArray("\x00\x00\x00", 3), kRigWait);
    QTRY_VERIFY_WITH_TIMEOUT(run->nextText().contains("1.0.0"), kRigWait);
    qInfo().noquote() << "[L5] after Stop:" << run->nowText() << "| next:" << run->nextText();
    // Back to Design: nothing runs, so it doesn't ask.
    settle(300);
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Design);
    doc->resetModified();
}

/* Each cue's layers and cue looks, with timing: what a lesson makes. */
static QStringList describeAll(Doc *doc)
{
    QStringList out = describe(doc) + cueLooks(doc);
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &l : p.lists)
        {
            if (l.baseCue != ShowTree::invalidId())
                out << QStringLiteral("%1 base %2").arg(l.name, doc->showTree()->cue(l.baseCue)
                                                                 ? doc->showTree()->cue(l.baseCue)->number : "?");
            for (const Cue &c : l.cues)
            {
                out << QStringLiteral("%1 mode %2 in %3 out %4 hold %5").arg(c.number).arg(int(c.mode))
                       .arg(c.fadeIn).arg(c.fadeOut).arg(c.hold);
                // Level 2: what a layer lets go of (Release).
                for (const Layer &ly : c.layers)
                    if (ly.release)
                        out << QStringLiteral("%1 releases %2").arg(c.number, ly.name.isEmpty()
                                                                    ? doc->showTree()->targetLabel(ly.target) : ly.name);
            }
        }
    return out;
}

void CourseShow_Test::aMouseBuiltShowSavesOpensAndPlays()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString saved = dir.filePath(QStringLiteral("my-show.qlcc"));

    QVERIFY(open(kL1Start));
    QVERIFY(lessonOneByMouse(m_app->doc(), showTree()));
    QVERIFY(lessonTwoByMouse(m_app->doc(), showTree()));
    QVERIFY(lessonThreeByMouse(m_app->doc(), showTree()));
    QVERIFY(lessonFourByMouse(m_app->doc(), showTree()));
    const QStringList built = describeAll(m_app->doc());
    QVERIFY(!built.isEmpty());

    QCOMPARE(m_app->saveXML(saved), QFile::NoError);
    QVERIFY(QFileInfo(saved).size() > 0);
    QVERIFY(open(saved));
    QCOMPARE(describeAll(m_app->doc()), built);
    // And it is the course's own Lesson 1.4 finish.
    QVERIFY(open(kL4Finish));
    QCOMPARE(describeAll(m_app->doc()), built);

    // The reopened show plays: GO, and the Pose lights the cyc blue.
    QVERIFY(open(saved));
    Doc *doc = m_app->doc();
    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    QVERIFY2(run->nextText().contains("1.0.0"), qPrintable(run->nextText()));
    press(run, "crGo");
    QTRY_VERIFY2_WITH_TIMEOUT(run->nowText().contains("1.0.0"), qPrintable(run->nowText()), kRigWait);
    QVERIFY2(waitUntil([doc]() { return sends(doc, "SR Truss Cyc 1") == QByteArray("\x00\x00\xff", 3); }),
             qPrintable(sends(doc, "SR Truss Cyc 1").toHex(' ')));
    press(run, "crStop");
    settle(200);
    doc->resetModified();
}

void CourseShow_Test::lessonsOneTwoToOneFourWithTheMouse()
{
    struct L { QString start, finish; bool (*byMouse)(Doc *, ShowTreeView *); };
    const QList<L> lessons = {
        { kL2Start, kL2Finish, [](Doc *d, ShowTreeView *v) { return lessonTwoByMouse(d, v); } },
        { kL3Start, kL3Finish, [](Doc *d, ShowTreeView *v) { return lessonThreeByMouse(d, v); } },
        { kL4Start, kL4Finish, [](Doc *d, ShowTreeView *v) { return lessonFourByMouse(d, v); } },
    };
    QStringList before;
    for (const L &l : lessons)
    {
        QVERIFY2(open(l.start), qPrintable(l.start));
        // Each lesson starts where the one before finished.
        if (!before.isEmpty())
            QCOMPARE(describeAll(m_app->doc()), before);
        QVERIFY2(l.byMouse(m_app->doc(), showTree()), qPrintable(l.start));
        const QStringList built = describeAll(m_app->doc());
        m_app->doc()->resetModified();
        QVERIFY(open(l.finish));
        QCOMPARE(describeAll(m_app->doc()), built);
        before = built;
    }
    QVERIFY(open(kL5Start));
    QCOMPARE(describeAll(m_app->doc()), before);
    // Lesson 1.2's start is Lesson 1.1's finish.
    QVERIFY(open(kL1Finish));
    const QStringList l1 = describeAll(m_app->doc());
    QVERIFY(open(kL2Start));
    QCOMPARE(describeAll(m_app->doc()), l1);
}

void CourseShow_Test::lessonZeroEightTheVirtualStage()
{
    // Lesson 0.8: the finished Lesson 1.1, the Rig Overview, its views and
    // room light, and GO three times.
    QVERIFY(open(kL1Finish));
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    mon->toggleStageOverview();
    settle(300);
    QVERIFY(mon->m_overviewDlg != nullptr && mon->m_overviewDlg->isVisible());
    QCOMPARE(mon->m_overviewDlg->windowTitle(), QStringLiteral("Rig Overview"));
    QList<QComboBox *> combos = mon->m_overviewDlg->findChildren<QComboBox *>();
    QVERIFY(combos.count() >= 2);
    QStringList views, rooms;
    for (int i = 0; i < combos.at(0)->count(); ++i)
        views << combos.at(0)->itemText(i);
    for (int i = 0; i < combos.at(1)->count(); ++i)
        rooms << combos.at(1)->itemText(i);
    QCOMPARE(views, QStringList({ "Top", "Front", "Back", "Side", "45°" }));
    QCOMPARE(rooms, QStringList({ "Work light", "House", "Dim", "Blackout" }));
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("45°"));
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("House"));
    QStringList buttons;
    for (QToolButton *b : mon->m_overviewDlg->findChildren<QToolButton *>())
        if (b->isCheckable())
            buttons << QStringLiteral("%1=%2").arg(b->text()).arg(b->isChecked());
    qInfo().noquote() << "[L0] Rig Overview toggles:" << buttons.join(", ");
    QVERIFY(buttons.contains("Live=1"));
    closeRig();

    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    for (const QString cue : { "1.0.0", "1.0.1", "1.0.2" })
    {
        press(run, "crGo");
        QTRY_VERIFY2_WITH_TIMEOUT(run->nowText().contains(cue), qPrintable(run->nowText()), kRigWait);
    }
    press(run, "crStop");
    settle(200);
    m_app->doc()->resetModified();
}

void CourseShow_Test::lessonTwoColourPicker()
{
    // Lesson 1.2's extension: ⋯ → Colour… on the verse's StepTops, a colour
    // picked in the colour window and OK.
    QVERIFY(open(kL2Finish));
    Doc *doc = m_app->doc();
    ShowTreeView *view = showTree();
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.1")));
    settle(50);
    QVERIFY(clickCueRow(view, "StepTops"));
    QVERIFY(rowMenu(view, KeyColour, "Colour…"));
    QVERIFY(view->dockedPickerOpen());          // in the window (Q61)
    QVERIFY(pickDockedColour(view, QColor(0x6a, 0x0d, 0xad)));
    settle(100);
    qInfo().noquote() << "[L2] picked colour row:" << rowText(view, KeyColour);
    QVERIFY2(rowText(view, KeyColour).startsWith("#6a0dad"), qPrintable(rowText(view, KeyColour)));
    QVERIFY(stFind<QLabel>(view, QStringLiteral("stReuse:%1").arg(KeyColour)) != nullptr);

    // The puzzle: a cue look on the Pose reaches none of its layers (its
    // one layer sets its own colour).
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.0")));
    settle(50);
    const QString said = dropPaletteOnZone(doc, view, "Green");
    qInfo().noquote() << "[L2] Green on the Pose:" << said;
    QCOMPARE(said, QStringLiteral("Green → cue look on 1.0.0 Pose: reaches 0 of 1 layers"));
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), kRigWait);
    doc->resetModified();
}

void CourseShow_Test::lessonThreeExtension()
{
    // Lesson 1.3's extension: the spots aimed elsewhere with ⋯ → Set to on
    // the Position row; a back light behind the soloist.
    QVERIFY(open(kL3Finish));
    Doc *doc = m_app->doc();
    ShowTreeView *view = showTree();
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.4")));
    settle(50);
    QVERIFY(clickCueRow(view, "AT-Spots"));
    const QPair<int, int> soloist = panTilt(doc, "SR AT Spot");
    QVERIFY(rowMenu(view, KeyPosition, "Set to", "Aim Center"));
    settle(300);
    QVERIFY2(rowText(view, KeyPosition).startsWith("Aim Center"), qPrintable(rowText(view, KeyPosition)));
    QTRY_VERIFY_WITH_TIMEOUT(panTilt(doc, "SR AT Spot") != soloist, kRigWait);
    stFind<QToolButton>(view, "stUndo")->click();
    settle(300);
    QTRY_COMPARE_WITH_TIMEOUT(panTilt(doc, "SR AT Spot"), soloist, kRigWait);

    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.4")));
    settle(50);
    QCOMPARE(dropGroupOnZone(doc, view, "TrussW-RGB"), QStringLiteral("a new layer on TrussW-RGB in 1.0.4 Solo"));
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("25%") })
        QVERIFY(dropPaletteOnRow(doc, view, "TrussW-RGB", pal).startsWith(pal + " → layer TrussW-RGB in 1.0.4 Solo"));
    settle(300);
    QVERIFY2(cueRow(view, "TrussW-RGB") != nullptr && cueRow(view, "TrussW-RGB")->text().contains("Blue, 25%"),
             qPrintable(view->cueLayerItems().join(" | ")));
    const auto blue25 = [doc]() {
        const QByteArray wash = sends(doc, "SR Truss Wash 1");
        return uchar(wash.at(0)) == 0 && uchar(wash.at(2)) > 40 && uchar(wash.at(2)) < 90;
    };
    QVERIFY2(waitUntil(blue25), qPrintable(sends(doc, "SR Truss Wash 1").toHex(' ')));
    doc->resetModified();
}

void CourseShow_Test::lessonFiveOperate()
{
    // Lesson 1.5's show time: the Operate button (top right) makes it LIVE;
    // then a click on a cue in the Show Tree doesn't touch the stage; Stop,
    // and Design again.
    QVERIFY(open(kL5Start));
    Doc *doc = m_app->doc();
    QCOMPARE(m_app->m_modeToggleAction->text().remove(QLatin1Char('&')), QStringLiteral("Operate"));
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Operate);
    QCOMPARE(m_app->m_modeToggleAction->text(), QStringLiteral("Design"));
    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(run->modeText(), QStringLiteral("LIVE"), kRigWait);
    QVERIFY(!m_app->m_fileOpenAction->isEnabled());

    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.0"), kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), 5000);
    // In LIVE, the Show Tree shows nothing new on stage: the Scream clicked
    // there doesn't turn the stage red.
    ShowTreeView *view = showTree();
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.3")));
    QTest::qWait(600);
    QCOMPARE(sends(doc, "SR Truss Wash 1"), QByteArray("\x00\x00\x00", 3));
    QCOMPARE(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3));
    qInfo().noquote() << "[L5] Show Tree in LIVE says:" << hintOf(view)->text()
                      << "|" << stFind<QLabel>(view, "stPreview")->text();

    run = runTab(m_app);
    press(run, "crStop");
    /* Dark within the same kRigWait (it takes ~20-45 ms, also at a load of
       100). Once, under a gate at a load of ~187, it stayed 00 00 FF; that
       never came back in 56 runs. If it does, say what still plays: a stop
       that was lost (1.0.0 or its list still running) or a starved engine
       (nothing running, the output not yet written). */
    QElapsedTimer sinceStop;
    sinceStop.start();
    while (sinceStop.elapsed() < kRigWait && sends(doc, "SR Truss Cyc 1") != QByteArray("\x00\x00\x00", 3))
        QTest::qWait(20);
    QStringList running;
    for (Function *f : doc->functions())
        if (f != nullptr && f->isRunning())
            running << f->name();
    QVERIFY2(sends(doc, "SR Truss Cyc 1") == QByteArray("\x00\x00\x00", 3),
             qPrintable(QStringLiteral("cyc %1 %2 ms after Stop; still running: %3")
                            .arg(QString::fromLatin1(sends(doc, "SR Truss Cyc 1").toHex(' ')))
                            .arg(sinceStop.elapsed())
                            .arg(running.isEmpty() ? QStringLiteral("nothing") : running.join(QStringLiteral(", ")))));
    settle(300);
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Design);
    QTRY_COMPARE_WITH_TIMEOUT(run->modeText(), QStringLiteral("DESIGN"), kRigWait);
    doc->resetModified();
}

void CourseShow_Test::lessonFiveLateCue()
{
    // Lesson 1.5's extension 3, in Design: a point cue after the Chorus,
    // dragged after the Solo (renumbered for its place), and Undo.
    QVERIFY(open(kL5Start));
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    ShowTreeView *view = showTree();
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.2")));
    settle(50);
    QVERIFY(newCueNamed(view, "Fill"));
    const quint32 hit = view->currentCueId();
    QCOMPARE(st->cue(hit)->number, QStringLiteral("1.0.2.5"));
    const CueList *list = st->listOfCue(hit);
    QVERIFY(list != nullptr);
    QCOMPARE(list->indexOf(hit), list->indexOf(cueNumbered(doc, "1.0.2")) + 1);
    QCOMPARE(cueNumbered(doc, "1.0.3"), list->cues.at(list->indexOf(hit) + 1).id);

    QCOMPARE(dropGroupOnZone(doc, view, "Lanterns"), QStringLiteral("a new layer on Lanterns in 1.0.2.5 Fill"));
    for (const QString &pal : { QStringLiteral("White"), QStringLiteral("Full") })
        QVERIFY(dropPaletteOnRow(doc, view, "Lanterns", pal).startsWith(pal + " → layer Lanterns in 1.0.2.5 Fill"));
    settle(100);
    QVERIFY2(cueRow(view, "Lanterns") != nullptr && cueRow(view, "Lanterns")->text().contains("White, Full"),
             qPrintable(view->cueLayerItems().join(" | ")));

    // The drag onto the lower half of 1.0.4 Solo: the note first, then the drop.
    const quint32 solo = cueNumbered(doc, "1.0.4");
    bool refused = true;
    const QString note = view->cuesDropHint({ hit }, solo, true, &refused);
    QVERIFY2(!refused && note.contains("1.0.5"), qPrintable(note));
    QVERIFY(view->dropCues({ hit }, solo, true));
    settle(100);
    QCOMPARE(st->cue(hit)->number, QStringLiteral("1.0.5"));
    list = st->listOfCue(hit);
    QCOMPARE(list->indexOf(hit), int(list->cues.count()) - 1);
    QCOMPARE(st->cue(solo)->number, QStringLiteral("1.0.4"));
    QVERIFY(cueNumbered(doc, "1.0.3") != ShowTree::invalidId());

    stFind<QToolButton>(view, "stUndo")->click();
    settle(100);
    QCOMPARE(st->cue(hit)->number, QStringLiteral("1.0.2.5"));
    list = st->listOfCue(hit);
    QCOMPARE(list->indexOf(hit), list->indexOf(cueNumbered(doc, "1.0.2")) + 1);
    doc->resetModified();
}

static QList<quint32> fixturesOf(Doc *doc, const QString &group)
{
    FixtureGroup *g = groupNamed(doc, group);
    return g == nullptr ? QList<quint32>() : g->fixtureList();
}

void CourseShow_Test::lessonStageLooks()
{
    // Lesson 3.3: what each kind of light is drawn as, on Auto; the plot's
    // Show menu; Plot Symbol ▸ Lantern for the Lanterns.
    QVERIFY(open(kClassroom));
    Doc *doc = m_app->doc();
    QStringList seen;
    for (Fixture *f : doc->fixtures())
    {
        const QString line = QStringLiteral("%1 (%2) -> %3")
            .arg(QString(f->name()).remove(QRegularExpression(" ?[0-9]+$| Top S[RL]$")),
                 f->fixtureDef() != nullptr ? f->fixtureDef()->model() : QStringLiteral("generic"),
                 Fixture::stageLookName(f->autoStageLook()));
        if (!seen.contains(line))
            seen << line;
    }
    qInfo().noquote() << "[3.3] Auto looks:" << seen.join(" | ");
    QVERIFY(!doc->monitorProperties()->autoStageLooks());

    QAction *autoLooks = nullptr;
    for (QAction *a : Monitor::instance()->findChildren<QAction *>())
        if (a->text() == QLatin1String("Show Auto fixtures as their closest plot symbol"))
            autoLooks = a;
    QVERIFY(autoLooks != nullptr);
    autoLooks->trigger();
    settle(50);
    QVERIFY(doc->monitorProperties()->autoStageLooks());

    // Fixtures tab: right-click the Lanterns → Plot Symbol ▸.
    const QList<quint32> lanterns = fixturesOf(doc, "Lanterns");
    QCOMPARE(lanterns.count(), 6);
    QMenu menu;
    FixtureManager::fillStageLookMenu(&menu, doc, lanterns);
    QStringList sections, items;
    for (QAction *a : menu.actions())
        (a->isSeparator() ? sections : items) << a->text();
    QCOMPARE(sections, QStringList({ "Moving", "Fixed conventional", "Fixed LED", "Practicals",
                                     "Scenery" }));
    qInfo().noquote() << "[3.3] Lanterns' Plot Symbol menu:" << items.join(" | ");
    QVERIFY(items.first().startsWith("Auto"));
    QAction *lantern = menu.findChild<QAction *>(
        QStringLiteral("stageLook_%1").arg(Fixture::stageLookToString(Fixture::LanternLook)));
    QVERIFY(lantern != nullptr);
    QCOMPARE(lantern->text(), QStringLiteral("Lantern"));
    lantern->trigger();
    settle(50);
    for (quint32 id : lanterns)
        QCOMPARE(doc->fixture(id)->stageLook(), Fixture::LanternLook);
    QCOMPARE(Fixture::stageLookSection(Fixture::LanternLook), int(Fixture::PracticalSection));

    // The front washes: Auto guesses a spot; they are round washes.
    const QList<quint32> washes = fixturesOf(doc, "AT-W-ALL");
    QCOMPARE(washes.count(), 4);
    QMenu washMenu;
    FixtureManager::fillStageLookMenu(&washMenu, doc, washes);
    QCOMPARE(washMenu.actions().first()->text(), QStringLiteral("Auto (Moving head spot)"));
    QAction *round = washMenu.findChild<QAction *>(
        QStringLiteral("stageLook_%1").arg(Fixture::stageLookToString(Fixture::MovingRoundWashLook)));
    QVERIFY(round != nullptr);
    QCOMPARE(round->text(), QStringLiteral("Moving head round wash"));
    round->trigger();
    settle(50);
    for (quint32 id : washes)
        QCOMPARE(doc->fixture(id)->stageLook(), Fixture::MovingRoundWashLook);
    doc->resetModified();
}

void CourseShow_Test::lessonHouseLights()
{
    // Lesson 3.7, in the classroom show: House Lights, Work Lights and
    // Stage Power, each a one-channel generic dimmer as Add fixture makes.
    QVERIFY(open(kClassroom));
    Doc *doc = m_app->doc();
    QMap<QString, quint32> ids;
    for (const QString &name : { QStringLiteral("House Lights"), QStringLiteral("Work Lights"),
                                 QStringLiteral("Stage Power") })
    {
        const quint32 at = AddFixture::findAddress(1, doc->fixtures(), doc->inputOutputMap()->universesCount());
        QVERIFY(at != QLCChannel::invalid());
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setUniverse(at >> 9);
        f->setAddress(at & 0x01FF);
        f->setChannels(1);
        QVERIFY(doc->addFixture(f));
        ids[name] = f->id();
    }
    // Output Rules…: the boxes as the handout ticks them.
    auto rules = [doc](quint32 id, QList<Fixture::OutputRule> on) {
        OutputRulesDialog dlg(doc, { id });
        for (Fixture::OutputRule r : on)
            dlg.box(r)->setChecked(true);
        return dlg.apply();
    };
    QVERIFY(rules(ids["House Lights"], { Fixture::ExemptBlackout, Fixture::ExemptGrandMaster }));
    QVERIFY(rules(ids["Work Lights"], { Fixture::ExemptBlackout, Fixture::ExemptGrandMaster }));
    QVERIFY(rules(ids["Stage Power"], { Fixture::Switch, Fixture::ExemptBlackout, Fixture::ExemptGrandMaster,
                                        Fixture::OnAtOpen }));
    settle(100);
    const auto out = [doc](const QString &name) {
        for (Fixture *f : doc->fixtures())
            if (f->name() == name)
                return outputValue(doc, name, f->channel(0)->name());
        return -1;
    };
    // The relay waits for Operate (the default), then is on by itself.
    QTest::qWait(200);
    QCOMPARE(out("Stage Power"), 0);
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Operate);
    QTRY_COMPARE_WITH_TIMEOUT(out("Stage Power"), 255, kRigWait);
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Design);
    QTRY_COMPARE_WITH_TIMEOUT(out("Stage Power"), 255, kRigWait);

    // The preshow list, typed.
    ShowTreeView *view = showTree();
    for (const QString &cmd : { QStringLiteral("production \"Spring Show\""), QStringLiteral("list \"00-Preshow\""),
                                QStringLiteral("cue + \"House up\""),
                                QStringLiteral("\"House Lights\" dimmer Full"),
                                QStringLiteral("TrussC-RGB colour Blue dimmer Full"),
                                QStringLiteral("cue + \"House to half\""),
                                QStringLiteral("\"House Lights\" dimmer Half"),
                                QStringLiteral("cue + \"House out\""),
                                QStringLiteral("\"House Lights\" dimmer Out") })
        QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    qInfo().noquote() << "[3.7]" << describe(doc).join(" | ");
    QVERIFY(view->selectNode(cueNumbered(doc, "0.0.0")));
    settle(50);
    QTRY_COMPARE_WITH_TIMEOUT(out("House Lights"), 255, kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), kRigWait);

    // Blackout: the cyc goes dark, the house lights and the relay don't.
    // (The classroom show has no outputs, so read the frame Blackout sends.)
    const auto cycOut = [doc]() {
        for (Fixture *f : doc->fixtures())
            if (f->name() == QLatin1String("SR Truss Cyc 1"))
                return outputValue(doc, f->name(), f->channel(2)->name());
        return -1;
    };
    const auto blackoutOut = [doc](const QString &name, quint32 ch) {
        for (Fixture *f : doc->fixtures())
            if (f->name() == name)
            {
                QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
                Universe *u = us.at(int(f->universe()));
                const QByteArray frame = u->blackoutFrame(*u->postGMValues());
                doc->inputOutputMap()->releaseUniverses(false);
                return int(uchar(frame.at(int(f->address() + ch))));
            }
        return -1;
    };
    m_app->m_controlBlackoutAction->trigger();
    QVERIFY(doc->inputOutputMap()->blackout());
    QCOMPARE(cycOut(), 255);
    QCOMPARE(blackoutOut("SR Truss Cyc 1", 2), 0);
    QCOMPARE(blackoutOut("House Lights", 0), 255);
    QCOMPARE(blackoutOut("Stage Power", 0), 255);
    m_app->m_controlBlackoutAction->trigger();
    QVERIFY(!doc->inputOutputMap()->blackout());

    // The Grand Master at 0: the same.
    doc->inputOutputMap()->setGrandMasterValue(0);
    QTRY_COMPARE_WITH_TIMEOUT(cycOut(), 0, kRigWait);
    QCOMPARE(out("House Lights"), 255);
    // The relay ("On when not in use", set by nothing) is put back at the
    // next frame: a Grand Master move rewrites it from its unset 0 first.
    QTRY_COMPARE_WITH_TIMEOUT(out("Stage Power"), 255, kRigWait);
    doc->inputOutputMap()->setGrandMasterValue(255);
    QTRY_COMPARE_WITH_TIMEOUT(cycOut(), 255, kRigWait);

    // House to half, then out: the cues still move them.
    QVERIFY(view->selectNode(cueNumbered(doc, "0.0.1")));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(out("House Lights") - 127) <= 1, kRigWait);
    QVERIFY(view->selectNode(cueNumbered(doc, "0.0.2")));
    QTRY_COMPARE_WITH_TIMEOUT(out("House Lights"), 0, kRigWait);
    doc->resetModified();
}

void CourseShow_Test::lessonFromEos()
{
    // Lesson 3.10: File ▸ New, then File ▸ Import ▸ ETC Eos Show with the
    // course's sample; the wizard's guesses, one fixed; then what lands.
    const QString asc = kShows + QStringLiteral("/classroom-eos.asc");
    QVERIFY(QFileInfo::exists(asc));
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();

    EosImportWizard w(nullptr, doc, asc);
    w.setRemember(false);
    QVERIFY(w.readAscii());
    QStringList roles;
    for (int ch : w.circuitChannels())
        roles << QStringLiteral("%1=%2").arg(ch).arg(EosCircuit::roleName(EosCircuit::Role(w.circuitRole(ch))));
    qInfo().noquote() << "[3.10] circuits:" << roles.join(" ");
    QCOMPARE(w.circuitRole(11), int(EosCircuit::House));
    QCOMPARE(w.circuitRole(12), int(EosCircuit::Work));
    QCOMPARE(w.circuitRole(13), int(EosCircuit::Relay));
    QCOMPARE(w.circuitRole(14), int(EosCircuit::Backstage));
    // The cyc's blue is a stage light, not a backstage blue: set it back.
    QCOMPARE(w.circuitRole(5), int(EosCircuit::Backstage));
    w.setCircuitRoles({ 5 }, EosCircuit::Normal);
    QVERIFY(w.toShowTree());

    EosImportResult eos;
    Doc *scratch = new Doc(this);
    scratch->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    QCOMPARE(EosImporter::load(w.bundle(), eos, w.mapper(), scratch), scratch);
    QList<quint32> fixtures, groups, functions, palettes;
    for (Fixture *f : scratch->fixtures())
        fixtures << f->id();
    for (FixtureGroup *g : scratch->fixtureGroups())
        groups << g->id();
    for (Function *f : scratch->functions())
        functions << f->id();
    for (QLCPalette *pl : scratch->palettes())
        palettes << pl->id();
    const QxwImportResult m = QxwImporter::import(scratch, doc, fixtures, groups, functions, palettes);
    delete scratch;
    QList<quint32> chasers;
    for (quint32 id : eos.chasers)
        if (m.functionMap.contains(id))
            chasers << m.functionMap.value(id);
    QStringList lines;
    EosImporter::toShowTree(doc, chasers, &lines);
    QList<EosImportedSub> subs;
    for (EosImportedSub s : eos.subs)
        if (m.functionMap.contains(s.function))
        {
            s.function = m.functionMap.value(s.function);
            subs << s;
        }
    VCFrame *frame = EosImportWizard::addSubFaders(doc, subs, &lines);
    settle(100);
    qInfo().noquote() << "[3.10] report:" << eos.warnings.join(" | ") << "||" << lines.join(" | ");

    QCOMPARE(doc->fixtures().count(), 10);
    QStringList groupNames;
    for (FixtureGroup *g : doc->fixtureGroups())
        groupNames << QStringLiteral("%1/%2 (%3)").arg(g->path(), g->name()).arg(g->fixtureList().count());
    groupNames.sort();
    qInfo().noquote() << "[3.10] groups:" << groupNames.join(", ");
    QMap<QString, Fixture::OutputRules> rulesByName;
    for (Fixture *f : doc->fixtures())
        rulesByName[f->name()] = f->outputRules();
    QStringList names = rulesByName.keys();
    qInfo().noquote() << "[3.10] fixtures:" << names.join(", ");
    for (Fixture *f : doc->fixtures())
    {
        const Fixture::OutputRules r = f->outputRules();
        if (f->name().contains("House") || f->name().contains("Work") || f->name().contains("Backstage"))
            QCOMPARE(r, Fixture::OutputRules(Fixture::ExemptBlackout | Fixture::ExemptGrandMaster));
        else if (f->name().contains("Relay"))
            QCOMPARE(r, Fixture::OutputRules(Fixture::Switch | Fixture::ExemptBlackout
                                             | Fixture::ExemptGrandMaster | Fixture::OnAtOpen));
        else
            QCOMPARE(int(r), 0);
    }
    QVERIFY(frame != nullptr);
    QCOMPARE(frame->caption(), QStringLiteral("Eos subs"));
    const QList<VCSlider *> sliders = frame->findChildren<VCSlider *>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(sliders.count(), 1);
    qInfo().noquote() << "[3.10] sub fader:" << sliders.first()->caption();
    QStringList tree;
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &l : p.lists)
        {
            QStringList cues;
            for (const Cue &c : l.cues)
                cues << c.number + " " + c.name;
            tree << p.name + " / " + l.name + ": " + cues.join(", ");
        }
    qInfo().noquote() << "[3.10] Show Tree:" << tree.join(" | ");
    QCOMPARE(tree.count(), 1);
    QVERIFY2(tree.first().contains("Preshow") && tree.first().contains("Opening"), qPrintable(tree.first()));
    doc->resetModified();
}

/* Answers the next modal question box with @a answer, and keeps its text. */
static void answerNextQuestion(QMessageBox::StandardButton answer, QString *text)
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

void CourseShow_Test::lessonFiveDesignWhileRunningAsksFirst()
{
    QVERIFY(open(kL5Start));
    Doc *doc = m_app->doc();
    m_app->m_modeToggleAction->trigger();
    settle(100);
    QCOMPARE(doc->mode(), Doc::Operate);
    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.0"), kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), kRigWait);
    QVERIFY(doc->masterTimer()->runningFunctions() > 0);

    // No: still LIVE, the cue still on stage.
    QString asked;
    answerNextQuestion(QMessageBox::No, &asked);
    m_app->m_modeToggleAction->trigger();
    settle(200);
    QVERIFY2(asked.contains(QStringLiteral("Really stop them")), qPrintable(asked));
    QCOMPARE(doc->mode(), Doc::Operate);
    QCOMPARE(run->modeText(), QStringLiteral("LIVE"));
    QCOMPARE(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3));

    // Yes: everything stops, Design, a dark stage.
    asked.clear();
    answerNextQuestion(QMessageBox::Yes, &asked);
    m_app->m_modeToggleAction->trigger();
    settle(200);
    QVERIFY2(asked.contains(QStringLiteral("Really stop them")), qPrintable(asked));
    QCOMPARE(doc->mode(), Doc::Design);
    QTRY_COMPARE_WITH_TIMEOUT(run->modeText(), QStringLiteral("DESIGN"), kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(doc->masterTimer()->runningFunctions(), 0, kRigWait);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\x00", 3), kRigWait);
    doc->resetModified();
}

namespace
{
HelpShots::Callout mark(QWidget *w, int n, HelpShots::Place place = HelpShots::Auto)
{
    HelpShots::Callout c;
    c.widget = w;
    c.number = n;
    c.place = place;
    return c;
}

/* A row of a tree (or list) as a callout in @a grab's coordinates. */
HelpShots::Callout markRect(QWidget *viewport, QRect r, QWidget *grab, int n,
                            HelpShots::Place place = HelpShots::Auto)
{
    HelpShots::Callout c;
    c.number = n;
    c.place = place;
    r.setLeft(0);
    r.setRight(viewport->width() - 1);
    c.rect = QRect(viewport->mapTo(grab, r.topLeft()), r.size());
    return c;
}

/* Shows the Library row @a text (opening its folders) and selects it. */
QTreeWidgetItem *showLibraryRow(QTreeWidget *lib, const QString &text)
{
    QTreeWidgetItem *it = libraryRow(lib, text);
    if (it == nullptr)
        return nullptr;
    for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    lib->clearSelection();
    lib->scrollToItem(it, QAbstractItemView::PositionAtCenter);
    it->setSelected(true);
    settle();
    return it;
}

bool save(const QImage &img, const QString &dir, const QString &name)
{
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + name + QStringLiteral(".png");
    const bool ok = img.save(path);
    qInfo().noquote() << "[COURSESHOT]" << path << (ok ? "" : "CAN'T WRITE");
    return ok;
}

bool shoot(const HelpShots::Frame &f, const QString &dir, const QString &name)
{
    // Room for the cue names in the tree, as a student would drag it.
    if (ShowTreeView *v = ShowTreeView::instance())
    {
        if (QSplitter *split = stFind<QSplitter>(v, "stSplitter"))
        {
            const int w = split->width();
            split->setSizes(QList<int>() << 260 << 520 << w - 780);
        }
        // The name, then room for where a grey row comes from (1.0.0)
        // beside the S T R M F columns.
        v->tree()->setColumnWidth(0, 260);
        v->tree()->setColumnWidth(ShowTreeView::InfoCol, 90);
    }
    settle();
    // A widget outside the grab would be left out without a word: the
    // Show Tree's header, say, which the top bar holds while it's on show.
    for (const HelpShots::Callout &c : f.callouts)
        if (c.widget.isNull() ? c.rect.isEmpty()
                              : (!c.widget->isVisible() || !f.grab->isAncestorOf(c.widget)))
        {
            qWarning() << name << "callout" << c.number << "has nothing on show";
            return false;
        }
    return save(HelpShots::annotate(f.grab->grab().toImage(), f), dir, name);
}
/* A picture of the Show Tree as a student sees it: the tab and, above it,
   the window's top bar, which holds the tab's header (+ Production, + Cue,
   Undo, the hint) while the Show Tree is the tab on show. The grab is the
   whole window, cropped to those two. */
HelpShots::Frame showTreeFrame(App *app, ShowTreeView *v)
{
    HelpShots::Frame f;
    f.grab = app;
    QRect r(v->mapTo(app, QPoint(0, 0)), v->size());
    if (app->m_toolbar != nullptr && app->m_toolbar->isVisible())
        r |= QRect(app->m_toolbar->mapTo(app, QPoint(0, 0)), app->m_toolbar->size());
    f.crop = r;
    /* A cue's page is taller than the window: scroll it so its Layers (the
       list, or the dashed drop area while there are none) are on show,
       as a student scrolls to them. The callouts are placed after this. */
    if (QScrollArea *page = qobject_cast<QScrollArea *>(v->m_cuePage))
        if (page->isVisible())
        {
            if (v->m_cueLayers != nullptr && v->m_cueLayers->isVisible())
                page->ensureWidgetVisible(v->m_cueLayers, 0, 8);
            if (QWidget *zone = v->findChild<QWidget *>(QStringLiteral("stCueDropZone")))
                if (zone->isVisible())
                    page->ensureWidgetVisible(zone, 0, 8);
        }
    return f;
}
}

void CourseShow_Test::lessonFocusAndCalibration()
{
    // Get ready: the classroom show, saved as the student's own (a venue
    // lives in a file beside the show, never beside the course's).
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString mine = home.filePath(QStringLiteral("Lesson3-6-Sam.qlcc"));
    QVERIFY(open(kClassroom));
    QCOMPARE(m_app->saveXML(mine), QFile::NoError);
    Doc *doc = m_app->doc();
    MonitorProperties *props = doc->monitorProperties();
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    QCOMPARE(QFileInfo(venues->filePath()).absolutePath(), QFileInfo(mine).absolutePath());
    const QString shots = qEnvironmentVariable("COURSE_SHOTS");
    const QString dir = shots.isEmpty() ? QString() : shots + QStringLiteral("/lesson-3-6");
    if (!dir.isEmpty())
    {
        m_app->setTheme(App::ThemeBlue);
        m_app->resize(1280, 800);
    }

    /* Part A: the stage marks, by the names the handout uses -- rows on
       the Stage layer in Plot layers, each turned on with its eye -- and
       Show ▸ Target positions, on already. */
    Monitor *studio = Monitor::instance();
    QVERIFY(studio != nullptr);
    QVERIFY(studio->m_layersPanel != nullptr);
    studio->m_layersPanel->setVisible(true);
    studio->m_layersPanel->reload();
    QAction *positions = studio->findChild<QAction *>(QStringLiteral("showTargetPositions"));
    QVERIFY(positions != nullptr);
    QCOMPARE(positions->text(), QStringLiteral("Target positions"));
    QVERIFY(positions->isChecked());       // on already
    QCOMPARE(MonitorProperties::stagePieceName(MonitorProperties::CentrePiece), QStringLiteral("Centre line / DSC mark"));
    QCOMPARE(MonitorProperties::stagePieceName(MonitorProperties::CurtainPiece), QStringLiteral("Curtain line"));
    QCOMPARE(MonitorProperties::stagePieceName(MonitorProperties::NumberPiece), QStringLiteral("Number line"));
    for (int piece : { int(MonitorProperties::CentrePiece), int(MonitorProperties::CurtainPiece),
                       int(MonitorProperties::NumberPiece) })
    {
        QWidget *row = studio->findChild<QWidget *>(QStringLiteral("stagePieceToggles%1").arg(piece));
        QVERIFY2(row != nullptr, qPrintable(QString::number(piece)));
        QToolButton *eye = row->findChild<QToolButton *>(QStringLiteral("stagePieceEye"));
        QVERIFY(eye != nullptr);
        if (!eye->isChecked())
            eye->click();
        settle(50);
    }
    settle(50);
    QVERIFY(props->stageMarkVisible(MonitorProperties::CurtainLineFlag));
    QVERIFY(props->stageMarkVisible(MonitorProperties::NumberLineFlag));
    QMap<QString, quint32> target;
    for (const StageTarget *t : props->stageTargets())
    {
        target[t->name()] = t->id();
        qInfo().noquote() << "[3.6]" << t->name() << "reads" << props->stagePositionText(t->position())
                          << "/ said" << props->stagePositionText(t->position(), MonitorProperties::SpokenOffset);
    }
    QVERIFY(target.contains("Center") && target.contains("Soloist DS"));
    // As the handout reads them off the plot, and as a call says them.
    QCOMPARE(props->stagePositionText(props->stageTarget(target["Soloist DS"])->position()),
             QString::fromUtf8("SL 1' \302\267 DS 1' 1\""));
    QCOMPARE(props->stagePositionText(props->stageTarget(target["Center"])->position()),
             QString::fromUtf8("SL 1' \302\267 US 6' \302\267 \342\206\221 1' 4\""));
    QCOMPARE(props->stagePositionText(props->stageTarget(target["Soloist DS"])->position(),
                                      MonitorProperties::SpokenOffset), QStringLiteral("SL 1, DS 1"));
    qInfo().noquote() << "[3.6] apron" << props->apronDepth() << "m";
    if (!dir.isEmpty())
    {
        // The plot as the student sees it, with the marks and the readouts.
        QWidget *w = studio;
        while (w != nullptr && m_app->m_tab->indexOf(w) < 0)
            w = w->parentWidget();
        if (w != nullptr)
            m_app->m_tab->setCurrentWidget(w);
        settle(400);
        studio->updateView();
        settle(400);
        QVERIFY(save(studio->m_graphicsView->grab().toImage(), dir, "stage-marks"));
    }

    // Part B: three focus calls and a venue, in the Focus calls dialog.
    QMap<QString, quint32> group;
    for (const FixtureGroup *g : doc->fixtureGroups())
        group[g->name()] = g->id();
    {
        FocusCallsDialog d(doc, m_app);
        QPushButton *add = d.findChild<QPushButton *>("fcAdd");
        QListWidget *groups = d.findChild<QListWidget *>("fcGroups");
        QComboBox *to = d.findChild<QComboBox *>("fcTarget");
        QLineEdit *note = d.findChild<QLineEdit *>("fcNote");
        QVERIFY(add != nullptr && groups != nullptr && to != nullptr);
        const struct { const char *group; const char *target; const char *note; } calls[] = {
            { "Truss-Spots", "Center", "" },
            { "AT-Spots", "Soloist DS", "Off the proscenium" },
            { "AT-W-ALL", "Center", "" },
        };
        int row = 0;
        for (const auto &c : calls)
        {
            add->click();
            for (int i = 0; i < groups->count(); ++i)
                groups->item(i)->setCheckState(groups->item(i)->text() == QLatin1String(c.group)
                                               ? Qt::Checked : Qt::Unchecked);
            QVERIFY2(to->findText(c.target) >= 0, c.target);
            to->setCurrentIndex(to->findText(c.target));
            if (note != nullptr && *c.note)
            {
                note->clear();
                QTest::keyClicks(note, QString::fromUtf8(c.note));
            }
            QCOMPARE(d.callList()->item(row)->text(),
                     QString::fromUtf8("%1. %2 \342\206\222 %3").arg(row + 1).arg(c.group, c.target));
            ++row;
        }
        d.setVenueChoice(QStringLiteral("Classroom"));
        QLineEdit *by = d.findChild<QLineEdit *>("fcCheckedBy");
        QVERIFY(by != nullptr);
        by->clear();
        QTest::keyClicks(by, "Sam");
        if (!dir.isEmpty())
        {
            d.resize(980, 560);
            d.show();
            settle(100);
            HelpShots::Frame f;
            f.grab = &d;
            f.callouts << mark(d.findChild<QWidget *>("fcCalls"), 1) << mark(groups, 2)
                       << mark(to, 3, HelpShots::Above)
                       << mark(d.findChild<QWidget *>("fcVenue"), 4, HelpShots::Above)
                       << mark(by, 5, HelpShots::Above)
                       << mark(d.findChild<QWidget *>("fcStart"), 6);
            QVERIFY(save(HelpShots::annotate(d.grab().toImage(), f), dir, "focus-calls"));
        }
        QVERIFY(d.apply());
        QCOMPARE(doc->programmer()->focusCalls()->count(), 3);
        QCOMPARE(venues->activeVenue(), QStringLiteral("Classroom"));
        QVERIFY(QFile::exists(home.filePath(QStringLiteral("Lesson3-6-Sam.venues.xml"))));
    }

    // Part C: run them on the keys.
    FocusSession *fs = doc->programmer()->focusSession();
    fs->setFlashEnabled(false);
    {
        FocusSession::Options o;
        o.calls = doc->programmer()->focusCalls()->sessionCalls();
        o.others = FocusSession::OthersDim;
        QString why;
        FocusModePanel *panel = FocusModePanel::startWith(doc, m_app, o, &why, int(FocusSession::KeyboardInput));
        QVERIFY2(panel != nullptr, qPrintable(why));
        QCOMPARE(fs->count(), 4);                               // the four truss spots
        const quint32 first = fs->currentFixture();
        QTest::keyClick(panel, Qt::Key_BracketRight);           // the next spot
        QVERIFY(fs->currentFixture() != first);
        const quint32 spot = fs->currentFixture();
        QTest::keyClick(panel, Qt::Key_Right, Qt::ShiftModifier);   // 1° of pan
        QTest::keyClick(panel, Qt::Key_Return);                 // kept for Classroom
        QVERIFY(venues->hasCorrection(spot));
        QVERIFY(qAbs(venues->correction(spot).panOffsetDeg - 1.0f) < 0.01f);
        QTest::keyClick(panel, Qt::Key_G);                      // the whole call
        QCOMPARE(fs->scope(), FocusSession::GroupScope);
        QTest::keyClick(panel, Qt::Key_PageDown);               // call 2: the AT spots
        QCOMPARE(fs->count(), 2);
        QTest::keyClick(panel, Qt::Key_Comma);                  // spot down
        QTest::keyClick(panel, Qt::Key_Apostrophe);             // sharper
        fs->stop();
        settle(50);
        if (FocusModePanel *p = FocusModePanel::instance())
            delete p;
    }

    // Part D: the scramble, easy, focused back, scored, and the report.
    {
        FocusCallsDialog d(doc, m_app);
        QString why;
        QVERIFY2(d.startPractice(1.0f, 3.0f, 36, &why), qPrintable(why));
        QVERIFY(venues->isPracticeActive());
        const VenueCorrections::Venue pv = venues->venue(venues->activeVenue());
        QCOMPARE(pv.practice.truth.size(), 10);                 // 4 truss spots, 2 AT spots, 4 AT washes
        FocusSession::Options o;
        o.calls = doc->programmer()->focusCalls()->sessionCalls();
        FocusModePanel *panel = FocusModePanel::startWith(doc, m_app, o, &why, int(FocusSession::KeyboardInput));
        QVERIFY2(panel != nullptr, qPrintable(why));
        // Each light back on its mark (but the last, a little off), call by call.
        for (int call = 0; call < 3; ++call)
        {
            for (int i = 0; i < fs->count(); ++i)
            {
                const quint32 fid = fs->currentFixture();
                const RigCalibration truth = pv.practice.truth.value(fid);
                const FixtureRigProps rp = props->fixtureRigProps(fid);
                const float miss = (call == 2 && i == fs->count() - 1) ? 1.8f : 0.2f;
                fs->nudge(truth.panOffsetDeg - rp.panOffsetDeg + miss, truth.tiltOffsetDeg - rp.tiltOffsetDeg);
                QTest::keyClick(panel, Qt::Key_Return);
                QTest::keyClick(panel, Qt::Key_BracketRight);
            }
            QTest::keyClick(panel, Qt::Key_PageDown);
        }
        fs->stop();
        settle(50);
        if (FocusModePanel *p = FocusModePanel::instance())
            delete p;
        const QString practice = venues->activeVenue();
        const VenueCorrections::Score sc = venues->practiceScore(practice);
        qInfo().noquote() << "[3.6] practice:" << sc.withinOne << "of" << sc.lights << "within 1°, mean"
                          << sc.meanError << "worst" << sc.worstError << "saves" << sc.saves;
        QCOMPARE(sc.lights, 10);
        QCOMPARE(sc.withinOne, 9);
        QCOMPARE(sc.checked, 10);
        const QString pdf = home.filePath(QStringLiteral("Classroom practice focus report.pdf"));
        QVERIFY2(FocusCallsDialog::writeReport(doc, practice, pdf, &why), qPrintable(why));
        QVERIFY(QFileInfo(pdf).size() > 1000);
        QDialog *result = FocusCallsDialog::showPracticeResult(doc, m_app);
        QVERIFY(result != nullptr);
        QLabel *text = result->findChild<QLabel *>("practiceResultText");
        QVERIFY2(text->text().contains("9 of 10 lights within 1"), qPrintable(text->text()));
        QVERIFY2(text->text().contains("Classroom is back on the rig"), qPrintable(text->text()));
        QCOMPARE(venues->activeVenue(), QStringLiteral("Classroom"));
        if (!dir.isEmpty())
        {
            result->resize(620, result->sizeHint().height());
            result->show();
            settle(100);
            HelpShots::Frame f;
            f.grab = result;
            f.callouts << mark(text, 1) << mark(result->findChild<QWidget *>("practiceResultReport"), 2);
            QVERIFY(save(HelpShots::annotate(result->grab().toImage(), f), dir, "practice-score"));
        }
        delete result;
    }

    // Part E: Refine on a typed solo cue.
    ShowTreeView *view = showTree();
    for (const QString &cmd : { QStringLiteral("production \"Spring Show\""), QStringLiteral("list \"Solo\""),
                                QStringLiteral("cue + \"Solo\""),
                                QStringLiteral("AT-Spots colour White dimmer Full aim \"Soloist DS\"") })
        QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    qInfo().noquote() << "[3.6]" << describe(doc).join(" | ");
    const quint32 solo = cueNumbered(doc, "1");
    QVERIFY(view->selectNode(solo));
    settle(50);
    {
        RefinePanel *panel = RefinePanel::open(doc, m_app, solo, 0xFFFFFFFF);
        QVERIFY(panel != nullptr);
        RefineSession *r = doc->programmer()->refineSession();
        r->setFlashEnabled(false);
        QCOMPARE(r->scope(), RefineSession::LayerScope);
        QVERIFY(r->nudgeZoom(-3.0f));                           // spot down, the whole layer
        QString why;
        QVERIFY2(r->keep(&why), qPrintable(why));
        // An aimed layer can't be moved as a whole; one spot can.
        QVERIFY(!r->nudgePosition(2.0f, 0.0f));
        r->setScope(RefineSession::FixtureScope);
        QVERIFY(r->nudgePosition(2.0f, 0.0f));
        QVERIFY2(r->keep(&why), qPrintable(why));
        qInfo().noquote() << "[3.6] after Refine:" << describe(doc).join(" | ");
        r->stop();
        settle(50);
        if (RefinePanel *p = RefinePanel::instance())
            delete p;
    }
    doc->resetModified();
}

void CourseShow_Test::lessonPictures()
{
    // COURSE_SHOTS is docs/course/images: each lesson's pictures go in a
    // folder of its own there (lesson-1-1, lesson-0-8, ...).
    const QString root = qEnvironmentVariable("COURSE_SHOTS");
    if (root.isEmpty())
        QSKIP("set COURSE_SHOTS to the course's images folder");
    const QString dir = root + QStringLiteral("/lesson-1-1");
    // As the help's pictures are: the Blue theme reads best printed small.
    m_app->setTheme(App::ThemeBlue);
    m_app->resize(1280, 800);

    // --- The tab, with the finished lesson in it.
    QVERIFY(open(kL1Finish));
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    ShowTreeView *view = showTree();
    QTreeWidget *tree = view->tree();
    const CueList &list = st->productions().first().lists.first();
    const quint32 pose = list.cues.at(0).id, verse = list.cues.at(1).id, chorus = list.cues.at(2).id;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        (*it)->setExpanded(true);
    QVERIFY(view->selectNode(pose));
    settle(200);
    {
        HelpShots::Frame f;
        f.grab = m_app;
        QRect buttons;
        for (QWidget *b : { (QWidget *)view->m_newProduction, (QWidget *)view->m_newList, (QWidget *)view->m_newCue })
            buttons |= QRect(b->mapTo(m_app, QPoint(0, 0)), b->size());
        HelpShots::Callout bar;
        bar.rect = buttons;
        bar.number = 1;
        f.callouts << bar << mark(view->library(), 2) << mark(tree, 3) << mark(view->m_pages, 4)
                   << mark(view->commandLine(), 5)
                   << mark(m_app->m_tab->tabBar(), 6);
        QVERIFY(shoot(f, dir, "showtree-tab"));
    }

    // --- The steps, from the start file: + Production, + Cue list, + Cue.
    QVERIFY(open(kL1Start));
    doc = m_app->doc();
    st = doc->showTree();
    view = showTree();
    tree = view->tree();
    answerNextInputDialog(QStringLiteral("Spring Show"));
    view->m_newProduction->click();
    settle();
    answerNextInputDialog(QStringLiteral("01-Everybody Scream"));
    view->m_newList->click();
    settle();
    view->m_newCue->click();
    settle();
    QLineEdit *name = stFind<QLineEdit>(view, "stCueName");
    name->setText(QStringLiteral("Pose"));
    QTest::keyClick(name, Qt::Key_Return);
    settle(100);
    {
        HelpShots::Frame f = showTreeFrame(m_app, view);
        f.callouts << mark(view->m_newCue, 1) << mark(name, 2)
                   << mark(stFind<QLabel>(view, "stCueDropZone"), 3);
        QTreeWidgetItem *cueRow = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->text(0).contains(QLatin1String("Pose")))
                cueRow = *it;
        QVERIFY(cueRow != nullptr);
        f.callouts << markRect(tree->viewport(), tree->visualItemRect(cueRow), m_app, 4);
        QVERIFY(shoot(f, dir, "new-cue"));
    }

    // Drag TrussC-RGB onto the dashed area.
    QLabel *hint = stFind<QLabel>(view, "stDropHint");
    QWidget *page = stFind<QWidget>(view, "stCuePage");
    QLabel *zone = stFind<QLabel>(view, "stCueDropZone");
    QTreeWidgetItem *cyc = showLibraryRow(view->library(), "TrussC-RGB");
    QVERIFY(cyc != nullptr);
    dragAndDrop(page, zone->mapTo(page, zone->rect().center()),
                idsMime(FixtureGroupSource::fixtureGroupMimeType(), { groupNamed(doc, "TrussC-RGB")->id() }), hint);
    settle(100);
    QListWidget *rows = view->cueLayerList();
    auto rowRect = [rows](const QString &start) {
        for (int i = 0; i < rows->count(); ++i)
            if (rows->item(i)->text().contains(start))
                return rows->visualItemRect(rows->item(i));
        return QRect();
    };
    {
        HelpShots::Frame f = showTreeFrame(m_app, view);
        cyc = showLibraryRow(view->library(), "TrussC-RGB");
        f.callouts << markRect(view->library()->viewport(), view->library()->visualItemRect(cyc), m_app, 1)
                   << markRect(rows->viewport(), rowRect("TrussC-RGB"), m_app, 2);
        QVERIFY(shoot(f, dir, "drop-group"));
    }
    // Blue and Full onto the row.
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("Full") })
        dragAndDrop(rows->viewport(), rowRect("TrussC-RGB").center(),
                    idsMime(FunctionsTreeWidget::paletteDragMimeType(), { paletteNamed(doc, pal)->id() }), hint);
    settle(100);
    {
        HelpShots::Frame f = showTreeFrame(m_app, view);
        QTreeWidgetItem *blue = showLibraryRow(view->library(), "Blue");
        QRect r = view->library()->visualItemRect(blue);
        f.callouts << markRect(view->library()->viewport(), r, m_app, 1)
                   << markRect(rows->viewport(), rowRect("TrussC-RGB"), m_app, 2)
                   << mark(hint, 3);
        QVERIFY(shoot(f, dir, "drop-palettes"));
    }

    // --- The finished lesson again: the tracked row, the rig, Run.
    QVERIFY(open(kL1Finish));
    doc = m_app->doc();
    st = doc->showTree();
    view = showTree();
    tree = view->tree();
    rows = view->cueLayerList();
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        (*it)->setExpanded(true);
    QVERIFY(view->selectNode(verse));
    settle(200);
    {
        HelpShots::Frame f = showTreeFrame(m_app, view);
        f.callouts << markRect(rows->viewport(), rowRect("⤳ TrussC-RGB"), m_app, 1)
                   << markRect(rows->viewport(), rowRect("StepTops"), m_app, 2);
        QTreeWidgetItem *grey = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->text(0).contains(QStringLiteral("⤳")) && (*it)->parent()
                && (*it)->parent()->text(0).contains(QLatin1String("1.0.1")))
                grey = *it;
        if (grey != nullptr)
            f.callouts << markRect(tree->viewport(), tree->visualItemRect(grey), m_app, 3);
        QVERIFY(shoot(f, dir, "tracked-row"));
    }
    // The Rig Overview of each cue, as Design shows it while it's selected.
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    view = showTree();
    QVERIFY(view->selectNode(st->productions().first().id));     // nothing on the rig
    settle(300);
    mon->toggleStageOverview();
    settle(300);
    QVERIFY(mon->m_overviewDlg != nullptr);
    mon->m_overviewDlg->resize(1100, 700);
    settle(200);
    mon->m_overviewView->repaint();
    QVERIFY(save(mon->m_overviewDlg->grab().toImage(), dir, "rig-dark"));
    const QList<QPair<quint32, QString>> cues = { { pose, "rig-pose" }, { verse, "rig-verse" },
                                                  { chorus, "rig-chorus" } };
    for (const auto &c : cues)
    {
        QVERIFY(view->selectNode(c.first));
        settle(600);
        mon->m_overviewView->repaint();
        QVERIFY(save(mon->m_overviewDlg->grab().toImage(), dir, c.second));
    }
    mon->m_overviewDlg->close();
    settle();

    // The Run tab, ready to GO the first cue.
    CueRunner *run = m_app->findChild<CueRunner *>();
    QVERIFY(run != nullptr);
    m_app->m_tab->setCurrentWidget(run);
    settle(300);
    {
        HelpShots::Frame f;
        f.grab = m_app;
        f.callouts << mark(run->findChild<QToolButton *>("crGo"), 1)
                   << mark(run->cueTree(), 2)
                   << mark(run->findChild<QWidget *>("crNext"), 3);
        QVERIFY(shoot(f, dir, "run-tab"));
    }
    doc->resetModified();

    // ================= Level 0 and Lessons 1.2-1.5 =================
    closeRig();

    // --- 0.8: the stage from the top, the front and at 45°, work light on;
    //     and the Rig Overview's own controls.
    {
        const QString d = root + QStringLiteral("/lesson-0-8");
        QVERIFY(open(kL1Finish));
        ShowTreeView *v = showTree();
        const quint32 prod = m_app->doc()->showTree()->productions().first().id;
        QVERIFY(rigShot(v, prod, d + "/stage-top.png", 0, 0));
        QVERIFY(rigShot(v, prod, d + "/stage-front.png", 1, 0));
        QVERIFY(rigShot(v, prod, d + "/stage-45.png", -1, 0));
        QVERIFY(rigShot(v, cueNumbered(m_app->doc(), "1.0.2"), d + "/stage-chorus.png", -1, 1));
        Monitor *mon = Monitor::instance();
        QList<QComboBox *> combos = mon->m_overviewDlg->findChildren<QComboBox *>();
        QToolButton *live = nullptr;
        for (QToolButton *b : mon->m_overviewDlg->findChildren<QToolButton *>())
            if (b->text() == QStringLiteral("Live"))
                live = b;
        QVERIFY(live != nullptr && combos.count() >= 2);
        HelpShots::Frame f;
        f.grab = mon->m_overviewDlg;
        f.callouts << mark(combos.at(0), 1) << mark(live, 2) << mark(combos.at(1), 3);
        QVERIFY(shoot(f, d, "rig-overview"));
        closeRig();
        m_app->doc()->resetModified();
    }

    // --- 1.2: the verse before and after; the layer page and its ⋯; the
    //     picked level and Make reusable; the Scream's cue looks.
    {
        const QString d = root + QStringLiteral("/lesson-1-2");
        QVERIFY(open(kL2Start));
        Doc *dc = m_app->doc();
        ShowTreeView *v = showTree();
        QVERIFY(rigShot(v, cueNumbered(dc, "1.0.1"), d + "/rig-verse-warm.png"));
        closeRig();
        v = showTree();
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.1")));
        settle(100);
        QVERIFY(clickCueRow(v, "StepTops"));
        settle(100);
        {
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyColour)), 1)
                       << mark(moreButton(v, KeyColour), 2)
                       << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyDimmer)), 3)
                       << mark(moreButton(v, KeyDimmer), 4);
            QVERIFY(shoot(f, d, "layer-page"));
        }
        QVERIFY(rowMenu(v, KeyColour, "Set to", "Lavender"));
        QVERIFY(rowMenu(v, KeyDimmer, "Level…"));
        QVERIFY(pickDockedLevel(v, 40));
        settle(100);
        {
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyColour)), 1)
                       << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyDimmer)), 2)
                       << mark(stFind<QLabel>(v, QStringLiteral("stReuse:%1").arg(KeyDimmer)), 3)
                       << mark(hintOf(v), 4);
            QVERIFY(shoot(f, d, "make-reusable"));
        }
        dc->resetModified();

        QVERIFY(open(kL2Finish));
        dc = m_app->doc();
        v = showTree();
        QVERIFY(rigShot(v, cueNumbered(dc, "1.0.1"), d + "/rig-verse-lavender.png"));
        QVERIFY(rigShot(v, cueNumbered(dc, "1.0.3"), d + "/rig-scream.png"));
        closeRig();
        v = showTree();
        for (QTreeWidgetItemIterator it(v->tree()); *it; ++it)
            (*it)->setExpanded(true);
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.3")));
        settle(200);
        {
            QListWidget *rows = v->cueLayerList();
            QRect own = rows->visualItemRect(cueRow(v, "TrussW-RGB")) | rows->visualItemRect(cueRow(v, "Lanterns"));
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(stFind<QLabel>(v, "stCueLooks"), 1)
                       << markRect(rows->viewport(), own, m_app, 2);
            QVERIFY(shoot(f, d, "cue-looks"));
        }
        dc->resetModified();
    }

    // --- 1.3: done with the mouse, a picture at each "you should see";
    //     then the aim row, the Solo's page and the spots' layer.
    {
        const QString d = root + QStringLiteral("/lesson-1-3");
        QVERIFY(open(kL3Start));
        QVERIFY(lessonThreeByMouse(m_app->doc(), showTree(), d));
        m_app->doc()->resetModified();

        QVERIFY(open(kL3Finish));
        Doc *dc = m_app->doc();
        ShowTreeView *v = showTree();
        for (QTreeWidgetItemIterator it(v->tree()); *it; ++it)
            (*it)->setExpanded(true);
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.1")));
        settle(100);
        QVERIFY(clickCueRow(v, "AT-W-ALL"));
        settle(100);
        {
            QTreeWidgetItem *center = showLibraryRow(v->library(), "Center");
            QVERIFY(center != nullptr);
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << markRect(v->library()->viewport(), v->library()->visualItemRect(center), m_app, 1)
                       << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyPosition)), 2);
            QVERIFY(shoot(f, d, "aim-row"));
        }
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.4")));
        settle(200);
        {
            QTreeWidgetItem *solo = nullptr;
            for (QTreeWidgetItemIterator it(v->tree()); *it; ++it)
                if ((*it)->text(0).contains(QLatin1String("1.0.4")))
                    solo = *it;
            QVERIFY(solo != nullptr);
            QListWidget *rows = v->cueLayerList();
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(stFind<QComboBox>(v, "stCueMode"), 1)
                       << markRect(rows->viewport(), rows->visualItemRect(cueRow(v, "AT-Spots")), m_app, 2)
                       << markRect(v->tree()->viewport(), v->tree()->visualItemRect(solo), m_app, 3);
            QVERIFY(shoot(f, d, "solo-cue"));
        }
        QVERIFY(clickCueRow(v, "AT-Spots"));
        settle(100);
        {
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyDimmer)), 1)
                       << mark(stFind<QLabel>(v, QStringLiteral("stRow:%1").arg(KeyPosition)), 2);
            QVERIFY(shoot(f, d, "spot-layer"));
        }
        dc->resetModified();
    }

    // --- 1.4: the Scream's timing, and the Run tab mid-fade.
    {
        const QString d = root + QStringLiteral("/lesson-1-4");
        QVERIFY(open(kL4Finish));
        Doc *dc = m_app->doc();
        ShowTreeView *v = showTree();
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.3")));
        settle(200);
        {
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(v->m_cueFadeIn, 1) << mark(v->m_cueFadeOut, 2) << mark(v->m_cueHold, 3);
            QVERIFY(shoot(f, d, "cue-timing"));
        }
        CueRunner *run = runTab(m_app);
        QVERIFY(run != nullptr);
        press(run, "crGo");
        QTest::qWait(1300);
        {
            HelpShots::Frame f;
            f.grab = m_app;
            f.callouts << mark(run->findChild<QWidget *>("crNow"), 1) << mark(run->cueTree(), 2);
            QVERIFY(shoot(f, d, "run-fading"));
        }
        press(run, "crStop");
        settle(300);
        dc->resetModified();
    }

    // --- 1.5: the Run tab with a cue loaded (STANDBY).
    {
        const QString d = root + QStringLiteral("/lesson-1-5");
        QVERIFY(open(kL5Start));
        Doc *dc = m_app->doc();
        CueRunner *run = runTab(m_app);
        QVERIFY(run != nullptr);
        m_app->m_modeToggleAction->trigger();      // show time: LIVE
        settle(300);
        press(run, "crGo");
        QTest::qWait(3300);
        QTreeWidgetItem *chorus = runRow(run, "1.0.2");
        QVERIFY(chorus != nullptr);
        QTest::mouseClick(run->cueTree()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          run->cueTree()->visualItemRect(chorus).center());
        settle(200);
        {
            QTreeWidget *t = run->cueTree();
            HelpShots::Frame f;
            f.grab = m_app;
            f.callouts << markRect(t->viewport(), t->visualItemRect(chorus), m_app, 1)
                       << mark(run->findChild<QWidget *>("crNext"), 2)
                       << mark(run->findChild<QToolButton *>("crGo"), 3)
                       << mark(run->findChild<QToolButton *>("crBack"), 4)
                       << mark(run->findChild<QToolButton *>("crStop"), 5)
                       << mark(run->findChild<QWidget *>("crMode"), 6);
            QVERIFY(shoot(f, d, "run-standby"));
        }
        press(run, "crStop");
        QTest::qWait(800);
        m_app->m_modeToggleAction->trigger();      // Design again
        settle(300);
        QCOMPARE(dc->mode(), Doc::Design);
        dc->resetModified();
    }

    // Last: it types a production of its own, which nothing after it
    // should see.
    // --- 0.2: the four qualities of light, typed on the classroom show
    //     (not saved): each a Block cue, so it shows only itself.
    {
        const QString d = root + QStringLiteral("/lesson-0-2");
        QVERIFY(open(kClassroom));
        ShowTreeView *v = showTree();
        const QList<QPair<QString, QStringList>> looks = {
            { "intensity-full", { "Steps colour Magenta dimmer Full" } },
            { "intensity-low", { "Steps colour Magenta dimmer 25" } },
            { "colour-warm", { "TrussW-RGB colour Amber dimmer Full", "StepTops colour Amber dimmer Full" } },
            { "colour-cool", { "TrussW-RGB colour Blue dimmer Full", "StepTops colour Blue dimmer Full" } },
            { "direction-front", { "AT-W-ALL colour White dimmer Full" } },
            { "direction-below", { "StepTops colour White dimmer Full" } },
            { "direction-back", { "TrussW-RGB colour White dimmer Full" } },
        };
        QVERIFY(v->runCommand("production \"Qualities\""));
        QVERIFY(v->runCommand("list \"01-Qualities\""));
        for (const auto &l : looks)
        {
            QVERIFY(v->runCommand(QStringLiteral("cue + \"%1\" block").arg(l.first)));
            for (const QString &c : l.second)
                QVERIFY2(v->runCommand(c), qPrintable(c + ": " + v->commandMessage()));
            QVERIFY(rigShot(v, v->currentCueId(), d + QLatin1Char('/') + l.first + QStringLiteral(".png")));
            closeRig();
            v = showTree();
        }
        m_app->doc()->resetModified();
    }

}


/* ===================== Level 2 =====================
   Each lesson with the mouse, from its start file; with @a shots (the
   lesson's images folder) the pictures at its "you should see" lines. */

namespace
{
/* The tree's row for the cue numbered @a number (its text starts so). */
QTreeWidgetItem *treeCue(ShowTreeView *view, const QString &number)
{
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        if ((*it)->text(ShowTreeView::NameCol).startsWith(number + QLatin1Char(' ')))
            return *it;
    return nullptr;
}

/* A cue row's letter columns, as the tree shows them: "S1 T3 R0 M-". */
QString cueCounts(ShowTreeView *view, const QString &number)
{
    QTreeWidgetItem *it = treeCue(view, number);
    if (it == nullptr)
        return QStringLiteral("no row ") + number;
    auto col = [it](int c) { return it->text(c).isEmpty() ? QStringLiteral("0") : it->text(c); };
    return QStringLiteral("S%1 T%2 R%3 M%4").arg(col(ShowTreeView::SetsCol), col(ShowTreeView::TracksCol),
                                                 col(ShowTreeView::ReleasesCol),
                                                 it->text(ShowTreeView::ModeCol).isEmpty()
                                                     ? QStringLiteral("-") : it->text(ShowTreeView::ModeCol));
}

/* Whether every channel of @a fixture is at 0. */
bool dark(Doc *doc, const QString &fixture)
{
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == fixture)
        {
            const QByteArray v = sends(doc, fixture, int(fx->channels()));
            return v.count(char(0)) == v.size();
        }
    return false;
}

/* Picks @a mode ("Block", "Cue-only"…) in the cue page's Mode box. */
bool setCueMode(ShowTreeView *view, const QString &mode)
{
    QComboBox *box = stFind<QComboBox>(view, "stCueMode");
    if (box == nullptr)
        return false;
    const int i = box->findText(mode);
    if (i < 0)
        return false;
    box->setCurrentIndex(i);
    emit box->activated(i);
    settle(50);
    return true;
}

/* The Show Tree laid out as shoot() lays it out, so a callout's place
   worked out before the shot is where it will be in it. */
void layOutForShot(ShowTreeView *v)
{
    if (QSplitter *split = stFind<QSplitter>(v, "stSplitter"))
        split->setSizes(QList<int>() << 260 << 520 << split->width() - 780);
    v->tree()->setColumnWidth(0, 260);
    v->tree()->setColumnWidth(ShowTreeView::InfoCol, 90);
    settle();
}

/* The tree's S, T, R and M cells of a cue's row, as a callout. */
HelpShots::Callout countsOf(ShowTreeView *v, QTreeWidgetItem *row, QWidget *grab, int n)
{
    QTreeWidget *t = v->tree();
    const QRect r = t->visualItemRect(row);
    const int x0 = t->columnViewportPosition(ShowTreeView::SetsCol);
    const int x1 = t->columnViewportPosition(ShowTreeView::ModeCol) + t->columnWidth(ShowTreeView::ModeCol);
    HelpShots::Callout c;
    c.number = n;
    c.rect = QRect(t->viewport()->mapTo(grab, QPoint(x0, r.top())), QSize(qMax(1, x1 - x0), r.height()));
    return c;
}

/* Right-click the Library row @a row and choose @a path (a menu, then its
   submenus' entries). Returns whether every step was found. */
bool libraryMenu(ShowTreeView *view, const QString &row, const QStringList &path)
{
    QTreeWidget *lib = view->library();
    QTreeWidgetItem *it = showLibraryRow(lib, row);
    if (it == nullptr)
        return false;
    bool chose = false;
    QTimer::singleShot(100, [&chose, path]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMenu *m = qobject_cast<QMenu *>(w))
                if (m->isVisible())
                {
                    QMenu *at = m;
                    for (int i = 0; i < path.count() && at != nullptr; ++i)
                    {
                        QAction *found = nullptr;
                        for (QAction *a : at->actions())
                            if (a->text() == path.at(i))
                                found = a;
                        if (found == nullptr)
                            break;
                        if (i == path.count() - 1)
                        {
                            chose = true;
                            m->close();
                            found->trigger();
                            return;
                        }
                        at = found->menu();
                    }
                    m->close();
                    return;
                }
    });
    emit lib->customContextMenuRequested(lib->visualItemRect(it).center());
    settle(100);
    return chose;
}

/* On a layer's page: the ✎ of the @a key row (the Look Editor). */
QToolButton *editButton(ShowTreeView *view, const QString &key)
{
    QLabel *row = stFind<QLabel>(view, QStringLiteral("stRow:%1").arg(key));
    if (row == nullptr)
        return nullptr;
    const int y = row->mapToGlobal(row->rect().center()).y();
    QWidget *host = row->parentWidget();
    while (host != nullptr && host->findChildren<QToolButton *>().isEmpty())
        host = host->parentWidget();
    if (host == nullptr)
        return nullptr;
    for (QToolButton *b : host->findChildren<QToolButton *>())
        if (b->text() == QStringLiteral("✎") && b->isVisible()
            && qAbs(b->mapToGlobal(b->rect().center()).y() - y) < b->height())
            return b;
    return nullptr;
}

/* The tree's row of layer @a name under cue @a number (its own, not grey). */
QTreeWidgetItem *treeLayer(ShowTreeView *view, const QString &number, const QString &name)
{
    QTreeWidgetItem *cue = treeCue(view, number);
    if (cue == nullptr)
        return nullptr;
    QList<QTreeWidgetItem *> todo;
    for (int i = 0; i < cue->childCount(); ++i)
        todo << cue->child(i);
    while (!todo.isEmpty())
    {
        QTreeWidgetItem *it = todo.takeFirst();
        if (it->text(ShowTreeView::NameCol) == name)
            return it;
        for (int i = 0; i < it->childCount(); ++i)
            todo << it->child(i);
    }
    return nullptr;
}

/* Right-click the tree's row @a item and choose @a path. */
bool treeMenu(ShowTreeView *view, QTreeWidgetItem *item, const QStringList &path)
{
    if (item == nullptr)
        return false;
    QTreeWidget *t = view->tree();
    t->scrollToItem(item);
    t->setCurrentItem(item);
    settle(50);
    bool chose = false;
    QStringList seen;
    QTimer::singleShot(100, [&chose, &seen, path]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMenu *m = qobject_cast<QMenu *>(w))
                if (m->isVisible())
                {
                    QMenu *at = m;
                    for (int i = 0; i < path.count() && at != nullptr; ++i)
                    {
                        QAction *found = nullptr;
                        for (QAction *a : at->actions())
                        {
                            if (i == 0)
                                seen << a->text();
                            if (a->text() == path.at(i))
                                found = a;
                        }
                        if (found == nullptr || !found->isEnabled())
                            break;
                        if (i == path.count() - 1)
                        {
                            chose = true;
                            m->close();
                            found->trigger();
                            return;
                        }
                        at = found->menu();
                    }
                    m->close();
                    return;
                }
    });
    emit t->customContextMenuRequested(t->visualItemRect(item).center());
    settle(100);
    if (!chose)
        qWarning().noquote() << "tree menu" << path << "not found in" << seen;
    return chose;
}

QString nextReceives(ShowTreeView *view)
{
    return view->m_cueNext != nullptr ? view->m_cueNext->text() : QString();
}
}

/* Lesson 2.1, tracking and Block: what tracks into the chorus (the grey
   rows, S/T/R, Tracked in, Next cue receives); the verse's step tops
   released; a cue-only flash before the scream. */
static bool lessonTwoOneByMouse(Doc *doc, ShowTreeView *view, const QString &shots = QString())
{
    ShowTreeController *st = doc->showTree();
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    auto rig = [view, &shots](const QString &name) {
        if (shots.isEmpty())
            return;
        const quint32 cue = view->currentCueId();
        rigShot(view, cue, shots + QLatin1Char('/') + name + QStringLiteral(".png"));
        closeRig();
        view->selectNode(cue);
        settle();
    };

    // Part A: read the chorus.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.2")), "select 1.0.2");
    settle(100);
    qInfo().noquote() << "[2.1] chorus counts" << cueCounts(view, "1.0.2") << "| rows" << view->cueLayerItems().join(" | ");
    qInfo().noquote() << "[2.1] chorus tracked in:" << view->m_cueTracked->text();
    qInfo().noquote() << "[2.1] chorus next receives:" << nextReceives(view);
    for (const QString &n : { "1.0.0", "1.0.1", "1.0.2", "1.0.3", "1.0.4" })
        qInfo().noquote() << "[2.1] counts" << n << cueCounts(view, n);
    LESSON_STEP(cueCounts(view, "1.0.2") == QStringLiteral("S1 T3 R0 M-"), cueCounts(view, "1.0.2"));
    LESSON_STEP(cueRow(view, "⤳ StepTops") != nullptr && cueRow(view, "⤳ AT-W-ALL") != nullptr
                && cueRow(view, "⤳ TrussC-RGB") != nullptr, view->cueLayerItems().join(" | "));
    LESSON_STEP(waitUntil([doc]() { return !dark(doc, "US Step 1 Top SR"); }), "step tops dark in the chorus");
    rig(QStringLiteral("rig-chorus-tracked"));
    // The Solo: Block, nothing tracks in.
    LESSON_STEP(cueCounts(view, "1.0.4") == QStringLiteral("S1 T0 R0 MB"), cueCounts(view, "1.0.4"));

    // Part B: the verse's step tops, released.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.1")), "select 1.0.1");
    settle(50);
    LESSON_STEP(clickCueRow(view, "StepTops"), "click StepTops");
    QCheckBox *release = stFind<QCheckBox>(view, "stLayerRelease");
    LESSON_STEP(release != nullptr && release->isVisible() && !release->isChecked(), "the Release box");
    qInfo().noquote() << "[2.1] release box:" << release->text();
    release->click();
    settle(100);
    qInfo().noquote() << "[2.1] after Release:" << hintOf(view)->text() << "| verse" << cueCounts(view, "1.0.1")
                      << "| chorus" << cueCounts(view, "1.0.2");
    LESSON_STEP(cueCounts(view, "1.0.1") == QStringLiteral("S1 T1 R1 M-"), cueCounts(view, "1.0.1"));
    LESSON_STEP(cueCounts(view, "1.0.2") == QStringLiteral("S1 T2 R0 M-"), cueCounts(view, "1.0.2"));
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.1")), "select 1.0.1");
    settle(50);
    qInfo().noquote() << "[2.1] verse next receives:" << nextReceives(view);
    LESSON_STEP(nextReceives(view).contains(QStringLiteral("StepTops — released, dark")), nextReceives(view));
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.2")), "select 1.0.2");
    settle(50);
    LESSON_STEP(cueRow(view, "⤳ StepTops") == nullptr, view->cueLayerItems().join(" | "));
    LESSON_STEP(waitUntil([doc]() { return dark(doc, "US Step 1 Top SR"); }), "step tops still lit in the chorus");
    rig(QStringLiteral("rig-chorus-released"));

    // Part C: a flash between the chorus and the scream, cue-only.
    LESSON_STEP(newCueNamed(view, "Flash"), "new cue Flash");
    LESSON_STEP(st->cue(view->currentCueId())->number == "1.0.2.5", st->cue(view->currentCueId())->number);
    {
        const QString said = dropGroupOnZone(doc, view, "StepTops");
        LESSON_STEP(said == QStringLiteral("a new layer on StepTops in 1.0.2.5 Flash"), said);
    }
    for (const QString &pal : { QStringLiteral("White"), QStringLiteral("Full") })
    {
        const QString said = dropPaletteOnRow(doc, view, "StepTops", pal);
        LESSON_STEP(said.startsWith(QStringLiteral("%1 → layer StepTops in 1.0.2.5 Flash").arg(pal)), said);
    }
    // The step tops lit (white: their white LEDs at full).
    LESSON_STEP(waitUntil([doc]() { return !dark(doc, "US Step 1 Top SR"); }), "the step tops are dark");
    qInfo().noquote() << "[2.1] flash step top" << sends(doc, "US Step 1 Top SR", 8).toHex(' ');
    LESSON_STEP(setCueMode(view, "Cue-only"), "Mode Cue-only");
    LESSON_STEP(st->cue(view->currentCueId())->mode == CueMode::CueOnly, "not cue-only");
    LESSON_STEP(cueCounts(view, "1.0.2.5").endsWith(QStringLiteral("MC")), cueCounts(view, "1.0.2.5"));
    qInfo().noquote() << "[2.1] flash next receives:" << nextReceives(view) << "| hint" << hintOf(view)->text();
    LESSON_STEP(nextReceives(view).startsWith(QStringLiteral("what this cue started from (it is cue-only)")),
                nextReceives(view));
    rig(QStringLiteral("rig-flash"));
    // The scream doesn't get the white: it starts from the chorus.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.3")), "select 1.0.3");
    settle(100);
    qInfo().noquote() << "[2.1] scream rows" << view->cueLayerItems().join(" | ") << "| counts" << cueCounts(view, "1.0.3");
    LESSON_STEP(cueRow(view, "⤳ StepTops") == nullptr, view->cueLayerItems().join(" | "));
    LESSON_STEP(cueCounts(view, "1.0.3") == QStringLiteral("S2 T3 R0 M-"), cueCounts(view, "1.0.3"));
    LESSON_STEP(waitUntil([doc]() { return dark(doc, "US Step 1 Top SR")
                                           && uchar(sends(doc, "SR Truss Wash 1").at(0)) == 255; }),
                QString::fromLatin1(sends(doc, "US Step 1 Top SR", 6).toHex(' ')));
    return true;
}

/* Lesson 2.2, palettes: the song's own colour. Magenta duplicated and
   named "Chorus Pink" in the Look Editor, filed in a folder for the song,
   dropped on the chorus's steps and its cyc; then the director's "less
   pink": the palette changed once, in the Look Editor, and every cue that
   shows it follows. */
static bool lessonTwoTwoByMouse(Doc *doc, ShowTreeView *view, const QString &shots = QString())
{
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    auto rig = [view, &shots](const QString &name) {
        if (shots.isEmpty())
            return;
        const quint32 cue = view->currentCueId();
        rigShot(view, cue, shots + QLatin1Char('/') + name + QStringLiteral(".png"));
        closeRig();
        view->selectNode(cue);
        settle();
    };
    PaletteOps *ops = doc->paletteOps();
    // The Show Tree as a student sees it, with numbered callouts.
    App *app = qobject_cast<App *>(view->window());
    auto ui = [view, app, &shots](const QString &name, const QList<HelpShots::Callout> &marks) {
        if (shots.isEmpty() || app == nullptr)
            return true;
        HelpShots::Frame f = showTreeFrame(app, view);
        f.callouts = marks;
        return shoot(f, shots, name);
    };

    // Part A: who uses Magenta (Library → right-click → Used by).
    QLCPalette *magenta = paletteNamed(doc, "Magenta");
    LESSON_STEP(magenta != nullptr, "no Magenta");
    {
        QStringList uses;
        for (const PaletteOps::Use &u : ops->usage(magenta->id()))
            uses << u.label;
        qInfo().noquote() << "[2.2] Magenta used by:" << uses.join(" | ");
        LESSON_STEP(uses.count() == 1 && uses.first().contains(QStringLiteral("1.0.2")), uses.join(" | "));
        // The menu has it (the click on a use goes there).
        LESSON_STEP(libraryMenu(view, "Magenta", { QStringLiteral("Used by"), uses.first() }), "Used by menu");
        qInfo().noquote() << "[2.2] after Used by click: kind" << int(view->currentKind()) << "cue"
                          << (doc->showTree()->cue(view->currentCueId()) ? doc->showTree()->cue(view->currentCueId())->number : "-");
    }

    // Part B: the song's own colour. Duplicate Magenta: the copy opens in
    // the Look Editor; its name typed there.
    const QList<quint32> before = [doc]() { QList<quint32> l; for (QLCPalette *p : doc->palettes()) l << p->id(); return l; }();
    LESSON_STEP(libraryMenu(view, "Magenta", { QStringLiteral("Duplicate") }), "Duplicate");
    settle(100);
    QLCPalette *copy = nullptr;
    for (QLCPalette *p : doc->palettes())
        if (!before.contains(p->id()))
            copy = p;
    LESSON_STEP(copy != nullptr, "no copy");
    qInfo().noquote() << "[2.2] the copy:" << copy->name() << "| path" << copy->path();
    QLineEdit *name = view->findChild<QLineEdit *>(QStringLiteral("lookName"));
    LESSON_STEP(name != nullptr && name->isVisible(), "the Look Editor's Name");
    qInfo().noquote() << "[2.2] Look Editor name:" << name->text()
                      << "| used by:" << view->findChild<QLabel *>(QStringLiteral("lookEditorUsedBy"))->text();
    LESSON_STEP(ui(QStringLiteral("duplicate"), { mark(name, 1),
                   mark(view->findChild<QLabel *>(QStringLiteral("lookEditorUsedBy")), 2) }), "duplicate shot");
    name->setFocus();
    name->selectAll();
    QTest::keyClicks(name, QStringLiteral("Chorus Colour"));
    QTest::keyClick(name, Qt::Key_Return);
    settle(100);
    LESSON_STEP(copy->name() == QStringLiteral("Chorus Colour"), copy->name());
    // Into a folder of the song's own: Move to folder → New folder….
    answerNextInputDialog(QStringLiteral("01-Everybody Scream"));
    LESSON_STEP(libraryMenu(view, "Chorus Colour", { QStringLiteral("Move to folder"), QStringLiteral("New folder…") }),
                "Move to folder");
    settle(100);
    qInfo().noquote() << "[2.2] Chorus Colour path:" << copy->path();
    LESSON_STEP(copy->path().contains(QStringLiteral("01-Everybody Scream")), copy->path());
    {
        QTreeWidgetItem *row = libraryRow(view->library(), "Chorus Colour");
        QStringList where;
        for (QTreeWidgetItem *p = row; p != nullptr; p = p->parent())
            where.prepend(p->text(0));
        qInfo().noquote() << "[2.2] in the Library:" << where.join(" → ");
        LESSON_STEP(where.contains(QStringLiteral("01-Everybody Scream")), where.join(" → "));
        if (!shots.isEmpty())
        {
            QTreeWidget *lib = view->library();
            QTreeWidgetItem *shown = showLibraryRow(lib, "Chorus Colour");
            LESSON_STEP(ui(QStringLiteral("song-folder"),
                           { markRect(lib->viewport(), lib->visualItemRect(shown->parent()) | lib->visualItemRect(shown),
                                      app, 1) }), "folder shot");
        }
    }

    // Part C: on the chorus: the steps, and the cyc (a grey row: it
    // becomes the chorus's own).
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.2")), "select 1.0.2");
    settle(100);
    {
        const QString said = dropPaletteOnRow(doc, view, "Steps", "Chorus Colour");
        qInfo().noquote() << "[2.2] Chorus Colour on Steps:" << said;
        LESSON_STEP(said.startsWith(QStringLiteral("Chorus Colour → layer Steps in 1.0.2 Chorus")), said);
    }
    {
        const QString said = dropPaletteOnRow(doc, view, "⤳ TrussC-RGB", "Chorus Colour");
        qInfo().noquote() << "[2.2] Chorus Colour on the grey cyc:" << said << "| rows" << view->cueLayerItems().join(" | ");
        LESSON_STEP(said.startsWith(QStringLiteral("Chorus Colour → ")), said);
    }
    settle(200);
    LESSON_STEP(cueRow(view, "TrussC-RGB") != nullptr, view->cueLayerItems().join(" | "));
    LESSON_STEP(waitUntil([doc]() { return sends(doc, "SR Truss Cyc 1") == QByteArray("\xff\x00\xff", 3)
                                           && sends(doc, "US Step 1") == QByteArray("\xff\x00\xff", 3); }),
                QString::fromLatin1(sends(doc, "SR Truss Cyc 1").toHex(' ')));
    {
        QStringList uses;
        for (const PaletteOps::Use &u : ops->usage(copy->id()))
            uses << u.label;
        qInfo().noquote() << "[2.2] Chorus Colour used by:" << uses.join(" | ");
    }
    if (!shots.isEmpty())
    {
        layOutForShot(view);
        QListWidget *rows = view->cueLayerList();
        LESSON_STEP(ui(QStringLiteral("chorus-colour"),
                       { markRect(rows->viewport(), rows->visualItemRect(cueRow(view, "Steps"))
                                                    | rows->visualItemRect(cueRow(view, "TrussC-RGB")), app, 1) }),
                    "chorus shot");
    }
    rig(QStringLiteral("rig-chorus-pink"));

    // Part D: "less pink". The Steps' ✎ opens Chorus Colour in the Look
    // Editor; a colour picked there changes it everywhere.
    LESSON_STEP(clickCueRow(view, "Steps"), "click Steps");
    QToolButton *edit = editButton(view, KeyColour);
    LESSON_STEP(edit != nullptr, "no ✎ on Colour");
    edit->click();
    settle(100);
    QLabel *used = view->findChild<QLabel *>(QStringLiteral("lookEditorUsedBy"));
    LESSON_STEP(used != nullptr && used->isVisible(), "no Used by line");
    qInfo().noquote() << "[2.2] Look Editor:" << view->findChild<QLineEdit *>(QStringLiteral("lookName"))->text()
                      << "| used by:" << used->text();
    QColorDialog *colour = view->findChild<QColorDialog *>(QStringLiteral("lookColor"));
    LESSON_STEP(colour != nullptr && colour->isVisible(), "no colour picker");
    {
        // Basic colors: the violet, third row, second column -- a click on
        // the swatch grid (Qt's QColorWell, 8 columns of 6).
        QWidget *well = nullptr;
        for (QWidget *w : colour->findChildren<QWidget *>())
            if (QLatin1String(w->metaObject()->className()) == QLatin1String("QColorWell") && well == nullptr)
                well = w;
        LESSON_STEP(well != nullptr, "no Basic colors grid");
        const QPoint cell(well->width() * 3 / 16, well->height() * 5 / 12);     // column 2, row 3
        QTest::mouseClick(well, Qt::LeftButton, Qt::NoModifier, cell);
        settle(200);
        qInfo().noquote() << "[2.2] swatch picked:" << colour->currentColor().name();
        LESSON_STEP(colour->currentColor() == QColor(0xaa, 0x00, 0xff), colour->currentColor().name());
    }
    qInfo().noquote() << "[2.2] after the pick: name" << copy->name() << "| value" << copy->values()
                      << "| Look Editor name" << view->findChild<QLineEdit *>(QStringLiteral("lookName"))->text()
                      << "| hint" << hintOf(view)->text();
    LESSON_STEP(waitUntil([doc]() { return sends(doc, "US Step 1") == QByteArray("\xaa\x00\xff", 3)
                                           && sends(doc, "SR Truss Cyc 1") == QByteArray("\xaa\x00\xff", 3); }),
                QString::fromLatin1(sends(doc, "US Step 1").toHex(' ')));
    LESSON_STEP(hintOf(view)->text() == QStringLiteral("Changed: 1.0.2 Chorus, 1.0.2.5 Flash, 1.0.3 Scream"),
                hintOf(view)->text());
    LESSON_STEP(ui(QStringLiteral("look-editor"), { mark(used, 1), mark(colour, 2), mark(hintOf(view), 3) }),
                "Look Editor shot");
    // Magenta itself is as it was.
    LESSON_STEP(magenta->name() == QStringLiteral("Magenta"), magenta->name());
    rig(QStringLiteral("rig-chorus-violet"));
    // The scream, tracking the cyc from the chorus, shows it too.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.3")), "select 1.0.3");
    LESSON_STEP(waitUntil([doc]() { return sends(doc, "SR Truss Cyc 1") == QByteArray("\xaa\x00\xff", 3); }),
                QString::fromLatin1(sends(doc, "SR Truss Cyc 1").toHex(' ')));
    // ↩ Back closes the Look Editor.
    if (QToolButton *back = view->findChild<QToolButton *>(QStringLiteral("lookBack")))
        back->click();
    else if (QPushButton *back = view->findChild<QPushButton *>(QStringLiteral("lookBack")))
        back->click();
    settle(100);
    qInfo().noquote() << "[2.2] describe:" << describe(doc).join(" || ");
    return true;
}

/* Lesson 2.3, parts of a group: the bridge, call and response. A Block
   cue 1.0.5 Bridge; a group set "Truss Sides" (Trusses SR + Trusses SL),
   one layer, Blue; split, SR in Amber; the Steps dim with a child layer
   "US in Steps" picked out in White. */
static bool lessonTwoThreeByMouse(Doc *doc, ShowTreeView *view, const QString &shots = QString())
{
    ShowTreeController *st = doc->showTree();
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    App *app = qobject_cast<App *>(view->window());
    auto ui = [view, app, &shots](const QString &name, const QList<HelpShots::Callout> &marks) {
        if (shots.isEmpty() || app == nullptr)
            return true;
        HelpShots::Frame f = showTreeFrame(app, view);
        f.callouts = marks;
        return shoot(f, shots, name);
    };
    auto rig = [view, &shots](const QString &name) {
        if (shots.isEmpty())
            return;
        const quint32 cue = view->currentCueId();
        rigShot(view, cue, shots + QLatin1Char('/') + name + QStringLiteral(".png"));
        closeRig();
        view->selectNode(cue);
        settle();
    };

    // The bridge: after the Solo, a Block cue.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.4")), "select 1.0.4");
    settle(50);
    LESSON_STEP(newCueNamed(view, "Bridge"), "new cue Bridge");
    LESSON_STEP(st->cue(view->currentCueId())->number == "1.0.5", st->cue(view->currentCueId())->number);
    LESSON_STEP(setCueMode(view, "Block"), "Mode Block");
    const quint32 bridge = view->currentCueId();

    // Part A: a group set, Truss Sides = Trusses SR + Trusses SL.
    {
        QTreeWidget *lib = view->library();
        QTreeWidgetItem *sr = showLibraryRow(lib, "Trusses SR");
        QTreeWidgetItem *sl = libraryRow(lib, "Trusses SL");
        LESSON_STEP(sr != nullptr && sl != nullptr, "no Trusses SR / SL");
        sr->setSelected(true);
        sl->setSelected(true);
        answerNextInputDialog(QStringLiteral("Truss Sides"));
        // Right-click one of them (the selection stays).
        bool chose = false;
        QTimer::singleShot(100, [&chose]() {
            for (QWidget *w : QApplication::topLevelWidgets())
                if (QMenu *m = qobject_cast<QMenu *>(w))
                    if (m->isVisible())
                        for (QAction *a : m->actions())
                            if (a->text() == QStringLiteral("Make set…"))
                            {
                                chose = true;
                                m->close();
                                a->trigger();
                                return;
                            }
        });
        emit lib->customContextMenuRequested(lib->visualItemRect(sl).center());
        settle(200);
        LESSON_STEP(chose, "Make set…");
        QTreeWidgetItem *set = libraryRow(lib, "Truss Sides");
        qInfo().noquote() << "[2.3] the set in the Library:" << (set ? set->text(0) : QString("none"));
        LESSON_STEP(set != nullptr, "no Truss Sides in the Library");
        if (!shots.isEmpty())
        {
            QTreeWidgetItem *shown = showLibraryRow(lib, "Truss Sides");
            LESSON_STEP(ui(QStringLiteral("group-set"),
                           { markRect(lib->viewport(), lib->visualItemRect(shown), app, 1) }), "set shot");
        }
    }
    quint32 setId = ShowTree::invalidId();
    for (const ShowTree::GroupSet &gs : st->groupSets())
        if (gs.name == QStringLiteral("Truss Sides"))
            setId = gs.id;
    LESSON_STEP(setId != ShowTree::invalidId(), "no set");
    LESSON_STEP(view->selectNode(bridge), "select the bridge");
    settle(50);
    {
        const QString said = dropOnZone(view, idsMime(ShowTreeView::setMimeType(), { setId }));
        qInfo().noquote() << "[2.3] drop the set:" << said << "| rows" << view->cueLayerItems().join(" | ");
        LESSON_STEP(said.startsWith(QStringLiteral("one layer on Truss Sides in 1.0.5 Bridge")), said);
    }
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("Full") })
    {
        const QString said = dropPaletteOnRow(doc, view, "Truss Sides", pal);
        LESSON_STEP(said.startsWith(QStringLiteral("%1 → layer Truss Sides in 1.0.5 Bridge").arg(pal)), said);
    }
    LESSON_STEP(waitUntil([doc]() { return sends(doc, "SR Truss Wash 1") == QByteArray("\x00\x00\xff", 3)
                                           && sends(doc, "SL Truss Wash 1") == QByteArray("\x00\x00\xff", 3); }),
                QString::fromLatin1(sends(doc, "SR Truss Wash 1").toHex(' ') + " / " + sends(doc, "SL Truss Wash 1").toHex(' ')));
    rig(QStringLiteral("rig-bridge-sides"));

    // Part B: call and response -- the SR side apart. A set stays one
    // layer (it can't be split: the set is what the layer is), so SR gets
    // a child layer of its own: right-click → Add child layer → Trusses SR.
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    // Split is there, greyed: the set is one layer.
    LESSON_STEP(!treeMenu(view, treeLayer(view, "1.0.5", "Truss Sides"), { QStringLiteral("Split layer (one per group)") }),
                "Split isn't greyed on a set's layer");
    LESSON_STEP(treeMenu(view, treeLayer(view, "1.0.5", "Truss Sides"),
                         { QStringLiteral("Add child layer"), QStringLiteral("Trusses SR") }), "Add child layer → Trusses SR");
    settle(100);
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    LESSON_STEP(view->selectNode(bridge), "select the bridge");
    settle(50);
    qInfo().noquote() << "[2.3] after the SR child:" << view->cueLayerItems().join(" | ") << "| hint" << hintOf(view)->text();
    {
        QTreeWidgetItem *child = treeLayer(view, "1.0.5", "Trusses SR");
        LESSON_STEP(child != nullptr && child->parent() == treeLayer(view, "1.0.5", "Truss Sides"),
                    "Trusses SR isn't under Truss Sides");
    }
    {
        const QString said = dropPaletteOnRow(doc, view, "Trusses SR", "Amber");
        qInfo().noquote() << "[2.3] Amber on the child:" << said;
        LESSON_STEP(said.startsWith(QStringLiteral("Amber → layer Trusses SR in 1.0.5 Bridge")), said);
    }
    LESSON_STEP(waitUntil([doc]() { return sends(doc, "SR Truss Wash 1") == QByteArray("\xff\xb0\x00", 3)
                                           && sends(doc, "SL Truss Wash 1") == QByteArray("\x00\x00\xff", 3); }),
                QString::fromLatin1(sends(doc, "SR Truss Wash 1").toHex(' ') + " / " + sends(doc, "SL Truss Wash 1").toHex(' ')));
    rig(QStringLiteral("rig-bridge-split"));

    // Part C: the steps dim, the top row picked out -- a child layer.
    {
        const QString said = dropGroupOnZone(doc, view, "Steps");
        LESSON_STEP(said == QStringLiteral("a new layer on Steps in 1.0.5 Bridge"), said);
    }
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("25%") })
    {
        const QString said = dropPaletteOnRow(doc, view, "Steps", pal);
        LESSON_STEP(said.startsWith(QStringLiteral("%1 → layer Steps in 1.0.5 Bridge").arg(pal)), said);
    }
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    {
        // ⇧ held as US is dropped on the Steps row: a child layer.
        QListWidgetItem *row = cueRow(view, "Steps");
        LESSON_STEP(row != nullptr, view->cueLayerItems().join(" | "));
        QListWidget *rows = view->cueLayerList();
        const QString said = dragAndDrop(rows->viewport(), rows->visualItemRect(row).center(),
                                         idsMime(FixtureGroupSource::fixtureGroupMimeType(), { groupNamed(doc, "US")->id() }),
                                         hintOf(view), Qt::ShiftModifier);
        qInfo().noquote() << "[2.3] ⇧-drop US on Steps:" << said;
        LESSON_STEP(said == QStringLiteral("a child layer on US under Steps in 1.0.5 Bridge: it sets only what it sets"), said);
    }
    settle(100);
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    LESSON_STEP(view->selectNode(bridge), "select the bridge");
    settle(50);
    qInfo().noquote() << "[2.3] after the child:" << view->cueLayerItems().join(" | ");
    {
        QTreeWidgetItem *child = treeLayer(view, "1.0.5", "US");
        QTreeWidgetItem *parent = treeLayer(view, "1.0.5", "Steps");
        qInfo().noquote() << "[2.3] child row:" << (child ? child->text(0) : QString("none"))
                          << "| under" << (child && child->parent() ? child->parent()->text(0) : QString());
        LESSON_STEP(child != nullptr && child->parent() == parent, "the child isn't under Steps");
    }
    for (const QString &pal : { QStringLiteral("White"), QStringLiteral("Full") })
    {
        const QString said = dropPaletteOnRow(doc, view, "US", pal);
        qInfo().noquote() << "[2.3] on the child:" << said;
        LESSON_STEP(said.startsWith(QStringLiteral("%1 → layer US in 1.0.5 Bridge").arg(pal)), said);
    }
    LESSON_STEP(waitUntil([doc]() { return sends(doc, "US Step 1") == QByteArray("\xff\xff\xff", 3)
                                           && uchar(sends(doc, "CS Step 1").at(2)) > 40
                                           && uchar(sends(doc, "CS Step 1").at(2)) < 90
                                           && uchar(sends(doc, "CS Step 1").at(0)) == 0; }),
                QString::fromLatin1(sends(doc, "US Step 1").toHex(' ') + " / " + sends(doc, "CS Step 1").toHex(' ')));
    rig(QStringLiteral("rig-bridge"));
    qInfo().noquote() << "[2.3] describe:" << describe(doc).join(" || ");
    if (!shots.isEmpty())
    {
        for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
            (*it)->setExpanded(true);
        LESSON_STEP(view->selectNode(bridge), "select the bridge");
        settle(100);
        layOutForShot(view);
        QTreeWidget *t = view->tree();
        t->scrollToItem(treeLayer(view, "1.0.5", "US"));
        settle(50);
        LESSON_STEP(ui(QStringLiteral("bridge-layers"),
                       { markRect(t->viewport(), t->visualItemRect(treeLayer(view, "1.0.5", "Truss Sides")), app, 1),
                         markRect(t->viewport(), t->visualItemRect(treeLayer(view, "1.0.5", "Trusses SR")), app, 2),
                         markRect(t->viewport(), t->visualItemRect(treeLayer(view, "1.0.5", "US")), app, 3) }),
                    "bridge shot");
    }
    return true;
}

void CourseShow_Test::lessonTwoOneExtension()
{
    // Lesson 2.1's extension: a base cue. The Pose gets the step tops at a
    // dim blue and is ticked as the list's base cue: in the chorus the
    // released step tops go back to that blue, not dark.
    QVERIFY(open(levelTwoFinish(1)));
    Doc *doc = m_app->doc();
    ShowTreeView *view = showTree();
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.0")));
    settle(50);
    QCOMPARE(dropGroupOnZone(doc, view, "StepTops"), QStringLiteral("a new layer on StepTops in 1.0.0 Pose"));
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("10%") })
        QVERIFY(dropPaletteOnRow(doc, view, "StepTops", pal).startsWith(pal + " → layer StepTops in 1.0.0 Pose"));
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.0")));
    settle(50);
    QCheckBox *base = stFind<QCheckBox>(view, "stCueBase");
    QVERIFY(base != nullptr && base->isVisible() && !base->isChecked());
    qInfo().noquote() << "[2.1x] base box:" << base->text();
    base->click();
    settle(100);
    QVERIFY(doc->showTree()->productions().first().lists.first().baseCue == cueNumbered(doc, "1.0.0"));
    QTreeWidgetItem *pose = treeCue(view, "1.0.0");
    QVERIFY(pose != nullptr);
    QVERIFY2(pose->text(ShowTreeView::FlagsCol).contains(QStringLiteral("base")), qPrintable(pose->text(ShowTreeView::FlagsCol)));
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.1")));
    settle(50);
    qInfo().noquote() << "[2.1x] verse next receives:" << nextReceives(view);
    QVERIFY2(nextReceives(view).contains(QStringLiteral("StepTops (Blue, 10%) — back to the base")), qPrintable(nextReceives(view)));
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.2")));
    settle(50);
    qInfo().noquote() << "[2.1x] chorus rows:" << view->cueLayerItems().join(" | ");
    const auto dimBlue = [doc]() {
        const QByteArray v = sends(doc, "US Step 1 Top SR", 3);
        return uchar(v.at(0)) == 0 && uchar(v.at(2)) > 10 && uchar(v.at(2)) < 40;
    };
    QVERIFY2(waitUntil(dimBlue), qPrintable(sends(doc, "US Step 1 Top SR", 4).toHex(' ')));
    doc->resetModified();
}

void CourseShow_Test::lessonTwoTwoExtension()
{
    if (kLevelTwoLessons < 2)
        QSKIP("not written yet");
    // Lesson 2.2's extension: the Palettes heading's Group by type off --
    // the song's folder first, with its palettes of every type in it --
    // and on again.
    QVERIFY(open(levelTwoFinish(2)));
    ShowTreeView *view = showTree();
    auto where = [view](const QString &name) {
        QStringList w;
        for (QTreeWidgetItem *p = libraryRow(view->library(), name); p != nullptr; p = p->parent())
            w.prepend(p->text(0));
        return w.join(QStringLiteral(" → "));
    };
    qInfo().noquote() << "[2.2x] by type:" << where("Chorus Colour");
    QCOMPARE(where("Chorus Colour"), QStringLiteral("All → Palettes → Colour → 01-Everybody Scream → Chorus Colour"));
    QVERIFY(libraryMenu(view, "Palettes", { QStringLiteral("Group by type") }));
    settle(100);
    qInfo().noquote() << "[2.2x] folders first:" << where("Chorus Colour") << "| Magenta:" << where("Magenta");
    QCOMPARE(where("Chorus Colour"), QStringLiteral("All → Palettes → 01-Everybody Scream → Chorus Colour"));
    QVERIFY(libraryMenu(view, "Palettes", { QStringLiteral("Group by type") }));
    settle(100);
    QCOMPARE(where("Chorus Colour"), QStringLiteral("All → Palettes → Colour → 01-Everybody Scream → Chorus Colour"));

    // At home: Verse Colour, the same way, into the folder that's there.
    Doc *doc = m_app->doc();
    qInfo().noquote() << "[2.2x] folders:" << doc->paletteOps()->folders();
    QVERIFY(libraryMenu(view, "Lavender", { QStringLiteral("Duplicate") }));
    QLineEdit *name = view->findChild<QLineEdit *>(QStringLiteral("lookName"));
    QVERIFY(name != nullptr && name->isVisible());
    name->setFocus();
    name->selectAll();
    QTest::keyClicks(name, QStringLiteral("Verse Colour"));
    QTest::keyClick(name, Qt::Key_Return);
    settle(100);
    QVERIFY(paletteNamed(doc, "Verse Colour") != nullptr);
    QVERIFY(libraryMenu(view, "Verse Colour", { QStringLiteral("Move to folder"), QStringLiteral("01-Everybody Scream") }));
    QCOMPARE(where("Verse Colour"), QStringLiteral("All → Palettes → Colour → 01-Everybody Scream → Verse Colour"));
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.1")));
    settle(50);
    QVERIFY(dropPaletteOnRow(doc, view, "StepTops", "Verse Colour").startsWith("Verse Colour → layer StepTops in 1.0.1 Verse"));
    doc->resetModified();
}

void CourseShow_Test::lessonTwoThreeExtension()
{
    if (kLevelTwoLessons < 3)
        QSKIP("not written yet");
    // Lesson 2.3's extension: two groups dropped together are one layer;
    // Split makes a layer each (nothing on stage changes); More ▾ → Merge
    // puts them back.
    QVERIFY(open(levelTwoFinish(3)));
    Doc *doc = m_app->doc();
    ShowTreeView *view = showTree();
    const quint32 bridge = cueNumbered(doc, "1.0.5");
    QVERIFY(view->selectNode(bridge));
    settle(50);
    const QString said = dropOnZone(view, idsMime(FixtureGroupSource::fixtureGroupMimeType(),
                                                  { groupNamed(doc, "CS")->id(), groupNamed(doc, "DS")->id() }));
    qInfo().noquote() << "[2.3x] CS + DS:" << said << "|" << view->cueLayerItems().join(" | ");
    QVERIFY2(said.startsWith(QStringLiteral("one layer on CS + DS in 1.0.5 Bridge")), qPrintable(said));
    QVERIFY(dropPaletteOnRow(doc, view, "CS + DS", "Red").startsWith(QStringLiteral("Red → layer CS + DS in 1.0.5 Bridge")));
    QTRY_VERIFY_WITH_TIMEOUT(uchar(sends(doc, "DS Step 1").at(0)) > 0 && uchar(sends(doc, "DS Step 1").at(2)) == 0, kRigWait);
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    QVERIFY(treeMenu(view, treeLayer(view, "1.0.5", "CS + DS"), { QStringLiteral("Split layer (one per group)") }));
    QVERIFY(view->selectNode(bridge));
    settle(50);
    qInfo().noquote() << "[2.3x] after Split:" << view->cueLayerItems().join(" | ");
    QVERIFY(cueRow(view, "CS ") != nullptr && cueRow(view, "DS ") != nullptr);
    QVERIFY(uchar(sends(doc, "DS Step 1").at(0)) > 0);
    // Both selected in the tree (⌘-click), More ▾ → Merge.
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    QTreeWidgetItem *cs = treeLayer(view, "1.0.5", "CS");
    QTreeWidgetItem *ds = treeLayer(view, "1.0.5", "DS");
    QVERIFY(cs != nullptr && ds != nullptr);
    QTreeWidget *t = view->tree();
    QTest::mouseClick(t->viewport(), Qt::LeftButton, Qt::NoModifier, t->visualItemRect(cs).center());
    QTest::mouseClick(t->viewport(), Qt::LeftButton, Qt::ControlModifier, t->visualItemRect(ds).center());
    settle(100);
    QVERIFY(view->m_merge->isEnabled());
    view->m_merge->click();
    settle(100);
    QVERIFY(view->selectNode(bridge));
    settle(50);
    qInfo().noquote() << "[2.3x] after Merge:" << view->cueLayerItems().join(" | ");
    QVERIFY(cueRow(view, "CS + DS") != nullptr || cueRow(view, "DS + CS") != nullptr);

    // At home: 1.0.6 Response, the mirror: Truss Sides with SL in Amber,
    // the Steps dim with DS picked out.
    QVERIFY(view->selectNode(bridge));
    settle(50);
    QVERIFY(newCueNamed(view, "Response"));
    QCOMPARE(doc->showTree()->cue(view->currentCueId())->number, QStringLiteral("1.0.6"));
    QVERIFY(setCueMode(view, "Block"));
    quint32 setId = ShowTree::invalidId();
    for (const ShowTree::GroupSet &gs : doc->showTree()->groupSets())
        if (gs.name == QStringLiteral("Truss Sides"))
            setId = gs.id;
    QVERIFY(dropOnZone(view, idsMime(ShowTreeView::setMimeType(), { setId })).startsWith("one layer on Truss Sides in 1.0.6"));
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("Full") })
        QVERIFY(dropPaletteOnRow(doc, view, "Truss Sides", pal).startsWith(pal + " → layer Truss Sides in 1.0.6"));
    for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
        (*it)->setExpanded(true);
    QVERIFY(treeMenu(view, treeLayer(view, "1.0.6", "Truss Sides"), { QStringLiteral("Add child layer"), QStringLiteral("Trusses SL") }));
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.6")));
    settle(50);
    QVERIFY(dropPaletteOnRow(doc, view, "Trusses SL", "Amber").startsWith("Amber → layer Trusses SL in 1.0.6"));
    QVERIFY(dropGroupOnZone(doc, view, "Steps") == QStringLiteral("a new layer on Steps in 1.0.6 Response"));
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("25%") })
        QVERIFY(dropPaletteOnRow(doc, view, "Steps", pal).startsWith(pal + " → layer Steps in 1.0.6"));
    {
        QListWidget *rows = view->cueLayerList();
        const QString said = dragAndDrop(rows->viewport(), rows->visualItemRect(cueRow(view, "Steps")).center(),
                                         idsMime(FixtureGroupSource::fixtureGroupMimeType(), { groupNamed(doc, "DS")->id() }),
                                         hintOf(view), Qt::ShiftModifier);
        QCOMPARE(said, QStringLiteral("a child layer on DS under Steps in 1.0.6 Response: it sets only what it sets"));
    }
    for (const QString &pal : { QStringLiteral("White"), QStringLiteral("Full") })
        QVERIFY(dropPaletteOnRow(doc, view, "DS", pal).startsWith(pal + " → layer DS in 1.0.6"));
    QTRY_VERIFY_WITH_TIMEOUT(sends(doc, "SL Truss Wash 1") == QByteArray("\xff\xb0\x00", 3)
                             && sends(doc, "SR Truss Wash 1") == QByteArray("\x00\x00\xff", 3)
                             && sends(doc, "DS Step 1") == QByteArray("\xff\xff\xff", 3), kRigWait);
    doc->resetModified();
}

void CourseShow_Test::levelTwoWithTheMouse()
{
    struct L { int n; bool (*byMouse)(Doc *, ShowTreeView *, const QString &); };
    const QList<L> lessons = {
        { 1, lessonTwoOneByMouse },
        { 2, lessonTwoTwoByMouse },
        { 3, lessonTwoThreeByMouse },
    };
    // Lesson 2.1 starts where Level 1 finished.
    QVERIFY(open(kL4Finish));
    QStringList before = describeAll(m_app->doc());
    for (const L &l : lessons)
    {
        if (l.n > kLevelTwoLessons)
            break;
        QVERIFY2(open(levelTwoStart(l.n)), qPrintable(levelTwoStart(l.n)));
        QCOMPARE(describeAll(m_app->doc()), before);
        QVERIFY2(l.byMouse(m_app->doc(), showTree(), QString()), qPrintable(levelTwoStart(l.n)));
        const QStringList built = describeAll(m_app->doc());
        qInfo().noquote() << "[2." + QString::number(l.n) + "] built:" << built.join(" || ");
        m_app->doc()->resetModified();
        QVERIFY(open(levelTwoFinish(l.n)));
        QCOMPARE(describeAll(m_app->doc()), built);
        before = built;
    }
}

void CourseShow_Test::buildLevelTwoShows()
{
    if (qEnvironmentVariableIsEmpty("COURSE_BUILD"))
        QSKIP("set COURSE_BUILD=1 (and COURSE_LESSON=2.N for one lesson) to make Level 2's show files");
    struct L { int n; bool (*byMouse)(Doc *, ShowTreeView *, const QString &); };
    const QList<L> lessons = {
        { 1, lessonTwoOneByMouse },
        { 2, lessonTwoTwoByMouse },
        { 3, lessonTwoThreeByMouse },
    };
    const QString only = qEnvironmentVariable("COURSE_LESSON");
    for (const L &l : lessons)
    {
        if (!only.isEmpty() && only != QStringLiteral("2.%1").arg(l.n))
            continue;
        if (only.isEmpty() && l.n > kLevelTwoLessons)
            break;
        const QString from = l.n == 1 ? kL4Finish : levelTwoFinish(l.n - 1);
        QVERIFY2(QFile::exists(from), qPrintable(from));
        QVERIFY(QFile::remove(levelTwoStart(l.n)) || !QFile::exists(levelTwoStart(l.n)));
        QVERIFY(QFile::copy(from, levelTwoStart(l.n)));
        QVERIFY(open(levelTwoStart(l.n)));
        QVERIFY(l.byMouse(m_app->doc(), showTree(), QString()));
        QCOMPARE(m_app->saveXML(levelTwoFinish(l.n)), QFile::NoError);
        m_app->doc()->resetModified();
    }
}

void CourseShow_Test::levelTwoPictures()
{
    const QString root = qEnvironmentVariable("COURSE_SHOTS");
    if (root.isEmpty())
        QSKIP("set COURSE_SHOTS to the course's images folder");
    m_app->setTheme(App::ThemeBlue);
    m_app->resize(1280, 800);
    const QString only = qEnvironmentVariable("COURSE_LESSON");
    auto wanted = [&only](int n) { return only.isEmpty() ? n <= kLevelTwoLessons : only == QStringLiteral("2.%1").arg(n); };

    // --- 2.1: done with the mouse (the Rig Overview at each step), then the
    //     chorus's tracking, the Release box and the flash's page.
    if (wanted(1))
    {
        const QString d = root + QStringLiteral("/lesson-2-1");
        QVERIFY(open(levelTwoStart(1)));
        ShowTreeView *v = showTree();
        {
            // Before: what tracks into the chorus.
            Doc *dc = m_app->doc();
            for (QTreeWidgetItemIterator it(v->tree()); *it; ++it)
                (*it)->setExpanded(true);
            QVERIFY(v->selectNode(cueNumbered(dc, "1.0.2")));
            settle(200);
            layOutForShot(v);
            QListWidget *rows = v->cueLayerList();
            QRect grey;
            for (int i = 0; i < rows->count(); ++i)
                if (rows->item(i)->text().startsWith(QStringLiteral("⤳")))
                    grey |= rows->visualItemRect(rows->item(i));
            QTreeWidgetItem *chorus = treeCue(v, "1.0.2");
            QVERIFY(chorus != nullptr);
            HelpShots::Frame f = showTreeFrame(m_app, v);
            const HelpShots::Callout c = countsOf(v, chorus, m_app, 1);
            f.callouts << c << markRect(rows->viewport(), grey, m_app, 2);
            QVERIFY(shoot(f, d, "chorus-tracking"));
            // Scrolled down: Tracked in and Next cue receives.
            if (QScrollArea *page = qobject_cast<QScrollArea *>(v->m_cuePage))
                page->ensureWidgetVisible(v->m_cueNext, 0, 8);
            settle(100);
            HelpShots::Frame g;
            g.grab = m_app;
            QRect r(v->mapTo(m_app, QPoint(0, 0)), v->size());
            g.crop = r;
            g.callouts << mark(v->m_cueTracked, 1) << mark(v->m_cueNext, 2);
            QVERIFY(shoot(g, d, "chorus-receives"));
            dc->resetModified();
        }
        QVERIFY(open(levelTwoStart(1)));
        QVERIFY(lessonTwoOneByMouse(m_app->doc(), showTree(), d));
        m_app->doc()->resetModified();

        QVERIFY(open(levelTwoFinish(1)));
        Doc *dc = m_app->doc();
        v = showTree();
        for (QTreeWidgetItemIterator it(v->tree()); *it; ++it)
            (*it)->setExpanded(true);
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.1")));
        settle(100);
        QVERIFY(clickCueRow(v, "StepTops"));
        settle(100);
        layOutForShot(v);
        {
            QTreeWidgetItem *verse = treeCue(v, "1.0.1");
            QVERIFY(verse != nullptr);
            HelpShots::Frame f = showTreeFrame(m_app, v);
            f.callouts << mark(stFind<QCheckBox>(v, "stLayerRelease"), 1) << countsOf(v, verse, m_app, 2);
            QVERIFY(shoot(f, d, "release"));
        }
        QVERIFY(v->selectNode(cueNumbered(dc, "1.0.2.5")));
        settle(200);
        layOutForShot(v);
        {
            QTreeWidgetItem *flash = treeCue(v, "1.0.2.5");
            QVERIFY(flash != nullptr);
            HelpShots::Frame f = showTreeFrame(m_app, v);
            // The Mode box is near the top of the page: scrolled back to it.
            if (QScrollArea *page = qobject_cast<QScrollArea *>(v->m_cuePage))
                page->ensureWidgetVisible(stFind<QComboBox>(v, "stCueMode"), 0, 60);
            settle(100);
            f.callouts << mark(stFind<QComboBox>(v, "stCueMode"), 1) << countsOf(v, flash, m_app, 2);
            QVERIFY(shoot(f, d, "flash-cue"));
        }
        dc->resetModified();
    }

    // --- 2.2: done with the mouse, the pictures at its steps.
    if (wanted(2))
    {
        const QString d = root + QStringLiteral("/lesson-2-2");
        QVERIFY(open(levelTwoStart(2)));
        QVERIFY(lessonTwoTwoByMouse(m_app->doc(), showTree(), d));
        m_app->doc()->resetModified();
    }

    // --- 2.3
    if (wanted(3))
    {
        const QString d = root + QStringLiteral("/lesson-2-3");
        QVERIFY(open(levelTwoStart(3)));
        QVERIFY(lessonTwoThreeByMouse(m_app->doc(), showTree(), d));
        m_app->doc()->resetModified();
    }
}

int main(int argc, char **argv)
{
    // A home of its own: the definitions a student's computer doesn't have
    // must come from the show itself, and nothing here touches yours.
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
#ifdef Q_OS_WIN
    qputenv("USERPROFILE", home.path().toUtf8());
#endif
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    CourseShow_Test test;
    return QTest::qExec(&test, argc, argv);
}
