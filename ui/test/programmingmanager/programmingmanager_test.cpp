/*
  Q Light Controller Plus - Test Unit
  programmingmanager_test.cpp

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
#include <QSignalSpy>
#include <QMimeData>
#include <QDropEvent>
#include <QDataStream>
#include <QTreeWidgetItemIterator>
#include <QSplitter>
#include <QLayout>

#define protected public
#define private public
#include "programmingmanager.h"
#include "scenegrouplooks.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "lookeditor.h"
#include "scene.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "doc.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "monitor/structurestudioview.h"
#undef private
#undef protected

#include "programmingmanager_test.h"

/* Real QMimeData for a palette, built the same way FunctionsTreeWidget's own
 * external drag would (m_paletteTree->setExternalDragMode(true) is set
 * unconditionally at construction -- see programmingmanager.cpp -- so its
 * mimeData() always returns the external payload, never QTreeWidget's
 * default). mimeData() is protected; reachable via the
 * "#define protected public" above the includes. */
static QMimeData *paletteMimeData(FunctionsTreeWidget *tree, QLCPalette *pal)
{
    QTreeWidgetItem *item = tree->paletteItem(pal);
    if (item == nullptr)
        return nullptr;
    return tree->buildMimeData(QList<QTreeWidgetItem *>() << item);
}

/* Real QMimeData for a fixture group or an individual fixture, built the
 * same way FixtureGroupSource's own drag would. Finds the source row by
 * walking the tree for the matching IdRole/KindRole pair -- there's no
 * public itemById() for a plain fixture leaf the way groupItemById() exists
 * for a group. */
static QTreeWidgetItem *fixtureGroupSourceItem(FixtureGroupSource *tree, quint32 id,
                                               FixtureGroupSource::NodeKind kind)
{
    QTreeWidgetItemIterator it(tree);
    for (; *it != nullptr; ++it)
    {
        if ((*it)->data(0, FixtureGroupSource::KindRole).toInt() == int(kind)
            && (*it)->data(0, FixtureGroupSource::IdRole).toUInt() == id)
            return *it;
    }
    return nullptr;
}

static QMimeData *fixtureGroupMimeData(FixtureGroupSource *tree, quint32 groupId)
{
    QTreeWidgetItem *item = fixtureGroupSourceItem(tree, groupId, FixtureGroupSource::GroupNode);
    if (item == nullptr)
        return nullptr;
    return tree->buildMimeData(QList<QTreeWidgetItem *>() << item);
}

static QMimeData *fixtureMimeData(FixtureGroupSource *tree, quint32 fixtureId)
{
    QTreeWidgetItem *item = fixtureGroupSourceItem(tree, fixtureId, FixtureGroupSource::FixtureNode);
    if (item == nullptr)
        return nullptr;
    return tree->buildMimeData(QList<QTreeWidgetItem *>() << item);
}

/* Open the Programming tab's canvas for @p sceneId with REAL, non-degenerate
 * geometry for m_targetList/m_lookList -- without this, mgr's top-level
 * QSplitter (a local variable in ProgrammingManager's constructor, not
 * exposed as a member the way FixtureManager::m_splitter is) never gets
 * explicit sizes, and every child widget stays stuck at Qt's default
 * unconfigured QRect(0,0 100x30) -- an unshown top-level widget's layout is
 * never activated by resize() alone the way FixtureGroupEditor's table was
 * in the Fixture Manager tests, because THAT test could reach the actual
 * splitter member and call setSizes() on it directly. Found via
 * findChild<QSplitter*>() instead, since ProgrammingManager has exactly one. */
static SceneGroupLooks *openCanvas(ProgrammingManager &mgr, quint32 sceneId)
{
    mgr.resize(1400, 900);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // resize() alone leaves every descendant's geometry at Qt's unconfigured
    // default (100x30): an unshown top-level widget's layout is never
    // activated by the normal resize-event cascade the way a visible
    // window's is. Force it explicitly, or the splitter (and everything
    // inside it) never gets real, non-degenerate geometry at all.
    mgr.layout()->activate();
    QSplitter *splitter = mgr.findChild<QSplitter *>();
    if (splitter != nullptr)
        splitter->setSizes(QList<int>() << 300 << 700 << 300);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    mgr.loadCanvas(sceneId);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // m_canvas is created fresh INSIDE loadCanvas(), after the splitter
    // above already settled -- it's a brand new child of an already-laid-out
    // ancestor, and the same activation gap applies to it individually: the
    // LayoutRequest its insertion posts never gets delivered for a hidden
    // widget under offscreen QTest. Force it directly on the canvas itself.
    if (mgr.m_canvas != nullptr)
    {
        mgr.m_canvas->resize(900, 700);
        mgr.m_canvas->layout()->activate();
        QCoreApplication::processEvents();
    }
    return mgr.m_canvas;
}

/* Deliver a synthetic drop straight to SceneGroupLooks::dropEvent() -- the
 * real internal Qt::QDrag session this simulates can't run under offscreen
 * QTest (no window manager, no real drop target to negotiate with), the
 * same reasoning as the Fixture Manager sub-group drag test earlier this
 * session. @p posInCanvas MUST be in the canvas ("this") widget's own local
 * coordinates, not m_targetList's/m_lookList's viewport coordinates --
 * dropEvent()'s own column-precision guard reads qlcEventPos(event)
 * directly against m_targetList->geometry()/m_lookList->geometry(), which
 * are themselves canvas-local (see scenegrouplooks.cpp's dropEvent()). */
static void deliverDrop(SceneGroupLooks *canvas, QMimeData *mime, const QPoint &posInCanvas)
{
    QVERIFY(mime != nullptr);
    QDropEvent drop(QPointF(posInCanvas), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    canvas->dropEvent(&drop);
    delete mime;
}

void ProgrammingManager_Test::init()
{
    m_doc = new Doc(this);
}

void ProgrammingManager_Test::cleanup()
{
    delete m_doc;
}

void ProgrammingManager_Test::editingALookColourNotifiesItsScene()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Full");
    dim->setValue(255);
    QVERIFY(m_doc->addPalette(dim));

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("White");
    col->setValue(QColor(255, 255, 255));
    QVERIFY(m_doc->addPalette(col));

    Scene *scene = new Scene(m_doc);
    scene->setName("Edited Look");
    scene->addFixture(fxi->id());
    scene->addPalette(dim->id());
    scene->addPalette(col->id());
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    QVERIFY(mgr.m_lookEditor != nullptr);
    mgr.m_lookEditor->setPalette(col->id());
    QCOMPARE(mgr.m_lookEditor->paletteId(), col->id());

    QSignalSpy spy(m_doc, &Doc::functionChanged);

    // What the real colour picker in the look editor does when dragged --
    // not a manually-injected Doc::functionChanged like a shallower test
    // would use, which proves nothing about whether the real UI action
    // actually reaches the scene.
    mgr.m_lookEditor->slotColorChanged(QColor(0, 0, 255));

    QVERIFY2(spy.count() > 0,
             "Doc::functionChanged never fired -- editing the look's colour "
             "through the real editor left the scene looking unchanged to "
             "anything listening for it (e.g. the rig view)");
    bool sawScene = false;
    for (const QList<QVariant> &call : spy)
        if (call.at(0).toUInt() == scene->id())
            sawScene = true;
    QVERIFY2(sawScene, "functionChanged fired, but not for the scene that "
                        "actually uses the edited palette");
}

/* The Programming tab's own construction gestures -- CLAUDE.md: "Drag
 * palettes→looks, groups→dynamic targets, fixtures→fixed targets" -- had no
 * test anywhere driving the real drop (WORKFLOW-SUBTLETIES.md #6, accepted
 * as a real but speculative gap; this closes it now that it's the thing
 * being worked on). Each step below builds real QMimeData the same way the
 * real source trees (FunctionsTreeWidget, FixtureGroupSource) build it and
 * delivers it via a synthetic QDropEvent straight to
 * SceneGroupLooks::dropEvent() -- the real internal Qt::QDrag session this
 * simulates can't run under offscreen QTest, the same reasoning as the
 * Fixture Manager sub-group drag test earlier this session.
 */
void ProgrammingManager_Test::buildingALookThroughRealProgrammingTabDragAndDropStaysConsistent()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fixedFx = new Fixture(m_doc);
    fixedFx->setName("Fixed Target");
    fixedFx->setChannels(4);
    QVERIFY(m_doc->addFixture(fixedFx));

    Fixture *groupFx = new Fixture(m_doc);
    groupFx->setName("Group Member");
    groupFx->setChannels(4);
    groupFx->setAddress(4);
    QVERIFY(m_doc->addFixture(groupFx));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Wash Group");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(groupFx->id());

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Blue");
    col->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(col));

    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("Followspot");
    fx->setScriptPath("Palettes/Effect/Position/Followspot/");
    QVERIFY(m_doc->addPalette(fx));

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Half");
    dim->setValue(128);
    QVERIFY(m_doc->addPalette(dim));

    Scene *scene = new Scene(m_doc);
    scene->setName("Drag Built Look");
    QVERIFY(m_doc->addFunction(scene));

    // 0. Open the tab and load the (empty) scene into the real canvas --
    //    ProgrammingManager::loadCanvas(), the exact function selecting a
    //    scene in the nav tree calls.
    ProgrammingManager mgr(nullptr, m_doc);
    SceneGroupLooks *canvas = openCanvas(mgr, scene->id());
    QVERIFY2(canvas != nullptr, "loadCanvas() did not build a SceneGroupLooks canvas");

    // 1. Drag the Colour palette from the real palette tree onto empty
    //    space in the Looks column -- the base-look gesture.
    QMimeData *colMime = paletteMimeData(mgr.m_paletteTree, col);
    QVERIFY2(colMime != nullptr, "the colour palette has no row in the real palette tree");
    deliverDrop(canvas, colMime, canvas->m_lookList->geometry().center());
    QCoreApplication::processEvents();   // reconcileAfterPaletteApply()'s deferred timer
    QVERIFY2(scene->palettes().contains(col->id()),
             "dropping the colour palette onto the Looks list did not add it to the scene");

    // 2. Drag the fixture group from the real fixtures & groups tree onto
    //    the Targets list -- a dynamic target.
    QMimeData *grpMime = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY2(grpMime != nullptr, "the group has no row in the real fixtures & groups tree");
    deliverDrop(canvas, grpMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtureGroups().contains(grp->id()),
             "dropping the group onto the Targets list did not add it as a dynamic target");

    // 3. Drag an individual fixture onto the Targets list -- a fixed
    //    target (only accepted because loadCanvas() built this canvas with
    //    includeFixtureTargets=true, unlike the classic scene editor).
    QMimeData *fxTargetMime = fixtureMimeData(mgr.m_fixGroupSource, fixedFx->id());
    QVERIFY2(fxTargetMime != nullptr, "the fixture has no row in the real fixtures & groups tree");
    deliverDrop(canvas, fxTargetMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtures().contains(fixedFx->id()),
             "dropping the fixture onto the Targets list did not add it as a fixed target");

    // 4. Drag the Effect palette onto empty Looks space -- with no other
    //    effect yet, it becomes a base look too, same as the colour.
    QMimeData *fxPalMime = paletteMimeData(mgr.m_paletteTree, fx);
    QVERIFY2(fxPalMime != nullptr, "the effect palette has no row in the real palette tree");
    deliverDrop(canvas, fxPalMime, canvas->m_lookList->geometry().center());
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().contains(fx->id()),
             "dropping the effect palette onto the Looks list did not add it to the scene");

    // 5. Drag the Dimmer palette directly onto the effect's OWN row -- must
    //    nest UNDER it (feed it) rather than land as another base look.
    auto findLookRow = [&](quint32 pid) -> QTreeWidgetItem * {
        QTreeWidgetItemIterator it(canvas->m_lookList);
        for (; *it != nullptr; ++it)
            if ((*it)->data(0, Qt::UserRole).toUInt() == pid)
                return *it;
        return nullptr;
    };
    QTreeWidgetItem *effectItem = findLookRow(fx->id());
    QVERIFY2(effectItem != nullptr, "the effect palette never got a row in the Looks list");
    const QPoint effectRowInCanvas = canvas->m_lookList->viewport()->mapTo(
        canvas, canvas->m_lookList->visualItemRect(effectItem).center());

    QMimeData *dimMime = paletteMimeData(mgr.m_paletteTree, dim);
    QVERIFY2(dimMime != nullptr, "the dimmer palette has no row in the real palette tree");
    deliverDrop(canvas, dimMime, effectRowInCanvas);
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().contains(dim->id()),
             "dropping the dimmer onto the effect row did not add it to the scene");

    // moveLooksToTarget() rebuilt the tree (see its own "Rebuilds the tree"
    // doc comment) -- effectItem above is dead the moment that ran. Re-find
    // both rows fresh rather than dereference a stale pointer.
    QTreeWidgetItem *effectItemNow = findLookRow(fx->id());
    QTreeWidgetItem *dimItemNow = findLookRow(dim->id());
    QVERIFY2(effectItemNow != nullptr, "the effect row is gone after the nested drop");
    QVERIFY2(dimItemNow != nullptr, "the dimmer never got a row in the Looks list");
    QVERIFY2(dimItemNow->parent() == effectItemNow,
             qPrintable(QString("dropping the dimmer on the effect row did not nest it -- "
                                "landed as a %1 instead of a child of the effect")
                        .arg(dimItemNow->parent() == nullptr ? "top-level base look"
                                                              : "child of something else")));
}

/* SceneGroupLooks::dropEvent()'s column-precision guard: the two-column
 * layout visually promises "drag here means this," so a palette (-> Looks)
 * dropped squarely on the Targets column, or a fixture group (-> Targets)
 * dropped squarely on the Looks column, is rejected -- not silently
 * accepted anywhere the way a naive "just add whatever it is" drop handler
 * would. The guard reads qlcEventPos(event) against
 * m_targetList->geometry()/m_lookList->geometry(), both canvas-local -- get
 * that coordinate space wrong (viewport-local instead) and a test could
 * "pass" for the wrong reason, seeing nothing happen because the drop
 * landed off both widgets entirely, not because the guard caught it. This
 * test rules that out explicitly: the SAME mime, dropped in the SAME
 * column, immediately afterward, must succeed.
 */
void ProgrammingManager_Test::droppingOnTheWrongColumnIsRejected()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Member");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Some Group");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fxi->id());

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Green");
    col->setValue(QColor(0, 255, 0));
    QVERIFY(m_doc->addPalette(col));

    Scene *scene = new Scene(m_doc);
    scene->setName("Column Guard");
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    SceneGroupLooks *canvas = openCanvas(mgr, scene->id());
    QVERIFY(canvas != nullptr);

    // A palette dropped on the TARGETS column: rejected, nothing changes.
    QMimeData *colMimeWrong = paletteMimeData(mgr.m_paletteTree, col);
    QVERIFY(colMimeWrong != nullptr);
    deliverDrop(canvas, colMimeWrong, canvas->m_targetList->geometry().center());
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().isEmpty(),
             "a palette dropped on the Targets column was accepted -- the "
             "column-precision guard let it through");

    // The SAME palette, dropped on the LOOKS column right after: succeeds.
    // Rules out "the coordinates just miss both widgets" as the reason the
    // drop above did nothing.
    QMimeData *colMimeRight = paletteMimeData(mgr.m_paletteTree, col);
    QVERIFY(colMimeRight != nullptr);
    deliverDrop(canvas, colMimeRight, canvas->m_lookList->geometry().center());
    QCoreApplication::processEvents();
    QVERIFY2(scene->palettes().contains(col->id()),
             "the same palette, dropped on the correct column right after, still "
             "did not land -- the earlier rejection was not the guard working, "
             "the drop coordinates just never reach either widget");

    // A fixture group dropped on the LOOKS column: rejected, nothing changes.
    QMimeData *grpMimeWrong = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY(grpMimeWrong != nullptr);
    deliverDrop(canvas, grpMimeWrong, canvas->m_lookList->geometry().center());
    QVERIFY2(scene->fixtureGroups().isEmpty(),
             "a fixture group dropped on the Looks column was accepted -- the "
             "column-precision guard let it through");

    // The SAME group, dropped on the TARGETS column right after: succeeds.
    QMimeData *grpMimeRight = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY(grpMimeRight != nullptr);
    deliverDrop(canvas, grpMimeRight, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtureGroups().contains(grp->id()),
             "the same group, dropped on the correct column right after, still "
             "did not land");
}

/* scenegrouplooks.cpp's group-drop branch strips a fixed target's baked
 * values that the group's own looks now cover, and drops the fixed target
 * entirely once nothing uncovered is left on it
 * (WORKFLOW-SUBTLETIES.md-style finding from surveying this code: the check
 * is `keep.isEmpty()`, built ONLY from the fixture's existing baked
 * SceneValues -- a freshly-added fixed target with zero baked values has
 * nothing to iterate, so `keep` never gets anything added to it and stays
 * empty regardless of whether the group's looks cover anything at all. A
 * fixture added as an individual target and never given a single value
 * disappears from Targets the instant its group is dropped in, even with no
 * palette applied to either).
 */
void ProgrammingManager_Test::droppingAGroupFoldsAwayAnAlreadyCoveredFixedTarget()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Shared Member");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Covering Group");
    grp->setSize(QSize(1, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(fxi->id());

    Scene *scene = new Scene(m_doc);
    scene->setName("Overlap");
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    SceneGroupLooks *canvas = openCanvas(mgr, scene->id());
    QVERIFY(canvas != nullptr);

    // 1. Drop the fixture individually first -- a fixed target with no
    //    baked values at all (nothing was ever set on it).
    QMimeData *fxMime = fixtureMimeData(mgr.m_fixGroupSource, fxi->id());
    QVERIFY(fxMime != nullptr);
    deliverDrop(canvas, fxMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtures().contains(fxi->id()),
             "the individual fixture drop did not add it as a fixed target");
    bool hasBakedValue = false;
    foreach (const SceneValue &scv, scene->values())
        if (scv.fxi == fxi->id())
            hasBakedValue = true;
    QVERIFY2(hasBakedValue == false,
             "test setup assumption broken: the fixed target already has a "
             "baked value, which is supposed to be the point of this scenario");

    // 2. Drop the group that shares this fixture as a member. The fixed
    //    target must fold away -- the group now covers the same fixture,
    //    and it had nothing uncovered to keep it around for.
    QMimeData *grpMime = fixtureGroupMimeData(mgr.m_fixGroupSource, grp->id());
    QVERIFY(grpMime != nullptr);
    deliverDrop(canvas, grpMime, canvas->m_targetList->geometry().center());
    QVERIFY2(scene->fixtureGroups().contains(grp->id()),
             "dropping the group did not add it as a dynamic target");
    QVERIFY2(scene->fixtures().contains(fxi->id()) == false,
             "the individual fixed target survived the group drop -- it should "
             "have folded away (group now covers the same fixture with nothing "
             "of its own left uncovered)");
}

/* A 540/270 moving head with pan and tilt only. */
static Fixture *makeMover(Doc *doc, QLCFixtureDef *def)
{
    def->setManufacturer("Test");
    def->setModel("Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(k == 0 ? QLCChannel::Pan : QLCChannel::Tilt);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName("Mover");
    fxi->setFixtureDefinition(def, mode);
    return doc->addFixture(fxi) ? fxi : nullptr;
}

void ProgrammingManager_Test::aNewPanTiltIsTriedOnTheLookSoTheRigFollows()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    QLCFixtureDef *def = new QLCFixtureDef();
    Fixture *mover = makeMover(m_doc, def);
    QVERIFY(mover != nullptr);

    QLCPalette *base = new QLCPalette(QLCPalette::PanTilt, m_doc);
    base->setName("Base");
    base->setValue(90, 45);
    QVERIFY(m_doc->addPalette(base));
    Scene *look = new Scene(m_doc);
    look->setName("Spots");
    look->addFixture(mover->id());
    look->addPalette(base->id());
    QVERIFY(m_doc->addFunction(look));

    StructureStudioView rig(m_doc, StructureStudioView::StageKind, 0);
    rig.setActiveScene(look->id());
    rig.rebuildAimTargets();
    auto rigPan = [&]() {
        const QByteArray v = rig.m_sceneValues.value(mover->id());
        return v.isEmpty() ? -1 : int(uchar(v.at(0)));
    };
    const int basePan = rigPan();

    {
        ProgrammingManager mgr(nullptr, m_doc);
        QVERIFY(openCanvas(mgr, look->id()) != nullptr);
        QVERIFY(mgr.m_lookEditor->trialNote().isEmpty());

        // "New Pan/Tilt" from the palette list: it opens in the editor,
        // and the look on the canvas tries it on.
        const int before = m_doc->palettes().size();
        mgr.createPalette(QLCPalette::PanTilt);
        QCOMPARE(m_doc->palettes().size(), before + 1);
        const quint32 pid = mgr.m_lookEditor->paletteId();
        QLCPalette *fresh = m_doc->palette(pid);
        QVERIFY(fresh != nullptr && fresh->type() == QLCPalette::PanTilt);
        QCOMPARE(look->trialPalette(), pid);
        QVERIFY(!look->palettes().contains(pid));
        // Its new "Palettes/PanTilt" folder didn't rename the look (id 0).
        QCOMPARE(look->name(), QString("Spots"));
        QVERIFY2(mgr.m_lookEditor->trialNote().contains("Spots"),
                 qPrintable(mgr.m_lookEditor->trialNote()));

        // Moving the pad moves the rig: the look is re-resolved with the trial.
        QSignalSpy changed(m_doc, &Doc::functionChanged);
        mgr.m_lookEditor->slotPanTiltChanged(QPointF(200.0, 100.0));
        QVERIFY(changed.count() > 0);
        QCOMPARE(changed.last().at(0).toUInt(), look->id());
        rig.rebuildAimTargets();
        const QList<SceneValue> want = fresh->valuesFromFixtures(m_doc, QList<quint32>() << mover->id());
        QVERIFY(!want.isEmpty());
        QCOMPARE(rigPan(), int(want.first().value));
        QVERIFY2(rigPan() != basePan, "the rig still shows the look's own position");

        // Dropped INTO the look for real: no longer a trial, no note.
        look->addPalette(pid);
        mgr.slotCanvasModified();
        QCOMPARE(look->trialPalette(), QLCPalette::invalidId());
        QVERIFY(mgr.m_lookEditor->trialNote().isEmpty());
        look->removePalette(pid);

        // Editing one of the look's own palettes tries nothing on.
        mgr.m_lookEditor->setPalette(base->id());
        QCOMPARE(look->trialPalette(), QLCPalette::invalidId());
        QVERIFY(mgr.m_lookEditor->trialNote().isEmpty());

        // Back to the loose one, then away from the look: the trial ends.
        mgr.m_lookEditor->setPalette(pid);
        QCOMPARE(look->trialPalette(), pid);
        mgr.loadCanvas(Function::invalidId());
        QCOMPARE(look->trialPalette(), QLCPalette::invalidId());

        // No look on the canvas: nothing to try it on, and the editor says so.
        mgr.m_lookEditor->setPalette(pid);
        QVERIFY2(mgr.m_lookEditor->trialNote().contains("Not in a look"),
                 qPrintable(mgr.m_lookEditor->trialNote()));
    }
    rig.rebuildAimTargets();
    QCOMPARE(rigPan(), basePan);                    // nothing was saved into it
    QCOMPARE(look->palettes(), QList<quint32>() << base->id());
    m_doc->deleteFixture(mover->id());
    delete def;
}

QTEST_MAIN(ProgrammingManager_Test)
