/*
  Q Light Controller Plus - Test Unit
  workbenchchaser_test.cpp

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
#include <QTreeWidgetItemIterator>
#include <QXmlStreamReader>
#include <QTemporaryDir>
#include <QToolButton>
#include <QPushButton>
#include <QDataStream>
#include <QDropEvent>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QMimeData>
#include <QSplitter>
#include <QAction>
#include <QLabel>
#include <QFile>

#define protected public
#define private public
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "functionstreewidget.h"
#include "chaserwalker.h"
#include "chasereditor.h"
#include "workbench.h"
#include "workbenchstack.h"
#include "mastertimer.h"
#include "sceneeditor.h"
#include "functionselection.h"
#include "functionwizard.h"
#undef private
#undef protected

#include "qlcfixturedefcache.h"
#include "showembedding.h"
#include "noteedit.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "chaserstep.h"
#include "paletteops.h"
#include "collection.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "efx.h"
#include "sequence.h"
#include "video.h"
#include "doc.h"
#include <QApplication>
#include <QMenu>

#include "workbenchchaser_test.h"

static quint32 s_address = 0;

void WorkbenchChaser_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
    s_address = 0;
}

void WorkbenchChaser_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

Scene *WorkbenchChaser_Test::scene(const QString &name, const QString &path)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    m_doc->addFunction(s);
    s->setPath(path);
    return s;
}

Collection *WorkbenchChaser_Test::collection(const QString &name, const QList<quint32> &members)
{
    Collection *c = new Collection(m_doc);
    c->setName(name);
    m_doc->addFunction(c);
    for (quint32 m : members)
        c->addFunction(m);
    return c;
}

FixtureGroup *WorkbenchChaser_Test::group(const QString &name, int count)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    g->setSize(QSize(count, 1));
    m_doc->addFixtureGroup(g);
    for (int i = 0; i < count; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("%1 %2").arg(name).arg(i + 1));
        f->setChannels(3);
        f->setAddress(s_address);
        s_address += 3;
        m_doc->addFixture(f);
        g->assignFixture(f->id(), QLCPoint(i, 0));
    }
    return g;
}

QList<int> WorkbenchChaser_Test::settle(int ticks, int channels)
{
    for (int i = 0; i < ticks; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    QList<int> out;
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (int i = 0; i < channels; ++i)
        out << uchar(ua.first()->preGMValues().at(i));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

bool WorkbenchChaser_Test::loadCopy(const QString &path, QTemporaryDir &dir)
{
    // Read a copy: the show itself is never touched.
    const QString copy = dir.filePath(QStringLiteral("show.") + QFileInfo(path).suffix());
    if (!QFile::copy(path, copy))
        return false;
    m_doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_doc->setWorkspacePath(dir.path());
    if (m_doc->showEmbedding()->readWorkspace(copy))
        m_doc->showEmbedding()->apply(dir.path());
    QFile file(copy);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QXmlStreamReader r(&file);
    while (!r.atEnd())
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            return m_doc->loadXML(r, false);
    return false;
}

static ChaserWalker *walkerOf(Workbench &wb)
{
    return qobject_cast<ChaserWalker *>(wb.m_stackWidget.data());
}

static FunctionStack *stackOf(Workbench &wb)
{
    return qobject_cast<FunctionStack *>(wb.stack());
}

static QList<quint32> layerIds(Workbench &wb)
{
    QList<quint32> ids;
    for (const WorkbenchStack::Layer &l : wb.stack()->layers())
        ids << l.functionId;
    return ids;
}

static QMimeData *rowsMime(const QList<QPair<int, quint32>> &rows)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    for (const QPair<int, quint32> &r : rows)
        out << qint32(r.first) << r.second << r.second;
    QMimeData *mime = new QMimeData;
    mime->setData(QString::fromLatin1(ProgrammingOutline::rowMimeType()), data);
    return mime;
}

static void select(QTreeWidget *t, QTreeWidgetItem *it, bool add = false)
{
    if (!add)
        t->clearSelection();
    t->setCurrentItem(it, 0, add ? QItemSelectionModel::Select : QItemSelectionModel::ClearAndSelect);
    it->setSelected(true);
}

/****************************************************************************/

void WorkbenchChaser_Test::walkingAChasersSteps()
{
    FixtureGroup *g = group("Washes", 2);
    Scene *a = scene("A");
    a->setValue(g->fixtureList().first(), 0, 100);
    Scene *b = scene("B");
    b->setValue(g->fixtureList().last(), 0, 50);
    Collection *c = collection("0.0.1-Startup", { a->id(), b->id() });
    EFX *efx = new EFX(m_doc);
    efx->setName("Sweep");
    m_doc->addFunction(efx);
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    ChaserStep s1(c->id());
    s1.note = "House out";
    ch->addStep(s1);
    ch->addStep(ChaserStep(a->id()));
    ch->addStep(ChaserStep(efx->id()));

    Workbench wb(m_doc);
    wb.resize(1300, 800);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    m_doc->masterTimer()->start();
    wb.load(ch->id());
    ChaserWalker *walker = walkerOf(wb);
    QVERIFY(walker != nullptr);
    FunctionStack *stack = stackOf(wb);

    // Step 1: the collection's members are the layers.
    QCOMPARE(stack->step(), 0);
    QCOMPARE(layerIds(wb), QList<quint32>({ a->id(), b->id() }));
    QCOMPARE(wb.rows()->layerItem(0)->data(0, ProgrammingOutline::KindRole).toInt(),
             int(ProgrammingOutline::SceneRow));
    QVERIFY(walker->m_where->text().contains("Step 1 of 3 · 0.0.1-Startup"));
    QCOMPARE(walker->m_note->text(), QString("House out"));
    QCOMPARE(walker->stepTable()->topLevelItemCount(), 3);
    QCOMPARE(walker->stepTable()->topLevelItem(0)->text(6), QString("House out"));
    QVERIFY(!walker->m_prev->isEnabled());
    QVERIFY(wb.m_addSceneBtn->isEnabled());           // a collection step: add layers
    QVERIFY(wb.m_info->text().contains("Step 1: 0.0.1-Startup"));
    // The step being worked on plays.
    QTRY_VERIFY(c->isRunning());
    QVERIFY(!ch->isRunning());

    // ▶: step 2, a scene, opened up; it plays instead.
    walker->m_next->click();
    QCOMPARE(stack->step(), 1);
    QCOMPARE(layerIds(wb), QList<quint32>({ a->id() }));
    QVERIFY(walker->m_where->text().contains("Step 2 of 3 · A"));
    QVERIFY(!wb.m_addSceneBtn->isEnabled());
    QTRY_VERIFY(!c->isRunning());
    QTRY_VERIFY(a->isRunning());

    // The step table: click step 3, an EFX: one ▣ row, its editor on a click.
    select(walker->stepTable(), walker->stepTable()->topLevelItem(2));
    QCOMPARE(stack->step(), 2);
    QCOMPARE(layerIds(wb), QList<quint32>({ efx->id() }));
    QCOMPARE(wb.rows()->layerItem(0)->data(0, ProgrammingOutline::KindRole).toInt(),
             int(ProgrammingOutline::FunctionRow));
    QVERIFY(!walker->m_next->isEnabled());
    select(wb.rows(), wb.rows()->layerItem(0));
    QCOMPARE(wb.currentPage(), Workbench::FunctionPage);

    // ◀ back to 2, then Open the step on its own; ↩ returns to step 2.
    walker->m_prev->click();
    QCOMPARE(stack->step(), 1);
    walker->m_openStep->click();
    QCOMPARE(wb.loadedFunction(), a->id());
    QVERIFY(walkerOf(wb) == nullptr);
    QVERIFY(wb.goBack());
    QCOMPARE(wb.loadedFunction(), ch->id());
    QCOMPARE(stackOf(wb)->step(), 1);
    QVERIFY(walkerOf(wb) != nullptr);
    QVERIFY(walkerOf(wb)->m_where->text().contains("Step 2 of 3"));

    wb.hide();
    QTRY_VERIFY(!a->isRunning());
    m_doc->masterTimer()->stop();
}

void WorkbenchChaser_Test::typedStepTimesGoToTheirStep()
{
    Scene *a = scene("A");
    Scene *b = scene("B");
    Scene *c = scene("C");
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Walk");
    m_doc->addFunction(ch);
    ch->setFadeInMode(Chaser::PerStep);
    ch->setFadeOutMode(Chaser::PerStep);
    ch->setDurationMode(Chaser::PerStep);
    ch->addStep(ChaserStep(a->id(), 0, 1000, 0));
    ch->addStep(ChaserStep(b->id(), 0, 1000, 0));
    ch->addStep(ChaserStep(c->id(), 0, 1000, 0));

    Workbench wb(m_doc);
    wb.resize(1300, 800);
    wb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wb));
    wb.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&wb));
    wb.load(ch->id());
    ChaserWalker *walker = walkerOf(wb);
    FunctionStack *stack = stackOf(wb);
    QVERIFY(walker != nullptr);
    auto typeIn = [](QLineEdit *e, const QString &text) {
        e->setFocus();
        QTRY_VERIFY(e->hasFocus());
        e->selectAll();
        QTest::keyClicks(e, text);
    };

    // Typed, no Return, then ▶ (a button: the field keeps the focus): the
    // time is step 1's; the field shows step 2's own.
    typeIn(walker->m_fadeIn, "2s");
    walker->m_next->click();
    QCOMPARE(stack->step(), 1);
    QCOMPARE(ch->steps().at(0).fadeIn, 2000u);
    QCOMPARE(ch->steps().at(1).fadeIn, 0u);
    QCOMPARE(walker->m_fadeIn->text(), Function::speedToString(0));
    // Typed, then step 3 picked in the table (the field still has the
    // keyboard): step 2's.
    typeIn(walker->m_fadeOut, "3s");
    select(walker->stepTable(), walker->stepTable()->topLevelItem(2));
    QCOMPARE(stack->step(), 2);
    QCOMPARE(ch->steps().at(1).fadeOut, 3000u);
    QCOMPARE(ch->steps().at(2).fadeOut, 0u);
    // Tab: applied, to the step shown.
    typeIn(walker->m_hold, "4s");
    QTest::keyClick(walker->m_hold, Qt::Key_Tab);
    QCOMPARE(ch->steps().at(2).hold, 4000u);
    // Esc puts it back: nothing applied, not when walking on either.
    typeIn(walker->m_fadeIn, "9s");
    QTest::keyClick(walker->m_fadeIn, Qt::Key_Escape);
    QCOMPARE(walker->m_fadeIn->text(), Function::speedToString(0));
    walker->m_prev->click();
    QCOMPARE(stack->step(), 1);
    QCOMPARE(ch->steps().at(2).fadeIn, 0u);
    QCOMPARE(ch->steps().at(1).fadeIn, 0u);
}

void WorkbenchChaser_Test::theStepHeaderEditsTheStep()
{
    Scene *a = scene("A");
    Scene *b = scene("B");
    Scene *x = scene("X");
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    ch->setFadeInMode(Chaser::PerStep);
    ch->setFadeOutMode(Chaser::PerStep);
    ch->setDurationMode(Chaser::PerStep);
    ch->addStep(ChaserStep(a->id(), 0, 1000, 0));
    ch->addStep(ChaserStep(b->id(), 0, Function::infiniteSpeed(), 0));
    PaletteOps *ops = m_doc->paletteOps();

    Workbench wb(m_doc);
    wb.load(ch->id());
    ChaserWalker *walker = walkerOf(wb);
    FunctionStack *stack = stackOf(wb);
    QVERIFY(walker->m_fadeIn->isEnabled());
    QCOMPARE(walker->m_hold->text(), Function::speedToString(1000));

    // Fade in: the step's, its duration fade in + hold, as the Chaser editor keeps it.
    walker->m_fadeIn->setText(Function::speedToString(1500));
    emit walker->m_fadeIn->editingFinished();
    QCOMPARE(ch->steps().at(0).fadeIn, 1500u);
    QCOMPARE(ch->steps().at(0).duration, 2500u);
    QVERIFY(ops->undoText().contains("fade in"));
    walker->m_note->setText("Listen for the cue");
    emit walker->m_note->editingFinished();
    walker->m_cue->setText(" 12.5 ");
    emit walker->m_cue->editingFinished();
    QCOMPARE(ch->steps().at(0).note, QString("Listen for the cue"));
    QCOMPARE(ch->steps().at(0).cueNumber, QString("12.5"));
    QCoreApplication::processEvents();
    QCOMPARE(walker->stepTable()->topLevelItem(0)->text(7), QString("12.5"));
    // ∞ for the hold of step 2.
    walker->m_next->click();
    QCOMPARE(walker->m_hold->text(), Function::speedToString(Function::infiniteSpeed()));
    walker->m_hold->setText("2s");
    emit walker->m_hold->editingFinished();
    QCOMPARE(ch->steps().at(1).hold, 2000u);

    // One step each, undone in turn, redone.
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(1).hold, Function::infiniteSpeed());
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(0).cueNumber, QString());
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(0).note, QString());
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(0).fadeIn, 0u);
    QVERIFY(ops->redo());
    QCOMPARE(ch->steps().at(0).fadeIn, 1500u);
    QCoreApplication::processEvents();
    QCOMPARE(walker->stepTable()->topLevelItem(0)->text(2), Function::speedToString(1500));

    // Steps added, moved, removed: the step walked to goes with them.
    QVERIFY(stack->insertSteps({ x->id() }, 1));
    QCOMPARE(ch->stepsCount(), 3);
    QCOMPARE(ch->steps().at(1).fid, x->id());
    stack->setStep(1);
    QCOMPARE(stack->step(), 1);
    QVERIFY(stack->moveStep(1, 2));
    QCOMPARE(ch->steps().at(2).fid, x->id());
    QCOMPARE(stack->step(), 2);
    QCOMPARE(layerIds(wb), QList<quint32>({ x->id() }));
    QVERIFY(stack->removeStep(2));
    QCOMPARE(ch->stepsCount(), 2);
    QCOMPARE(stack->step(), 1);
    QVERIFY(ops->undo());
    QCOMPARE(ch->stepsCount(), 3);

    // Common timing: shown, not edited per step.
    ch->setFadeInMode(Chaser::Common);
    walker->refresh();
    QVERIFY(!walker->m_fadeIn->isEnabled());
    QVERIFY(walker->m_fadeIn->toolTip().contains("same for every step"));
}

static void typeInto(QWidget *w, const QString &text)
{
    for (const QChar &c : text)
        QTest::keyClick(w, c.toLatin1());
}

/* The note cell's editor open in @a window (not one closed and going). */
static NoteEdit *cellEditor(QWidget *window)
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    for (NoteEdit *e : window->findChildren<NoteEdit *>("noteCellEditor"))
        if (e->isVisible())
            return e;
    return nullptr;
}

void WorkbenchChaser_Test::theNoteOpensUpWhileTyped()
{
    Scene *a = scene("A");
    Scene *b = scene("B");
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    ch->addStep(ChaserStep(a->id()));
    ch->addStep(ChaserStep(b->id()));
    PaletteOps *ops = m_doc->paletteOps();

    QWidget win;
    QVBoxLayout *lay = new QVBoxLayout(&win);
    Workbench *wb = new Workbench(m_doc, &win);
    lay->addWidget(wb);
    win.resize(900, 700);
    win.show();
    QVERIFY(QTest::qWaitForWindowExposed(&win));
    win.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&win));
    wb->load(ch->id());
    ChaserWalker *walker = walkerOf(*wb);
    NoteEdit *note = walker->m_note;
    QVERIFY(note->isEnabled());

    // Closed up: one line. Typed in: it grows, a line at a time, to six.
    const int closed = note->height();
    QVERIFY(!note->isExpanded());
    note->setFocus(Qt::MouseFocusReason);
    QVERIFY(note->isExpanded());
    const QString longNote = QString("Wait for the band to finish the bridge then go. ").repeated(4).trimmed();
    typeInto(note, longNote);
    QCoreApplication::processEvents();
    QVERIFY2(note->height() > closed * 2, qPrintable(QString("%1 vs %2").arg(note->height()).arg(closed)));
    QVERIFY(note->height() <= closed * 7);
    QCOMPARE(ch->steps().at(0).note, QString());        // not yet: Return applies it

    // Shift+Return: a new line, not a commit. Return: one step on the history.
    QTest::keyClick(note, Qt::Key_Return, Qt::ShiftModifier);
    typeInto(note, "Spot on");
    QCOMPARE(ch->steps().at(0).note, QString());
    QTest::keyClick(note, Qt::Key_Return);
    const QString typed = longNote + "\nSpot on";
    QCOMPARE(ch->steps().at(0).note, typed);
    QVERIFY(ops->undoText().contains("note"));
    QVERIFY(!note->hasFocus());
    QVERIFY(!note->isExpanded());
    QCOMPARE(note->height(), closed);
    QVERIFY(note->toolTip().contains("Spot on"));
    QCOMPARE(walker->stepTable()->topLevelItem(0)->text(6), typed);

    // Esc puts it back, and nothing is committed.
    note->setFocus(Qt::MouseFocusReason);
    typeInto(note, " later");
    QTest::keyClick(note, Qt::Key_Escape);
    QCOMPARE(note->text(), typed);
    QCOMPARE(ch->steps().at(0).note, typed);
    QVERIFY(ops->undoText().contains("note"));
    QVERIFY(!note->isExpanded());

    // Undo / redo as before: one step.
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(0).note, QString());
    QCoreApplication::processEvents();
    QCOMPARE(note->text(), QString());
    QVERIFY(ops->redo());
    QCOMPARE(ch->steps().at(0).note, typed);
    QCoreApplication::processEvents();

    // The step table: double-click the note of step 2 to type there.
    QTreeWidget *table = walker->stepTable();
    QTreeWidgetItem *row2 = table->topLevelItem(1);
    emit table->itemDoubleClicked(row2, 6);
    QCoreApplication::processEvents();
    NoteEdit *cell = cellEditor(&win);
    QVERIFY(cell != nullptr);
    cell->setFocus(Qt::MouseFocusReason);
    const int cellClosed = table->visualItemRect(row2).height();
    QVERIFY(cell->width() >= table->columnWidth(6));
    typeInto(cell, longNote);
    QCoreApplication::processEvents();
    QVERIFY(cell->height() > cellClosed * 2);
    const QString shots = qEnvironmentVariable("QLC_NOTE_SHOTS");
    if (!shots.isEmpty())
        QVERIFY(win.grab().save(shots + "/chaser-note-expanded.png"));
    QTest::keyClick(cell, Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(ch->steps().at(1).note, longNote);
    QCOMPARE(m_doc->paletteOps()->undoText().contains("step 2"), true);
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(1).note, QString());
    QCoreApplication::processEvents();

    // Esc in a cell: nothing changes.
    row2 = table->topLevelItem(1);
    emit table->itemDoubleClicked(row2, 6);
    QCoreApplication::processEvents();
    cell = cellEditor(&win);
    QVERIFY(cell != nullptr);
    cell->setFocus(Qt::MouseFocusReason);
    typeInto(cell, "nope");
    QTest::keyClick(cell, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(ch->steps().at(1).note, QString());
    QCOMPARE(row2->text(6), QString());

    // Closed up, a cell shows the note on one line, new lines as ⏎.
    QCOMPARE(NoteEdit::flattened("a\nb"), QString("a ⏎ b"));
    QVERIFY(NoteEdit::tipFor(typed).contains("Spot on"));

    // The Chaser editor's Notes column: the same editor.
    ChaserEditor ed(nullptr, ch, m_doc);
    ed.resize(900, 400);
    ed.show();
    QVERIFY(QTest::qWaitForWindowExposed(&ed));
    ed.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&ed));
    QTreeWidget *tree = ed.findChild<QTreeWidget *>("m_tree");
    QVERIFY(tree != nullptr);
    QVERIFY(qobject_cast<NoteDelegate *>(tree->itemDelegateForColumn(6)) != nullptr);
    tree->editItem(tree->topLevelItem(1), 6);
    QCoreApplication::processEvents();
    cell = cellEditor(&ed);
    QVERIFY(cell != nullptr);
    cell->setFocus(Qt::MouseFocusReason);
    typeInto(cell, "Haze up");
    QTest::keyClick(cell, Qt::Key_Return, Qt::AltModifier);
    typeInto(cell, "Strobe");
    QTest::keyClick(cell, Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(ch->steps().at(1).note, QString("Haze up\nStrobe"));
    tree->editItem(tree->topLevelItem(1), 6);
    QCoreApplication::processEvents();
    cell = cellEditor(&ed);
    QVERIFY(cell != nullptr);
    cell->setFocus(Qt::MouseFocusReason);
    typeInto(cell, " more");
    QTest::keyClick(cell, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(ch->steps().at(1).note, QString("Haze up\nStrobe"));
}

void WorkbenchChaser_Test::editingAStepsCollectionChangesTheChasersOutput()
{
    FixtureGroup *g = group("Washes", 2);
    const quint32 f1 = g->fixtureList().first();
    Scene *a = scene("A");
    a->setValue(f1, 0, 100);
    Scene *b = scene("B");                // not in the show's chaser yet
    b->setValue(f1, 1, 200);
    Collection *c = collection("0.0.1-Startup", { a->id() });
    Collection *c2 = collection("0.0.2-Preshow", { b->id() });
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    ch->setDurationMode(Chaser::PerStep);
    ch->addStep(ChaserStep(c->id(), 0, Function::infiniteSpeed(), 0));
    ch->addStep(ChaserStep(c2->id(), 0, Function::infiniteSpeed(), 0));
    PaletteOps *ops = m_doc->paletteOps();

    auto chaserOutput = [this, ch]() {
        ch->start(m_doc->masterTimer(), FunctionParent::master());
        const QList<int> out = settle(6, 3);
        ch->stop(FunctionParent::master());
        settle(4, 3);
        return out;
    };
    QCOMPARE(chaserOutput(), QList<int>({ 100, 0, 0 }));

    // In the Workbench: the chaser, step 1, + B as a layer of its collection.
    Workbench wb(m_doc);
    wb.load(ch->id());
    QVERIFY(wb.stack()->addPlan({ b->id() }).contains("0.0.1-Startup"));
    QVERIFY(wb.stack()->addLayers({ b->id() }, -1));
    QCOMPARE(c->functions(), QList<quint32>({ a->id(), b->id() }));
    QCOMPARE(ops->undoText(), QString("add B to 0.0.1-Startup"));
    // The chaser plays step 1 with it.
    QCOMPARE(chaserOutput(), QList<int>({ 100, 200, 0 }));

    // Removed from the layers: gone from the chaser's output too.
    QVERIFY(wb.stack()->removeLayers({ 0 }));
    QCOMPARE(chaserOutput(), QList<int>({ 0, 200, 0 }));

    // Undo, Undo: as it was; Redo: B back.
    QVERIFY(ops->undo());
    QVERIFY(ops->undo());
    QCOMPARE(c->functions(), QList<quint32>({ a->id() }));
    QCOMPARE(chaserOutput(), QList<int>({ 100, 0, 0 }));
    QVERIFY(ops->redo());
    QCOMPARE(chaserOutput(), QList<int>({ 100, 200, 0 }));
    QCoreApplication::processEvents();
    QCOMPARE(layerIds(wb), QList<quint32>({ a->id(), b->id() }));

    // + EFX on a step: made new, filed with the step's collection, one step.
    const int functions = m_doc->functions().size();
    const quint32 made = wb.stack()->addNew(Function::EFXType, -1, {}, FixtureGroup::invalidId());
    QVERIFY(made != Function::invalidId());
    QVERIFY(c->functions().contains(made));
    QCOMPARE(m_doc->functions().size(), functions + 1);
    QVERIFY(ops->undo());
    QCOMPARE(m_doc->function(made), nullptr);
    QCOMPARE(m_doc->functions().size(), functions);
}

void WorkbenchChaser_Test::chaserFromSelectionAndDropsOnAChaser()
{
    Scene *a = scene("01-01.01-Walk-in", "Act 1");
    Scene *b = scene("01-01.02-Reveal", "Act 1");
    Scene *c = scene("01-01.03-Blackout", "Act 1");
    PaletteOps *ops = m_doc->paletteOps();

    ProgrammingManager mgr(nullptr, m_doc);
    ProgrammingOutline *nav = mgr.m_outline;
    nav->rebuild();
    QCOMPARE(nav->arrangement(), ProgrammingOutline::Folders);   // a new show

    // Picked C, then A, then B: that is the chaser's order.
    select(nav, nav->sceneItem(c->id()));
    select(nav, nav->sceneItem(a->id()), true);
    select(nav, nav->sceneItem(b->id()), true);
    QCOMPARE(nav->selectedFunctionsInOrder(), QList<quint32>({ c->id(), a->id(), b->id() }));
    QAction *chase = mgr.findChild<QAction *>("chaserFromSelection");
    QVERIFY(chase != nullptr);
    const int functions = m_doc->functions().size();
    chase->trigger();
    QCOMPARE(m_doc->functions().size(), functions + 1);
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(mgr.m_workbench->loadedFunction()));
    QVERIFY(ch != nullptr);
    QCOMPARE(ch->stepsCount(), 3);
    QCOMPARE(ch->steps().at(0).fid, c->id());
    QCOMPARE(ch->steps().at(1).fid, a->id());
    QCOMPARE(ch->steps().at(2).fid, b->id());
    QCOMPARE(ch->path(true), QString("Act 1"));
    // The Workbench walks it at once.
    QVERIFY(walkerOf(*mgr.m_workbench) != nullptr);
    QCOMPARE(layerIds(*mgr.m_workbench), QList<quint32>({ c->id() }));
    const quint32 chId = ch->id();

    // Scenes dropped on the chaser in the navigator: appended, in the drag's order.
    nav->rebuild();
    ProgrammingOutline::Row r;
    r.kind = ProgrammingOutline::FunctionRow;
    r.scene = r.id = chId;
    QTreeWidgetItem *chRow = nav->itemFor(r);
    QVERIFY(chRow != nullptr);
    nav->scrollToItem(chRow);
    const QPoint pos = nav->visualItemRect(chRow).center();
    QMimeData *mime = rowsMime({ { ProgrammingOutline::SceneRow, b->id() }, { ProgrammingOutline::SceneRow, a->id() } });
    QCOMPARE(nav->dropPlan(mime, pos, false), QString("Add 2 step(s) to “%1”").arg(ch->name()));
    QDropEvent ev(QPointF(pos), Qt::CopyAction | Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
    nav->dropEvent(&ev);
    QVERIFY(ev.isAccepted());
    delete mime;
    QCOMPARE(ch->stepsCount(), 5);
    QCOMPARE(ch->steps().at(3).fid, b->id());
    QCOMPARE(ch->steps().at(4).fid, a->id());
    // A chaser can't hold itself.
    QMimeData *self = rowsMime({ { ProgrammingOutline::FunctionRow, chId } });
    QVERIFY(nav->dropPlan(self, pos, false).startsWith("!"));
    delete self;

    // Dropped on the walker's step table: before the row it lands on.
    mgr.resize(1400, 900);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    ChaserWalker *walker = walkerOf(*mgr.m_workbench);
    QTRY_COMPARE(walker->stepTable()->topLevelItemCount(), 5);
    QTreeWidgetItem *second = walker->stepTable()->topLevelItem(1);
    const QPoint at = walker->stepTable()->visualItemRect(second).center();
    QMimeData *one = rowsMime({ { ProgrammingOutline::SceneRow, c->id() } });
    QDropEvent drop(QPointF(at), Qt::CopyAction, one, Qt::LeftButton, Qt::NoModifier);
    // As the view delivers it (a synthetic drop has no drag session to go through).
    QVERIFY(walker->eventFilter(walker->stepTable()->viewport(), &drop));
    QVERIFY2(drop.isAccepted(), qPrintable(walker->m_dropHint->text()));
    delete one;
    QCOMPARE(ch->stepsCount(), 6);
    QCOMPARE(ch->steps().at(1).fid, c->id());
    QCOMPARE(stackOf(*mgr.m_workbench)->step(), 1);

    // Each one step: back to the made chaser, then no chaser at all.
    QVERIFY(ops->undo());
    QCOMPARE(ch->stepsCount(), 5);
    QVERIFY(ops->undo());
    QCOMPARE(ch->stepsCount(), 3);
    QVERIFY(ops->undo());
    QTRY_VERIFY(m_doc->function(chId) == nullptr);
    QVERIFY(ops->redo());
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(chId)) != nullptr);
}

void WorkbenchChaser_Test::makeOwnCopySplitsASharedMember()
{
    FixtureGroup *g = group("Washes", 2);
    Scene *back = scene("BackStage", "Show");
    back->setValue(g->fixtureList().first(), 2, 90);
    Scene *other = scene("Other", "Show");
    Collection *c3 = collection("0.0.3-Announcer", { other->id(), back->id() });
    Collection *c4 = collection("0.0.4-SHOUT", { back->id() });
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    ch->addStep(ChaserStep(c3->id()));
    ch->addStep(ChaserStep(c4->id()));
    ch->addStep(ChaserStep(c4->id()));      // played twice
    PaletteOps *ops = m_doc->paletteOps();

    Workbench wb(m_doc);
    wb.load(ch->id());
    // Step 1's BackStage is shared with 0.0.4-SHOUT: the badge, and the way out.
    QVERIFY(wb.rows()->layerItem(1)->text(1).contains("also in 1"));
    QVERIFY(!wb.stack()->canMakeOwnCopy(0));
    QVERIFY(wb.stack()->canMakeOwnCopy(1));
    select(wb.rows(), wb.rows()->layerItem(1));
    QVERIFY(wb.m_ownCopyBtn->isEnabled());
    const int functions = m_doc->functions().size();
    wb.m_ownCopyBtn->click();
    QCOMPARE(m_doc->functions().size(), functions + 1);
    const quint32 copy = c3->functions().at(1);
    QVERIFY(copy != back->id());
    QCOMPARE(c4->functions(), QList<quint32>({ back->id() }));      // untouched
    Scene *own = qobject_cast<Scene *>(m_doc->function(copy));
    QVERIFY(own != nullptr);
    QCOMPARE(own->values(), back->values());
    QCOMPARE(own->path(true), QString("Show"));
    QVERIFY(own->name() != back->name());
    QVERIFY(!wb.rows()->layerItem(1)->text(1).contains("also in"));
    // An edit to the copy is this step's alone.
    own->setValue(g->fixtureList().first(), 2, 10);
    QCOMPARE(back->value(g->fixtureList().first(), 2), uchar(90));
    own->setValue(g->fixtureList().first(), 2, 90);

    // One step: undo puts BackStage back and the copy goes.
    QVERIFY(ops->undoText().contains("own copy"));
    QVERIFY(ops->undo());
    QCOMPARE(c3->functions(), QList<quint32>({ other->id(), back->id() }));
    QTRY_VERIFY(m_doc->function(copy) == nullptr);
    QCOMPARE(m_doc->functions().size(), functions);
    QVERIFY(ops->redo());
    QCOMPARE(c3->functions().at(1), copy);

    // A shared step (0.0.4-SHOUT plays in steps 2 and 3): this step's own.
    FunctionStack *stack = stackOf(wb);
    stack->setStep(1);
    ChaserWalker *walker = walkerOf(wb);
    walker->refresh();
    QVERIFY(!walker->m_shared->isHidden() || !walker->m_shared->text().isEmpty());
    QVERIFY(walker->m_shared->text().contains("Full Show (step 3)"));
    walker->m_ownCopy->click();
    const quint32 stepCopy = ch->steps().at(1).fid;
    QVERIFY(stepCopy != c4->id());
    QCOMPARE(ch->steps().at(2).fid, c4->id());
    QCOMPARE(qobject_cast<Collection *>(m_doc->function(stepCopy))->functions(), c4->functions());
    QCOMPARE(stack->stepFunction(), stepCopy);
    QVERIFY(ops->undo());
    QCOMPARE(ch->steps().at(1).fid, c4->id());
    QTRY_VERIFY(m_doc->function(stepCopy) == nullptr);
}

static QSet<quint32> outlineFunctions(ProgrammingOutline *o, QHash<quint32, QString> *types = nullptr)
{
    QSet<quint32> ids;
    for (QTreeWidgetItemIterator it(o); *it; ++it)
    {
        const int kind = (*it)->data(0, ProgrammingOutline::KindRole).toInt();
        if (kind != ProgrammingOutline::SceneRow && kind != ProgrammingOutline::FunctionRow)
            continue;
        const quint32 id = (*it)->data(0, ProgrammingOutline::IdRole).toUInt();
        ids.insert(id);
        if (types != nullptr)
        {
            QTreeWidgetItem *top = *it;
            while (top->parent() != nullptr)
                top = top->parent();
            types->insert(id, top->text(0));
        }
    }
    return ids;
}

static QSet<quint32> functionManagerFunctions(Doc *doc)
{
    // The Function Manager's own tree, as it fills it.
    FunctionsTreeWidget tree(doc, nullptr);
    tree.updateTree();
    QSet<quint32> ids;
    for (QTreeWidgetItemIterator it(&tree); *it; ++it)
    {
        const quint32 id = tree.itemFunctionId(*it);
        if (id != Function::invalidId() && doc->function(id) != nullptr)
            ids.insert(id);
    }
    return ids;
}

void WorkbenchChaser_Test::classicListsWhatTheFunctionManagerDoes()
{
    Scene *a = scene("Walk-in", "Act 1");
    scene("Loose");
    Collection *c = collection("0.0.1-Startup", { a->id() });
    c->setPath("Act 1/Opening");
    Chaser *ch = new Chaser(m_doc);
    ch->setName("Full Show");
    m_doc->addFunction(ch);
    EFX *efx = new EFX(m_doc);
    efx->setName("Ballyhoo");
    m_doc->addFunction(efx);
    efx->setPath("Effects");
    const int scenes = 2;

    ProgrammingManager mgr(nullptr, m_doc);
    ProgrammingOutline *nav = mgr.m_outline;
    nav->rebuild();
    // Folders: the EFX filed alone in its folder shows; Classic: everything.
    QToolButton *classicBtn = mgr.findChild<QToolButton *>("arrangeClassic");
    QVERIFY(classicBtn != nullptr);
    classicBtn->click();
    QCOMPARE(nav->arrangement(), ProgrammingOutline::Classic);
    QVERIFY(mgr.m_outlineHint->text().contains("Function Manager"));
    QHash<quint32, QString> types;
    const QSet<quint32> listed = outlineFunctions(nav, &types);
    QCOMPARE(listed, functionManagerFunctions(m_doc));
    QCOMPARE(listed.size(), m_doc->functions().size());
    QCOMPARE(types.value(efx->id()), QString("EFX"));
    QCOMPARE(types.value(ch->id()), QString("Chaser"));
    QCOMPARE(types.value(a->id()), QString("Scene"));
    // Types in the design's order, Scene first; each type's folders under it.
    QCOMPARE(nav->topLevelItem(0)->text(0), QString("Scene"));
    QCOMPARE(nav->topLevelItem(0)->text(1), QString::number(scenes));
    QCOMPARE(nav->topLevelItem(1)->text(0), QString("Chaser"));
    QCOMPARE(nav->topLevelItem(2)->text(0), QString("Collection"));
    QTreeWidgetItem *collRow = nav->itemFor({ ProgrammingOutline::FunctionRow, c->id(), c->id() });
    QVERIFY(collRow != nullptr);
    QCOMPARE(collRow->parent()->text(0), QString("Opening"));
    QCOMPARE(collRow->parent()->parent()->text(0), QString("Act 1"));
    QCOMPARE(collRow->parent()->parent()->parent()->text(0), QString("Collection"));
    // A scene opens up in Classic too; clicking a row loads it.
    QVERIFY(nav->sceneItem(a->id()) != nullptr);
    select(nav, collRow);
    QCOMPARE(mgr.m_workbench->loadedFunction(), c->id());

    // A new show remembers nothing; a saved one remembers its switch.
    QTemporaryDir dir;
    m_doc->setWorkspacePath(dir.path());
    QCOMPARE(nav->savedArrangement(), ProgrammingOutline::Classic);   // no Show Tree: Classic
    mgr.findChild<QToolButton *>("arrangeFolders")->click();
    QCOMPARE(nav->arrangement(), ProgrammingOutline::Folders);
    QCOMPARE(nav->savedArrangement(), ProgrammingOutline::Folders);
    QVERIFY(!outlineFunctions(nav).contains(efx->id()) || efx->path(true) == "Effects");
}

void WorkbenchChaser_Test::danceHysteriaFullShow()
{
    QTemporaryDir dir;
    QVERIFY(loadCopy(QStringLiteral(WB_DH_SHOW), dir));
    Chaser *full = nullptr;
    for (Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType && f->name() == "Full Show")
            full = qobject_cast<Chaser *>(f);
    QVERIFY(full != nullptr);
    QCOMPARE(full->stepsCount(), 7);

    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1700, 1000);
    mgr.setSection(ProgrammingManager::ScenesSection);
    // An old show, no Show Tree: Build opens on Classic.
    QCOMPARE(mgr.m_outline->arrangement(), ProgrammingOutline::Classic);
    mgr.m_outline->rebuild();
    QCOMPARE(outlineFunctions(mgr.m_outline), functionManagerFunctions(m_doc));
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    Workbench *wb = mgr.m_workbench;
    mgr.showFunction(full->id());
    ChaserWalker *walker = walkerOf(*wb);
    QVERIFY(walker != nullptr);
    FunctionStack *stack = stackOf(*wb);

    // Every step: its collection's members are the layers, the header its timing.
    for (int i = 0; i < full->stepsCount(); ++i)
    {
        stack->setStep(i);
        const ChaserStep st = full->steps().at(i);
        Collection *c = qobject_cast<Collection *>(m_doc->function(st.fid));
        QVERIFY(c != nullptr);
        QCOMPARE(layerIds(*wb), c->functions());
        QCoreApplication::processEvents();
        QCOMPARE(walker->m_note->text(), st.note);
        QCOMPARE(walker->m_fadeIn->text(), Function::speedToString(st.fadeIn));
        QVERIFY(walker->m_where->text().startsWith(QString("Step %1 of 7 · %2").arg(i + 1).arg(c->name())));
    }

    // 0.0.4-SHOUT: + EFX, take a layer out, then both undone.
    stack->setStep(3);
    Collection *shout = qobject_cast<Collection *>(m_doc->function(stack->stepFunction()));
    QVERIFY(shout != nullptr);
    const QList<quint32> was = shout->functions();
    const int functions = m_doc->functions().size();
    const quint32 efx = stack->addNew(Function::EFXType, -1, {}, FixtureGroup::invalidId());
    QVERIFY(efx != Function::invalidId());
    QVERIFY(shout->functions().contains(efx));
    if (!was.isEmpty())
    {
        QVERIFY(stack->removeLayers({ 0 }));
        QVERIFY(!shout->functions().contains(was.first()));
    }
    PaletteOps *ops = m_doc->paletteOps();
    if (!was.isEmpty())
        QVERIFY(ops->undo());
    QVERIFY(ops->undo());
    QCOMPARE(shout->functions(), was);
    QCOMPARE(m_doc->functions().size(), functions);
    qInfo() << "[W2] Full Show:" << full->stepsCount() << "steps walked;" << was.size() << "layers in 0.0.4-SHOUT";

    const QString shots = qEnvironmentVariable("QLC_W2_SHOTS");
    if (shots.isEmpty())
        return;
    mgr.setFixedSize(1700, 1000);     // the picture's size, whatever the content asks
    if (QSplitter *split = mgr.findChild<QSplitter *>("programmingSplitter"))
        split->setSizes({ 380, 1320 });
    if (QSplitter *bench = mgr.findChild<QSplitter *>("workbenchSplitter"))
        bench->setSizes({ 720, 600 });
    stack->setStep(2);
    QTest::qWait(150);
    QVERIFY(mgr.grab().save(shots + "/w2-chaser-step.png"));
    // A layer of the step selected: its editor beside the walk.
    for (int i = 0; i < wb->rows()->topLevelItemCount(); ++i)
        if (wb->rows()->layerItem(i) != nullptr
            && wb->rows()->layerItem(i)->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::SceneRow)
        {
            select(wb->rows(), wb->rows()->layerItem(i));
            break;
        }
    QTest::qWait(150);
    QVERIFY(mgr.grab().save(shots + "/w2-chaser-step-layer.png"));
}

void WorkbenchChaser_Test::sampleShowsChasers()
{
    QTemporaryDir dir;
    QVERIFY(loadCopy(QStringLiteral(WB_SAMPLE_SHOW), dir));
    ProgrammingManager mgr(nullptr, m_doc);
    mgr.resize(1700, 1000);
    QCOMPARE(mgr.m_outline->arrangement(), ProgrammingOutline::Classic);
    mgr.m_outline->rebuild();
    QHash<quint32, QString> types;
    QCOMPARE(outlineFunctions(mgr.m_outline, &types), functionManagerFunctions(m_doc));
    Workbench *wb = mgr.m_workbench;

    // Every stock chaser walks, step by step, each step its function.
    int chasers = 0, steps = 0;
    for (Function *f : m_doc->functions())
    {
        Chaser *ch = qobject_cast<Chaser *>(f);
        if (ch == nullptr || ch->type() != Function::ChaserType)
            continue;
        QCOMPARE(types.value(ch->id()), QString("Chaser"));
        mgr.showFunction(ch->id());
        FunctionStack *stack = stackOf(*wb);
        QVERIFY(stack != nullptr && walkerOf(*wb) != nullptr);
        for (int i = 0; i < ch->stepsCount(); ++i)
        {
            if (i > 0)
                QVERIFY(stack->setStep(i));
            QCOMPARE(stack->stepFunction(), ch->steps().at(i).fid);
            const QList<quint32> ids = layerIds(*wb);
            QCOMPARE(ids.size(), 1);
            QCOMPARE(ids.first(), ch->steps().at(i).fid);
            ++steps;
        }
        ++chasers;
    }
    QVERIFY(chasers > 0);
    qInfo() << "[W2] Sample.qxw:" << chasers << "chasers," << steps << "steps walked";

    const QString shots = qEnvironmentVariable("QLC_W2_SHOTS");
    if (shots.isEmpty())
        return;
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    mgr.setFixedSize(1700, 1000);     // the picture's size, whatever the content asks
    if (QSplitter *split = mgr.findChild<QSplitter *>("programmingSplitter"))
        split->setSizes({ 380, 1320 });
    for (Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType)
        {
            mgr.showFunction(f->id());
            break;
        }
    QTest::qWait(150);
    QVERIFY(mgr.grab().save(shots + "/w2-classic-sample.png"));
}

QTEST_MAIN(WorkbenchChaser_Test)

/****************************************************************************
 * The Functions window, retired (board Q32)
 ****************************************************************************/

/* Whatever modal dialog or popup comes up next: its class and the texts of
   its actions, then it is closed (accepted after @a onDialog, if given). */
static void whenItOpens(QStringList *seen, const std::function<void(QWidget *)> &onDialog = nullptr)
{
    QTimer::singleShot(0, qApp, [seen, onDialog]() {
        QWidget *w = QApplication::activePopupWidget();
        if (w == nullptr)
            w = QApplication::activeModalWidget();
        if (w == nullptr)
            return;
        seen->append(QString::fromLatin1(w->metaObject()->className()));
        if (QMenu *m = qobject_cast<QMenu *>(w))
        {
            for (QAction *a : m->actions())
                seen->append(a->text());
            m->close();
            return;
        }
        if (onDialog)
            onDialog(w);
        else if (QDialog *d = qobject_cast<QDialog *>(w))
            d->reject();
    });
}

void WorkbenchChaser_Test::functionsWindowToolsLiveInBuild()
{
    Scene *a = scene("Walk-in", "Act 1");
    ProgrammingManager mgr(nullptr, m_doc);
    QCOMPARE(ProgrammingManager::instance(), &mgr);
    ProgrammingOutline *nav = mgr.m_outline;
    mgr.showClassic();
    QCOMPARE(mgr.section(), ProgrammingManager::ScenesSection);
    QCOMPARE(nav->arrangement(), ProgrammingOutline::Classic);

    // + Scene ▾: New function ▸ every type, the wizard, the startup function.
    QMenu *plus = mgr.m_addFuncBtn->menu();
    QVERIFY(plus != nullptr);
    QMenu *sub = plus->findChild<QMenu *>("buildNewFunctionMenu");
    QVERIFY(sub != nullptr);
    QStringList kinds;
    for (QAction *act : sub->actions())
        kinds << act->text();
    QCOMPARE(kinds, QStringList({ "Scene", "Chaser", "Sequence", "EFX", "Collection", "RGB Matrix",
                                  "Script", "Audio…", "Video…", "Show" }));
    QVERIFY(plus->actions().contains(mgr.m_wizardAction));
    QVERIFY(plus->actions().contains(mgr.m_startupAction));
    const QString shots = qEnvironmentVariable("QLC_NOFM_SHOTS");
    if (!shots.isEmpty())
    {
        plus->popup(QPoint(0, 0));
        sub->popup(QPoint(plus->sizeHint().width(), 0));
        QVERIFY(plus->grab().save(shots + "/build-new-menu.png"));
        QVERIFY(sub->grab().save(shots + "/build-new-function-submenu.png"));
        sub->close();
        plus->close();
    }
    // ⌘1…⌘9 as the Functions window had them, live on the Scenes page.
    QWidget *page = mgr.findChild<QWidget *>("buildScenes");
    QVERIFY(page != nullptr);
    for (int i = 0; i < 9; ++i)
    {
        QAction *act = sub->actions().at(i);
        QCOMPARE(act->shortcut(), QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + i)));
        QCOMPARE(act->shortcutContext(), Qt::WidgetWithChildrenShortcut);
        QVERIFY(page->actions().contains(act));
    }

    // Each type: made in the current row's folder, selected, loaded.
    const QList<Function::Type> types = { Function::SceneType, Function::ChaserType, Function::SequenceType,
                                          Function::EFXType, Function::CollectionType, Function::RGBMatrixType,
                                          Function::ScriptType, Function::ShowType };
    for (Function::Type t : types)
    {
        nav->rebuild();
        QTreeWidgetItem *row = nav->itemFor({ ProgrammingOutline::SceneRow, a->id(), a->id() });
        QVERIFY(row != nullptr);
        nav->setCurrentItem(row, 0, QItemSelectionModel::NoUpdate);
        const QList<Function *> had = m_doc->functionsByType(t);
        QAction *act = nullptr;
        for (QAction *x : sub->actions())
            if (x->data().toInt() == int(t))
                act = x;
        QVERIFY(act != nullptr);
        act->trigger();
        QCOMPARE(m_doc->functionsByType(t).size(), had.size() + 1);
        Function *made = nullptr;
        for (Function *f : m_doc->functionsByType(t))
            if (!had.contains(f))
                made = f;
        QVERIFY(made != nullptr);
        QCOMPARE(made->path(true), QString("Act 1"));
        QCOMPARE(mgr.m_workbench->loadedFunction(), made->id());
        QList<ProgrammingOutline::Row> sel = nav->selectedRows();
        QVERIFY2(!sel.isEmpty() && sel.first().id == made->id(),
                 qPrintable(Function::typeToString(t) + " not selected in the outline"));
        if (Sequence *seq = qobject_cast<Sequence *>(made))
        {
            Function *bound = m_doc->function(seq->boundSceneID());
            QVERIFY(bound != nullptr && bound->type() == Function::SceneType && !bound->isVisible());
        }
    }
    // Audio / Video: a function per file (the picker is the menu's).
    const QList<quint32> videos = mgr.addMediaFunctions(Function::VideoType,
                                                        { "/nonexistent/clip.mp4", "/nonexistent/still.png" },
                                                        "Media");
    QCOMPARE(videos.size(), 2);
    QCOMPARE(m_doc->function(videos.first())->type(), Function::VideoType);
    QCOMPARE(m_doc->function(videos.first())->path(true), QString("Media"));
    QCOMPARE(mgr.m_workbench->loadedFunction(), videos.last());

    // The outline's right-click has them too.
    {
        QMenu m;
        emit nav->extendMenu(&m);
        QStringList texts;
        for (QAction *act : m.actions())
            texts << act->text();
        QVERIFY(texts.contains("New function"));
        QVERIFY(texts.contains(mgr.m_wizardAction->text()));
        QVERIFY(texts.contains(mgr.m_startupAction->text()));
    }

    // Function Wizard… opens the wizard.
    QStringList seen;
    whenItOpens(&seen);
    mgr.m_wizardAction->trigger();
    QCOMPARE(seen.value(0), QString("FunctionWizard"));

    // Startup function… picks it.
    seen.clear();
    QSignalSpy modified(m_doc, &Doc::modified);
    whenItOpens(&seen, [a](QWidget *w) {
        FunctionSelection *fs = qobject_cast<FunctionSelection *>(w);
        if (fs == nullptr)
            return;
        fs->setSelection({ a->id() });
        fs->accept();
    });
    mgr.m_startupAction->trigger();
    QCOMPARE(seen.value(0), QString("FunctionSelection"));
    QCOMPARE(m_doc->startupFunction(), a->id());

    // A folder row: Rename folder, Delete folder (what is in it moves up).
    Collection *c = collection("Opener", { a->id() });
    c->setPath("Act 1/Opening");
    nav->rebuild();
    QTreeWidgetItem *folder = nullptr;
    for (QTreeWidgetItemIterator it(nav); *it; ++it)
        if ((*it)->data(0, ProgrammingOutline::PathRole).toString() == "Act 1")
            folder = *it;
    QVERIFY(folder != nullptr);
    nav->scrollToItem(folder);
    seen.clear();
    whenItOpens(&seen);
    nav->slotContextMenu(nav->visualItemRect(folder).center());
    QVERIFY2(seen.contains("Rename folder") && seen.contains("Delete folder"), qPrintable(seen.join(" | ")));
    QVERIFY(seen.contains("New function"));
    QVERIFY(seen.contains("Function Wizard…"));
    mgr.removeFunctionFolder("Act 1");
    QCOMPARE(a->path(true), QString());
    QCOMPARE(c->path(true), QString("Opening"));
    QVERIFY(m_doc->paletteOps()->canUndo());
    m_doc->paletteOps()->undo();
    QCOMPARE(a->path(true), QString("Act 1"));
    QCOMPARE(c->path(true), QString("Act 1/Opening"));
}

void WorkbenchChaser_Test::aSequenceEditsInTheWorkbench()
{
    ProgrammingManager mgr(nullptr, m_doc);
    mgr.addNewFunction(Function::SequenceType, QString());
    QCOMPARE(m_doc->functionsByType(Function::SequenceType).size(), 1);
    Sequence *seq = qobject_cast<Sequence *>(m_doc->functionsByType(Function::SequenceType).first());
    QVERIFY(seq != nullptr);
    Workbench *wb = mgr.m_workbench;
    QCOMPARE(wb->loadedFunction(), seq->id());
    // No chaser walker: its steps are one scene's values, not scenes to add.
    QVERIFY(walkerOf(*wb) == nullptr);
    QVERIFY(wb->stack()->layers().isEmpty());

    // Root: its editor on the overview, steps over channels.
    QSplitter *ed = wb->findChild<QSplitter *>("workbenchSequenceEditor");
    QVERIFY(ed != nullptr);
    QVERIFY(ed->findChild<ChaserEditor *>() != nullptr);
    SceneEditor *se = ed->findChild<SceneEditor *>();
    QVERIFY(se != nullptr);
    QCOMPARE(se->m_scene->id(), seq->boundSceneID());
    const QString shots = qEnvironmentVariable("QLC_NOFM_SHOTS");
    if (!shots.isEmpty())
    {
        mgr.resize(1400, 860);
        QVERIFY(mgr.grab().save(shots + "/sequence-in-workbench.png"));
    }
    QVERIFY(!wb->m_overviewText->text().contains("Functions window"));

    // A step of a collection: the same editor in the editor pane.
    Collection *c = collection("Holder", { seq->id() });
    wb->load(c->id());
    QTreeWidgetItem *layer = wb->rows()->layerItem(0);
    QVERIFY(layer != nullptr);
    select(wb->rows(), layer);
    QCOMPARE(wb->m_editorFunction, seq->id());
    QVERIFY(wb->m_funcEditor != nullptr);
    QCOMPARE(wb->m_funcEditor->objectName(), QString("workbenchSequenceEditor"));
    QVERIFY(!wb->m_functionHeading->text().contains("Functions window"));
}

void WorkbenchChaser_Test::openingAFunctionLandsInTheWorkbench()
{
    Scene *a = scene("A");
    EFX *efx = new EFX(m_doc);
    efx->setName("Ballyhoo");
    m_doc->addFunction(efx);
    Chaser *inner = new Chaser(m_doc);
    inner->setName("Inner chase");
    m_doc->addFunction(inner);
    inner->addStep(ChaserStep(a->id()));
    Chaser *outer = new Chaser(m_doc);
    outer->setName("Outer");
    m_doc->addFunction(outer);
    outer->addStep(ChaserStep(inner->id()));

    QVERIFY(!ProgrammingManager::openFunction(efx->id()));   // no Build tab
    ProgrammingManager mgr(nullptr, m_doc);
    ProgrammingOutline *nav = mgr.m_outline;
    nav->setArrangement(ProgrammingOutline::Folders);
    mgr.setSection(ProgrammingManager::PalettesSection);
    QVERIFY(!ProgrammingManager::openFunction(Function::invalidId()));
    QVERIFY(ProgrammingManager::openFunction(efx->id()));
    QCOMPARE(mgr.section(), ProgrammingManager::ScenesSection);
    QCOMPARE(mgr.m_workbench->loadedFunction(), efx->id());
    QCOMPARE(nav->arrangement(), ProgrammingOutline::Classic);   // Folders doesn't list a loose EFX
    QVERIFY(nav->itemFor({ ProgrammingOutline::FunctionRow, efx->id(), efx->id() })->isSelected());

    // Edit Chase… on a step: in this Workbench, ↩ back to the chaser.
    QVERIFY(ProgrammingManager::openFunction(outer->id()));
    QCOMPARE(mgr.m_workbench->loadedFunction(), outer->id());
    ChaserEditor *ce = qobject_cast<ChaserEditor *>(mgr.m_workbench->m_overviewEditor);
    QVERIFY(ce != nullptr);
    ce->editSubCueChase(0);
    QTRY_COMPARE(mgr.m_workbench->loadedFunction(), inner->id());
    QVERIFY(mgr.m_workbench->canGoBack());
    QVERIFY(mgr.m_workbench->goBack());
    QCOMPARE(mgr.m_workbench->loadedFunction(), outer->id());
}
