/*
  Q Light Controller Plus
  shownotes_test.cpp

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
#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QApplication>
#include <QPushButton>
#include <QTreeWidget>
#include <QCheckBox>
#include <QSplitter>
#include <QToolTip>
#include <QLabel>
#include <QImage>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#include "showtreecontroller.h"
#include "structurestudioview.h"
#include "shownotespane.h"
#include "shownoteslist.h"
#include "showtreeview.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "app.h"
#include "doc.h"

#include "shownotes_test.h"

using namespace ShowTree;

/* Notes beside the Show Tree (Q212), offscreen: the pane follows the
   selection, adds and ticks notes; the row marks and their hover; a
   snapshot of the view and Show me; the show's notes list, its filters,
   jump and export. */

namespace
{
QTreeWidgetItem *rowOf(ShowTreeView &view, int kind, quint32 id)
{
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == kind
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id)
            return *it;
    return nullptr;
}

QTreeWidgetItem *libraryPalette(ShowTreeView &view, quint32 id)
{
    for (QTreeWidgetItemIterator it(view.library()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibPalette
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id)
            return *it;
    return nullptr;
}
} // namespace

void ShowNotes_Test::init()
{
    m_doc = new Doc(this);
    QList<quint32> fixtures;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        fixtures << fxi->id();
    }
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Towers");
    m_doc->addFixtureGroup(g);
    for (quint32 f : fixtures)
        g->assignFixture(f);
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName("Half");
    p->setValue(127);
    m_doc->addPalette(p);
    m_palette = p->id();

    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Dance Hysteria");
    m_list = st->addCueList(m_prod, "Main");
    m_cue1 = st->addCue(m_list, -1, "Walk-in");
    m_cue2 = st->addCue(m_list, 0, "Opener");
    m_cue3 = st->addCue(m_list, 1, "Ballad");
    Target t;
    t.groups << g->id();
    m_layer = st->addLayer(m_cue2, t);
    QVERIFY(st->setLayerLook(m_cue2, m_layer, st->lookForPalette(m_palette)));
    st->clearUndo();

    m_rig = new StructureStudioView(m_doc, StructureStudioView::StageKind, 0);
    m_rig->setPlane(StructureStudioView::Angled);
    m_rig->resize(640, 400);
    StructureStudioView *rig = m_rig;
    ShowNotesPane::setViewProvider([rig](bool) { return rig; });
}

void ShowNotes_Test::cleanup()
{
    ShowNotesPane::setViewProvider(nullptr);
    delete m_rig;
    m_rig = nullptr;
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

void ShowNotes_Test::thePaneFollowsTheSelection()
{
    ShowTreeView view(nullptr, m_doc);
    ShowNotesPane *pane = view.notesPane();
    QVERIFY(pane != nullptr);
    // Beside the tree, under the page.
    QSplitter *pageSplit = view.findChild<QSplitter *>("stVSplitter");
    QVERIFY(pageSplit != nullptr);
    QVERIFY(pageSplit->indexOf(pane) >= 0);

    QVERIFY(view.selectNode(m_cue2));
    QVERIFY(pane->hasItem());
    QCOMPARE(int(pane->itemKind()), int(NoteKind::Cue));
    QCOMPARE(pane->itemTarget(), m_cue2);
    QVERIFY(pane->findChild<QLabel *>("stNotesTitle")->text().contains("Opener"));

    QVERIFY(view.selectNode(m_layer, m_cue2));
    QCOMPARE(int(pane->itemKind()), int(NoteKind::Layer));
    QCOMPARE(pane->itemTarget(), m_layer);

    QVERIFY(view.selectNode(m_list));
    QCOMPARE(int(pane->itemKind()), int(NoteKind::List));
    QVERIFY(view.selectNode(m_prod));
    QCOMPARE(int(pane->itemKind()), int(NoteKind::Production));

    // A palette picked in the Library.
    QTreeWidgetItem *pal = libraryPalette(view, m_palette);
    QVERIFY(pal != nullptr);
    view.library()->setCurrentItem(pal);
    QCOMPARE(int(pane->itemKind()), int(NoteKind::Palette));
    QCOMPARE(pane->itemTarget(), m_palette);

    // Each kind takes a note from the pane.
    QVERIFY(pane->addNote("Half reads hot") != invalidId());
    QCOMPARE(m_doc->showTree()->notes(NoteKind::Palette, m_palette).count(), 1);
    QCOMPARE(pane->shownNotes().count(), 1);
}

void ShowNotes_Test::addTickAndUndoFromThePane()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    ShowNotesPane *pane = view.notesPane();
    QVERIFY(view.selectNode(m_cue3));

    // Typed and Return: one note, the box empties.
    QPlainTextEdit *input = pane->input();
    input->setFocus();
    QTest::keyClicks(input, "Chase too fast");
    QTest::keyClick(input, Qt::Key_Return, Qt::ShiftModifier);
    QTest::keyClicks(input, "on the chorus");
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(input->toPlainText().isEmpty());
    QList<Note> list = st->notes(NoteKind::Cue, m_cue3);
    QCOMPARE(list.count(), 1);
    QCOMPARE(list.first().text, QString("Chase too fast\non the chorus"));
    QVERIFY(st->isCueNeedsWork(m_cue3));
    QCOMPARE(pane->shownNotes(), QList<quint32>({ list.first().id }));

    // The tick: done, Needs work gone; undo on the tree's history.
    QCoreApplication::processEvents();
    QCheckBox *done = pane->findChild<QCheckBox *>("stNoteDone");
    QVERIFY(done != nullptr);
    done->setChecked(true);
    QVERIFY(st->note(list.first().id)->done);
    QVERIFY(!st->isCueNeedsWork(m_cue3));
    view.slotUndo();
    QVERIFY(!st->note(list.first().id)->done);
    view.slotUndo();
    QVERIFY(st->note(list.first().id) == nullptr);
    QVERIFY(pane->shownNotes().isEmpty());

    // A layer's note.
    QVERIFY(view.selectNode(m_layer, m_cue2));
    QVERIFY(pane->addNote("Towers late") != invalidId());
    QCOMPARE(st->notes(NoteKind::Layer, m_layer).first().cue, m_cue2);
}

void ShowNotes_Test::rowsWithNotesAreMarked()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    QTreeWidgetItem *row = rowOf(view, ShowTreeView::CueNode, m_cue2);
    QVERIFY(row != nullptr);
    QVERIFY(row->data(ShowTreeView::FlagsCol, ShowTreeView::NoteMarkRole).toString().isEmpty());

    const quint32 n = st->addNote(NoteKind::Cue, m_cue2, "Too blue on the DS edge");
    row = rowOf(view, ShowTreeView::CueNode, m_cue2);
    QCOMPARE(row->data(ShowTreeView::FlagsCol, ShowTreeView::NoteMarkRole).toString(), QString("✎"));
    QVERIFY(row->data(ShowTreeView::FlagsCol, ShowTreeView::NoteOpenRole).toBool());
    QVERIFY(row->toolTip(0).contains("Too blue on the DS edge"));
    QVERIFY(row->toolTip(ShowTreeView::FlagsCol).contains("Too blue on the DS edge"));
    // The mark is drawn first, in the warning colour.
    const QModelIndex idx = view.tree()->indexFromItem(row, ShowTreeView::FlagsCol);
    const QList<QPair<QString, QColor>> pieces = ShowTreeView::flagPieces(idx, Qt::gray);
    QVERIFY(!pieces.isEmpty());
    QCOMPARE(pieces.first().first, QString("✎"));
    QCOMPARE(pieces.first().second, ShowTreeView::warningColour());

    // Two open: the count. The hover says the newest.
    st->addNote(NoteKind::Cue, m_cue2, "Fix the fade");
    row = rowOf(view, ShowTreeView::CueNode, m_cue2);
    QCOMPARE(row->data(ShowTreeView::FlagsCol, ShowTreeView::NoteMarkRole).toString(), QString("✎ 2"));
    QVERIFY(row->toolTip(0).contains("Fix the fade"));
    QVERIFY(!row->toolTip(0).contains("Too blue"));     // one line, not each note again

    // Done only: the plain mark.
    for (const Note &x : st->notes(NoteKind::Cue, m_cue2))
        st->setNoteDone(x.id, true);
    row = rowOf(view, ShowTreeView::CueNode, m_cue2);
    QCOMPARE(row->data(ShowTreeView::FlagsCol, ShowTreeView::NoteMarkRole).toString(), QString("✎"));
    QVERIFY(!row->data(ShowTreeView::FlagsCol, ShowTreeView::NoteOpenRole).toBool());
    QCOMPARE(ShowTreeView::flagPieces(view.tree()->indexFromItem(row, ShowTreeView::FlagsCol), Qt::gray)
             .first().second, QColor(Qt::gray));

    // A list and a layer are marked too; removing the last note unmarks.
    const quint32 ln = st->addNote(NoteKind::List, m_list, "Act 1 long");
    st->addNote(NoteKind::Layer, m_layer, "Towers late", NoteView(), m_cue2);
    QVERIFY(!rowOf(view, ShowTreeView::ListNode, m_list)->data(ShowTreeView::FlagsCol,
                                                                ShowTreeView::NoteMarkRole).toString().isEmpty());
    QTreeWidgetItem *layerRow = nullptr;
    for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == m_layer)
            layerRow = *it;
    QVERIFY(layerRow != nullptr);
    QVERIFY(layerRow->toolTip(0).contains("Towers late"));
    QVERIFY(st->removeNote(ln));
    QVERIFY(rowOf(view, ShowTreeView::ListNode, m_list)->data(ShowTreeView::FlagsCol,
                                                               ShowTreeView::NoteMarkRole).toString().isEmpty());
    QVERIFY(!rowOf(view, ShowTreeView::ListNode, m_list)->toolTip(0).contains("Act 1 long"));
    Q_UNUSED(n)
}

void ShowNotes_Test::snapshotAndShowMe()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    ShowNotesPane *pane = view.notesPane();
    m_rig->setAngledView(55.0, 40.0);
    st->previewCue(m_cue2);         // on the rig (Design)
    QVERIFY(view.selectNode(m_cue3));

    const quint32 id = pane->addNote("Look at the floor", true);
    QVERIFY(id != invalidId());
    const Note *n = st->note(id);
    QVERIFY(n->view.valid);
    QCOMPARE(n->view.plane, int(StructureStudioView::Angled));
    QCOMPARE(n->view.azimuth, 55.0);
    QCOMPARE(n->view.elevation, 40.0);
    QCOMPARE(n->view.width, 640);
    QCOMPARE(n->view.cue, st->previewState() == ShowTreeController::PreviewState::Playing ? m_cue2
                                                                                          : invalidId());
    QVERIFY(!n->view.image.isEmpty());
    const QByteArray bytes = st->noteImage(n->view.image);
    QVERIFY(bytes.size() > 0);
    QVERIFY2(bytes.size() < 120 * 1024, qPrintable(QString::number(bytes.size())));
    QImage img;
    QVERIFY(img.loadFromData(bytes));
    QVERIFY(img.width() <= ShowNotesPane::imageWidth());
    // The entry has its picture and Show me.
    QVERIFY(pane->findChild<QPushButton *>("stNoteShowMe") != nullptr);

    // Somewhere else, then Show me: the camera as it was; in Design the cue
    // that was on stage is selected.
    m_rig->setAngledView(200.0, 10.0);
    m_rig->setRotation(2);
    QVERIFY(view.selectNode(m_cue1));
    QVERIFY(pane->showMe(id));
    QCOMPARE(m_rig->angledAzimuth(), 55.0);
    QCOMPARE(m_rig->angledElevation(), 40.0);
    QCOMPARE(m_rig->rotation(), 0);
    if (n->view.cue != invalidId())
        QCOMPARE(view.currentId(), m_cue2);

    // A zoom by hand comes back too, centred for the view's size.
    StructureStudioView::Camera c = m_rig->camera();
    c.zoomed = true;
    c.scale = 140.0;
    c.origin = QPointF(300, 180);
    m_rig->setCamera(c);
    const quint32 zoomed = pane->addNote("Zoomed", true);
    QVERIFY(st->note(zoomed)->view.zoomed);
    m_rig->setCamera(StructureStudioView::Camera());
    QVERIFY(!m_rig->camera().zoomed);
    QVERIFY(pane->showMe(zoomed));
    QVERIFY(m_rig->camera().zoomed);
    QCOMPARE(m_rig->camera().scale, 140.0);
    QCOMPARE(m_rig->camera().origin, QPointF(300, 180));

    // A note without a snapshot has no Show me.
    const quint32 plain = pane->addNote("No view");
    QVERIFY(!pane->showMe(plain));
}

void ShowNotes_Test::showMeInOperateOnlyMovesTheCamera()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    ShowNotesPane *pane = view.notesPane();
    QVERIFY(view.selectNode(m_cue3));
    NoteView v;
    v.valid = true;
    v.plane = StructureStudioView::Angled;
    v.azimuth = 70.0;
    v.elevation = 15.0;
    v.width = 640;
    v.height = 400;
    v.cue = m_cue2;
    v.cueLabel = "2 Opener";
    const quint32 id = st->addNote(NoteKind::Cue, m_cue3, "Operate", v);
    m_doc->setMode(Doc::Operate);
    QVERIFY(view.selectNode(m_cue1));
    QVERIFY(pane->showMe(id));
    QCOMPARE(m_rig->angledAzimuth(), 70.0);
    QCOMPARE(view.currentId(), m_cue1);      // the selection, and so the stage, left alone
    m_doc->setMode(Doc::Design);
    QVERIFY(pane->showMe(id));
    QCOMPARE(view.currentId(), m_cue2);
}

void ShowNotes_Test::theNotesListFiltersAndJumps()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    const quint32 a = st->addNote(NoteKind::Cue, m_cue1, "Walk-in too bright");
    const quint32 b = st->addNote(NoteKind::Cue, m_cue2, "Opener late");
    const quint32 c = st->addNote(NoteKind::Layer, m_layer, "Towers flicker", NoteView(), m_cue2);
    const quint32 d = st->addNote(NoteKind::Palette, m_palette, "Half hot");
    QVERIFY(st->setNoteDone(a, true));

    ShowNotesList *list = view.showAllNotes();
    QVERIFY(list != nullptr);
    QCOMPARE(view.showAllNotes(), list);      // the one window
    QCOMPARE(list->listedNotes().count(), 4);
    QCOMPARE(list->listedNotes().first(), d);  // newest first

    list->setStateFilter(ShowNotesList::OpenNotes);
    QCOMPARE(list->listedNotes().count(), 3);
    QVERIFY(!list->listedNotes().contains(a));
    list->setStateFilter(ShowNotesList::DoneNotes);
    QCOMPARE(list->listedNotes(), QList<quint32>({ a }));
    list->setStateFilter(ShowNotesList::AllNotes);

    list->setItemFilter(NoteKind::Cue, m_cue2);
    QCOMPARE(list->listedNotes(), QList<quint32>({ b }));
    list->setItemFilter(NoteKind::Cue, invalidId());
    list->setTextFilter("flicker");
    QCOMPARE(list->listedNotes(), QList<quint32>({ c }));
    list->setTextFilter("Opener");           // a cue's label finds its notes
    QVERIFY(list->listedNotes().contains(b));
    list->setTextFilter(QString());

    // Jump: the item selected in the tree.
    const int rowB = list->listedNotes().indexOf(b);
    QVERIFY(list->jumpTo(rowB));
    QCOMPARE(view.currentId(), m_cue2);
    QCOMPARE(int(view.currentKind()), int(ShowTreeView::CueNode));
    QVERIFY(list->jumpTo(list->listedNotes().indexOf(c)));
    QCOMPARE(view.currentId(), m_layer);
    QCOMPARE(int(view.currentKind()), int(ShowTreeView::LayerNode));

    // A new note shows up at once.
    st->addNote(NoteKind::List, m_list, "New");
    QCOMPARE(list->listedNotes().count(), 5);
    list->close();
}

void ShowNotes_Test::exportMakesMarkdownAndPdf()
{
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(m_cue2));
    view.notesPane()->addNote("Snap the blackout", true);
    const quint32 done = st->addNote(NoteKind::Cue, m_cue1, "Fixed");
    st->setNoteDone(done, true);
    ShowNotesList *list = view.showAllNotes();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString md = dir.filePath("notes.md");
    QVERIFY(list->exportTo(md));
    QFile f(md);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    QVERIFY(text.startsWith("# Show notes"));
    QVERIFY(text.contains("- [ ] "));
    QVERIFY(text.contains("Snap the blackout"));
    QVERIFY(text.contains("- [x] "));
    QVERIFY(text.contains("snapshot of the view"));
    QVERIFY(text.contains("## Cue "));

    const QString pdf = dir.filePath("notes.pdf");
    QVERIFY(list->exportTo(pdf));
    QFile p(pdf);
    QVERIFY(p.open(QIODevice::ReadOnly));
    QVERIFY(p.size() > 500);
    QVERIFY(p.read(5) == "%PDF-");
    list->close();
}

/* Q207's pictures: the pane with notes, a snapshot entry, the row marks,
   the hover and the notes list, at the minimum, laptop and wide sizes, in
   the dark theme. Only with QLC_NOTES_SHOTS=<dir>. */
void ShowNotes_Test::shots()
{
    const QString out = qEnvironmentVariable("QLC_NOTES_SHOTS");
    if (out.isEmpty())
        QSKIP("QLC_NOTES_SHOTS not set");
    QDir().mkpath(out);
    const QPalette was = qApp->palette();
    qApp->setPalette(App::themePalette(App::ThemeVSCodeDark, was));

    ShowTreeController *st = m_doc->showTree();
    st->addNote(NoteKind::Cue, m_cue1, "House to half on the second bell, not the first", NoteView(),
                invalidId(), "Branson");
    const quint32 fixed = st->addNote(NoteKind::Cue, m_cue2, "Towers come up late on the downbeat",
                                      NoteView(), invalidId(), "Branson");
    st->setNoteDone(fixed, true);
    st->addNote(NoteKind::Layer, m_layer, "Towers flicker below 10%", NoteView(), m_cue2, "Sam (LD)");
    st->addNote(NoteKind::List, m_list, "Act 1 runs four minutes long", NoteView(), invalidId(), "SM");
    st->addNote(NoteKind::Palette, m_palette, "Half reads hot on the stream camera", NoteView(), invalidId(),
                "Branson");
    m_rig->setAngledView(35.0, 28.0);
    st->previewCue(m_cue2);

    const QList<QSize> sizes { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (const QSize &size : sizes)
    {
        const QString tag = QString("%1x%2").arg(size.width()).arg(size.height());
        ShowTreeView view(nullptr, m_doc);
        view.resize(size);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(view.selectNode(m_cue2));
        if (size == sizes.first())
            view.notesPane()->addNote("The DS edge reads too blue against the cyc — warm it a touch", true);
        view.tree()->expandAll();
        QCoreApplication::processEvents();
        QTest::qWait(50);
        view.grab().save(QString("%1/after-showtree-%2.png").arg(out, tag));
        view.notesPane()->grab().save(QString("%1/after-pane-%2.png").arg(out, tag));
        view.tree()->grab().save(QString("%1/after-rowmarks-%2.png").arg(out, tag));

        // The hover: the tooltip a cue row with notes shows.
        if (QTreeWidgetItem *row = rowOf(view, ShowTreeView::CueNode, m_cue2))
        {
            const QRect r = view.tree()->visualItemRect(row);
            QToolTip::showText(view.tree()->viewport()->mapToGlobal(r.center()), row->toolTip(0),
                               view.tree()->viewport());
            QCoreApplication::processEvents();
            QTest::qWait(50);
            for (QWidget *w : QApplication::topLevelWidgets())
                if (w->inherits("QTipLabel") && w->isVisible())
                    w->grab().save(QString("%1/after-hover-%2.png").arg(out, tag));
            QToolTip::hideText();
        }

        // The notes list, at the same size class.
        ShowNotesList *list = view.showAllNotes();
        list->resize(qMin(size.width() - 80, 1100), qMin(size.height() - 120, 640));
        QVERIFY(QTest::qWaitForWindowExposed(list));
        QCoreApplication::processEvents();
        list->grab().save(QString("%1/after-noteslist-%2.png").arg(out, tag));
        list->close();
    }
    qApp->setPalette(was);
}

QTEST_MAIN(ShowNotes_Test)
