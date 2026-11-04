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
#include <QDir>
#include <QToolTip>
#include <QScrollBar>
#include "app.h"
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
    // Other rows say Not a Problem… and Reopen.
    row = rowOf(panel.tree(), "PAT-01");
    QVERIFY(row != nullptr);
    panel.tree()->clearSelection();
    row->setSelected(true);
    QCOMPARE(panel.acceptButton()->text(), QStringLiteral("Not a Problem…"));
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
        // Meant: green "cyc ✓" in the cue it starts in, nothing in the cues it carries on into.
        if (c == cues.first())
            QVERIFY(item->text(ShowTreeView::FlagsCol).contains(QStringLiteral("cyc ✓")));
        else
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
    // Any other check's mark offers Not a Problem… (with a note), never I Meant It.
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

/* Open (amber "⚠ cyc"), Meant (green "cyc ✓"), Not a Problem (no badge, listed
   under Accepted): every change from the cue's badge, a Review row and the
   footer chip's menu, one undo step each for the mark. */
void ShowReviewPanel_Test::threeStatesEverywhere()
{
    Fixture *front = nullptr;
    const QList<quint32> cues = backwashShow(m_doc, &front);
    ShowTreeView view(nullptr, m_doc);
    ShowReview *sr = m_doc->showReview();
    sr->run();
    auto rowOfCue = [&view](quint32 id) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == id)
                return *it;
        return nullptr;
    };
    const QColor normal(Qt::gray);
    auto pieces = [&](quint32 cue) {
        QTreeWidget *tw = view.tree();
        QTreeWidgetItem *item = rowOfCue(cue);
        tw->setCurrentItem(item, ShowTreeView::FlagsCol);
        return ShowTreeView::flagPieces(tw->currentIndex(), normal);
    };
    auto menuFor = [&](const QList<ReviewFinding> &fs) {
        QMenu *m = new QMenu;
        ContextMenu cm(m);
        ShowReviewMarker::fillMenu(cm, m_doc, fs, nullptr);
        return m;
    };
    auto only = [&]() {
        for (const ReviewFinding &f : sr->lastResult().findings)
            if (f.check == "PWR-12")
                return f;
        return ReviewFinding();
    };
    auto state = [&]() { return QString(only().intended ? "meant" : only().isOpen() ? "open" : "notaproblem"); };
    const quint32 first = cues.first(), last = cues.last();

    // Open: amber "⚠ cyc"; the chip counts it.
    QCOMPARE(state(), QStringLiteral("open"));
    QCOMPARE(pieces(last).first().first, QStringLiteral("⚠ cyc"));
    QCOMPARE(pieces(last).first().second, ShowTreeView::warningColour());
    const int others = sr->lastResult().open[ReviewFinding::Blocker] + sr->lastResult().open[ReviewFinding::Warning] - 1;
    QVERIFY(others >= 0);
    auto summary = [&](int open, int meant, int notAProblem) {
        return QStringLiteral("%1 open · %2 meant · %3 not a problem").arg(open + others).arg(meant).arg(notAProblem);
    };
    QCOMPARE(ShowReviewMarker::stateSummary(m_doc), summary(1, 0, 0));
    {
        QScopedPointer<QMenu> m(menuFor(ShowReviewMarker::cueAll(m_doc, last)));
        QVERIFY(m->findChild<QAction *>("reviewMeant") && m->findChild<QAction *>("reviewAccept"));
        QVERIFY(!m->findChild<QAction *>("reviewNotMeant") && !m->findChild<QAction *>("reviewReopen"));
        QVERIFY(ContextMenu::checkCanon(m.data()).isEmpty());
        // Open -> Meant, from the cue's badge.
        m->findChild<QAction *>("reviewMeant")->trigger();
    }
    // Meant: green "cyc ✓" on the cue it starts in, who/when on hover; not counted open.
    QCOMPARE(state(), QStringLiteral("meant"));
    QCOMPARE(m_doc->showTree()->undoText(), QStringLiteral("I meant it"));
    QCOMPARE(pieces(first).first().first, QStringLiteral("cyc ✓"));
    QCOMPARE(pieces(first).first().second, ShowTreeView::meantColour(normal));
    QVERIFY(ShowTreeView::meantColour(normal) != ShowTreeView::warningColour());
    QVERIFY(!rowOfCue(first)->text(ShowTreeView::FlagsCol).contains("⚠"));
    const QString meantTip = rowOfCue(first)->toolTip(ShowTreeView::FlagsCol);
    QVERIFY2(meantTip.contains("Meant") && meantTip.contains("Not Meant") && meantTip.contains("Not a Problem…"),
             qPrintable(meantTip));
    if (!ShowReview::defaultName().isEmpty())
        QVERIFY(meantTip.contains(ShowReview::defaultName()));
    QCOMPARE(ShowReviewMarker::stateSummary(m_doc), summary(0, 1, 0));
    QCOMPARE(ShowReviewMarker::cueFindings(m_doc, first).count(), 0);
    QCOMPARE(ShowReviewMarker::cueMeant(m_doc, first).count(), 1);

    // Review panel: its own "Meant" group, I Meant It no longer on offer.
    ShowReviewPanel panel(m_doc);
    panel.setFilter(ShowReviewPanel::All);
    panel.recheck();
    auto group = [&panel](const QString &prefix) -> QTreeWidgetItem * {
        for (int i = 0; i < panel.tree()->topLevelItemCount(); ++i)
            if (panel.tree()->topLevelItem(i)->text(0).startsWith(prefix))
                return panel.tree()->topLevelItem(i);
        return nullptr;
    };
    QVERIFY(group("Meant (1)") != nullptr);
    QVERIFY(group("Accepted") == nullptr);
    panel.tree()->clearSelection();
    rowOf(panel.tree(), "PWR-12")->setSelected(true);
    QCOMPARE(panel.reopenButton()->text(), QStringLiteral("Not Meant"));
    QVERIFY(panel.acceptButton()->isEnabled());

    // Meant -> Open, from the Review row's button.
    panel.reopenButton()->click();
    QCOMPARE(state(), QStringLiteral("open"));
    QVERIFY(rowOfCue(first)->text(ShowTreeView::FlagsCol).contains("⚠ cyc"));
    QVERIFY(group("Meant") == nullptr);
    // Open -> Meant, row button; then Meant -> Open from the cue badge's menu.
    panel.tree()->clearSelection();
    rowOf(panel.tree(), "PWR-12")->setSelected(true);
    panel.acceptButton()->click();
    QCOMPARE(state(), QStringLiteral("meant"));
    {
        QScopedPointer<QMenu> m(menuFor(ShowReviewMarker::cueAll(m_doc, first)));
        QVERIFY(m->findChild<QAction *>("reviewNotMeant") && m->findChild<QAction *>("reviewAccept"));
        QVERIFY(!m->findChild<QAction *>("reviewMeant"));
        m->findChild<QAction *>("reviewNotMeant")->trigger();
    }
    QCOMPARE(state(), QStringLiteral("open"));

    // Open -> Not a Problem, from the footer chip's menu (the open list): no badge anywhere.
    {
        QScopedPointer<QMenu> m(menuFor(ShowReviewMarker::openFindings(m_doc)));
        QVERIFY(m->findChild<QAction *>("reviewAccept") != nullptr);
    }
    QVERIFY(ShowReviewMarker::notAProblem(m_doc, only(), nullptr, "a scrim hides it"));
    QCOMPARE(state(), QStringLiteral("notaproblem"));
    for (quint32 c : cues)
    {
        QVERIFY(!rowOfCue(c)->text(ShowTreeView::FlagsCol).contains("cyc"));
        QVERIFY(view.backwashFindingKey(c).isEmpty());
    }
    QCOMPARE(ShowReviewMarker::stateSummary(m_doc), summary(0, 0, 1));
    panel.recheck();
    QVERIFY(group("Accepted (1)") != nullptr);
    QVERIFY(group("Meant") == nullptr);
    // Not a Problem -> Open (Reopen), from the Review row's right-click menu.
    panel.tree()->clearSelection();
    rowOf(panel.tree(), "PWR-12")->setSelected(true);
    {
        QScopedPointer<QMenu> m(menuFor(panel.selectedFindings()));
        QVERIFY(m->findChild<QAction *>("reviewReopen") != nullptr);
        QVERIFY(!m->findChild<QAction *>("reviewMeant") && !m->findChild<QAction *>("reviewAccept"));
        m->findChild<QAction *>("reviewReopen")->trigger();
    }
    QCOMPARE(state(), QStringLiteral("open"));
    QVERIFY(rowOfCue(last)->text(ShowTreeView::FlagsCol).contains("⚠ cyc"));

    // Meant -> Not a Problem: the mark comes off, the acceptance stays, the cue is clean.
    QVERIFY(ShowReviewMarker::meant(m_doc, only(), nullptr, "front light spills on purpose"));
    QCOMPARE(state(), QStringLiteral("meant"));
    QVERIFY(ShowReviewMarker::notAProblem(m_doc, only(), nullptr, "fine"));
    QCOMPARE(state(), QStringLiteral("notaproblem"));
    QVERIFY(m_doc->showTree()->cue(first)->layers.first().meantOnScenery.isEmpty());
    QVERIFY(!rowOfCue(first)->text(ShowTreeView::FlagsCol).contains("cyc"));
    QCOMPARE(only().acceptedNote, QStringLiteral("fine"));
    // Not a Problem again from the row (Reopen), then Open -> Not a Problem via the panel button needs a note: skipped (dialog).
    QVERIFY(ShowReviewMarker::notMeant(m_doc, only()));
    QCOMPARE(state(), QStringLiteral("open"));
    Q_UNUSED(front)
}

/* Pictures of the three states (Q207), only when QLC_WARN_SHOTS names a
   folder: the cue rows, the hover, the right-click menus and the Review
   panel, dark theme, at the widgets' minimum, 1280x800 and 1920x1080. */
void ShowReviewPanel_Test::screenshots()
{
    const QString dir = qEnvironmentVariable("QLC_WARN_SHOTS");
    if (dir.isEmpty())
        QSKIP("QLC_WARN_SHOTS not set");
    QDir().mkpath(dir);
    const QString tag = qEnvironmentVariable("QLC_WARN_TAG", "after");
    const QPalette was = qApp->palette();
    qApp->setPalette(App::themePalette(App::ThemeVSCodeDark, was));
    ShowReview::setDefaultName("Sam");
    Fixture *front = nullptr;
    const QList<quint32> cues = backwashShow(m_doc, &front);
    ShowTreeView view(nullptr, m_doc);
    ShowReview *sr = m_doc->showReview();
    sr->run();
    ShowReviewPanel panel(m_doc);
    panel.setFilter(ShowReviewPanel::All);
    const QList<QPair<QString, QSize>> sizes {
        { "min", QSize() }, { "1280x800", QSize(1280, 800) }, { "1920x1080", QSize(1920, 1080) } };
    auto only = [&]() {
        for (const ReviewFinding &f : sr->lastResult().findings)
            if (f.check == "PWR-12")
                return f;
        return ReviewFinding();
    };
    auto snapAll = [&](const QString &state) {
        panel.recheck();
        for (const auto &sz : sizes)
        {
            for (QWidget *w : { static_cast<QWidget *>(&view), static_cast<QWidget *>(&panel) })
            {
                w->resize(sz.second.isValid() ? sz.second : w->minimumSizeHint());
                w->show();
                QTest::qWait(30);
            }
            view.tree()->expandAll();
            QTest::qWait(30);
            QVERIFY(view.grab().save(QStringLiteral("%1/%2-cues-%3-%4.png").arg(dir, tag, state, sz.first)));
            QVERIFY(panel.grab().save(QStringLiteral("%1/%2-review-%3-%4.png").arg(dir, tag, state, sz.first)));
        }
        // The tree scrolled to its flags column (the pane is narrow at 1280).
        view.tree()->horizontalScrollBar()->setValue(view.tree()->horizontalScrollBar()->maximum());
        QTest::qWait(30);
        QVERIFY(view.tree()->grab().save(QStringLiteral("%1/%2-tree-%3.png").arg(dir, tag, state)));
        view.tree()->horizontalScrollBar()->setValue(0);
        // The hover on the cue badge: its rich text on a tooltip-styled label.
        QTreeWidgetItem *item = nullptr;
        for (QTreeWidgetItemIterator it(view.tree()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::IdRole).toUInt() == cues.first()
                && (*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode)
                item = *it;
        QVERIFY(item != nullptr);
        QLabel tip(item->toolTip(ShowTreeView::FlagsCol));
        tip.setTextFormat(Qt::RichText);
        tip.setWordWrap(true);
        tip.setMargin(6);
        tip.setFrameShape(QFrame::Box);
        tip.setAutoFillBackground(true);
        tip.setPalette(QToolTip::palette());
        tip.setFixedWidth(520);
        tip.setFixedHeight(tip.heightForWidth(520));
        tip.show();
        QVERIFY(tip.grab().save(QStringLiteral("%1/%2-hover-%3.png").arg(dir, tag, state)));
    };
    auto snapMenu = [&](const QList<ReviewFinding> &fs, const QString &name) {
        QMenu m;
        m.setToolTipsVisible(true);
        {
            ContextMenu cm(&m);
            ShowReviewMarker::fillMenu(cm, m_doc, fs, nullptr);
        }
        m.ensurePolished();
        m.resize(m.sizeHint());
        m.show();
        QTest::qWait(30);
        QVERIFY(m.grab().save(QStringLiteral("%1/%2-menu-%3.png").arg(dir, tag, name)));
    };
    snapAll("open");
    snapMenu(ShowReviewMarker::cueAll(m_doc, cues.first()), "open");
    QVERIFY(ShowReviewMarker::meant(m_doc, only(), nullptr, "cyc wash for the finale"));
    snapAll("meant");
    snapMenu(ShowReviewMarker::cueAll(m_doc, cues.first()), "meant");
    QVERIFY(ShowReviewMarker::notAProblem(m_doc, only(), nullptr, "a scrim hides it"));
    snapAll("notaproblem");
    panel.tree()->clearSelection();
    rowOf(panel.tree(), "PWR-12")->setSelected(true);
    snapMenu(panel.selectedFindings(), "notaproblem");
    qApp->setPalette(was);
    Q_UNUSED(front)
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
    // Needs work is an open note (Q212): its ✎ mark leads, in the warning colour.
    QCOMPARE(p.count(), 3);
    QCOMPARE(p.at(0).first, QStringLiteral("✎"));
    QCOMPARE(p.at(0).second, ShowTreeView::warningColour());
    QCOMPARE(p.at(1).second, normal);           // mix
    QCOMPARE(p.at(2).first, QStringLiteral("needs work"));
    QCOMPARE(p.at(2).second, ShowTreeView::warningColour());
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
