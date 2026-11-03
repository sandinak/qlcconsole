/*
  Q Light Controller Plus - Unit test
  showreviewpanel_test.cpp

  The Review panel (docs/features/show-review.md), offscreen.

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
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>

#include "showreviewpanel_test.h"
#include "showstatusdialog.h"
#include "showreviewpanel.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "showtreeview.h"
#include "qlcpalette.h"
#include "scenery.h"
#include "showreviewmarker.h"
#include "structurestudioview.h"
#include "stageplatform.h"
#include "contextmenus.h"
#include <QMenu>
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "showstatus.h"
#include "showreview.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
Fixture *dimmer(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Lamp");
    def->setType(QLCFixtureDef::Dimmer);
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setPreset(QLCChannel::IntensityDimmer);
    def->addChannel(ch);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Mode");
    mode->insertChannel(ch, 0);
    QLCPhysical phy;
    phy.setPowerConsumption(500);
    mode->setPhysical(phy);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

QTreeWidgetItem *rowOf(QTreeWidget *tree, const QString &check)
{
    QTreeWidgetItemIterator it(tree);
    while (*it)
    {
        if ((*it)->text(0) == check)
            return *it;
        ++it;
    }
    return nullptr;
}

int topCount(QTreeWidget *tree, const QString &prefix)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        if (tree->topLevelItem(i)->text(0).startsWith(prefix))
            return tree->topLevelItem(i)->childCount();
    return -1;
}
}

void ShowReviewPanel_Test::init()
{
    m_doc = new Doc(this);
    // A blocker (an overlap) and a note (two names alike).
    dimmer(m_doc, "Lamp 1", 0);
    Fixture *b = dimmer(m_doc, "Lamp 2", 5);
    b->setAddress(0);
    b->setName("Lamp 1");
}

void ShowReviewPanel_Test::cleanup()
{
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        ShowStatus::instance()->clearStatus(e.key);
    delete m_doc;
    m_doc = nullptr;
}

void ShowReviewPanel_Test::listsBySeverity()
{
    ShowReviewPanel panel(m_doc);
    QVERIFY(panel.summary()->text().contains("Not checked"));
    panel.recheck();
    QCOMPARE(topCount(panel.tree(), "Blockers"), 1);
    QVERIFY(topCount(panel.tree(), "Notes") >= 1);    // and "no output anywhere"
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
    QVERIFY(rowOf(panel.tree(), "FIX-14") != nullptr);
    QVERIFY(panel.summary()->text().contains("1 blockers"));
    // Why? is on the row: the check's title and paragraph.
    QVERIFY(rowOf(panel.tree(), "PAT-01")->toolTip(1).contains("fight"));
}

void ShowReviewPanel_Test::acceptAndReopen()
{
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("PAT-01"));
    QVERIFY(panel.acceptButton()->isEnabled());
    QVERIFY(!panel.reopenButton()->isEnabled());
    QVERIFY(!panel.acceptSelected("  ", "Ava"));            // a note is required
    QVERIFY(panel.acceptSelected("Doubled lamp on purpose", "Ava R."));
    QCOMPARE(m_doc->showReview()->acceptances().count(), 1);
    QCOMPARE(m_doc->showReview()->acceptances().first().by, QStringLiteral("Ava R."));
    // Gone from Open, there under Accepted with its note.
    QVERIFY(rowOf(panel.tree(), "PAT-01") == nullptr);
    panel.setFilter(ShowReviewPanel::AcceptedOnly);
    QTreeWidgetItem *row = rowOf(panel.tree(), "PAT-01");
    QVERIFY(row != nullptr);
    QVERIFY(row->text(1).contains("Doubled lamp on purpose"));
    QVERIFY(row->text(1).contains("Ava R."));
    QVERIFY(panel.summary()->text().contains("1 accepted"));
    QVERIFY(panel.selectCheck("PAT-01"));
    QVERIFY(panel.reopenButton()->isEnabled());
    QVERIFY(panel.reopenSelected());
    QVERIFY(m_doc->showReview()->acceptances().isEmpty());
}

void ShowReviewPanel_Test::filters()
{
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("FIX-14"));
    QVERIFY(panel.acceptSelected("Fine", "Ava"));
    panel.setFilter(ShowReviewPanel::OpenOnly);
    QVERIFY(rowOf(panel.tree(), "FIX-14") == nullptr);
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
    panel.setFilter(ShowReviewPanel::All);
    QVERIFY(rowOf(panel.tree(), "FIX-14") != nullptr);
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
    panel.setSearch("share");
    QVERIFY(rowOf(panel.tree(), "FIX-14") == nullptr);
    QVERIFY(rowOf(panel.tree(), "PAT-01") != nullptr);
}

void ShowReviewPanel_Test::fixGoesToThePlace()
{
    QStringList ran;
    ShowStatus::instance()->setAction("fixtures.duplicates", "Number Them",
        [&ran](QWidget *, const QVariantList &items) { ran << items.first().toString(); });
    QString jumped;
    ShowReviewPanel::setJumper([&jumped](QWidget *, const QString &jump, const QVariantList &) {
        jumped = jump;
        return true;
    });
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("FIX-14"));
    QVERIFY(panel.fixButton()->isEnabled());
    QVERIFY(panel.fixSelected());
    QCOMPARE(ran, QStringList() << "Lamp 1");              // the footer's own fix
    ShowReviewPanel::setJumper(ShowReviewPanel::Jumper());
}

void ShowReviewPanel_Test::footerEntriesFoldIn()
{
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(rowOf(panel.tree(), "FIX-03") == nullptr);
    // A design entry of the footer that no check reads yet: under its id.
    ShowStatus::instance()->setStatus("definitions.outofdate", ShowStatus::Warning,
                                      "2 fixture definitions are out of date");
    QTreeWidgetItem *row = rowOf(panel.tree(), "FIX-03");
    QVERIFY(row != nullptr);
    QVERIFY(row->text(1).contains("out of date"));
    // A runtime one stays the footer's.
    ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "Node not answering");
    QTreeWidgetItemIterator it(panel.tree());
    while (*it)
    {
        QVERIFY(!(*it)->text(1).contains("Node not answering"));
        ++it;
    }
    // A footer entry a check reads is not listed twice.
    ShowStatus::instance()->setStatus("fixtures.clash", ShowStatus::Warning, "1 fixture clashes");
    int pat01 = 0;
    QTreeWidgetItemIterator again(panel.tree());
    while (*again)
    {
        if ((*again)->text(0) == "PAT-01")
            ++pat01;
        ++again;
    }
    QCOMPARE(pat01, 1);
}

void ShowReviewPanel_Test::readinessDialogLeadsHere()
{
    ShowStatus::instance()->setStatus("fixtures.clash", ShowStatus::Warning, "1 fixture clashes");
    ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "Node not answering");
    QWidget host;
    ShowReviewPanel::setShared(m_doc, &host);
    ShowStatusDialog dlg;
    bool sawId = false, sawRuntime = false;
    for (int i = 0; i < dlg.tree()->topLevelItemCount(); ++i)
    {
        const QString t = dlg.tree()->topLevelItem(i)->text(0);
        sawId = sawId || t == QStringLiteral("PAT-01 · 1 fixture clashes");
        sawRuntime = sawRuntime || t == QStringLiteral("Node not answering");
    }
    QVERIFY(sawId);
    QVERIFY(sawRuntime);
    QPushButton *review = dlg.findChild<QPushButton *>("showStatusReview");
    QVERIFY(review != nullptr);
    review->click();
    QVERIFY(ShowReviewPanel::shared() != nullptr);
    QVERIFY(ShowReviewPanel::shared()->isVisible());
    QVERIFY(m_doc->showReview()->lastResult().ran);         // opened: checked
    ShowReviewPanel::shared()->close();
    ShowReviewPanel::setShared(nullptr, nullptr);
}

namespace
{
/* A cyc 10 m wide at y = 2 m, a front light 10 m downstage of it at 6 m,
   thrown upstage 20 degrees down -- its light carries on to the cyc -- and
   a list of two cues, the light up in both. Returns the cue ids. */
QList<quint32> backwashShow(Doc *doc, Fixture **lightOut)
{
    MonitorProperties *mp = doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    Scenery *cyc = mp->addScenery();
    cyc->setName("Cyc");
    cyc->setOriginX(5.0f);
    cyc->setOriginY(2.0f);
    Fixture *front = dimmer(doc, "Front 1", 20);
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(5000.0f, 12000.0f, 6.0f);
    mp->setFixtureProperties(front->id(), item);
    FixtureRigProps rp = mp->fixtureRigProps(front->id());
    rp.fixedAim = true;
    rp.panZeroDir = 180;
    rp.aimDown = 20;
    mp->setFixtureRigProps(front->id(), rp);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setValue(255);
    full->setName("Full");
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    QList<quint32> cues { st->addCue(lid, -1, "One") };
    cues << st->addCue(lid, 0, "Two");
    for (quint32 c : cues)
    {
        ShowTree::Target t;
        t.fixtures = { front->id() };
        const quint32 layer = st->addLayer(c, t);
        ShowTree::Look d;
        d.key = ShowTree::KeyDimmer;
        d.palettes = { full->id() };
        st->setLayerLook(c, layer, d);
    }
    if (lightOut != nullptr)
        *lightOut = front;
    return cues;
}
}

void ShowReviewPanel_Test::iMeantItFromTheRow()
{
    Fixture *front = nullptr;
    const QList<quint32> cues = backwashShow(m_doc, &front);
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QTreeWidgetItem *row = rowOf(panel.tree(), "PWR-12");
    QVERIFY(row != nullptr);
    QVERIFY(row->text(1).contains("Front 1"));
    panel.tree()->clearSelection();
    row->setSelected(true);
    QCOMPARE(panel.acceptButton()->text(), QStringLiteral("I Meant It"));
    QCOMPARE(panel.fixButton()->text(), QStringLiteral("Show Me ▸"));
    // Show Me: the layer that lights it in its first cue.
    QString jump;
    QVariantList data;
    ShowReviewPanel::setJumper([&](QWidget *, const QString &j, const QVariantList &d) {
        jump = j;
        data = d;
        return true;
    });
    QVERIFY(panel.fixSelected());
    QCOMPARE(jump, QStringLiteral("cue"));
    QCOMPARE(data.first().toUInt(), m_doc->showTree()->cue(cues.first())->layers.first().id);
    ShowReviewPanel::setJumper(ShowReviewPanel::Jumper());
    // I Meant It: no note asked; the row goes to Accepted.
    panel.acceptButton()->click();
    QCOMPARE(m_doc->showTree()->cue(cues.first())->layers.first().meantOnScenery,
             QList<quint32>() << front->id());
    panel.setFilter(ShowReviewPanel::OpenOnly);
    QVERIFY(rowOf(panel.tree(), "PWR-12") == nullptr);
    panel.setFilter(ShowReviewPanel::AcceptedOnly);
    row = rowOf(panel.tree(), "PWR-12");
    QVERIFY(row != nullptr);
    QVERIFY(row->text(1).contains("I meant it"));
    row->setSelected(true);
    QCOMPARE(panel.reopenButton()->text(), QStringLiteral("Not Meant"));
    panel.reopenButton()->click();
    QVERIFY(m_doc->showTree()->cue(cues.first())->layers.first().meantOnScenery.isEmpty());
    panel.setFilter(ShowReviewPanel::OpenOnly);
    QVERIFY(rowOf(panel.tree(), "PWR-12") != nullptr);
    // Other rows keep Accept… and Reopen.
    row = rowOf(panel.tree(), "PAT-01");
    QVERIFY(row != nullptr);
    panel.tree()->clearSelection();
    row->setSelected(true);
    QCOMPARE(panel.acceptButton()->text(), QStringLiteral("Accept…"));
    QCOMPARE(panel.fixButton()->text(), QStringLiteral("Fix ▸"));
}

void ShowReviewPanel_Test::showTreeMarksBreachingCues()
{
    const QList<quint32> cues = backwashShow(m_doc, nullptr);
    ShowTreeView view(nullptr, m_doc);
    m_doc->showReview()->run();
    auto rowOfCue = [&view](quint32 id) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id)
                return *it;
        return nullptr;
    };
    // Both cues the light lands in are marked, the fixture named.
    for (quint32 c : cues)
    {
        QTreeWidgetItem *item = rowOfCue(c);
        QVERIFY(item != nullptr);
        QVERIFY(item->text(ShowTreeView::FlagsCol).contains(QStringLiteral("⚠ cyc")));
        QVERIFY(item->toolTip(ShowTreeView::FlagsCol).contains("Front 1"));
        QVERIFY(item->toolTip(ShowTreeView::FlagsCol).contains("not marked as meant"));
        QVERIFY(!view.backwashFindingKey(c).isEmpty());
    }
    // A click on the mark opens Show Review on that row.
    ShowReviewPanel::setShared(m_doc, nullptr);
    QVERIFY(view.openBackwashFinding(cues.last()));
    ShowReviewPanel *panel = ShowReviewPanel::shared();
    QVERIFY(panel != nullptr);
    QCOMPARE(panel->selectedFindings().count(), 1);
    QCOMPARE(panel->selectedFindings().first().check, QStringLiteral("PWR-12"));
    // I meant it: the marks go, the rebuilt tree stays clean.
    QVERIFY(ShowReview::setLightMeant(m_doc, panel->selectedFindings().first(), true));
    m_doc->showReview()->run();
    for (quint32 c : cues)
    {
        QTreeWidgetItem *item = rowOfCue(c);
        QVERIFY(item != nullptr);
        QVERIFY(!item->text(ShowTreeView::FlagsCol).contains(QStringLiteral("⚠ cyc")));
        QVERIFY(view.backwashFindingKey(c).isEmpty());
    }
    // Taken back from the layer: marked again.
    QVERIFY(ShowReview::clearLayerMeant(m_doc, cues.first(),
                                        m_doc->showTree()->cue(cues.first())->layers.first().id));
    m_doc->showReview()->run();
    QVERIFY(rowOfCue(cues.first())->text(ShowTreeView::FlagsCol).contains(QStringLiteral("⚠ cyc")));
    delete panel;
    ShowReviewPanel::setShared(nullptr, nullptr);
}

QTEST_MAIN(ShowReviewPanel_Test)

/* One tooltip and one set of answers for a warning wherever it shows: the
   "⚠ cyc" mark on a cue, the footer chip, a Review row. A right-click clears
   it without the Review panel. */
void ShowReviewPanel_Test::warningMarksShareDetailAndAnswers()
{
    Fixture *front = nullptr;
    const QList<quint32> cues = backwashShow(m_doc, &front);
    ShowTreeView view(nullptr, m_doc);
    m_doc->showReview()->run();
    auto rowOfCue = [&view](quint32 id) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id)
                return *it;
        return nullptr;
    };
    const QList<ReviewFinding> onCue = ShowReviewMarker::cueFindings(m_doc, cues.last());
    QCOMPARE(onCue.count(), 1);
    // The detail: which fixture, how much, on what, the cues, the answers.
    const QString tip = ShowReviewMarker::tooltip(m_doc, onCue);
    QVERIFY2(tip.contains("Front 1") && tip.contains("about") && tip.contains("Cyc"), qPrintable(tip));
    QVERIFY(tip.contains("PWR-12"));
    QVERIFY(tip.contains("One") && tip.contains("Two"));         // lands in both cues
    QVERIFY(tip.contains("I Meant It") && tip.contains("Open in Show Review"));
    // The same builder on the cue's mark, a Review row's badge and the footer.
    QVERIFY(rowOfCue(cues.last())->toolTip(ShowTreeView::FlagsCol).contains(tip.mid(4, tip.size() - 9)));
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QTreeWidgetItem *row = rowOf(panel.tree(), "PWR-12");
    QVERIFY(row != nullptr);
    QCOMPARE(row->toolTip(0), tip);
    QVERIFY(ShowReviewMarker::tooltip(m_doc, ShowReviewMarker::openFindings(m_doc)).contains("Front 1"));

    // The mark's menu: the canon, the backwash answers.
    QMenu menu;
    {
        ContextMenu cm(&menu);
        ShowReviewMarker::fillMenu(cm, m_doc, onCue, nullptr);
    }
    QVERIFY2(ContextMenu::checkCanon(&menu).isEmpty(), qPrintable(ContextMenu::checkCanon(&menu).join("; ")));
    QAction *meantAct = menu.findChild<QAction *>("reviewMeant");
    QVERIFY(meantAct != nullptr);
    QVERIFY(menu.findChild<QAction *>("reviewShowMe") != nullptr);
    QVERIFY(menu.findChild<QAction *>("reviewOpen") != nullptr);
    QCOMPARE(menu.actions().first()->objectName(), QStringLiteral("reviewOpen"));
    // Show Me: the layer that lights it.
    QString jump;
    ShowReviewPanel::setJumper([&](QWidget *, const QString &j, const QVariantList &) { jump = j; return true; });
    menu.findChild<QAction *>("reviewShowMe")->trigger();
    QCOMPARE(jump, QStringLiteral("cue"));
    ShowReviewPanel::setJumper(ShowReviewPanel::Jumper());
    // I Meant It from the mark: kept on the layer, the marks go, no panel.
    QVERIFY(ShowReviewPanel::shared() == nullptr);
    meantAct->trigger();
    QCOMPARE(m_doc->showTree()->cue(cues.first())->layers.first().meantOnScenery, QList<quint32>() << front->id());
    QVERIFY(!rowOfCue(cues.last())->text(ShowTreeView::FlagsCol).contains(QStringLiteral("⚠ cyc")));
    QVERIFY(ShowReviewMarker::cueFindings(m_doc, cues.last()).isEmpty());
    QVERIFY(ShowReviewPanel::shared() == nullptr);
    // Not Meant takes it back, the same engine call as the panel's.
    ReviewFinding intended;
    for (const ReviewFinding &f : m_doc->showReview()->lastResult().findings)
        if (f.check == "PWR-12")
            intended = f;
    QVERIFY(intended.intended);
    QVERIFY(ShowReviewMarker::notMeant(m_doc, intended));
    QVERIFY(rowOfCue(cues.last())->text(ShowTreeView::FlagsCol).contains(QStringLiteral("⚠ cyc")));

    // Several open warnings (the footer chip): one submenu each, canon kept.
    QMenu many;
    {
        ContextMenu cm(&many);
        ShowReviewMarker::fillMenu(cm, m_doc, ShowReviewMarker::openFindings(m_doc), nullptr);
    }
    QVERIFY(ContextMenu::checkCanon(&many).isEmpty());
    QVERIFY(many.findChild<QMenu *>("reviewFinding0") != nullptr);
    // Any other check's mark offers Accept… (with a note) instead.
    for (const ReviewFinding &f : ShowReviewMarker::openFindings(m_doc))
    {
        if (f.check == "PWR-12")
            continue;
        QMenu other;
        {
            ContextMenu cm(&other);
            ShowReviewMarker::fillMenu(cm, m_doc, { f }, nullptr);
        }
        QVERIFY(other.findChild<QAction *>("reviewAccept") != nullptr);
        QVERIFY(other.findChild<QAction *>("reviewMeant") == nullptr);
        QVERIFY(ShowReviewMarker::meant(m_doc, f, nullptr, "on purpose"));
        break;
    }
}

/* "Why is mix in warning colour if the cyc warning is on the same line?" Only
   a warning's badge takes the warning colour; Mix and the rest keep the row's. */
void ShowReviewPanel_Test::onlyTheWarningBadgeIsColoured()
{
    const QList<quint32> cues = backwashShow(m_doc, nullptr);
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->edit("mix", [&cues](QList<ShowTree::Production> &tree) {
        for (ShowTree::Production &p : tree)
            for (ShowTree::CueList &l : p.lists)
                for (ShowTree::Cue &c : l.cues)
                    c.mixOverlap = true;
        return true;
    }));
    ShowTreeView view(nullptr, m_doc);
    m_doc->showReview()->run();
    auto rowOfCue = [&view](quint32 id) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id)
                return *it;
        return nullptr;
    };
    const QColor normal(Qt::gray);
    auto pieces = [&](quint32 cue) {
        QTreeWidgetItem *item = rowOfCue(cue);
        const QModelIndex idx = view.tree()->model()->index(view.tree()->indexOfTopLevelItem(item), 0);
        Q_UNUSED(idx)
        // The item's own index in the flags column.
        QTreeWidget *tw = view.tree();
        QModelIndex at;
        for (QTreeWidgetItemIterator it(tw); *it; ++it)
            if (*it == item)
            {
                tw->setCurrentItem(item, ShowTreeView::FlagsCol);
                at = tw->currentIndex();
            }
        return ShowTreeView::flagPieces(at, normal);
    };
    // With a warning: "mix" in the row's colour, "⚠ cyc" in the warning colour.
    QList<QPair<QString, QColor>> p = pieces(cues.first());
    QCOMPARE(p.count(), 2);
    QCOMPARE(p.at(0).first, QStringLiteral("mix"));
    QCOMPARE(p.at(0).second, normal);
    QCOMPARE(p.at(1).first, QStringLiteral("⚠ cyc"));
    QCOMPARE(p.at(1).second, ShowTreeView::warningColour());
    // The cell itself is not one colour any more.
    QVERIFY(rowOfCue(cues.first())->foreground(ShowTreeView::FlagsCol).color() != ShowTreeView::warningColour());
    // Without a warning (the light dark in cue two): everything normal.
    QVERIFY(st->edit("dark", [&cues](QList<ShowTree::Production> &tree) {
        for (ShowTree::Production &pr : tree)
            for (ShowTree::CueList &l : pr.lists)
                for (ShowTree::Cue &c : l.cues)
                    if (c.id == cues.last())
                        c.layers.clear();
        return true;
    }));
    QVERIFY(st->edit("first only", [&cues](QList<ShowTree::Production> &tree) {
        for (ShowTree::Production &pr : tree)
            for (ShowTree::CueList &l : pr.lists)
                for (ShowTree::Cue &c : l.cues)
                    if (c.id == cues.first())
                        c.layers.clear();
        return true;
    }));
    m_doc->showReview()->run();
    p = pieces(cues.first());
    QCOMPARE(p.count(), 1);
    QCOMPARE(p.at(0).first, QStringLiteral("mix"));
    QCOMPARE(p.at(0).second, normal);
    // Needs work, the other warning badge: that word alone.
    QVERIFY(st->edit("needs work", [&cues](QList<ShowTree::Production> &tree) {
        for (ShowTree::Production &pr : tree)
            for (ShowTree::CueList &l : pr.lists)
                for (ShowTree::Cue &c : l.cues)
                    if (c.id == cues.first())
                        c.needsWork = true;
        return true;
    }));
    p = pieces(cues.first());
    QCOMPARE(p.count(), 2);
    QCOMPARE(p.at(0).second, normal);           // mix
    QCOMPARE(p.at(1).first, QStringLiteral("needs work"));
    QCOMPARE(p.at(1).second, ShowTreeView::warningColour());
}

/* "On the rig .. we shouldn't show colorization of the props .. they should be
   their correct color (black for risers)": a riser is drawn in its finish, not
   its plot colour; a warning on a cyc is an outline, its cloth keeps its colour. */
void ShowReviewPanel_Test::propsKeepTheirOwnColourOnTheRig()
{
    StagePlatform riser(0);
    riser.setColor(QColor("#dd4f18"));          // orange on the plot
    StructureStudioView::PropPaint pp = StructureStudioView::platformPaint(&riser);
    QCOMPARE(pp.fill, StagePlatform::defaultFinish());
    QVERIFY(pp.fill != riser.color());
    QVERIFY(!pp.outline);
    QVERIFY(pp.edge.lightness() > pp.fill.lightness());     // its edge still reads on the dark canvas
    riser.setFinish(QColor("#5a3a22"));
    QCOMPARE(StructureStudioView::platformPaint(&riser).fill, QColor("#5a3a22"));
    pp = StructureStudioView::platformPaint(&riser, true);
    QCOMPARE(pp.fill, QColor("#5a3a22"));                    // a warning never fills
    QVERIFY(pp.outline);

    // The cyc in its own colour; with PWR-12 open on it, the outline flag.
    backwashShow(m_doc, nullptr);
    Scenery *cyc = m_doc->monitorProperties()->sceneries().first();
    StructureStudioView view(m_doc, StructureStudioView::StageKind, 0);
    QVERIFY(!view.sceneryWarned(cyc->id()));                 // not reviewed yet
    m_doc->showReview()->run();
    QVERIFY(view.sceneryWarned(cyc->id()));
    const StructureStudioView::PropPaint cp = StructureStudioView::sceneryPaint(cyc, view.sceneryWarned(cyc->id()));
    QCOMPARE(cp.fill, Scenery::defaultColor(cyc->kind()));
    QVERIFY(cp.outline);
    QCOMPARE(StructureStudioView::sceneryPaint(cyc).fill, cp.fill);
    QVERIFY(!StructureStudioView::sceneryPaint(cyc).outline);
    cyc->setColor(QColor("#e2e4ec"));
    QCOMPARE(StructureStudioView::sceneryPaint(cyc).fill, QColor("#e2e4ec"));
}
