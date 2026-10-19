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
#include <QMimeData>
#include <QTabBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QDialog>
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
const QString kL1Start = kShows + QStringLiteral("/lesson-01-start.qlcc");
const QString kL1Finish = kShows + QStringLiteral("/lesson-01-finish.qlcc");

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
    for (const QString &path : { kClassroom, kL1Start, kL1Finish })
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
    const QString dir = qEnvironmentVariable("COURSE_SHOTS");
    if (dir.isEmpty())
        QSKIP("set COURSE_SHOTS to the folder for the pictures");
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
