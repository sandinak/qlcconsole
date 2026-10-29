/*
  Q Light Controller Plus - Test Unit
  performers_test.cpp

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
#include <QApplication>
#include <QGraphicsScene>
#include <QElapsedTimer>
#include <QPainter>
#include <functional>
#include <ctime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QToolButton>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>

#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "structurestudioview.h"
#include "performersdialog.h"
#include "platformitem.h"
#include "targetitem.h"
#undef private
#undef protected

#include "performers_test.h"
#include "performerfill.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{

/* A choir riser: three steps 0.4 m high and 0.8 m deep, 3 m across, the
   bottom step downstage. */
StagePlatform *threeTier(MonitorProperties *props, float x = 2.0f, float y = 1.0f)
{
    StagePlatform *p = props->addPlatform();
    p->setName(QStringLiteral("Choir riser"));
    p->setOriginX(x);
    p->setOriginY(y);
    p->setSteps(3, 0.4f, 0.8f, 3.0f, StagePlatform::ClimbUpstage);
    return p;
}

QList<StageTarget *> people(MonitorProperties *props)
{
    QList<StageTarget *> out;
    foreach (StageTarget *t, props->stageTargets())
        if (t->kind() == StageTarget::Kind::Person)
            out << t;
    return out;
}

Fixture *makeWash(Doc *doc, quint32 addr, const QVector3D &at)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel(QString("Wash %1").arg(addr));
    def->setType(QLCFixtureDef::MovingHead);
    const char *nm[] = { "Pan", "Tilt", "Dimmer", "Red", "Green", "Blue" };
    const QLCChannel::Group gp[] = { QLCChannel::Pan, QLCChannel::Tilt,
        QLCChannel::Intensity, QLCChannel::Intensity, QLCChannel::Intensity,
        QLCChannel::Intensity };
    const QLCChannel::PrimaryColour cl[] = { QLCChannel::NoColour, QLCChannel::NoColour,
        QLCChannel::NoColour, QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[k]); ch->setGroup(gp[k]);
        if (cl[k] != QLCChannel::NoColour) ch->setColour(cl[k]);
        if (k < 2) ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setFocusPanMax(360); ph.setFocusTiltMax(180);
    ph.setLensDegreesMin(40.0); ph.setLensDegreesMax(40.0);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 6; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setName(QString("Wash %1").arg(addr)); f->setFixtureDefinition(def, mode);
    f->setUniverse(0); f->setAddress(addr);
    if (doc->addFixture(f) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(
        f->id(), 0, 0, QVector3D(at.x() * 1000.0f, at.y() * 1000.0f, at.z()));
    return f;
}

struct Plot
{
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;
    explicit Plot(Doc *doc)
    {
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        gv->resize(1000, 700);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(10, 7));
        gv->updatePlatforms();
        gv->updateTargets();
    }
    ~Plot() { delete mon; }
};

} // namespace

void Performers_Test::fillStandsARowOnEveryTier()
{
    MonitorProperties props;
    StagePlatform *p = threeTier(&props);

    PerformerFill::Options o;
    o.spacing = 0.6f;                       // 3 m: 5 a row, 4 between them
    const QList<QVector3D> spots = PerformerFill::spots(&props, p, o);
    QCOMPARE(spots.size(), 5 + 4 + 5);

    // Bottom row first: downstage, on the first tread; the top row on the third.
    QMap<int, int> perHeight;
    foreach (const QVector3D &s, spots)
    {
        const int cm = qRound(s.z() * 100.0f);
        perHeight[cm]++;
        QVERIFY2(p->containsPoint(s.x(), s.y()), "a person stands off the riser");
        QCOMPARE(props.platformHeightAt(s.x(), s.y()), s.z());
    }
    QCOMPARE(perHeight.value(40), 5);
    QCOMPARE(perHeight.value(80), 4);
    QCOMPARE(perHeight.value(120), 5);
    QVERIFY2(spots.first().y() > spots.last().y(), "the bottom row is not downstage of the top");
    // Each in the middle of its tread: the bottom tread is the downstage 0.8 m.
    QVERIFY(qAbs(spots.first().y() - (1.0f + 2.4f - 0.4f)) < 1e-3f);
    // The staggered row stands in the gaps of the row in front.
    QVERIFY(qAbs((spots.at(5).x() - spots.at(0).x()) - 0.3f) < 1e-3f);

    o.stagger = false;
    QCOMPARE(PerformerFill::spots(&props, p, o).size(), 15);

    o.perRow = 6;                           // spread evenly, any spacing
    const QList<QVector3D> six = PerformerFill::spots(&props, p, o);
    QCOMPARE(six.size(), 18);
    QVERIFY(qAbs(six.at(0).x() - (2.0f + 0.25f)) < 1e-3f);
    QVERIFY(qAbs(six.at(5).x() - (2.0f + 2.75f)) < 1e-3f);
}

void Performers_Test::deckRowsAndTurnedRisers()
{
    MonitorProperties props;
    StagePlatform *deck = props.addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(2.4f); deck->setDepth(2.0f); deck->setHeight(0.6f);

    PerformerFill::Options o;
    o.spacing = 0.6f;
    o.stagger = false;
    const QList<QVector3D> s = PerformerFill::spots(&props, deck, o);
    QCOMPARE(s.size(), 2 * 4);              // 2 m deep: two rows of 4
    foreach (const QVector3D &v, s)
        QCOMPARE(v.z(), 0.6f);
    QVERIFY(s.first().y() > 1.0f && s.last().y() < 1.0f);   // front row first

    // Turned a quarter: the rows run up and down the stage now.
    deck->setRotation(90.0f);
    const QList<QVector3D> t = PerformerFill::spots(&props, deck, o);
    QCOMPARE(t.size(), 8);
    QVERIFY(qAbs(t.at(0).x() - t.at(1).x()) < 1e-3f);
    foreach (const QVector3D &v, t)
        QVERIFY(deck->containsPoint(v.x(), v.y()));

    // A rounded front loses the corners of its row.
    StagePlatform *apron = props.addPlatform();
    apron->setOriginX(5.0f); apron->setOriginY(0.0f);
    apron->setWidth(3.0f); apron->setDepth(0.9f); apron->setHeight(0.3f);
    QCOMPARE(PerformerFill::spots(&props, apron, o).size(), 5);
    apron->setFrontCurve(0.9f);
    QCOMPARE(PerformerFill::spots(&props, apron, o).size(), 3);
}

void Performers_Test::theyAreOneSetInThePerformersLayer()
{
    MonitorProperties props;
    StagePlatform *p = threeTier(&props);

    PerformerFill::Options o;
    o.setName = QStringLiteral("Choir");
    o.height = PerformerFill::presetHeight(PerformerFill::Students);
    o.costume = QColor(160, 20, 40);
    const PerformerFill::Result r = PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(r.targets.size(), 14);
    QVERIFY(r.groupId != 0);
    QVERIFY(props.hasLayer(r.layerId));
    QCOMPARE(props.layer(r.layerId).name, QStringLiteral("Performers"));
    QCOMPARE(props.group(r.groupId).name, QStringLiteral("Choir"));
    QCOMPARE(props.group(r.groupId).layerId, r.layerId);

    foreach (quint32 id, r.targets)
    {
        StageTarget *t = props.stageTarget(id);
        QCOMPARE(t->kind(), StageTarget::Kind::Person);
        QCOMPARE(t->layerId(), r.layerId);
        QCOMPARE(t->groupId(), r.groupId);
        QCOMPARE(t->personHeight(), 1.60f);
        QCOMPARE(t->costumeColor(), QColor(160, 20, 40));
        QVERIFY(t->name().startsWith(QStringLiteral("Choir ")));
        // Drawn standing on its tread, at its own height.
        QCOMPARE(props.targetFigureHeight(t), 1.60f);
        const QVector3D aim = props.targetAimPoint(t, true);
        QVERIFY(qAbs(aim.z() - props.targetAimHeight(t, true) - t->z()) < 1e-4f);
    }

    // One of a set: no position readout each (a hand-placed person keeps one).
    QVERIFY(PerformerFill::isPerformer(&props, props.stageTarget(r.targets.first())));
    StageTarget *solo = props.addStageTarget();
    solo->setKind(StageTarget::Kind::Person);
    QVERIFY(!PerformerFill::isPerformer(&props, solo));

    // A second riser's set goes in the same layer, not a new one.
    StagePlatform *q = threeTier(&props, 6.0f, 1.0f);
    const PerformerFill::Result r2 = PerformerFill::fill(&props, { q->id() }, o);
    QCOMPARE(r2.layerId, r.layerId);
    QVERIFY(r2.groupId != r.groupId);
    int named = 0;
    foreach (const MonitorProperties::MonitorLayer &l, props.layers())
        named += (l.name == QStringLiteral("Performers"));
    QCOMPARE(named, 1);
}

void Performers_Test::fillingAgainReplaces()
{
    MonitorProperties props;
    StagePlatform *p = threeTier(&props);
    PerformerFill::Options o;
    const PerformerFill::Result r = PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(people(&props).size(), 14);

    // A soloist placed by hand on the same riser, outside the layer, stays.
    StageTarget *solo = props.addStageTarget();
    solo->setKind(StageTarget::Kind::Person);
    solo->setPosition(QVector3D(3.0f, 3.0f, 0.4f));

    const quint32 kept = r.targets.at(3);
    o.perRow = 4;
    const PerformerFill::Result r2 = PerformerFill::fill(&props, { p->id() }, o,
        [kept](quint32 id) { return id == kept; });
    QCOMPARE(r2.replaced, 13);
    QCOMPARE(r2.targets.size(), 12);
    QCOMPARE(people(&props).size(), 12 + 1 + 1);
    QVERIFY(props.stageTarget(kept) != nullptr);
    QVERIFY(props.stageTarget(solo->id()) != nullptr);
    QVERIFY2(props.hasGroup(r.groupId), "the old set still holds the kept person");

    // Without the kept one, the old set goes when it empties.
    const PerformerFill::Result r3 = PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(r3.replaced, 13);
    QCOMPARE(props.groups().size(), 1);         // the emptied sets went (an id may be reused)
    QVERIFY(props.hasGroup(r3.groupId));

    // Replace off: the new set joins the old.
    o.replace = false;
    PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(people(&props).size(), 12 + 12 + 1);
}

void Performers_Test::theLayerHidesAndLocksThem()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);
    Plot plot(&doc);

    const PerformerFill::Result r = plot.mon->fillWithPeople({ p->id() }, PerformerFill::Options());
    QCOMPARE(r.targets.size(), 14);
    foreach (quint32 id, r.targets)
    {
        TargetItem *ti = plot.gv->m_targetItems.value(id, nullptr);
        QVERIFY(ti != nullptr);
        QVERIFY(ti->isVisible());
        QVERIFY(ti->flags() & QGraphicsItem::ItemIsMovable);
    }

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    QVERIFY(v.layerShown(r.layerId));

    props->setLayerVisible(r.layerId, false);
    plot.gv->refreshItemLayerState();
    foreach (quint32 id, r.targets)
        QVERIFY2(!plot.gv->m_targetItems.value(id)->isVisible(), "a hidden performer is drawn on the plot");
    QVERIFY(!v.layerShown(r.layerId));
    // The rig view's fit leaves them out too: nothing at their crowns.
    QList<QVector3D> pts;
    v.collectPoints(pts);
    foreach (const QVector3D &q, pts)
        QVERIFY2(q.z() < 1.2f + 1.0f, "a hidden performer is still framed in the rig view");

    props->setLayerVisible(r.layerId, true);
    props->setLayerLocked(r.layerId, true);
    plot.gv->refreshItemLayerState();
    foreach (quint32 id, r.targets)
    {
        TargetItem *ti = plot.gv->m_targetItems.value(id);
        QVERIFY(ti->isVisible());
        QVERIFY2(!MonitorGraphicsView::itemDraggable(ti), "a locked layer's performer can be dragged");
    }
}

void Performers_Test::movingTheRiserMovesThem()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);
    Plot plot(&doc);
    const PerformerFill::Result r = plot.mon->fillWithPeople({ p->id() }, PerformerFill::Options());

    // Someone standing on the floor beside it does not ride it.
    StageTarget *beside = props->addStageTarget();
    beside->setKind(StageTarget::Kind::Person);
    beside->setPosition(QVector3D(1.0f, 1.0f, 0.0f));
    plot.gv->updateTargets();

    QHash<quint32, QVector3D> before;
    foreach (quint32 id, r.targets)
        before.insert(id, props->stageTarget(id)->position());

    PlatformItem *pi = plot.gv->m_platformItems.value(p->id());
    QVERIFY(pi != nullptr);
    plot.gv->scene()->clearSelection();
    pi->setPos(plot.gv->realPositionToPixels(4000.0, 2000.0));   // 2 m right, 1 m down
    plot.gv->slotPlatformMoved(pi);
    QVERIFY(qAbs(p->originX() - 4.0f) < 1e-3f && qAbs(p->originY() - 2.0f) < 1e-3f);

    foreach (quint32 id, r.targets)
    {
        StageTarget *t = props->stageTarget(id);
        const QVector3D was = before.value(id);
        QVERIFY2(qAbs(t->x() - (was.x() + 2.0f)) < 1e-3f && qAbs(t->y() - (was.y() + 1.0f)) < 1e-3f,
                 qPrintable(QString("%1 stayed behind").arg(t->name())));
        QCOMPARE(t->z(), was.z());          // still on its own tread
        // Drawn where it is now.
        TargetItem *ti = plot.gv->m_targetItems.value(id);
        const QPointF want = plot.gv->realPositionToPixels(t->x() * 1000.0, t->y() * 1000.0);
        QVERIFY(QLineF(ti->pos(), want).length() < 1.0);
    }
    QCOMPARE(beside->position(), QVector3D(1.0f, 1.0f, 0.0f));

    // One Undo: the riser and its people back.
    plot.gv->undoLastMove();
    QVERIFY(qAbs(p->originX() - 2.0f) < 1e-3f);
    foreach (quint32 id, r.targets)
        QVERIFY(qAbs(props->stageTarget(id)->x() - before.value(id).x()) < 1e-3f
                && qAbs(props->stageTarget(id)->y() - before.value(id).y()) < 1e-3f);

    // Turned with it too.
    plot.gv->scene()->clearSelection();
    plot.gv->m_platformItems.value(p->id())->setSelected(true);
    const QPointF c = p->centre();
    plot.gv->rotateSelectedItems(180.0);
    foreach (quint32 id, r.targets)
    {
        StageTarget *t = props->stageTarget(id);
        const QVector3D was = before.value(id);
        QVERIFY2(qAbs(t->x() - float(2.0 * c.x() - was.x())) < 1e-3f
                 && qAbs(t->y() - float(2.0 * c.y() - was.y())) < 1e-3f,
                 qPrintable(QString("%1 did not turn with the riser").arg(t->name())));
    }
}

void Performers_Test::theSetMovesAsOnePiece()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);
    Plot plot(&doc);
    const PerformerFill::Result r = plot.mon->fillWithPeople({ p->id() }, PerformerFill::Options());

    // Picking one picks the set.
    plot.gv->scene()->clearSelection();
    plot.gv->m_targetItems.value(r.targets.first())->setSelected(true);
    plot.gv->extendSelectionToGroups();
    int selected = 0;
    foreach (quint32 id, r.targets)
        selected += plot.gv->m_targetItems.value(id)->isSelected() ? 1 : 0;
    QCOMPARE(selected, r.targets.size());

    // Dragged together: each moves by the same, and keeps its tread.
    QHash<quint32, QVector3D> before;
    foreach (quint32 id, r.targets)
        before.insert(id, props->stageTarget(id)->position());
    const QPointF d = plot.gv->realPositionToPixels(0.5 * 1000.0, 0.0)
                      - plot.gv->realPositionToPixels(0.0, 0.0);
    foreach (quint32 id, r.targets)
    {
        TargetItem *ti = plot.gv->m_targetItems.value(id);
        ti->setPos(ti->pos() + d);
    }
    plot.gv->slotTargetMoved(plot.gv->m_targetItems.value(r.targets.first()));
    foreach (quint32 id, r.targets)
    {
        StageTarget *t = props->stageTarget(id);
        QVERIFY(qAbs(t->x() - (before.value(id).x() + 0.5f)) < 1e-3f);
        QVERIFY(qAbs(t->y() - before.value(id).y()) < 1e-3f);
        QCOMPARE(t->z(), props->platformHeightAt(t->x(), t->y()));
    }
}

void Performers_Test::saveAndLoadRoundTrip()
{
    MonitorProperties mp;
    StagePlatform *p = threeTier(&mp);
    PerformerFill::Options o;
    o.setName = QStringLiteral("Choir");
    o.height = PerformerFill::presetHeight(PerformerFill::Children);
    o.costume = QColor(20, 60, 200);
    const PerformerFill::Result r = PerformerFill::fill(&mp, { p->id() }, o);

    // One with nothing of this set: written as before (no Costume).
    StageTarget *plain = mp.addStageTarget();
    plain->setName(QStringLiteral("DSC"));

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    QCOMPARE(buf.count("Costume=\"#143cc8\""), r.targets.size());
    QCOMPARE(buf.count("Costume="), r.targets.size());

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));

    QVERIFY(mp2.hasLayer(r.layerId));
    QCOMPARE(mp2.layer(r.layerId).name, QStringLiteral("Performers"));
    QCOMPARE(mp2.group(r.groupId).name, QStringLiteral("Choir"));
    foreach (quint32 id, r.targets)
    {
        const StageTarget *a = mp.stageTarget(id);
        const StageTarget *b = mp2.stageTarget(id);
        QVERIFY(b != nullptr);
        QCOMPARE(b->kind(), StageTarget::Kind::Person);
        QCOMPARE(b->name(), a->name());
        QCOMPARE(b->personHeight(), 1.30f);
        QCOMPARE(b->costumeColor(), QColor(20, 60, 200));
        QCOMPARE(b->layerId(), r.layerId);
        QCOMPARE(b->groupId(), r.groupId);
        QVERIFY((b->position() - a->position()).length() < 1e-3f);
    }
    QVERIFY(!mp2.stageTarget(plain->id())->costumeColor().isValid());
}

void Performers_Test::aCostumeTakesTheLightInItsColour()
{
    /* A red robe and a white one side by side, one white wash on both: the
       red reads red, the white neutral. */
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *red = props->addStageTarget();
    red->setKind(StageTarget::Kind::Person);
    red->setPosition(QVector3D(2.5f, 3.0f, 0.0f));
    red->setCostumeColor(QColor(200, 30, 30));
    StageTarget *plain = props->addStageTarget();
    plain->setKind(StageTarget::Kind::Person);
    plain->setPosition(QVector3D(3.5f, 3.0f, 0.0f));

    Fixture *wash = makeWash(&doc, 0, QVector3D(3.0f, 8.0f, 4.0f));
    QVERIFY(wash != nullptr);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, &doc);
    aim->setStageTargetId(plain->id());
    QVERIFY(doc.addPalette(aim));
    Scene *scene = new Scene(&doc);
    scene->addFixture(wash->id());
    scene->addPalette(aim->id());
    QVERIFY(doc.addFunction(scene));
    QByteArray u(512, char(0));
    u[2] = char(255); u[3] = char(255); u[4] = char(255); u[5] = char(255);
    wash->setChannelValues(u);

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(800, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(0.0, 12.0);
    v.setAmbient(0.4);
    v.setBeams(true);
    v.setLiveValues(true);
    v.setActiveScene(scene->id());

    auto chest = [&](StageTarget *t) {
        const QImage img = v.grab().toImage();
        const double dpr = img.devicePixelRatio();
        const float tall = props->targetFigureHeight(t);
        const QVector3D at(t->x(), t->y() + 0.13f * tall / 1.75f, 0.72f * tall);
        const QPoint c = (v.w2s(at) * dpr).toPoint();
        int r = 0, g = 0, b = 0, n = 0;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            {
                const QColor px = img.pixelColor(c + QPoint(dx, dy));
                r += px.red(); g += px.green(); b += px.blue(); ++n;
            }
        return QColor(r / n, g / n, b / n);
    };
    const QColor robe = chest(red);
    const QColor body = chest(plain);
    const QString report = QString("red robe %1, neutral %2").arg(robe.name(), body.name());
    QVERIFY2(robe.red() > robe.green() + 50 && robe.red() > robe.blue() + 50,
             qPrintable("a red costume does not read red: " + report));
    QVERIFY2(qAbs(body.red() - body.blue()) < 40, qPrintable("the neutral body took a colour: " + report));
}

void Performers_Test::theDialogAndTheAddMenu()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);

    PerformersDialog dlg(props, { p->id() });
    QCOMPARE(dlg.count(), 14);
    QVERIFY(dlg.findChild<QLabel *>(QStringLiteral("performersCount"))->text().startsWith(QStringLiteral("14 ")));
    dlg.m_by->setCurrentIndex(1);
    dlg.m_perRow->setValue(7);
    QCOMPARE(dlg.count(), 21);
    QCOMPARE(dlg.options().perRow, 7);
    QVERIFY(dlg.m_name->text().contains(QStringLiteral("Choir riser")));

    Plot plot(&doc);
    QAction *person = nullptr;
    foreach (QAction *a, plot.mon->findChildren<QAction *>())
        if (a->objectName() == QStringLiteral("addPersonAction"))
            person = a;
    QVERIFY(person != nullptr);
    QMenu *menu = nullptr;
    foreach (QObject *o, person->associatedObjects())
        if ((menu = qobject_cast<QMenu *>(o)) != nullptr)
            break;
    QVERIFY(menu != nullptr);
    emit menu->aboutToShow();
    QCOMPARE(person->text(), QStringLiteral("Add Person"));

    plot.gv->m_platformItems.value(p->id())->setSelected(true);
    emit menu->aboutToShow();
    QVERIFY2(person->text().startsWith(QStringLiteral("Fill 1 Selected Riser")), qPrintable(person->text()));

    // Triggered with the riser selected: the dialog, and OK fills it.
    QTimer::singleShot(0, [&]() {
        PerformersDialog *d = plot.mon->findChild<PerformersDialog *>();
        QVERIFY(d != nullptr);
        d->accept();
    });
    person->trigger();
    QCOMPARE(people(props).size(), 14);
}

void Performers_Test::manyPeopleRenderCost()
{
    /* Five 3-tier risers of students -- 70 people -- under a few washes.
       The rig view's frame and the plot's are timed with nobody on the
       risers and with them all: the quickest of several, so a busy machine
       measures the drawing and not the wait for a core. */
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QList<quint32> risers;
    for (int k = 0; k < 5; ++k)
        risers << threeTier(props, 0.3f + 3.1f * k, 1.0f)->id();
    for (int k = 0; k < 4; ++k)
    {
        Fixture *f = makeWash(&doc, quint32(k * 6), QVector3D(1.5f + 4.0f * k, 8.0f, 5.0f));
        QVERIFY(f != nullptr);
        QByteArray u(512, char(0));
        u[k * 6 + 2] = char(255); u[k * 6 + 3] = char(255); u[k * 6 + 5] = char(200);
        f->setChannelValues(u);
    }

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(1000, 700);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 15.0);
    v.setBeams(true);
    v.setLiveValues(true);
    Plot plot(&doc);

    auto quickest = [](const std::function<void()> &draw) {
        draw();                             // warm
        double best = 1e9;
        for (int i = 0; i < 7; ++i)
        {
            // Processor time, every thread's: a loaded machine's wait for
            // a core is not the drawing's cost.
            const std::clock_t c0 = std::clock();
            draw();
            best = qMin(best, double(std::clock() - c0) * 1000.0 / CLOCKS_PER_SEC);
        }
        return best;
    };
    auto rigFrame = [&]() { v.grab(); };
    auto plotFrame = [&]() {
        QImage img(1000, 700, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        QPainter p(&img);
        plot.gv->scene()->render(&p);
    };
    const double rigEmpty = quickest(rigFrame);
    const double plotEmpty = quickest(plotFrame);

    PerformerFill::Options o;
    o.height = PerformerFill::presetHeight(PerformerFill::Students);
    o.costume = QColor(30, 30, 120);
    const PerformerFill::Result r = plot.mon->fillWithPeople(risers, o);
    QCOMPARE(r.targets.size(), 70);
    v.reload();

    const double rigFull = quickest(rigFrame);
    const double plotFull = quickest(plotFrame);
    qInfo("render cost (CPU), %d people: rig view %.1f -> %.1f ms/frame, plot %.1f -> %.1f ms/frame",
          int(r.targets.size()), rigEmpty, rigFull, plotEmpty, plotFull);
    /* Generous: a debug build on a loaded gate machine. What it catches is
       a figure costing milliseconds, not noise. */
    QVERIFY2(rigFull < rigEmpty * 1.6 + 80.0, "70 people make the rig view crawl");
    QVERIFY2(plotFull < plotEmpty + 30.0, "70 people make the plot crawl");
}

QTEST_MAIN(Performers_Test)
