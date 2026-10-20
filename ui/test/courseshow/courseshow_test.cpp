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

#include <QTreeWidgetItemIterator>
#include <QApplication>
#include <QTemporaryDir>
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
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QColorDialog>
#include <QComboBox>
#include <QFileInfo>
#include <QLabel>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QtTest>

#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#include "fixturemanager.h"
#include "showtreeview.h"
#undef protected
#undef private
#include "structurestudioview.h"
#include "fixturegroupsource.h"
#include "functionstreewidget.h"
#include "showtreecontroller.h"
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
#include "helpshots.h"
#include "showstatus.h"
#include "universe.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "doc.h"

#include "courseshow_test.h"

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
QString dragAndDrop(QWidget *w, const QPoint &at, QMimeData *mime, QLabel *hint)
{
    QDragEnterEvent enter(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &enter);
    QDragMoveEvent move(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &move);
    const QString said = hint != nullptr ? hint->text() : QString();
    QDropEvent drop(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
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

/* Answers the next QInputDialog asking for a whole number (Level…). */
void answerNextIntDialog(int value)
{
    QTimer *t = new QTimer();
    t->setInterval(20);
    QObject::connect(t, &QTimer::timeout, [t, value]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QInputDialog *d = qobject_cast<QInputDialog *>(w))
                if (d->isVisible())
                {
                    d->setIntValue(value);
                    d->accept();
                    t->stop();
                    t->deleteLater();
                    return;
                }
    });
    t->start();
}

QLabel *hintOf(ShowTreeView *view)
{
    return view->findChild<QLabel *>("stDropHint");
}

/* + Cue, then its name typed in the Name box and Return. */
bool newCueNamed(ShowTreeView *view, const QString &name)
{
    view->findChild<QToolButton *>("stNewCue")->click();
    settle();
    QLineEdit *box = view->findChild<QLineEdit *>("stCueName");
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
    QWidget *page = view->findChild<QWidget *>("stCuePage");
    QLabel *zone = view->findChild<QLabel *>("stCueDropZone");
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
    QLabel *l = view->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(key));
    return l == nullptr ? QString() : l->text();
}

/* On a layer's page: the ⋯ button of the @a key row. */
QToolButton *moreButton(ShowTreeView *view, const QString &key)
{
    QLabel *row = view->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(key));
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
    QLabel *hint = view->findChild<QLabel *>("stDropHint");
    QWidget *page = view->findChild<QWidget *>("stCuePage");
    QLabel *zone = view->findChild<QLabel *>("stCueDropZone");
    QLineEdit *name = view->findChild<QLineEdit *>("stCueName");
    if (hint == nullptr || page == nullptr || zone == nullptr || name == nullptr)
        { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

    // Step 1: + Production, "Spring Show".
    answerNextInputDialog(QStringLiteral("Spring Show"));
    view->findChild<QToolButton *>("stNewProduction")->click();
    settle();
    if (st->productions().count() != 1 || st->productions().first().name != "Spring Show"
        || view->currentKind() != ShowTreeView::ProductionNode)
        { qWarning() << "lesson 1 by mouse: step failed at line" << __LINE__ << describe(doc); return false; }

    // Step 2: + Cue list, "01-Everybody Scream" -- Tracking.
    answerNextInputDialog(QStringLiteral("01-Everybody Scream"));
    view->findChild<QToolButton *>("stNewList")->click();
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
        view->findChild<QToolButton *>("stNewCue")->click();
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
    for (const QString &path : { kClassroom, kL1Start, kL1Finish, kL2Start, kL2Finish, kL3Start, kL3Finish,
                                 kL4Start, kL4Finish, kL5Start })
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
    QLabel *colour = view->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyColour));
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
    settle(500);
    QCOMPARE(sends(doc, "SR Truss Cyc 1"), QByteArray("\xff\x00\xff", 3));
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
        settle(400);
        const QString name = g == "TrussC-RGB" ? "SR Truss Cyc 1" : g == "StepTops" ? "US Step 1 Top SR"
                             : g == "Steps" ? "US Step 1" : "SR AT Wash 1";
        Fixture *fx = nullptr;
        for (Fixture *f : m_app->doc()->fixtures())
            if (f->name() == name)
                fx = f;
        QVERIFY(fx != nullptr);
        const QByteArray v = sends(m_app->doc(), name, int(fx->channels()));
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
        settle(600);
        QVERIFY2(run->nowText().contains(e.cue), qPrintable(run->nowText()));
        const QByteArray v = sends(doc, e.fixture);
        if (e.rgb.isEmpty())
            QVERIFY2(v.count(char(0)) < v.size(), qPrintable(e.fixture + " " + v.toHex(' ')));
        else
            QVERIFY2(v == e.rgb, qPrintable(e.fixture + " " + v.toHex(' ')));
    }
    // Tracking: in the chorus the cyc is still blue (from 1.0.0).
    QCOMPARE(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3));
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
    // View (Top, Front, Side, 45°) is the first; Room the second.
    if (combos.count() >= 2)
    {
        combos.at(0)->setCurrentIndex(plane >= 0 ? plane : 3);
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
                combos.at(0)->setCurrentIndex(3);
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
    answerNextIntDialog(40);
    LESSON_STEP(rowMenu(view, KeyDimmer, "Level…"), "Level…");
    settle(50);
    qInfo().noquote() << "[L2] after Level:" << hint->text() << "|" << rowText(view, KeyDimmer);
    LESSON_STEP(rowText(view, KeyDimmer).startsWith("40%"), rowText(view, KeyDimmer));

    // Step 5: Make reusable: 40% joins the palettes.
    QLabel *reuse = view->findChild<QLabel *>(QStringLiteral("stReuse:%1").arg(KeyDimmer));
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
        view->findChild<QToolButton *>("stUndo")->click();
        settle(50);
    }
    {
        const QStringList d = describe(doc);
        LESSON_STEP(d.value(2) == QStringLiteral("1.0.1 Verse: StepTops [Colour Warm White, Dimmer 75%]"), d.value(2));
    }
    for (int i = 0; i < 3; ++i)
    {
        view->findChild<QToolButton *>("stRedo")->click();
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
    QLabel *looks = view->findChild<QLabel *>("stCueLooks");
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
    LESSON_STEP(panTilt(doc, "SR AT Wash 1") == qMakePair(128, 32), "bare wash pan/tilt");
    shot(QStringLiteral("rig-verse-front"));
    {
        const QString said = dropTargetOnRow(doc, view, "AT-W-ALL", "Center");
        qInfo().noquote() << "[L3] drop Center:" << said << "| after:" << hint->text();
        LESSON_STEP(said.startsWith(QStringLiteral("Aim at Center → layer AT-W-ALL in 1.0.1 Verse")), said);
    }
    settle(300);
    qInfo() << "[L3] aimed AT wash pan/tilt" << panTilt(doc, "SR AT Wash 1") << panTilt(doc, "SL AT Wash 1");
    LESSON_STEP(panTilt(doc, "SR AT Wash 1") != qMakePair(128, 32), "the wash didn't move");
    shot(QStringLiteral("rig-verse-center"));

    // The solo: after the scream.
    LESSON_STEP(view->selectNode(cueNumbered(doc, "1.0.3")), "select 1.0.3");
    settle(50);
    LESSON_STEP(newCueNamed(view, "Solo"), "new cue Solo");
    LESSON_STEP(st->cue(view->currentCueId())->number == "1.0.4", st->cue(view->currentCueId())->number);
    // Mode: Block -- the solo starts from a dark stage.
    {
        QComboBox *mode = view->findChild<QComboBox *>("stCueMode");
        LESSON_STEP(mode != nullptr, "Mode box");
        const int block = mode->findText(QStringLiteral("Block"), Qt::MatchStartsWith);
        LESSON_STEP(block >= 0, "no Block in Mode");
        mode->setCurrentIndex(block);
        emit mode->activated(block);
        settle(50);
        qInfo().noquote() << "[L3] mode items" << mode->itemText(0) << mode->itemText(1) << mode->itemText(2)
                          << "| after Block:" << hint->text() << "| rows:" << view->cueLayerItems().join(" | ");
        LESSON_STEP(st->cue(view->currentCueId())->mode == CueMode::Block, "not Block");
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
    // White at Full, and still dark: the shutter is closed.
    LESSON_STEP(channelValue(doc, "SR AT Spot", "Master Dimmer") == 255, "spot dimmer");
    LESSON_STEP(channelValue(doc, "SR AT Spot", "Strobe/Shutter") < 8, "spot shutter already open");
    shot(QStringLiteral("rig-solo-closed"));
    {
        const QString said = dropPaletteOnRow(doc, view, "AT-Spots", "Open");
        qInfo().noquote() << "[L3] drop Open:" << said;
        LESSON_STEP(said.startsWith(QStringLiteral("Open → layer AT-Spots in 1.0.4 Solo")), said);
    }
    settle(300);
    qInfo() << "[L3] open AT spot pan/tilt" << panTilt(doc, "SR AT Spot") << panTilt(doc, "SL AT Spot")
            << "shutter" << channelValue(doc, "SR AT Spot", "Strobe/Shutter");
    LESSON_STEP(channelValue(doc, "SR AT Spot", "Strobe/Shutter") >= 8, "spot shutter still closed");
    LESSON_STEP(panTilt(doc, "SR AT Spot") == qMakePair(0, 0), "the spot has a position already");
    shot(QStringLiteral("rig-solo-floor"));
    {
        const QString said = dropTargetOnRow(doc, view, "AT-Spots", "Soloist DS");
        qInfo().noquote() << "[L3] drop Soloist DS:" << said;
        LESSON_STEP(said.startsWith(QStringLiteral("Aim at Soloist DS → layer AT-Spots in 1.0.4 Solo")), said);
    }
    settle(300);
    qInfo() << "[L3] aimed AT spot pan/tilt" << panTilt(doc, "SR AT Spot") << panTilt(doc, "SL AT Spot");
    LESSON_STEP(panTilt(doc, "SR AT Spot") != qMakePair(0, 0), "the spot didn't move");
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
        QTest::qWait(500);
        qInfo().noquote() << "[L4] Pose clicked, 0.5 s:" << sends(doc, "SR Truss Cyc 1").toHex(' ');
        QCOMPARE(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3));
        QVERIFY(view->selectNode(doc->showTree()->productions().first().id));
        settle(300);
    }
    CueRunner *run = runTab(m_app);
    QVERIFY(run != nullptr);
    QStringList timing;
    for (QTreeWidgetItemIterator it(run->cueTree()); *it; ++it)
        timing << (*it)->text(1) + " " + (*it)->text(3);
    qInfo().noquote() << "[L4] Run tab timing:" << timing.join(" | ");

    // GO: the Pose fades in over 3 s -- part way after 1 s, all there by 3.5.
    press(run, "crGo");
    QTest::qWait(1200);
    const uchar mid = uchar(sends(doc, "SR Truss Cyc 1").at(2));
    qInfo() << "[L4] cyc blue after 1.2 s" << mid;
    QVERIFY2(mid > 20 && mid < 235, qPrintable(QString::number(mid)));
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3), 4000);

    // GO, GO: Verse (2 s), Chorus (a snap).
    press(run, "crGo");
    QTest::qWait(2600);
    press(run, "crGo");
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "US Step 1"), QByteArray("\xff\x00\xff", 3), 2000);

    // GO: the Scream snaps in, holds 2 s, then the Solo comes by itself.
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.3"), 1500);
    qInfo().noquote() << "[L4] scream now:" << run->nowText() << "| next:" << run->nextText();
    QTRY_VERIFY_WITH_TIMEOUT(uchar(sends(doc, "SR Truss Wash 1").at(0)) > 200, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.4"), 4000);
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
    qInfo().noquote() << "[L5] mode:" << run->modeText() << "| header:" << run->headerText()
                      << "| next:" << run->nextText();
    QVERIFY2(run->nextText().contains("1.0.0"), qPrintable(run->nextText()));

    // "GO" twice: Pose, Verse.
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.0"), 2000);
    QTest::qWait(3300);
    press(run, "crGo");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.1"), 2000);
    QTest::qWait(2300);
    // Back: the Pose again.
    press(run, "crBack");
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.0"), 2000);
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
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.2"), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "US Step 1"), QByteArray("\xff\x00\xff", 3), 2000);

    // Double-click the Solo: jump there now.
    QTreeWidgetItem *solo = runRow(run, "1.0.4");
    QVERIFY(solo != nullptr);
    // A double-click, as the mouse sends it: a click, then the second press.
    const QPoint at = run->cueTree()->visualItemRect(solo).center();
    QTest::mouseClick(run->cueTree()->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QTest::mouseDClick(run->cueTree()->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    settle(50);
    QTRY_VERIFY_WITH_TIMEOUT(run->nowText().contains("1.0.4"), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(channelValue(doc, "SR AT Spot", "Master Dimmer"), 255, 6000);
    qInfo().noquote() << "[L5] after the jump:" << run->nowText() << "| next:" << run->nextText();

    // Stop: the stage goes dark.
    press(run, "crStop");
    QTRY_COMPARE_WITH_TIMEOUT(channelValue(doc, "SR AT Spot", "Master Dimmer"), 0, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(sends(doc, "US Step 1"), QByteArray("\x00\x00\x00", 3), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(run->nextText().contains("1.0.0"), 2000);
    qInfo().noquote() << "[L5] after Stop:" << run->nowText() << "| next:" << run->nextText();
    doc->resetModified();
}

/* Each cue's layers and cue looks, with timing: what a lesson makes. */
static QStringList describeAll(Doc *doc)
{
    QStringList out = describe(doc) + cueLooks(doc);
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                out << QStringLiteral("%1 mode %2 in %3 out %4 hold %5").arg(c.number).arg(int(c.mode))
                       .arg(c.fadeIn).arg(c.fadeOut).arg(c.hold);
    return out;
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

void CourseShow_Test::lessonZeroTwoTheVirtualStage()
{
    // Lesson 0.2: the finished Lesson 1.1, the Rig Overview, its views and
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
    QCOMPARE(views, QStringList({ "Top", "Front", "Side", "45°" }));
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
    for (const QString &cue : { "1.0.0", "1.0.1", "1.0.2" })
    {
        press(run, "crGo");
        settle(400);
        QVERIFY2(run->nowText().contains(cue), qPrintable(run->nowText()));
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
    bool answered = false;
    QTimer *t = new QTimer();
    t->setInterval(20);
    QObject::connect(t, &QTimer::timeout, [t, &answered]() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QColorDialog *d = qobject_cast<QColorDialog *>(w))
                if (d->isVisible())
                {
                    d->setCurrentColor(QColor(0x6a, 0x0d, 0xad));
                    d->accept();
                    answered = true;
                    t->stop();
                    t->deleteLater();
                    return;
                }
    });
    t->start();
    QVERIFY(rowMenu(view, KeyColour, "Colour…"));
    settle(100);
    QVERIFY(answered);
    qInfo().noquote() << "[L2] picked colour row:" << rowText(view, KeyColour);
    QVERIFY2(rowText(view, KeyColour).startsWith("#6a0dad"), qPrintable(rowText(view, KeyColour)));
    QVERIFY(view->findChild<QLabel *>(QStringLiteral("stReuse:%1").arg(KeyColour)) != nullptr);

    // The puzzle: a cue look on the Pose reaches none of its layers (its
    // one layer sets its own colour).
    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.0")));
    settle(50);
    const QString said = dropPaletteOnZone(doc, view, "Green");
    qInfo().noquote() << "[L2] Green on the Pose:" << said;
    QCOMPARE(said, QStringLiteral("Green → cue look on 1.0.0 Pose: reaches 0 of 1 layers"));
    QCOMPARE(sends(doc, "SR Truss Cyc 1"), QByteArray("\x00\x00\xff", 3));
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
    QVERIFY(panTilt(doc, "SR AT Spot") != soloist);
    view->findChild<QToolButton *>("stUndo")->click();
    settle(300);
    QCOMPARE(panTilt(doc, "SR AT Spot"), soloist);

    QVERIFY(view->selectNode(cueNumbered(doc, "1.0.4")));
    settle(50);
    QCOMPARE(dropGroupOnZone(doc, view, "TrussW-RGB"), QStringLiteral("a new layer on TrussW-RGB in 1.0.4 Solo"));
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("25%") })
        QVERIFY(dropPaletteOnRow(doc, view, "TrussW-RGB", pal).startsWith(pal + " → layer TrussW-RGB in 1.0.4 Solo"));
    settle(300);
    QVERIFY2(cueRow(view, "TrussW-RGB") != nullptr && cueRow(view, "TrussW-RGB")->text().contains("Blue, 25%"),
             qPrintable(view->cueLayerItems().join(" | ")));
    const QByteArray wash = sends(doc, "SR Truss Wash 1");
    QVERIFY2(uchar(wash.at(0)) == 0 && uchar(wash.at(2)) > 40 && uchar(wash.at(2)) < 90, qPrintable(wash.toHex(' ')));
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
        if (QSplitter *split = v->findChild<QSplitter *>("stSplitter"))
        {
            const int w = split->width();
            split->setSizes(QList<int>() << 300 << 420 << w - 720);
        }
        v->tree()->setColumnWidth(0, 250);
    }
    settle();
    for (const HelpShots::Callout &c : f.callouts)
        if (c.widget.isNull() ? c.rect.isEmpty() : !c.widget->isVisible())
        {
            qWarning() << name << "callout" << c.number << "has nothing on show";
            return false;
        }
    return save(HelpShots::annotate(f.grab->grab().toImage(), f), dir, name);
}
}

void CourseShow_Test::lessonPictures()
{
    // COURSE_SHOTS is docs/course/images: each lesson's pictures go in a
    // folder of its own there (lesson-1-1, lesson-0-2, ...).
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
    QLineEdit *name = view->findChild<QLineEdit *>("stCueName");
    name->setText(QStringLiteral("Pose"));
    QTest::keyClick(name, Qt::Key_Return);
    settle(100);
    {
        HelpShots::Frame f;
        f.grab = view;
        f.callouts << mark(view->m_newCue, 1) << mark(name, 2)
                   << mark(view->findChild<QLabel *>("stCueDropZone"), 3);
        QTreeWidgetItem *cueRow = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->text(0).contains(QLatin1String("Pose")))
                cueRow = *it;
        QVERIFY(cueRow != nullptr);
        f.callouts << markRect(tree->viewport(), tree->visualItemRect(cueRow), view, 4);
        QVERIFY(shoot(f, dir, "new-cue"));
    }

    // Drag TrussC-RGB onto the dashed area.
    QLabel *hint = view->findChild<QLabel *>("stDropHint");
    QWidget *page = view->findChild<QWidget *>("stCuePage");
    QLabel *zone = view->findChild<QLabel *>("stCueDropZone");
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
        HelpShots::Frame f;
        f.grab = view;
        cyc = showLibraryRow(view->library(), "TrussC-RGB");
        f.callouts << markRect(view->library()->viewport(), view->library()->visualItemRect(cyc), view, 1)
                   << markRect(rows->viewport(), rowRect("TrussC-RGB"), view, 2);
        QVERIFY(shoot(f, dir, "drop-group"));
    }
    // Blue and Full onto the row.
    for (const QString &pal : { QStringLiteral("Blue"), QStringLiteral("Full") })
        dragAndDrop(rows->viewport(), rowRect("TrussC-RGB").center(),
                    idsMime(FunctionsTreeWidget::paletteDragMimeType(), { paletteNamed(doc, pal)->id() }), hint);
    settle(100);
    {
        HelpShots::Frame f;
        f.grab = view;
        QTreeWidgetItem *blue = showLibraryRow(view->library(), "Blue");
        QRect r = view->library()->visualItemRect(blue);
        f.callouts << markRect(view->library()->viewport(), r, view, 1)
                   << markRect(rows->viewport(), rowRect("TrussC-RGB"), view, 2)
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
        HelpShots::Frame f;
        f.grab = view;
        f.callouts << markRect(rows->viewport(), rowRect("⤳ TrussC-RGB"), view, 1)
                   << markRect(rows->viewport(), rowRect("StepTops"), view, 2);
        QTreeWidgetItem *grey = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->text(0).contains(QStringLiteral("⤳")) && (*it)->parent()
                && (*it)->parent()->text(0).contains(QLatin1String("1.0.1")))
                grey = *it;
        if (grey != nullptr)
            f.callouts << markRect(tree->viewport(), tree->visualItemRect(grey), view, 3);
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

    // --- 0.2: the stage from the top, the front and at 45°, work light on;
    //     and the Rig Overview's own controls.
    {
        const QString d = root + QStringLiteral("/lesson-0-2");
        QVERIFY(open(kL1Finish));
        ShowTreeView *v = showTree();
        const quint32 prod = m_app->doc()->showTree()->productions().first().id;
        QVERIFY(rigShot(v, prod, d + "/stage-top.png", 0, 0));
        QVERIFY(rigShot(v, prod, d + "/stage-front.png", 1, 0));
        QVERIFY(rigShot(v, prod, d + "/stage-45.png", 3, 0));
        QVERIFY(rigShot(v, cueNumbered(m_app->doc(), "1.0.2"), d + "/stage-chorus.png", 3, 1));
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
            HelpShots::Frame f;
            f.grab = v;
            f.callouts << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyColour)), 1)
                       << mark(moreButton(v, KeyColour), 2)
                       << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyDimmer)), 3)
                       << mark(moreButton(v, KeyDimmer), 4);
            QVERIFY(shoot(f, d, "layer-page"));
        }
        QVERIFY(rowMenu(v, KeyColour, "Set to", "Lavender"));
        answerNextIntDialog(40);
        QVERIFY(rowMenu(v, KeyDimmer, "Level…"));
        settle(100);
        {
            HelpShots::Frame f;
            f.grab = v;
            f.callouts << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyColour)), 1)
                       << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyDimmer)), 2)
                       << mark(v->findChild<QLabel *>(QStringLiteral("stReuse:%1").arg(KeyDimmer)), 3)
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
            HelpShots::Frame f;
            f.grab = v;
            f.callouts << mark(v->findChild<QLabel *>("stCueLooks"), 1)
                       << markRect(rows->viewport(), own, v, 2);
            QVERIFY(shoot(f, d, "cue-looks"));
        }
        dc->resetModified();
    }

    // --- 1.3: done with the mouse, a picture at each "you should see";
    //     then the aim row, the Solo's page and its shutter.
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
            HelpShots::Frame f;
            f.grab = v;
            f.callouts << markRect(v->library()->viewport(), v->library()->visualItemRect(center), v, 1)
                       << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyPosition)), 2);
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
            HelpShots::Frame f;
            f.grab = v;
            f.callouts << mark(v->findChild<QComboBox *>("stCueMode"), 1)
                       << markRect(rows->viewport(), rows->visualItemRect(cueRow(v, "AT-Spots")), v, 2)
                       << markRect(v->tree()->viewport(), v->tree()->visualItemRect(solo), v, 3);
            QVERIFY(shoot(f, d, "solo-cue"));
        }
        QVERIFY(clickCueRow(v, "AT-Spots"));
        settle(100);
        {
            HelpShots::Frame f;
            f.grab = v;
            f.callouts << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyDimmer)), 1)
                       << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyPosition)), 2)
                       << mark(v->findChild<QLabel *>(QStringLiteral("stRow:%1").arg(KeyShutter)), 3);
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
            HelpShots::Frame f;
            f.grab = v;
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
        settle(300);
        dc->resetModified();
    }

    // Last: it types a production of its own, which nothing after it
    // should see.
    // --- 0.1: the four qualities of light, typed on the classroom show
    //     (not saved): each a Block cue, so it shows only itself.
    {
        const QString d = root + QStringLiteral("/lesson-0-1");
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
