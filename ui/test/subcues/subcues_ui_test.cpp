/*
  Q Light Controller Plus - Test Unit
  subcues_ui_test.cpp

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

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QSettings>
#include <QLabel>
#include <QtTest>

#define protected public
#define private public
#include "virtualconsole.h"
#include "chasereditor.h"
#include "mastertimer.h"
#include "vccuelist.h"
#undef private
#undef protected
#include "functionparent.h"
#include "showcontrol.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "app.h"
#include "doc.h"

#include "subcues_ui_test.h"

/* Sub-cues (SUBCUES_DESIGN.md, P2): what the chaser editor, the Virtual
   Console cue list and the footer show of them. */

// ChaserEditor's columns
#define ED_COL_NAME     1
#define ED_COL_NOTES    6
#define ED_COL_CUE      8
#define ED_COL_SUBCUES  9
// VCCueList's
#define CL_COL_NAME     1
#define CL_COL_CUE      6

void SubCuesUi_Test::initTestCase()
{
    // Never the real settings: an empty set of the test's own.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void SubCuesUi_Test::init()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);
}

void SubCuesUi_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    delete VirtualConsole::instance();
    delete m_doc;
    m_doc = nullptr;
}

Chaser *SubCuesUi_Test::makeInner(int steps)
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Inner");
    c->setDurationMode(Chaser::Common);
    c->setDuration(MasterTimer::tick() * 3);
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        Scene *s = new Scene(m_doc);
        s->setName(QString("Inner %1").arg(i + 1));
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

Chaser *SubCuesUi_Test::makeList(int steps, int chaseAt, Chaser *inner, int mode)
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Cue list");
    c->setDurationMode(Chaser::Common);
    c->setDuration(Function::infiniteSpeed());
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        if (i == chaseAt && inner != nullptr)
        {
            ChaserStep step(inner->id());
            step.subCues = ChaserStep::SubCueMode(mode);
            c->addStep(step);
            continue;
        }
        Scene *s = new Scene(m_doc);
        s->setName(QString("Cue %1").arg(i + 1));
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

void SubCuesUi_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
        m_doc->masterTimer()->timerTick();
}

void SubCuesUi_Test::theEditorListsAChaseStepsSubCues()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::Auto);
    ChaserEditor ed(nullptr, list, m_doc);
    QTreeWidget *tree = ed.m_tree;
    QVERIFY(tree->rootIsDecorated());
    QCOMPARE(tree->headerItem()->text(ED_COL_SUBCUES), QString("Sub-cues"));

    // A scene: nothing under it, no Sub-cues setting.
    QTreeWidgetItem *scene = tree->topLevelItem(0);
    QCOMPARE(scene->childCount(), 0);
    QCOMPARE(scene->text(ED_COL_SUBCUES), QString());
    QVERIFY(!scene->data(ED_COL_SUBCUES, Qt::UserRole + 1).toBool());

    // The chase: its steps under it, numbered under it, read-only.
    QTreeWidgetItem *chase = tree->topLevelItem(1);
    QCOMPARE(chase->childCount(), 3);
    QCOMPARE(chase->text(ED_COL_SUBCUES), QString("Own timing"));
    QTreeWidgetItem *first = chase->child(0);
    QCOMPARE(first->text(ED_COL_CUE), QString("2.1"));
    QCOMPARE(first->text(ED_COL_NAME), QString("Inner 1"));
    QVERIFY(!(first->flags() & Qt::ItemIsEditable));
    QVERIFY(!(first->flags() & Qt::ItemIsSelectable));
    const QBrush grey = tree->palette().brush(QPalette::Disabled, QPalette::Text);
    QCOMPARE(first->foreground(ED_COL_CUE), grey);              // derived: grey

    // An inner step's own number, shown as its own.
    ChaserStep own = inner->steps().at(2);
    own.cueNumber = "2.25";
    inner->replaceStep(own, 2);
    ed.updateTree();
    QCOMPARE(tree->topLevelItem(1)->child(2)->text(ED_COL_CUE), QString("2.25"));
    QVERIFY(tree->topLevelItem(1)->child(2)->foreground(ED_COL_CUE) != grey);

    // The Sub-cues column: Sub GO.
    chase = tree->topLevelItem(1);
    chase->setData(ED_COL_SUBCUES, Qt::UserRole, 1);
    QCOMPARE(list->stepSubCues(1), ChaserStep::SubGo);
    QCOMPARE(tree->topLevelItem(1)->text(ED_COL_SUBCUES), QString("Sub GO"));
    chase = tree->topLevelItem(1);
    chase->setData(ED_COL_SUBCUES, Qt::UserRole, 0);
    QCOMPARE(list->stepSubCues(1), ChaserStep::Auto);
    chase->setData(ED_COL_SUBCUES, Qt::UserRole, 1);
    // ...and not for a scene.
    scene->setData(ED_COL_SUBCUES, Qt::UserRole, 1);
    QCOMPARE(list->stepSubCues(0), ChaserStep::Auto);

    // The parent's number: its sub-cues follow it.
    chase = tree->topLevelItem(1);
    chase->setText(ED_COL_CUE, "4");
    QCOMPARE(list->stepCue(1), QString("4"));
    QCOMPARE(tree->topLevelItem(1)->child(0)->text(ED_COL_CUE), QString("4.1"));
    QCOMPARE(tree->topLevelItem(1)->child(2)->text(ED_COL_CUE), QString("2.25"));

    // A sub-cue row changes nothing (Edit Chase… does, where it lives).
    tree->topLevelItem(1)->child(1)->setText(ED_COL_NOTES, "typed here");
    QVERIFY(inner->steps().at(1).note.isEmpty());
    QVERIFY(list->steps().at(1).note.isEmpty());

    // Picked, a sub-cue row stands for its step (what Add inserts after).
    tree->topLevelItem(1)->setExpanded(true);
    tree->setCurrentItem(tree->topLevelItem(1)->child(1));
    QCOMPARE(ed.getCurrentIndex(), 1);
    QCOMPARE(ed.topLevelIndex(tree->topLevelItem(1)->child(1)), 1);
    QCOMPARE(ed.topLevelIndex(nullptr), -1);

    // Edit Chase… with no Function Manager about: nothing, and no crash.
    ed.editSubCueChase(1);
    ed.editSubCueChase(0);
    QCoreApplication::processEvents();
}

void SubCuesUi_Test::theCueListNestsAndHighlightsSubCues()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(list->id());

    QTreeWidget *tree = cl.m_tree;
    QVERIFY(tree->rootIsDecorated());
    QTreeWidgetItem *chase = tree->topLevelItem(1);
    QCOMPARE(chase->childCount(), 3);
    QVERIFY(chase->isExpanded());                   // Sub GO: in sight
    QCOMPARE(chase->child(1)->text(CL_COL_CUE), QString("2.2"));
    QCOMPARE(chase->child(1)->text(CL_COL_NAME), QString("Inner 2"));
    QVERIFY(!cl.m_subGoButton->isHidden());
    QVERIFY(!cl.m_subBackButton->isHidden());

    m_doc->setMode(Doc::Operate);
    cl.slotNextCue();
    tick();
    cl.slotNextCue();
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    cl.updateStandby();
    QVERIFY(chase->child(0)->font(CL_COL_NAME).bold());       // playing
    QVERIFY(!chase->child(1)->font(CL_COL_NAME).bold());
    QVERIFY2(cl.m_standbyLabel->text().startsWith(QString("Sub: 2.2 · Next: 3")),
             qPrintable(cl.m_standbyLabel->text()));

    cl.m_subGoButton->click();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 1);
    cl.updateStandby();
    QVERIFY(!chase->child(0)->font(CL_COL_NAME).bold());
    QVERIFY(chase->child(1)->font(CL_COL_NAME).bold());

    cl.m_subBackButton->click();
    tick();
    QCOMPARE(list->currentSubCueIndex(), 0);

    // A sub-cue clicked: played.
    cl.slotItemActivated(chase->child(2));
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 2);
    cl.updateStandby();
    QVERIFY(chase->child(2)->font(CL_COL_NAME).bold());
    QVERIFY(cl.m_standbyLabel->text().startsWith("Next: 3"));   // none left

    // GO: the next cue; nothing highlighted under 2.
    cl.slotNextCue();
    tick();
    QCOMPARE(list->currentStepIndex(), 2);
    cl.updateStandby();
    for (int j = 0; j < chase->childCount(); ++j)
        QVERIFY(!chase->child(j)->font(CL_COL_NAME).bold());

    // From another cue, a sub-cue clicked plays its cue first.
    cl.slotItemActivated(chase->child(1));
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 1);

    // Stopped: it starts there.
    cl.slotStop();
    tick();
    QVERIFY(!list->isRunning());
    cl.slotItemActivated(chase->child(2));
    tick();
    QVERIFY(list->isRunning());
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 2);

    // A note typed on a sub-cue row isn't the step's.
    chase->child(0)->setText(5, "typed");
    QVERIFY(list->steps().at(1).note.isEmpty());

    m_doc->setMode(Doc::Design);
}

void SubCuesUi_Test::aCueListWithoutSubGoHasNoSubGoButton()
{
    Chaser *inner = makeInner(2);
    Chaser *list = makeList(3, 1, inner, ChaserStep::Auto);
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(list->id());
    QVERIFY(cl.m_subGoButton->isHidden());
    QVERIFY(cl.m_subBackButton->isHidden());
    QCOMPARE(cl.m_tree->topLevelItem(1)->childCount(), 2);       // still listed
    QVERIFY(!cl.m_tree->topLevelItem(1)->isExpanded());

    // Switched to Sub GO: the buttons come.
    list->setStepSubCues(1, ChaserStep::SubGo);
    cl.updateStepList();
    QVERIFY(!cl.m_subGoButton->isHidden());

    // No chase steps at all: a flat list, as before.
    Chaser *plain = makeList(2, -1, nullptr, 0);
    cl.setChaser(plain->id());
    QVERIFY(!cl.m_tree->rootIsDecorated());
    QVERIFY(cl.m_subGoButton->isHidden());
}

void SubCuesUi_Test::theFooterSaysWhatSubGoPlays()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    QCOMPARE(App::standbySubCueText(nullptr), QString());
    QCOMPARE(App::standbySubCueText(list), QString());          // stopped

    ChaserAction go;
    go.m_action = ChaserNextStep;
    go.m_stepIndex = -1;
    go.m_masterIntensity = 1.0;
    go.m_stepIntensity = 1.0;
    go.m_fadeMode = Chaser::FromFunction;
    list->setAction(go);
    list->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    QCOMPARE(App::standbySubCueText(list), QString());          // cue 1: a scene
    list->setAction(go);
    tick();
    QCOMPARE(App::standbySubCueText(list), QString("Sub: 2.2"));
    list->subGo();
    tick();
    QCOMPARE(App::standbySubCueText(list), QString("Sub: 2.3"));
    list->subGo();
    tick();
    QCOMPARE(App::standbySubCueText(list), QString("Sub: (end of 2)"));
    list->setAction(go);
    tick();
    QCOMPARE(App::standbySubCueText(list), QString());

    // An Auto chase step: no Sub GO, so nothing said.
    list->setStepSubCues(1, ChaserStep::Auto);
    list->jumpToSubCue(1, 0);
    tick(1);
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(App::standbySubCueText(list), QString());
}

void SubCuesUi_Test::subGoAndSubBackHaveTheirShortcuts()
{
    // ⌥⌘↩ and ⌥⌘⇧↩ on macOS: Qt's Ctrl is ⌘ there.
    QCOMPARE(App::cueSubGoShortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Return));
    QCOMPARE(App::cueSubBackShortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Return));
    QCOMPARE(App::cueSubGoShortcut().toString(QKeySequence::PortableText), QString("Ctrl+Alt+Return"));
    // Not GO (⌘↩) or Back (⌘⇧↩).
    const QList<QKeySequence> taken = { QKeySequence(Qt::CTRL | Qt::Key_Return),
                                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return) };
    QVERIFY(!taken.contains(App::cueSubGoShortcut()));
    QVERIFY(!taken.contains(App::cueSubBackShortcut()));
    QVERIFY(App::cueSubGoShortcut() != App::cueSubBackShortcut());
}

/* P3: show control's LOAD 4.2 -- the footer's and the cue list's "Next:"
   say the sub-cue, by its number and its own name. */
void SubCuesUi_Test::aLoadedSubCueIsWhatNextSays()
{
    Chaser *inner = makeInner(3);
    Chaser *list = makeList(3, 1, inner, ChaserStep::SubGo);
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(list->id());
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(list->id());

    QCOMPARE(App::standbyNextText(nullptr, list), QString());
    QCOMPARE(App::standbyNextText(sc, list), QString("Next: 1 \u2014 Cue 1"));

    QVERIFY(sc->execute({ ShowControl::Load, "2.3", QString() }));
    QCOMPARE(App::standbyNextText(sc, list), QString("Next: 2.3 \u2014 Inner 3"));
    cl.updateStandby();
    QVERIFY2(cl.m_standbyLabel->text().startsWith(QString("Next: 2.3 \u2014 Inner 3")),
             qPrintable(cl.m_standbyLabel->text()));

    // A main cue LOADed instead: that's what it says.
    QVERIFY(sc->execute({ ShowControl::Load, "3", QString() }));
    QCOMPARE(App::standbyNextText(sc, list), QString("Next: 3 \u2014 Cue 3"));
    cl.updateStandby();
    QVERIFY(cl.m_standbyLabel->text().startsWith(QString("Next: 3 \u2014 Cue 3")));
    // A function named as a Show Tree cue's is ("3-Finale"): the number once.
    Function *third = m_doc->function(list->steps().at(2).fid);
    QVERIFY(third != nullptr);
    const QString wasName = third->name();
    third->setName("3-Finale");
    QCOMPARE(App::standbyNextText(sc, list), QString("Next: 3 \u2014 Finale"));
    third->setName(wasName);

    // GO 2.2 over OSC's way in (show control): cue 2 at 2.2, and the
    // footer's Sub GO part follows on from there.
    QVERIFY(sc->execute({ ShowControl::Go, "2.2", QString() }));
    tick();
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(list->currentSubCueIndex(), 1);
    QCOMPARE(App::standbySubCueText(list), QString("Sub: 2.3"));
    QVERIFY(sc->execute({ ShowControl::SubGo, QString(), QString() }));
    tick();
    QCOMPARE(App::standbySubCueText(list), QString("Sub: (end of 2)"));
}

QTEST_MAIN(SubCuesUi_Test)
