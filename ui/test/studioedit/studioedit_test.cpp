/*
  Q Light Controller Plus
  studioedit_test.cpp

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

#include <QApplication>
#include <QInputDialog>
#include <QMouseEvent>
#include <QLineEdit>
#include <QDialog>
#include <QPainter>
#include <QMenu>
#include <QTimer>
#include <QShortcut>
#include <QLabel>
#include <QSignalSpy>
#include <QTreeWidget>
#include <QDir>
#include <QtTest>
#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorlayerspanel.h"
#include "structurestudioview.h"
#include "shapehandles.h"
#include "platformitem.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "measurementunits.h"
#include "stageplatform.h"
#include "contextmenus.h"
#include "scenery.h"
#include "fixture.h"
#include "stand.h"
#include "truss.h"
#include "pipe.h"
#include "doc.h"

#include "studioedit_test.h"

namespace
{
/** A plot 20 by 12 m at the Monitor's own scale, in Design. RAII: a failed
 *  QVERIFY returns out of the test and must not leak the Monitor. */
struct Rig
{
    Doc *doc = nullptr;
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;

    ~Rig()
    {
        delete mon;
        delete doc;
    }

    MonitorProperties *props() const { return doc->monitorProperties(); }

    bool build()
    {
        doc = new Doc(nullptr);
        doc->setMode(Doc::Design);
        props()->setGridUnits(MonitorProperties::Meters);
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        if (gv == nullptr)
            return false;
        parent.resize(1230, 760);
        mon->resize(1230, 760);
        gv->resize(1230, 760);
        gv->setSnapDivisions(0);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(20, 12));
        return true;
    }

    bool show()
    {
        parent.show();
        return QTest::qWaitForWindowExposed(&parent);
    }

    StagePlatform *platform(float x, float y, float w, float d, const char *name = "Riser")
    {
        StagePlatform *p = props()->addPlatform();
        p->setName(name);
        p->setOriginX(x);
        p->setOriginY(y);
        p->setWidth(w);
        p->setDepth(d);
        p->setHeight(0.4f);
        gv->updatePlatforms();
        return p;
    }

    void select(const QString &kind, quint32 id)
    {
        gv->selectMapItem(kind, id);
        QApplication::processEvents();
    }

    QPoint view(const QPointF &scene) const { return gv->mapFromScene(scene); }
    QPoint viewAt(double xm, double ym) const
    {
        return gv->mapFromScene(gv->planToScene(QPointF(xm, ym)));
    }

    void drag(const QPoint &from, const QPoint &to, Qt::KeyboardModifiers mods = Qt::NoModifier,
              QString *readout = nullptr)
    {
        QWidget *vp = gv->viewport();
        QTest::mousePress(vp, Qt::LeftButton, mods, from);
        move(from + (to - from) / 3, mods);
        move(to, mods);
        if (readout != nullptr && gv->m_shapeHandles != nullptr)
            *readout = gv->m_shapeHandles->readout();
        QTest::mouseRelease(vp, Qt::LeftButton, mods, to);
        QApplication::processEvents();
    }

    void move(const QPoint &at, Qt::KeyboardModifiers mods)
    {
        QWidget *vp = gv->viewport();
        QMouseEvent ev(QEvent::MouseMove, QPointF(at), vp->mapToGlobal(QPointF(at)),
                       Qt::NoButton, Qt::LeftButton, mods);
        QApplication::sendEvent(vp, &ev);
    }
};

/** Run @p fn on the next popup menu, then close it. */
void onNextPopup(const std::function<void(QMenu *)> &fn)
{
    QTimer *poll = new QTimer;
    poll->setInterval(10);
    QObject::connect(poll, &QTimer::timeout, poll, [poll, fn]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
            return;
        poll->stop();
        poll->deleteLater();
        fn(m);
        m->close();
    });
    poll->start();
}

QAction *actionNamed(const QMenu *menu, const QString &name)
{
    for (QAction *a : menu->actions())
        if (a->objectName() == name)
            return a;
    return nullptr;
}

QMenu *submenuNamed(const QMenu *menu, const QString &name)
{
    for (QAction *a : menu->actions())
        if (a->menu() != nullptr && (a->objectName() == name || a->menu()->objectName() == name))
            return a->menu();
    return nullptr;
}
}

void StudioEdit_Test::cleanup()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
}

/* The geometry on its own: a 4 x 2 m box turned 90 degrees, its far end
   dragged out 1 m -- the near end stays put, the middle moves half -- and a
   stretch rounds to the snap step and never goes below the shortest. */
void StudioEdit_Test::aShapeStretchesFromTheOppositeSideAndSnaps()
{
    StageShape s;
    s.kind = QStringLiteral("platform");
    s.centre = QPointF(5, 5);
    s.angle = 90.0;
    s.length = 4.0;
    s.width = 2.0;
    s.stretchU = s.stretchV = s.canRotate = true;
    QCOMPARE(s.handles().size(), 9);
    // Turned 90: its length runs along +Y.
    const QPointF plusEnd = s.handlePoint(StageShape::EndPlusU);
    QVERIFY(qAbs(plusEnd.x() - 5.0) < 1e-9 && qAbs(plusEnd.y() - 7.0) < 1e-9);
    const QPointF minusEnd = s.handlePoint(StageShape::EndMinusU);

    StageShape t = s.stretched(StageShape::EndPlusU, QPointF(5.3, 8.0));
    QVERIFY(qAbs(t.length - 5.0) < 1e-9);
    QVERIFY(qAbs(t.width - 2.0) < 1e-9);             // the side it was not dragged by
    QVERIFY(QLineF(t.handlePoint(StageShape::EndMinusU), minusEnd).length() < 1e-9);
    QVERIFY(qAbs(t.centre.y() - 5.5) < 1e-9);

    // A corner: both at once, the opposite corner stays.
    const QPointF opp = s.handlePoint(StageShape::CornerMM);
    t = s.stretched(StageShape::CornerPP, s.handlePoint(StageShape::CornerPP) + QPointF(-0.5, 1.0));
    QVERIFY(qAbs(t.length - 5.0) < 1e-9 && qAbs(t.width - 2.5) < 1e-9);
    QVERIFY(QLineF(t.handlePoint(StageShape::CornerMM), opp).length() < 1e-9);

    // Snapped to half a metre; never shorter than the shortest.
    t = s.stretched(StageShape::EndPlusU, QPointF(5.0, 7.8), 0.5);
    QVERIFY(qAbs(t.length - 5.0) < 1e-9);
    t = s.stretched(StageShape::EndPlusU, QPointF(5.0, -20.0));
    QVERIFY(qAbs(t.length - s.minLength) < 1e-9);

    // A truss only stretches along itself: a side handle changes nothing.
    StageShape tr = s;
    tr.stretchV = false;
    QCOMPARE(tr.handles().size(), 3);                 // two ends and the knob
    t = tr.stretched(StageShape::SidePlusV, QPointF(9, 9));
    QVERIFY(qAbs(t.width - 2.0) < 1e-9 && t.centre == s.centre);

    QCOMPARE(StageShape::normalised(190.0), -170.0);
    QCOMPARE(StageShape::normalised(-180.0), 180.0);
}

/* "Should only be on when unlocked": selected on its own from above, a deck
   shows its handles; its own lock, its group's, its layer's, the plot's,
   Operate and an elevation each take them away, and lifting it puts them
   back. */
void StudioEdit_Test::handlesShowOnlyWhileNothingHoldsIt()
{
    Rig rig;
    QVERIFY(rig.build());
    MonitorProperties *props = rig.props();
    StagePlatform *p = rig.platform(4, 3, 4, 2);
    rig.select(QStringLiteral("platform"), p->id());
    QVERIFY(rig.gv->shapeHandles() != nullptr);
    QCOMPARE(rig.gv->shapeHandles()->count(), 9);      // 2 ends, 2 sides, 4 corners, the knob

    auto shown = [&rig]() { return rig.gv->shapeHandles() != nullptr; };
    QString why;

    p->setLocked(true);                                 // its own lock
    rig.gv->refreshItemLayerState();
    QVERIFY(!shown());
    QVERIFY(!rig.gv->shapeTarget(&why).isValid());
    QCOMPARE(why, QString("Locked — unlock it to change it"));
    p->setLocked(false);
    rig.gv->refreshItemLayerState();
    QVERIFY(shown());

    const quint32 gid = props->nextGroupId();         // its group's
    props->createGroup(gid, QStringLiteral("Band"), p->layerId(), 0);
    p->setGroupId(gid);
    props->setGroupLocked(gid, true);
    rig.gv->refreshItemLayerState();
    QVERIFY(!shown());
    rig.gv->shapeTarget(&why);
    QCOMPARE(why, QString("Group locked — unlock it in Plot layers"));
    props->setGroupLocked(gid, false);
    rig.gv->refreshItemLayerState();
    QVERIFY(shown());

    props->setLayerLocked(p->layerId(), true);         // its layer's
    rig.gv->refreshItemLayerState();
    QVERIFY(!shown());
    rig.gv->shapeTarget(&why);
    QCOMPARE(why, QString("Layer locked — unlock it in Plot layers"));
    props->setLayerLocked(p->layerId(), false);
    rig.gv->refreshItemLayerState();
    QVERIFY(shown());

    rig.gv->setLayoutLocked(true);                      // Plot Locked
    QVERIFY(!shown());
    rig.gv->shapeTarget(&why);
    QCOMPARE(why, QString("Plot locked — Edit Plot to change it"));
    rig.gv->setLayoutLocked(false);
    QVERIFY(shown());

    rig.doc->setMode(Doc::Operate);
    QVERIFY(!shown());
    rig.doc->setMode(Doc::Design);
    QVERIFY(shown());

    rig.gv->setViewPOV(MonitorGraphicsView::PovFront);
    QVERIFY(!shown());
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
    rig.select(QStringLiteral("platform"), p->id());
    QVERIFY(shown());

    rig.gv->scene()->clearSelection();
    QApplication::processEvents();
    QVERIFY(!shown());
}

/* Dragging a deck's side out 2 m: wider by 2 m, its other side where it
   was, the readout saying the size it will be in Settings' unit, and one
   Ctrl+Z puts it back. */
void StudioEdit_Test::stretchingAPlatformIsOneUndoStepWithAReadoutInSettingsUnits()
{
    for (MeasurementUnits::LengthUnit unit : { MeasurementUnits::Feet, MeasurementUnits::Metres })
    {
        MeasurementUnits::setPreferredLength(unit);
        Rig rig;
        QVERIFY(rig.build());
        QVERIFY(rig.show());
        StagePlatform *p = rig.platform(4, 3, 4, 2);
        rig.select(QStringLiteral("platform"), p->id());
        ShapeHandles *h = rig.gv->shapeHandles();
        QVERIFY(h != nullptr);
        QVERIFY(!rig.gv->canUndo());

        const QPoint from = rig.view(h->handlePos(StageShape::EndPlusU));
        QCOMPARE(from, rig.viewAt(8.0, 4.0));
        QString readout;
        rig.drag(from, rig.viewAt(10.0, 4.3), Qt::NoModifier, &readout);
        QVERIFY2(qAbs(p->width() - 6.0f) < 0.05f, qPrintable(QString::number(p->width())));
        QVERIFY(qAbs(p->depth() - 2.0f) < 1e-4f);
        QVERIFY(qAbs(p->originX() - 4.0f) < 1e-3f);        // the other side stayed
        QVERIFY(qAbs(p->originY() - 3.0f) < 1e-3f);
        QCOMPARE(readout, QString("%1 × %2").arg(MeasurementUnits::formatMetres(double(p->width())),
                                                 MeasurementUnits::formatMetres(2.0)));
        if (unit == MeasurementUnits::Feet)
            QVERIFY2(readout.contains(QLatin1Char('\'')), qPrintable(readout));
        // Still selected, the handles where it now is.
        QVERIFY(rig.gv->shapeHandles() != nullptr);
        QCOMPARE(rig.view(rig.gv->shapeHandles()->handlePos(StageShape::EndPlusU)), rig.viewAt(p->originX() + p->width(), 4.0));

        QVERIFY(rig.gv->canUndo());
        rig.gv->undoLastMove();
        p = rig.props()->platform(p->id());
        QVERIFY(p != nullptr);
        QVERIFY(qAbs(p->width() - 4.0f) < 1e-4f);
        QVERIFY(!rig.gv->canUndo());                       // one step

        // Pressed and let go on a handle without moving: nothing to undo.
        rig.select(QStringLiteral("platform"), p->id());
        const QPoint still = rig.view(rig.gv->shapeHandles()->handlePos(StageShape::SidePlusV));
        QTest::mousePress(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, still);
        QTest::mouseRelease(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, still);
        QVERIFY(!rig.gv->canUndo());
    }
}

/* The knob turns it about its middle in 15-degree steps, the readout in
   degrees; Alt turns it freely. One undo step each. */
void StudioEdit_Test::theKnobTurnsInFifteenDegreeStepsUnlessAltIsHeld()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    StagePlatform *p = rig.platform(6, 4, 4, 2);
    const QPointF mid = p->centre();
    rig.select(QStringLiteral("platform"), p->id());
    ShapeHandles *h = rig.gv->shapeHandles();
    QVERIFY(h != nullptr);
    const QPoint knob = rig.view(h->handlePos(StageShape::RotateKnob));

    // The knob is straight out from the middle at -90: drag it to about -50.
    const double r = 3.0;
    auto at = [&](double deg) {
        const double a = qDegreesToRadians(deg);
        return rig.viewAt(mid.x() + r * qCos(a), mid.y() + r * qSin(a));
    };
    QString readout;
    rig.drag(knob, at(-50.0), Qt::NoModifier, &readout);
    QVERIFY2(qAbs(p->rotation() - 45.0f) < 1e-3f, qPrintable(QString::number(p->rotation())));
    QCOMPARE(readout, QString("45° (+45°)"));
    QVERIFY(QLineF(p->centre(), mid).length() < 1e-4);   // about its middle
    QVERIFY(rig.gv->canUndo());
    rig.gv->undoLastMove();
    p = rig.props()->platform(p->id());
    QVERIFY(qAbs(p->rotation()) < 1e-4f);
    QVERIFY(!rig.gv->canUndo());

    // With Alt: as far as it is dragged.
    rig.select(QStringLiteral("platform"), p->id());
    const QPoint knob2 = rig.view(rig.gv->shapeHandles()->handlePos(StageShape::RotateKnob));
    rig.drag(knob2, at(-50.0), Qt::AltModifier);
    QVERIFY2(qAbs(p->rotation() - 40.0f) < 1.5f, qPrintable(QString::number(p->rotation())));
    QVERIFY(qAbs(p->rotation() - 45.0f) > 1.0f);
}

/* A truss stretched at its near end: longer, its far end where it was, and
   the light on it where it was on the stage (further along the truss). */
void StudioEdit_Test::aTrussStretchedAtItsNearEndKeepsItsLightsWhereTheyAre()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    MonitorProperties *props = rig.props();
    Truss *t = props->addTruss();
    t->setOrigin(QVector3D(4.0f, 2.0f, 6.0f));
    t->setLength(6.0f);
    Fixture *fx = new Fixture(rig.doc);
    fx->setName("Par");
    fx->setChannels(1);
    QVERIFY(rig.doc->addFixture(fx));
    FixtureRigProps rp = props->fixtureRigProps(fx->id());
    rp.trussId = t->id();
    rp.trussOffset = 2.0f;
    props->setFixtureRigProps(fx->id(), rp);
    rig.gv->updateTrusses();
    if (rig.gv->fixtureItemForId(fx->id()) == nullptr)
        rig.gv->addFixture(fx->id(), QPointF(6000, 2000));
    const QVector3D was = props->fixtureRigPosition(fx->id());

    rig.select(QStringLiteral("truss"), t->id());
    ShapeHandles *h = rig.gv->shapeHandles();
    QVERIFY(h != nullptr);
    QCOMPARE(h->count(), 3);                            // two ends and the knob
    QString readout;
    rig.drag(rig.view(h->handlePos(StageShape::EndMinusU)), rig.viewAt(3.0, 2.0), Qt::NoModifier, &readout);
    QVERIFY2(qAbs(t->length() - 7.0f) < 0.05f, qPrintable(QString::number(t->length())));
    QVERIFY(qAbs(t->origin().x() - 3.0f) < 0.05f);
    QVERIFY(qAbs(t->origin().z() - 6.0f) < 1e-4f);     // its height is its own
    QVERIFY(qAbs(t->positionAt(t->length()).x() - 10.0f) < 1e-3f);
    QCOMPARE(readout, QString("Length %1").arg(MeasurementUnits::formatMetres(double(t->length()))));
    QVERIFY(qAbs(props->fixtureRigProps(fx->id()).trussOffset - 3.0f) < 0.05f);
    QVERIFY((props->fixtureRigPosition(fx->id()) - was).length() < 0.05f);

    rig.gv->undoLastMove();
    t = props->truss(t->id());
    QVERIFY(qAbs(t->length() - 6.0f) < 1e-4f);
    QVERIFY(qAbs(props->fixtureRigProps(fx->id()).trussOffset - 2.0f) < 1e-4f);
}

/* A cyc stretches but has no knob; a leg has one. Two decks picked together
   get the knob alone, and turn about their middle as one. */
void StudioEdit_Test::aCycStretchesButDoesNotTurnAndAGroupTurnsAsOne()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    MonitorProperties *props = rig.props();
    Scenery *cyc = props->addScenery();
    cyc->setKind(Scenery::Cyclorama);
    cyc->setOriginX(10.0f);
    cyc->setOriginY(10.0f);
    cyc->setWidth(12.0f);
    Scenery *leg = props->addScenery();
    leg->setKind(Scenery::Leg);
    leg->setOriginX(3.0f);
    leg->setOriginY(6.0f);
    leg->setWidth(1.5f);
    rig.gv->updatePlatforms();

    rig.select(QStringLiteral("scenery"), cyc->id());
    QVERIFY(rig.gv->shapeHandles() != nullptr);
    QCOMPARE(rig.gv->shapeHandles()->count(), 2);
    QVERIFY(rig.gv->shapeHandles()->handlePos(StageShape::RotateKnob).isNull());
    rig.drag(rig.view(rig.gv->shapeHandles()->handlePos(StageShape::EndPlusU)), rig.viewAt(17.0, 10.0));
    QVERIFY2(qAbs(cyc->width() - 13.0f) < 0.05f, qPrintable(QString::number(cyc->width())));
    QVERIFY(qAbs(cyc->originX() - 10.5f) < 0.05f);      // its other end stayed at 4

    rig.select(QStringLiteral("scenery"), leg->id());
    QCOMPARE(rig.gv->shapeHandles()->count(), 3);

    // Two decks together: the knob alone, about the middle of both.
    StagePlatform *a = rig.platform(2, 2, 2, 1, "A");
    StagePlatform *b = rig.platform(6, 2, 2, 1, "B");
    rig.gv->selectMapItems({ qMakePair(QStringLiteral("platform"), a->id()),
                             qMakePair(QStringLiteral("platform"), b->id()) });
    QApplication::processEvents();
    const StageShape both = rig.gv->shapeTarget();
    QCOMPARE(both.kind, QString("selection"));
    QCOMPARE(rig.gv->shapeHandles()->count(), 1);
    const QPointF mid = both.centre;
    QVERIFY(qAbs(mid.x() - 5.0) < 1e-3 && qAbs(mid.y() - 2.5) < 1e-3);
    const QPoint knob = rig.view(rig.gv->shapeHandles()->handlePos(StageShape::RotateKnob));
    rig.drag(knob, rig.viewAt(mid.x() + 3.0, mid.y()));           // a quarter turn
    QVERIFY2(qAbs(a->rotation() - 90.0f) < 1e-3f && qAbs(b->rotation() - 90.0f) < 1e-3f,
             qPrintable(QString("%1 %2").arg(a->rotation()).arg(b->rotation())));
    QVERIFY(qAbs(a->centre().x() - 5.0) < 1e-3 && qAbs(a->centre().y() - 0.5) < 1e-3);
    rig.gv->undoLastMove();
    QVERIFY(qAbs(props->platform(a->id())->rotation()) < 1e-4f);
    QVERIFY(qAbs(props->platform(b->id())->rotation()) < 1e-4f);
}

/* "Two platforms .. right click .. add to group with list and new group":
   New Group… makes one of them, a third joins it from the list, Remove
   from Group takes one out again -- each one undo step. */
void StudioEdit_Test::groupMenuMakesAddsToAndRemovesFromGroupsOneUndoEach()
{
    Rig rig;
    QVERIFY(rig.build());
    MonitorProperties *props = rig.props();
    StagePlatform *a = rig.platform(2, 2, 2, 1, "A");
    StagePlatform *b = rig.platform(6, 2, 2, 1, "B");
    StagePlatform *c = rig.platform(10, 2, 2, 1, "C");
    typedef QPair<QString, quint32> Obj;
    const QList<Obj> ab = { Obj("platform", a->id()), Obj("platform", b->id()) };

    // The item menu of A with A and B selected: Group ▸ in place of the old two.
    rig.gv->selectMapItems(ab);
    QMenu menu;
    MonitorGraphicsView::MenuPlace place;
    place.selection = ab;
    rig.gv->buildItemMenu(&menu, QStringLiteral("platform"), a->id(), place);
    QMenu *group = submenuNamed(&menu, QStringLiteral("plotGroup"));
    QVERIFY(group != nullptr);
    for (QAction *act : menu.actions())
    {
        QVERIFY(!act->text().contains(QStringLiteral("Folder")));
    }
    QVERIFY(ContextMenu::checkCanon(&menu).isEmpty());
    QAction *made = actionNamed(group, QStringLiteral("plotNewGroup"));
    QVERIFY(made != nullptr && made->isEnabled());
    QCOMPARE(made->text(), QString("New Group"));
    QCOMPARE(made->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_G));
    QVERIFY(actionNamed(group, QStringLiteral("plotNoGroups")) != nullptr);
    QVERIFY(actionNamed(group, QStringLiteral("plotRemoveFromGroup")) == nullptr);   // in none yet

    // New Group makes it at once, named "Group N": no dialog, one undo.
    QSignalSpy created(rig.gv, SIGNAL(groupCreated(quint32)));
    made->trigger();
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QCOMPARE(created.count(), 1);
    const quint32 gid = a->groupId();
    QVERIFY(gid != 0);
    QCOMPARE(b->groupId(), gid);
    QVERIFY(props->group(gid).name.startsWith(QStringLiteral("Group ")));
    QCOMPARE(created.first().first().toUInt(), gid);
    QCOMPARE(c->groupId(), 0u);
    QVERIFY(rig.gv->canUndo());
    rig.gv->undoLastMove();
    QVERIFY(!rig.gv->canUndo());
    QCOMPARE(props->platform(a->id())->groupId(), 0u);
    a = props->platform(a->id());
    b = props->platform(b->id());
    c = props->platform(c->id());
    QCOMPARE(rig.gv->newGroupFrom(ab, QStringLiteral("Band Risers")) != 0u, true);
    const quint32 band = a->groupId();

    // C's menu lists it; adding is one step.
    QMenu cm;
    rig.gv->fillGroupMenu(&cm, { Obj("platform", c->id()) });
    QAction *add = nullptr;
    for (QAction *act : cm.actions())
        if (act->objectName() == QStringLiteral("plotAddToGroup") && act->data().toUInt() == band)
            add = act;
    QVERIFY(add != nullptr);
    QCOMPARE(add->text(), QString("Band Risers"));
    QVERIFY(add->isEnabled() && !add->isChecked());
    add->trigger();
    QCOMPARE(c->groupId(), band);
    // One of the group now: it selects and moves with A and B.
    QCOMPARE(rig.gv->itemsInGroup(band).size(), 3);

    // In its group: ticked, greyed, and Remove from Group is offered.
    QMenu again;
    rig.gv->fillGroupMenu(&again, { Obj("platform", c->id()) });
    for (QAction *act : again.actions())
    {
        if (act->data().toUInt() == band && act->objectName() == QStringLiteral("plotAddToGroup"))
        {
            QVERIFY(act->isChecked() && !act->isEnabled());
            QCOMPARE(act->toolTip(), QString("Already in “Band Risers”"));
        }
    }
    QAction *rem = actionNamed(&again, QStringLiteral("plotRemoveFromGroup"));
    QVERIFY(rem != nullptr && rem->isEnabled());
    QCOMPARE(rem->text(), QString("Remove from Group"));
    const quint32 layer = c->layerId();
    rem->trigger();
    QCOMPARE(c->groupId(), 0u);
    QCOMPARE(c->layerId(), layer);                     // still on its layer
    QCOMPARE(a->groupId(), band);
    rig.gv->undoLastMove();
    QCOMPARE(props->platform(c->id())->groupId(), band);

    // Ungroup still dissolves the whole of it (Ctrl+Shift+G).
    rig.gv->selectMapItem(QStringLiteral("platform"), a->id());
    QVERIFY(rig.gv->ungroupSelectedItems() > 0);
    QCOMPARE(props->platform(a->id())->groupId(), 0u);
}

/* A locked group, layer or plot: what it stops is greyed, and says why. */
void StudioEdit_Test::groupMenuIsGreyedWithTheReasonWhenALockStopsIt()
{
    Rig rig;
    QVERIFY(rig.build());
    MonitorProperties *props = rig.props();
    StagePlatform *a = rig.platform(2, 2, 2, 1, "A");
    StagePlatform *c = rig.platform(10, 2, 2, 1, "C");
    typedef QPair<QString, quint32> Obj;
    const quint32 band = rig.gv->newGroupFrom({ Obj("platform", a->id()) }, QStringLiteral("Band"));
    QVERIFY(band != 0);

    auto addFor = [&rig, band](const QMenu &m) -> QAction * {
        for (QAction *act : m.actions())
            if (act->objectName() == QStringLiteral("plotAddToGroup") && act->data().toUInt() == band)
                return act;
        return nullptr;
    };

    props->setGroupLocked(band, true);
    {
        QMenu m;
        rig.gv->fillGroupMenu(&m, { Obj("platform", c->id()) });
        QAction *add = addFor(m);
        QVERIFY(add != nullptr && !add->isEnabled());
        QCOMPARE(add->toolTip(), QString("Group “Band” is locked: unlock it in Plot layers"));
        QVERIFY(actionNamed(&m, QStringLiteral("plotNewGroup"))->isEnabled());   // C itself is free
        QVERIFY(m.toolTipsVisible());
    }
    QCOMPARE(rig.gv->addToGroup({ Obj("platform", c->id()) }, band), 0);
    {
        QMenu m;                                    // A, in the locked group, cannot leave it
        rig.gv->fillGroupMenu(&m, { Obj("platform", a->id()) });
        QAction *rem = actionNamed(&m, QStringLiteral("plotRemoveFromGroup"));
        QVERIFY(rem != nullptr && !rem->isEnabled());
        QCOMPARE(rem->toolTip(), QString("Group “Band” is locked: unlock it in Plot layers"));
    }
    props->setGroupLocked(band, false);

    const quint32 lid = c->layerId();
    props->setLayerLocked(lid, true);
    {
        QMenu m;
        rig.gv->fillGroupMenu(&m, { Obj("platform", c->id()) });
        QAction *made = actionNamed(&m, QStringLiteral("plotNewGroup"));
        QVERIFY(!made->isEnabled());
        QCOMPARE(made->toolTip(), QString("Layer “%1” is locked: unlock it in Plot layers").arg(props->layer(lid).name));
        QVERIFY(!addFor(m)->isEnabled());
    }
    QCOMPARE(rig.gv->newGroupFrom({ Obj("platform", c->id()) }, QStringLiteral("X")), 0u);
    props->setLayerLocked(lid, false);

    rig.gv->setLayoutLocked(true);
    {
        QMenu m;
        rig.gv->fillGroupMenu(&m, { Obj("platform", c->id()) });
        QAction *made = actionNamed(&m, QStringLiteral("plotNewGroup"));
        QVERIFY(!made->isEnabled());
        QCOMPARE(made->toolTip(), QString("The plot is locked: Edit Plot to change its groups"));
        QVERIFY(!addFor(m)->isEnabled());
    }
    rig.gv->setLayoutLocked(false);
    QVERIFY(rig.gv->addToGroup({ Obj("platform", c->id()) }, band) > 0);
}

/* Ctrl+G groups what is selected at once: no dialog, an automatic name, one
   undo; and no menu or tooltip in Plot layers still says "Folder". */
void StudioEdit_Test::ctrlGGroupsAtOnceAndPlotLayersSaysGroupNotFolder()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *a = rig.platform(2, 2, 2, 1, "A");
    StagePlatform *b = rig.platform(6, 2, 2, 1, "B");
    typedef QPair<QString, quint32> Obj;
    rig.gv->selectMapItems({ Obj("platform", a->id()), Obj("platform", b->id()) });
    QShortcut *sc = nullptr;
    for (QShortcut *c : rig.gv->findChildren<QShortcut *>())
    {
        if (c->key() == QKeySequence(QStringLiteral("Ctrl+G")))
            sc = c;
    }
    QVERIFY(sc != nullptr);
    QSignalSpy created(rig.gv, SIGNAL(groupCreated(quint32)));
    emit sc->activated();
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QCOMPARE(created.count(), 1);
    const quint32 gid = created.first().first().toUInt();
    MonitorProperties *props = rig.props();
    QVERIFY(gid != 0);
    QVERIFY(props->group(gid).name.startsWith(QStringLiteral("Group ")));
    QCOMPARE(props->platform(a->id())->groupId(), gid);
    QCOMPARE(props->platform(b->id())->groupId(), gid);
    QVERIFY(rig.gv->canUndo());
    rig.gv->undoLastMove();                       // one undo takes it all back
    QVERIFY(!rig.gv->canUndo());
    QCOMPARE(props->platform(a->id())->groupId(), 0u);

    // Plot layers: every menu on every row, and its hint, say Group.
    QVERIFY(rig.gv->newGroupFrom({ Obj("platform", a->id()) }, QStringLiteral("Band")) != 0u);
    MonitorLayersPanel panel(rig.doc, rig.gv);
    panel.resize(320, 400);
    panel.show();
    panel.reload();
    QStringList texts;
    for (QTreeWidgetItemIterator it(panel.m_tree); *it != nullptr; ++it)
    {
        QMenu m;
        panel.buildContextMenu(&m, *it);
        const QList<QAction *> all = m.findChildren<QAction *>();
        for (QAction *act : all)
            texts << act->text() << act->toolTip();
    }
    for (QLabel *l : panel.findChildren<QLabel *>())
        texts << l->text() << l->toolTip();
    QVERIFY2(texts.join(' ').contains(QStringLiteral("Group")), "no group text seen");
    for (const QString &t : texts)
        QVERIFY2(!t.contains(QStringLiteral("older"), Qt::CaseSensitive), qPrintable(t));
}

/* The empty plot's menu: Group ▸ where Group Selected was, Ungroup kept --
   as many items at the top as before. */
void StudioEdit_Test::theCanvasMenuHasGroupInPlaceOfGroupSelected()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *a = rig.platform(2, 2, 2, 1, "A");
    StagePlatform *b = rig.platform(6, 2, 2, 1, "B");
    rig.gv->selectMapItems({ qMakePair(QStringLiteral("platform"), a->id()),
                             qMakePair(QStringLiteral("platform"), b->id()) });
    QMenu menu;
    rig.mon->fillCanvasMenu(&menu, QPointF(0, 0));
    QStringList top;
    for (QAction *act : menu.actions())
    {
        if (!act->isSeparator())
            top << act->text();
    }
    QVERIFY(!top.contains(QStringLiteral("Group Selected")));
    QVERIFY(top.contains(QStringLiteral("Group")));
    QVERIFY(top.contains(QStringLiteral("Ungroup")));
    QVERIFY(ContextMenu::checkCanon(&menu).isEmpty());
    QMenu *group = submenuNamed(&menu, QStringLiteral("plotGroup"));
    QVERIFY(group != nullptr && group->menuAction()->isEnabled());
    QVERIFY(actionNamed(group, QStringLiteral("plotNewGroup"))->isEnabled());
}

/* Right-click a platform's Studio canvas: Group ▸ for that platform, as on
   the plot. */
void StudioEdit_Test::aStudioCanvasOffersGroupForItsStructure()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *a = rig.platform(2, 2, 2, 1, "A");
    StagePlatform *b = rig.platform(6, 2, 2, 1, "B");
    typedef QPair<QString, quint32> Obj;
    const quint32 band = rig.gv->newGroupFrom({ Obj("platform", b->id()) }, QStringLiteral("Band"));
    QDialog dlg;
    StructureStudioView *view = nullptr;
    QVERIFY(rig.mon->makeStudioPane(&dlg, 3 /* platform */, a->id(), nullptr, &view) != nullptr);
    QVERIFY(view != nullptr);
    view->resize(600, 400);

    bool sawGroup = false, sawBand = false;
    QAction *toBand = nullptr;
    onNextPopup([&](QMenu *m) {
        QMenu *g = submenuNamed(m, QStringLiteral("studioGroup"));
        sawGroup = g != nullptr;
        if (g != nullptr)
        {
            for (QAction *act : g->actions())
            {
                if (act->objectName() == QStringLiteral("plotAddToGroup") && act->data().toUInt() == band)
                {
                    sawBand = act->isEnabled();
                    toBand = act;
                }
            }
        }
        if (toBand != nullptr)
            toBand->trigger();
    });
    emit view->canvasContextMenu(view->mapToGlobal(QPoint(20, 20)), Fixture::invalidId());
    QVERIFY(sawGroup);
    QVERIFY(sawBand);
    QCOMPARE(rig.props()->platform(a->id())->groupId(), band);
}

/* The boom top's dot is drawn only while it can be dragged: never with the
   editor locked, nor with the boom, its stand, its layer or the plot
   locked; and a press there resizes nothing then. */
void StudioEdit_Test::theBoomTopHandleIsHiddenWhileLocked()
{
    Rig rig;
    QVERIFY(rig.build());
    MonitorProperties *props = rig.props();
    Stand *st = props->addStand();
    st->setOriginX(5.0f);
    st->setOriginY(3.0f);
    Pipe *boom = props->addPipe();
    boom->setOrientation(Pipe::Vertical);
    boom->setStandId(st->id());
    boom->setLength(1.5f);
    props->recomputeChildTrusses();
    rig.gv->updatePlatforms();

    QDialog dlg;
    StructureStudioView *view = nullptr;
    QVERIFY(rig.mon->makeStudioPane(&dlg, 0 /* stand */, st->id(), nullptr, &view) != nullptr);
    view->resize(600, 500);
    view->setPlane(StructureStudioView::Front);
    view->reload();

    QVERIFY(view->locked());
    QVERIFY(view->boomResizeLocked(boom));
    view->setLocked(false);
    QVERIFY(!view->boomResizeLocked(boom));
    boom->setLocked(true);
    QVERIFY(view->boomResizeLocked(boom));
    boom->setLocked(false);
    st->setLocked(true);
    QVERIFY(view->boomResizeLocked(boom));
    st->setLocked(false);
    props->setLayerLocked(boom->layerId(), true);
    QVERIFY(view->boomResizeLocked(boom));
    props->setLayerLocked(boom->layerId(), false);
    props->setLayoutLocked(true);
    QVERIFY(view->boomResizeLocked(boom));

    // Locked: a press on the top does not take it. (Drawn once first: the
    // view fits itself to its size as it paints.)
    {
        QImage warm(view->size(), QImage::Format_ARGB32);
        view->render(&warm);
    }
    const QPoint top = view->w2s(boom->positionAt(boom->length())).toPoint();
    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, top);
    QCOMPARE(view->m_resizeBoom, 0u);
    QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, top);
    props->setLayoutLocked(false);
    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, top);
    QCOMPARE(view->m_resizeBoom, boom->id() + 1);
    QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, top);

    // Drawn only while free: round its top, the picture differs by the dot
    // and nothing else changes with the lock.
    auto picture = [&]() {
        QImage img(view->size(), QImage::Format_ARGB32);
        img.fill(Qt::black);
        view->render(&img);
        return img.copy(QRect(top - QPoint(8, 8), QSize(17, 17)));
    };
    const QImage free = picture();
    view->setLocked(true);
    const QImage held = picture();
    QVERIFY(free != held);
    QCOMPARE(picture(), held);
    view->setLocked(false);
    QCOMPARE(picture(), free);
    props->setLayoutLocked(true);
    QCOMPARE(picture(), held);
}

/* Before/after pictures of the Plot layers panel's menus and the Group ▸
   submenu, three sizes (QLC_SHOT_DIR; offscreen grabs only). */
void StudioEdit_Test::groupFollowupPictures()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString tag = qEnvironmentVariable("QLC_SHOT_TAG", QStringLiteral("after"));
    typedef QPair<QString, quint32> Obj;
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        Rig rig;
        QVERIFY(rig.build());
        QVERIFY(rig.show());
        StagePlatform *a = rig.platform(2, 2, 2, 1, "Drum riser");
        StagePlatform *b = rig.platform(6, 2, 2, 1, "Keys riser");
        const QList<Obj> ab = { Obj("platform", a->id()), Obj("platform", b->id()) };
        const quint32 gid = rig.gv->newGroupFrom(ab, QStringLiteral("Band"));
        QVERIFY(gid != 0);
        rig.parent.resize(sz);
        rig.mon->resize(sz);
        rig.mon->m_layersPanel->setVisible(true);
        rig.mon->m_layersPanel->reload();
        QApplication::processEvents();
        const QString suffix = QString("%1x%2").arg(sz.width()).arg(sz.height());
        rig.parent.grab().save(QString("%1/panel-%2-%3.png").arg(dir, tag, suffix));

        MonitorLayersPanel &panel = *rig.mon->m_layersPanel;
        QTreeWidgetItem *groupRow = nullptr;
        QTreeWidgetItem *layerRow = nullptr;
        for (QTreeWidgetItemIterator it(panel.m_tree); *it != nullptr; ++it)
        {
            if ((*it)->data(0, 0).toString().startsWith(QStringLiteral("Band")))
                groupRow = *it;
            else if (layerRow == nullptr && (*it)->parent() == nullptr)
                layerRow = *it;
        }
        QMenu gm;
        if (groupRow != nullptr)
            panel.buildContextMenu(&gm, groupRow);
        gm.adjustSize();
        gm.grab().save(QString("%1/panel-group-menu-%2-%3.png").arg(dir, tag, suffix));
        QMenu lm;
        if (layerRow != nullptr)
            panel.buildContextMenu(&lm, layerRow);
        lm.adjustSize();
        lm.grab().save(QString("%1/panel-layer-menu-%2-%3.png").arg(dir, tag, suffix));

        QMenu menu;
        MonitorGraphicsView::MenuPlace place;
        place.selection = ab;
        rig.gv->buildItemMenu(&menu, QStringLiteral("platform"), a->id(), place);
        QMenu *group = submenuNamed(&menu, QStringLiteral("plotGroup"));
        QVERIFY(group != nullptr);
        group->adjustSize();
        group->grab().save(QString("%1/group-submenu-%2-%3.png").arg(dir, tag, suffix));
    }
}

void StudioEdit_Test::picturesForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    typedef QPair<QString, quint32> Obj;

    auto shoot = [&](Rig &rig, const QSize &sz, const QString &name) {
        rig.parent.resize(sz);
        rig.mon->resize(sz);
        QApplication::processEvents();
        rig.parent.grab().save(QString("%1/%2-%3x%4.png").arg(dir, name).arg(sz.width()).arg(sz.height()));
    };

    for (const QSize &sz : sizes)
    {
        Rig rig;
        QVERIFY(rig.build());
        rig.props()->setGridUnits(MonitorProperties::Feet);
        QVERIFY(rig.show());
        StagePlatform *deck = rig.platform(3, 3, 4.88f, 2.44f, "Drum riser");
        rig.platform(9, 3, 2.44f, 2.44f, "Keys riser");
        Truss *t = rig.props()->addTruss();
        t->setName("Mid truss");
        t->setOrigin(QVector3D(2.0f, 8.0f, 6.0f));
        t->setLength(9.0f);
        rig.gv->updateTrusses();
        rig.parent.resize(sz);
        rig.mon->resize(sz);
        QApplication::processEvents();

        // Before: what the plot showed for a selected deck (no handles).
        rig.select(QStringLiteral("platform"), deck->id());
        rig.gv->m_shapeHandles->hide();
        shoot(rig, sz, QStringLiteral("before-platform-selected"));

        // After: unlocked, the handles; locked, none.
        rig.gv->refreshShapeHandles();
        shoot(rig, sz, QStringLiteral("after-platform-unlocked"));
        deck->setLocked(true);
        rig.gv->refreshItemLayerState();
        shoot(rig, sz, QStringLiteral("after-platform-locked"));
        deck->setLocked(false);
        rig.gv->refreshItemLayerState();

        // Mid-drag: the shape it will take and its size, then a turn.
        ShapeHandles *h = rig.gv->shapeHandles();
        QVERIFY(h != nullptr);
        const QPoint from = rig.view(h->handlePos(StageShape::CornerPP));
        QTest::mousePress(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, from);
        rig.move(from + QPoint(40, 30), Qt::NoModifier);
        rig.move(from + QPoint(90, 60), Qt::NoModifier);
        shoot(rig, sz, QStringLiteral("after-mid-stretch"));
        // Let go without keeping it (the picture is all this wants).
        rig.gv->m_shapeDragMoved = false;
        QTest::mouseRelease(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, from);
        QVERIFY(!rig.gv->canUndo());
        h = rig.gv->shapeHandles();
        QVERIFY(h != nullptr);
        const QPoint knob = rig.view(h->handlePos(StageShape::RotateKnob));
        const QPoint mid = rig.view(rig.gv->planToScene(rig.props()->platform(deck->id())->centre()));
        QTest::mousePress(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, knob);
        rig.move(knob + QPoint(30, 0), Qt::NoModifier);
        rig.move(mid + QPoint(int((knob - mid).y() * -0.7), int((knob - mid).y() * 0.7)), Qt::NoModifier);
        shoot(rig, sz, QStringLiteral("after-mid-turn"));
        rig.gv->m_shapeDragMoved = false;
        QTest::mouseRelease(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, knob);
        QVERIFY(!rig.gv->canUndo());

        // The Group ▸ menu on the plot, open.
        rig.gv->newGroupFrom({ Obj("platform", deck->id()) }, QStringLiteral("Band risers"));
        rig.gv->selectMapItems({ Obj("platform", deck->id()), Obj("truss", t->id()) });
        QMenu menu;
        rig.mon->fillCanvasMenu(&menu, QPointF());
        QMenu *group = submenuNamed(&menu, QStringLiteral("plotGroup"));
        QVERIFY(group != nullptr);
        menu.popup(rig.parent.mapToGlobal(QPoint(sz.width() / 3, sz.height() / 4)));
        QApplication::processEvents();
        QImage canvas = rig.parent.grab().toImage();
        {
            // Drawn where they would open: the submenu beside its row.
            const QPixmap outer = menu.grab();
            const QPoint at = rig.parent.mapFromGlobal(menu.geometry().topLeft());
            QPainter pt(&canvas);
            pt.drawPixmap(at, outer);
            pt.drawPixmap(at + QPoint(outer.width() / outer.devicePixelRatio() - 4,
                                      menu.actionGeometry(group->menuAction()).top()), group->grab());
        }
        canvas.save(QString("%1/after-plot-group-menu-%2x%3.png").arg(dir).arg(sz.width()).arg(sz.height()));
        menu.close();

        // The same submenu from a platform's Studio canvas.
        QDialog dlg;
        StructureStudioView *view = nullptr;
        rig.mon->makeStudioPane(&dlg, 3, rig.props()->platforms().last()->id(), nullptr, &view);
        dlg.resize(sz.width() * 3 / 4, sz.height() * 3 / 4);
        dlg.show();
        QApplication::processEvents();
        onNextPopup([&](QMenu *m) {
            QMenu *g = submenuNamed(m, QStringLiteral("studioGroup"));
            if (g == nullptr)
                return;
            QImage img = dlg.grab().toImage();
            {
                const QPixmap outer = m->grab();
                const QPoint at = dlg.mapFromGlobal(m->geometry().topLeft());
                QPainter pt(&img);
                pt.drawPixmap(at, outer);
                pt.drawPixmap(at + QPoint(outer.width() / outer.devicePixelRatio() - 4,
                                          m->actionGeometry(g->menuAction()).top()), g->grab());
            }
            img.save(QString("%1/after-studio-group-menu-%2x%3.png").arg(dir).arg(sz.width()).arg(sz.height()));
        });
        emit view->canvasContextMenu(view->mapToGlobal(QPoint(40, 40)), Fixture::invalidId());
        dlg.close();
    }
}

QTEST_MAIN(StudioEdit_Test)
