/*
  Q Light Controller Plus
  songsections_test.cpp

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
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "showtreecontroller.h"
#include "functionparent.h"
#include "showtreeview.h"
#include "timecodesource.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "cuerunner.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

#include "songsections_test.h"

using namespace ShowTree;

/* Song sections in the Show Tree and the Run tab (Q190): header rows the
   cues fold under, a section's page, the production's Numbering, and Merge
   lists into one… -- after which Run shows the whole show. */

namespace
{
Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

QList<QTreeWidgetItem *> sectionRows(QTreeWidget *tree)
{
    QList<QTreeWidgetItem *> out;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::SectionNode)
            out << *it;
    return out;
}

QStringList childTexts(QTreeWidgetItem *item, int column)
{
    QStringList out;
    for (int i = 0; i < item->childCount(); ++i)
        out << item->child(i)->text(column);
    return out;
}
} // namespace

void ShowTreeSectionsUi_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
}

void ShowTreeSectionsUi_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

QList<quint32> ShowTreeSectionsUi_Test::twoSongs()
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Front");
    m_doc->addFixtureGroup(g);
    g->assignFixture(m_fixtures[0]);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);

    ShowTreeController *st = m_doc->showTree();
    m_production = st->addProduction("Dance Hysteria");
    const quint32 pre = st->addCueList(m_production, "00-Preshow");
    const quint32 song = st->addCueList(m_production, "01-Everybody Scream");
    const quint32 p1 = st->addCue(pre, -1, "Setup");
    const quint32 p2 = st->addCue(pre, 0, "Announcer");
    const quint32 s1 = st->addCue(song, -1, "Music starts");
    const quint32 s2 = st->addCue(song, 0, "Chorus");
    for (quint32 c : { p1, s1 })
        st->setTargetLook(c, groupTarget(g->id()), st->lookForPalette(full->id()));
    st->setCurrentProduction(m_production);
    return { pre, song, p1, p2, s1, s2 };
}

void ShowTreeSectionsUi_Test::sectionRowsFoldInTheTree()
{
    const QList<quint32> ids = twoSongs();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    view.reload();
    // No scheme: no section rows (every older show).
    QVERIFY(sectionRows(view.tree()).isEmpty());

    QVERIFY(st->mergeLists(m_production, { ids[0], ids[1] }));
    QList<QTreeWidgetItem *> rows = sectionRows(view.tree());
    QCOMPARE(rows.count(), 2);
    QCOMPARE(rows.at(0)->text(0), QString("0 — 00-Preshow"));
    QCOMPARE(rows.at(1)->text(0), QString("1 — 01-Everybody Scream"));
    QCOMPARE(rows.at(1)->parent()->data(0, ShowTreeView::IdRole).toUInt(), ids[0]);
    // The cues fold under their section.
    QCOMPARE(childTexts(rows.at(1), 0), QStringList({ "1.0.0 Music starts", "1.0.1 Chorus" }));
    QVERIFY(rows.at(1)->isExpanded());
    // A cue is found and selected under its section.
    QVERIFY(view.selectNode(ids[5]));
    QCOMPARE(view.currentCueId(), ids[5]);
    // Folded by hand: still folded after the tree is rebuilt.
    rows.at(0)->setExpanded(false);
    QVERIFY(st->setSectionName(ids[0], st->sections(ids[0]).at(1), "Everybody Scream"));
    rows = sectionRows(view.tree());
    QCOMPARE(rows.at(1)->text(0), QString("1 — Everybody Scream"));
    QVERIFY(!rows.at(0)->isExpanded());
    QVERIFY(rows.at(1)->isExpanded());

    // F2 on a section row renames the section (its name, not the key).
    view.tree()->setCurrentItem(rows.at(1));
    QVERIFY(view.startRename());
    QTreeWidgetItem *editing = view.tree()->currentItem();
    QCOMPARE(editing->text(0), QString("Everybody Scream"));
    view.tree()->closePersistentEditor(editing, 0);
    editing->setText(0, "Scream");
    QTRY_COMPARE(sectionTitle(st->sections(ids[0]).at(1)), QString("1 — Scream"));
    QCOMPARE(st->cueList(ids[0])->name, QString("Dance Hysteria"));     // not the list
}

void ShowTreeSectionsUi_Test::theSectionPageNamesAndBlocks()
{
    const QList<quint32> ids = twoSongs();
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->mergeLists(m_production, { ids[0], ids[1] }));
    ShowTreeView view(nullptr, m_doc);
    view.reload();
    QTreeWidgetItem *song = sectionRows(view.tree()).at(1);
    view.tree()->setCurrentItem(song);
    QCOMPARE(view.currentKind(), ShowTreeView::SectionNode);
    Section s;
    QVERIFY(view.currentSection(&s));
    QCOMPARE(s.key, QString("1"));

    QWidget *page = view.findChild<QWidget *>("stSectionPage");
    QVERIFY(page != nullptr && page->isVisibleTo(&view));
    QLineEdit *name = view.findChild<QLineEdit *>("stSectionName");
    QCOMPARE(name->text(), QString("01-Everybody Scream"));
    name->setText("Everybody Scream");
    emit name->editingFinished();
    QCOMPARE(sectionTitle(st->sections(ids[0]).at(1)), QString("1 — Everybody Scream"));
    QCOMPARE(sectionRows(view.tree()).at(1)->text(0), QString("1 — Everybody Scream"));

    // Block at section start: the merge made it so (the list's start);
    // unticked, the song's first cue tracks from the preshow.
    view.tree()->setCurrentItem(sectionRows(view.tree()).at(1));
    QCheckBox *block = view.findChild<QCheckBox *>("stSectionBlock");
    QVERIFY(block->isChecked());
    block->setChecked(false);
    QCOMPARE(st->cue(ids[4])->mode, CueMode::ListDefault);
    block->setChecked(true);
    QCOMPARE(st->cue(ids[4])->mode, CueMode::Block);

    // + Cue (and New here, ⇧⌘N) with the section selected: at its end,
    // numbered in it.
    view.tree()->setCurrentItem(sectionRows(view.tree()).at(0));
    QCOMPARE(view.newHereLabel(), QString("New Cue After 0.0.1"));
    view.tree()->setCurrentItem(sectionRows(view.tree()).at(1));
    QVERIFY(QMetaObject::invokeMethod(&view, "slotNewCue"));
    const CueList *list = st->cueList(ids[0]);
    QCOMPARE(list->cues.count(), 5);
    QCOMPARE(list->cues.last().number, QString("1.0.2"));
    // + Section: the next song, 2.0.1.
    const quint32 next = view.newSection("Closer", false);
    QCOMPARE(st->cue(next)->number, QString("2.0.1"));
    QCOMPARE(sectionRows(view.tree()).last()->text(0), QString("2 — Closer"));
}

void ShowTreeSectionsUi_Test::theProductionPageSuggestsAScheme()
{
    twoSongs();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    view.reload();
    QVERIFY(view.selectNode(m_production));
    QComboBox *numbering = view.findChild<QComboBox *>("stProdNumbering");
    QLabel *hint = view.findChild<QLabel *>("stProdNumberingHint");
    QVERIFY(numbering != nullptr && hint != nullptr);
    QCOMPARE(numbering->currentData().toInt(), int(Numbering::None));
    // "01-…" lists number 1.0.0..: it looks like Show.Part.Iteration.
    QVERIFY(!hint->isHidden());
    QVERIFY(hint->text().contains("Show.Part.Iteration"));
    // Confirmed with the link, it's the show's.
    emit hint->linkActivated("use:ShowPartIteration");
    QCOMPARE(st->numbering(m_production), Numbering::ShowPartIteration);
    QCOMPARE(numbering->currentData().toInt(), int(Numbering::ShowPartIteration));
    QVERIFY(hint->isHidden());
    // Or picked: Hundreds.
    numbering->setCurrentIndex(numbering->findData(int(Numbering::Hundreds)));
    emit numbering->activated(numbering->currentIndex());
    QCOMPARE(st->numbering(m_production), Numbering::Hundreds);
    QVERIFY(view.findChild<QPushButton *>("stProdMerge")->isEnabled());
}

/* A show with no scheme and S.P.I numbers: the cue list's page says so, where
   the user looks for sections; Use it sets the scheme; with nothing to
   suggest it points at the production's page. */
void ShowTreeSectionsUi_Test::theListPageSuggestsAScheme()
{
    const QList<quint32> ids = twoSongs();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeView view(nullptr, m_doc);
    view.resize(900, 700);
    view.reload();
    QVERIFY(view.selectNode(ids[1]));
    QLabel *hint = view.findChild<QLabel *>("stListSectionHint");
    QPushButton *add = view.findChild<QPushButton *>("stListAddSection");
    QVERIFY(hint != nullptr && add != nullptr);
    QVERIFY(!hint->isHidden());
    QVERIFY(hint->text().contains("Show.Part.Iteration"));
    QVERIFY(!add->isEnabled());
    QVERIFY(!add->toolTip().isEmpty());
    const QString shots = QString::fromLocal8Bit(qgetenv("QLC_SECHINT_SHOTS"));
    auto shoot = [&](const QString &name) {
        if (shots.isEmpty())
            return;
        QDir().mkpath(shots);
        for (const QSize &sz : { QSize(700, 500), QSize(1000, 700), QSize(1400, 900) })
        {
            view.resize(sz);
            QTest::qWait(50);
            QVERIFY(view.grab().save(QString("%1/%2-%3x%4.png").arg(shots, name).arg(sz.width()).arg(sz.height())));
        }
    };
    shoot("hint");
    emit hint->linkActivated("use:ShowPartIteration");
    QCOMPARE(st->numbering(m_production), Numbering::ShowPartIteration);
    QVERIFY(hint->isHidden());
    QVERIFY(add->isEnabled());
    QVERIFY(!st->sections(ids[1]).isEmpty());
    shoot("after");
    // One undo step puts it back.
    m_doc->showTree()->undo();
    QCOMPARE(st->numbering(m_production), Numbering::None);
    QVERIFY(!hint->isHidden());

    // Nothing to suggest: one quiet line, its link selects the production.
    const quint32 lone = st->addProduction("Plain");
    const quint32 list = st->addCueList(lone, "Cues");
    st->addCue(list, -1, "Odd");
    view.reload();
    QVERIFY(view.selectNode(list));
    QVERIFY(!hint->isHidden());
    QVERIFY(hint->text().contains("No song sections"));
    shoot("nosuggest");
    emit hint->linkActivated("production");
    QCOMPARE(view.currentId(), lone);
}

/* The user's case: a list per song, and GO / Run played only the first.
   Merge from the production page; Run then shows every section, its cues
   under it, and GO goes through the whole show. */
void ShowTreeSectionsUi_Test::runningOrderReordersTheRun()
{
    const QList<quint32> ids = twoSongs();
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->mergeLists(m_production, { ids[0], ids[1] }));
    ShowTreeView view(nullptr, m_doc);
    view.reload();
    QVERIFY(view.selectNode(m_production));
    QPushButton *order = view.findChild<QPushButton *>("stProdOrder");
    QVERIFY(order != nullptr);
    QVERIFY(order->isEnabled());
    // Song 1 first, the preshow after it, tonight.
    view.setOrderPicker([](const QList<RunningSong> &have) {
        QList<RunningSong> out = have;
        std::swap(out[0], out[1]);
        return out;
    });
    order->click();
    QVERIFY(st->hasRunningOrder(m_production));

    CueRunner runner(m_doc, CueRunner::Full);
    runner.refresh();
    QTreeWidget *tree = runner.cueTree();
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("1 — 01-Everybody Scream"));
    QCOMPARE(tree->topLevelItem(1)->text(0), QString("0 — 00-Preshow"));
    QCOMPARE(childTexts(tree->topLevelItem(0), 1), QStringList({ "1.0.0", "1.0.1" }));
    // The first cue to go is the song's.
    QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QString("NEXT"));
    // The cues are still where the numbers put them.
    QCOMPARE(st->cueList(ids[0])->cues.first().id, ids[2]);

    // Leave the song out: GO passes over it.
    view.setOrderPicker([](const QList<RunningSong> &have) {
        QList<RunningSong> out = have;
        out[0].skip = true;
        return out;
    });
    order->click();
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser));
    QVERIFY(ch->isStepSkipped(0));
    QVERIFY(!ch->isStepSkipped(2));

    QVERIFY(view.editRunningOrder(m_production, {}, true));
    QVERIFY(!st->hasRunningOrder(m_production));
    runner.refresh();
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("0 — 00-Preshow"));
}

void ShowTreeSectionsUi_Test::mergeThenRunShowsEverySection()
{
    const QList<quint32> ids = twoSongs();
    ShowTreeController *st = m_doc->showTree();
    {
        CueRunner before(m_doc, CueRunner::Full);
        before.refresh();
        QCOMPARE(before.cueTree()->topLevelItemCount(), 2);     // the preshow only
    }
    ShowTreeView view(nullptr, m_doc);
    view.reload();
    view.setMergePicker([ids](quint32) { return QList<quint32>({ ids[0], ids[1] }); });
    QStringList said;
    view.setConfirm([&said](const QString &, const QString &text, const QString &details) {
        said << text << details;
        return true;
    });
    QVERIFY(view.selectNode(m_production));
    QPushButton *merge = view.findChild<QPushButton *>("stProdMerge");
    QVERIFY(merge->isEnabled());
    merge->click();
    QCOMPARE(st->playableLists(m_production), QList<quint32>({ ids[0] }));
    QVERIFY(said.isEmpty());        // nothing renumbered, nothing lost: no dialog

    CueRunner runner(m_doc, CueRunner::Full);
    runner.refresh();
    QTreeWidget *tree = runner.cueTree();
    QCOMPARE(tree->topLevelItemCount(), 2);
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("0 — 00-Preshow"));
    QCOMPARE(tree->topLevelItem(1)->text(0), QString("1 — 01-Everybody Scream"));
    QCOMPARE(childTexts(tree->topLevelItem(1), 1), QStringList({ "1.0.0", "1.0.1" }));
    QCOMPARE(childTexts(tree->topLevelItem(0), 2), QStringList({ "Setup", "Announcer" }));
    // A header says nothing of its own state; the first cue is NEXT.
    QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QString("NEXT"));
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("0 — 00-Preshow"));
    // The song is the same list's: LOAD its first cue, it is on standby.
    runner.load("1.0.0");
    runner.refresh();
    QCOMPARE(tree->topLevelItem(1)->child(0)->text(0), QString("▷ STANDBY"));
    QVERIFY(runner.nextText().startsWith("▷ STANDBY 1.0.0 Music starts"));
    // Folded, it stays folded as the runner refreshes; a click on it loads nothing.
    tree->topLevelItem(0)->setExpanded(false);
    QVERIFY(st->setSectionName(ids[0], st->sections(ids[0]).at(0), "Preshow"));
    runner.refresh();
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("0 — Preshow"));
    QVERIFY(!tree->topLevelItem(0)->isExpanded());
    QVERIFY(tree->topLevelItem(1)->isExpanded());
}

/* Q198: a section's Starts at (typed, Later sections follow), a cue's
   Anchor and what its time counts from, the list page's Section times…,
   Import Logic markers… (a MIDI file's markers), Learn section starts,
   and the cue's ripple / stretch. */
void ShowTreeSectionsUi_Test::timecodeAnchorsOnThePages()
{
    const QList<quint32> ids = twoSongs();
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->mergeLists(m_production, { ids[0], ids[1] }));
    const quint32 list = ids[0];
    ShowTreeView view(nullptr, m_doc);
    QStringList asked;
    view.setConfirm([&asked](const QString &title, const QString &, const QString &details) {
        asked << title + QStringLiteral(": ") + details;
        return true;
    });
    view.resize(1280, 700);
    view.reload();

    // Not following timecode: no Starts at, no timecode row on the list.
    view.tree()->setCurrentItem(sectionRows(view.tree()).at(1));
    QLineEdit *start = view.findChild<QLineEdit *>("stSectionStart");
    QVERIFY(start != nullptr && !start->isVisibleTo(&view));
    QVERIFY(st->setListFollowTimecode(list, true));

    // The section page: typed.
    view.tree()->setCurrentItem(sectionRows(view.tree()).at(1));
    QVERIFY(start->isVisibleTo(&view));
    start->setText("00:00:30:00");
    emit start->editingFinished();
    QCOMPARE(st->sectionStart(list, st->sections(list).at(1)), 30000);

    // A cue's time counts from it; the cue page says from what.
    QVERIFY(st->setCueTimecode(ids[5], 34000));
    QVERIFY(view.selectNode(ids[5]));
    QLabel *base = view.findChild<QLabel *>("stCueTimecodeBase");
    QVERIFY(base != nullptr && base->isVisibleTo(&view));
    QCOMPARE(base->text(), QString("+4.0 s from 1 — 01-Everybody Scream"));
    QCheckBox *anchor = view.findChild<QCheckBox *>("stCueTimecodeAnchor");
    QVERIFY(anchor != nullptr && !anchor->isChecked());
    anchor->setChecked(true);
    QVERIFY(st->isCueTimecodeAnchor(ids[5]));
    QVERIFY(st->undo());

    // Moving the start moves the cue.
    view.tree()->setCurrentItem(sectionRows(view.tree()).at(1));
    QCheckBox *follow = view.findChild<QCheckBox *>("stSectionFollow");
    QVERIFY(follow != nullptr);
    start->setText("00:00:36:00");
    emit start->editingFinished();
    QCOMPARE(st->cue(ids[5])->timecode, 40000);

    // The list page: Section times… for the cue with a time of its own.
    QVERIFY(st->setCueTimecode(ids[3], 1000));
    QVERIFY(view.selectNode(list));
    QPushButton *convert = view.findChild<QPushButton *>("stListTcConvert");
    QVERIFY(convert != nullptr && convert->isVisibleTo(&view));
    convert->click();
    QVERIFY(st->cue(ids[3])->tcFromSection);
    QCOMPARE(st->sectionStart(list, st->sections(list).at(0)), 1000);
    QVERIFY(asked.last().startsWith("Section times"));
    QVERIFY(view.selectNode(list));
    QVERIFY(!convert->isVisibleTo(&view));

    // Import Logic markers…: a MIDI file's marker named for the song.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto vlq = [](quint32 n) {
        QByteArray out;
        out.prepend(char(n & 0x7F));
        while ((n >>= 7) != 0)
            out.prepend(char((n & 0x7F) | 0x80));
        return out;
    };
    const QByteArray name("01-Everybody Scream");
    QByteArray track = vlq(50000) + char(0xFF) + char(0x06) + vlq(quint32(name.size())) + name
                       + vlq(0) + char(0xFF) + char(0x2F) + char(0);
    auto be = [](quint32 v, int bytes) {
        QByteArray out;
        for (int i = bytes - 1; i >= 0; --i)
            out += char((v >> (8 * i)) & 0xFF);
        return out;
    };
    const QByteArray mid = QByteArray("MThd") + be(6, 4) + be(0, 2) + be(1, 2) + be(0xE728, 2)   // 25 fps x 40: 1 ms
                           + QByteArray("MTrk") + be(quint32(track.size()), 4) + track;
    const QString path = dir.filePath("export.mid");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(mid);
    }
    st->clearUndo();
    QVERIFY(view.importLogicMarkers(list, path));
    QVERIFY2(asked.last().contains("1 — 01-Everybody Scream moved +14.0 s, 1 cue(s) follow"), qPrintable(asked.last()));
    QVERIFY(asked.last().contains("No marker for 0 — 00-Preshow"));
    QCOMPARE(st->cue(ids[5])->timecode, 54000);
    QCOMPARE(st->undoText(), QString("Import Logic markers"));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(ids[5])->timecode, 40000);
    QVERIFY(!view.importLogicMarkers(list, dir.filePath("none.mid")));

    // Learn section starts: a tap a section.
    QVERIFY(view.selectNode(list));
    QPushButton *learn = view.findChild<QPushButton *>("stListTcLearn");
    QVERIFY(learn != nullptr && learn->isVisibleTo(&view));
    QCOMPARE(learn->text(), QString("Learn Section Starts"));
    learn->click();
    QCOMPARE(view.learnSection(), QString("0 — 00-Preshow"));
    QCOMPARE(learn->text(), QString("Tap at: 0 — 00-Preshow"));
    m_doc->timecodeSource()->updateTimeCode(0, 2000, 30);
    learn->click();
    QCOMPARE(st->sectionStart(list, st->sections(list).at(0)), 2000);
    QVERIFY(view.learnTap(41000));
    QCOMPARE(st->cue(ids[5])->timecode, 45000);
    QVERIFY(view.learnSection().isEmpty());
    QCOMPARE(learn->text(), QString("Learn Section Starts"));
    QVERIFY(!view.learnTap(50000));

    // Ripple and stretch, as the cue's Ripple timecode… does.
    QVERIFY(view.rippleCue(ids[5], ShowTreeView::RippleInsert, 1500));
    QCOMPARE(st->cue(ids[5])->timecode, 46500);
    QVERIFY(view.rippleCue(ids[5], ShowTreeView::RippleRemove, 1500));
    QCOMPARE(st->cue(ids[5])->timecode, 45000);
    QVERIFY(view.rippleCue(ids[5], ShowTreeView::RippleStretch, 8000));
    QCOMPARE(st->cue(ids[5])->timecode, 49000);
}

QTEST_MAIN(ShowTreeSectionsUi_Test)
