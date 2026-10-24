/*
  Q Light Controller Plus
  refinesession_test.cpp

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

#include <QSignalSpy>
#include <QtTest>

#include "refinesession_test.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "controllerscheme.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "refinesession.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scenevalue.h"
#include "paletteops.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const int kChannels = 7;   // pan, pan fine, tilt, tilt fine, dimmer, zoom, focus

/* A 540/270 spot with 16-bit pan and tilt, a dimmer, a 10-40 degree zoom and
   a focus channel, placed at (x, 2 m), hung at 5 m. */
Fixture *makeSpot(Doc *doc, const QString &name, float x)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Refine Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("7ch");
    struct Ch { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; QLCChannel::Preset preset; };
    const Ch chans[kChannels] = {
        { "Pan", QLCChannel::Pan, QLCChannel::MSB, QLCChannel::Custom },
        { "Pan fine", QLCChannel::Pan, QLCChannel::LSB, QLCChannel::Custom },
        { "Tilt", QLCChannel::Tilt, QLCChannel::MSB, QLCChannel::Custom },
        { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB, QLCChannel::Custom },
        { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Custom },
        { "Zoom", QLCChannel::Beam, QLCChannel::MSB, QLCChannel::BeamZoomSmallBig },
        { "Focus", QLCChannel::Beam, QLCChannel::MSB, QLCChannel::BeamFocusNearFar },
    };
    QLCFixtureHead head;
    for (int k = 0; k < kChannels; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans[k].name);
        ch->setGroup(chans[k].group);
        ch->setControlByte(chans[k].byte);
        if (chans[k].preset != QLCChannel::Custom)
            ch->setPreset(chans[k].preset);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    ph.setLensDegreesMin(10.0);
    ph.setLensDegreesMax(40.0);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * kChannels));
    doc->addFixture(fxi);
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(x * 1000.0f, 2000.0f, 5.0f));
    return fxi;
}

Look lookOf(const QString &key, const QList<quint32> &palettes)
{
    Look l;
    l.key = key;
    l.palettes = palettes;
    return l;
}

/* pan, tilt degrees @p fid is sent at in @p writes. */
bool sentDegrees(Doc *doc, const QHash<QPair<int, int>, uchar> &writes, quint32 fid,
                 float &pan, float &tilt)
{
    Fixture *fx = doc->fixture(fid);
    const int a = int(fx->address());
    if (!writes.contains(qMakePair(0, a)))
        return false;
    const quint16 p = quint16((writes.value(qMakePair(0, a)) << 8) | writes.value(qMakePair(0, a + 1)));
    const quint16 t = quint16((writes.value(qMakePair(0, a + 2)) << 8) | writes.value(qMakePair(0, a + 3)));
    pan = RefineSession::dmxToDegrees(fx, QLCChannel::Pan, p);
    tilt = RefineSession::dmxToDegrees(fx, QLCChannel::Tilt, t);
    return true;
}
}

void RefineSession_Test::init()
{
    m_doc = new Doc(this);
    m_doc->programmer()->refineSession()->setFlashEnabled(false);
    m_spots.clear();
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("AT spots");
    g->setSize(QSize(4, 1));
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fx = makeSpot(m_doc, QString("AT %1").arg(i + 1), 2.0f + 2.0f * i);
        m_spots << fx->id();
        g->assignFixture(fx->id(), QLCPoint(i, 0));
    }
    m_doc->addFixtureGroup(g);
    m_group = g->id();

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    const quint32 list = st->addCueList(prod, "Main");
    m_cue = st->addCue(list, -1, "Ballad");
    Target t;
    t.groups << m_group;
    m_layer = st->addLayer(m_cue, t);
    // A named palette, as other cues would share it.
    QLCPalette *centre = new QLCPalette(QLCPalette::PanTilt);
    centre->setName("Centre");
    centre->setValues({ 270, 135 });
    m_doc->addPalette(centre);
    m_position = centre->id();
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyPosition, { m_position })));
}

void RefineSession_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void RefineSession_Test::refusesWhatItCannotRefine()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QString why;
    QVERIFY(!r->start(9999, 0xFFFFFFFF, &why));
    QVERIFY(!why.isEmpty());

    ShowTreeController *st = m_doc->showTree();
    const quint32 empty = st->addCue(st->listOfCue(m_cue)->id, 0, "Empty");
    why.clear();
    QVERIFY(!r->start(empty, 0xFFFFFFFF, &why));
    QVERIFY(why.contains("layer"));

    QVERIFY(r->start(m_cue));
    QVERIFY(r->isActive());
    QCOMPARE(st->previewedCue(), m_cue);
    // Into Operate it carries on (Q112a).
    m_doc->setMode(Doc::Operate);
    QVERIFY(r->isActive());
    m_doc->setMode(Doc::Design);
    r->stop();
}

void RefineSession_Test::degreesFromDmx()
{
    Fixture *fx = m_doc->fixture(m_spots.first());
    for (float deg : { 0.0f, 12.5f, 270.0f, 400.25f, 540.0f })
    {
        quint16 v = 0;
        for (const SceneValue &sv : fx->positionToValues(QLCChannel::Pan, deg))
            v = sv.channel == 0 ? quint16((v & 0xFF) | (sv.value << 8)) : quint16((v & 0xFF00) | sv.value);
        QVERIFY(qAbs(RefineSession::dmxToDegrees(fx, QLCChannel::Pan, v) - deg) < 0.02f);
    }
}

void RefineSession_Test::scopesAndSteps()
{
    ShowTreeController *st = m_doc->showTree();
    // A second layer, on two loose fixtures besides the group: two pieces.
    Target t2;
    t2.groups << m_group;
    Fixture *x = makeSpot(m_doc, "X", 12.0f);
    t2.fixtures << x->id();
    const quint32 layer2 = st->addLayer(m_cue, t2, "Wide");
    QVERIFY(layer2 != invalidId());

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QCOMPARE(r->layers(), QList<quint32>({ m_layer, layer2 }));
    QCOMPARE(r->currentLayer(), m_layer);
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    QCOMPARE(r->scopeFixtures(), m_spots);

    // One group only: Y goes straight to Fixture.
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->scopeFixtures(), QList<quint32>({ m_spots.at(0) }));
    r->nextItem();
    QCOMPARE(r->currentFixture(), m_spots.at(1));
    r->previousItem();
    r->previousItem();
    QCOMPARE(r->currentFixture(), m_spots.at(3));   // wraps
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::LayerScope);

    // RT: the next layer, as a whole.
    r->setTrigger(1, 1.0f);
    QCOMPARE(r->currentLayer(), layer2);
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    QCOMPARE(r->groups().size(), 2);
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::GroupScope);
    QCOMPARE(r->scopeFixtures(), m_spots);
    r->nextItem();
    QCOMPARE(r->scopeFixtures(), QList<quint32>({ x->id() }));
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    r->setTrigger(1, 0.0f);
    r->setTrigger(0, 1.0f);
    QCOMPARE(r->currentLayer(), m_layer);
    // A bumper at Layer scope picks the first fixture.
    r->apply(ControllerScheme::NextItem);
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    r->stop();
}

void RefineSession_Test::layerScopeEditsTheLayerLook()
{
    ShowTreeController *st = m_doc->showTree();
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->cuePosition(m_spots.at(2), pan, tilt));
    QVERIFY(qAbs(pan - 270.0f) < 0.05f);
    QVERIFY(qAbs(tilt - 135.0f) < 0.05f);

    QVERIFY(r->nudgePosition(3.0f, -2.0f));
    QVERIFY(r->hasPending());
    // Seen on the rig at once, on every fixture of the layer, in whole degrees.
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    for (quint32 fid : m_spots)
    {
        QVERIFY(sentDegrees(m_doc, w, fid, pan, tilt));
        QVERIFY(qAbs(pan - 273.0f) < 0.05f);
        QVERIFY(qAbs(tilt - 133.0f) < 0.05f);
    }
    // Not in the cue yet.
    QCOMPARE(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes, QList<quint32>({ m_position }));

    const QString undoBefore = st->undoText();
    QVERIFY(r->keep());
    QVERIFY(!r->hasPending());
    QVERIFY(r->pendingWrites().isEmpty());
    const Look *pos = st->cue(m_cue)->layers.first().look(KeyPosition);
    QVERIFY(pos != nullptr);
    QCOMPARE(pos->palettes.size(), 1);
    QVERIFY(pos->palettes.first() != m_position);
    QCOMPARE(m_doc->palette(pos->palettes.first())->values(), QVariantList({ 273, 133 }));
    QVERIFY(st->isInlinePalette(pos->palettes.first()));
    // The named one is untouched (other cues use it).
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));
    QVERIFY(st->undoText() != undoBefore);

    // Back: one step puts it back.
    QVERIFY(r->undo());
    QCOMPARE(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes, QList<quint32>({ m_position }));

    // B drops what is pending.
    r->nudgePosition(1.0f, 0.0f);
    r->revert();
    QVERIFY(!r->hasPending());
    QVERIFY(r->pendingWrites().isEmpty());
    r->stop();
}

void RefineSession_Test::fixtureScopeMakesAnException()
{
    ShowTreeController *st = m_doc->showTree();
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QCOMPARE(r->currentFixture(), m_spots.at(1));
    QVERIFY(r->nudgePosition(0.0f, 5.0f));
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.at(1), pan, tilt));
    QVERIFY(qAbs(tilt - 140.0f) < 0.05f);
    QVERIFY(!r->pendingPosition(m_spots.at(0), pan, tilt));
    // Only that fixture moves.
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    QVERIFY(sentDegrees(m_doc, w, m_spots.at(1), pan, tilt));
    QVERIFY(!w.contains(qMakePair(0, int(m_doc->fixture(m_spots.at(0))->address()))));

    QVERIFY(r->keep());
    const Cue *c = st->cue(m_cue);
    QCOMPARE(c->layers.size(), 2);
    const Layer &child = c->layers.at(1);
    QCOMPARE(child.parent, m_layer);
    QCOMPARE(child.target.fixtures, QList<quint32>({ m_spots.at(1) }));
    QCOMPARE(m_doc->palette(child.look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 270, 140 }));
    // The layer's own look is as it was.
    QCOMPARE(c->layers.first().look(KeyPosition)->palettes, QList<quint32>({ m_position }));
    // What the cue now has for that fixture is the exception.
    QVERIFY(r->cuePosition(m_spots.at(1), pan, tilt));
    QVERIFY(qAbs(tilt - 140.0f) < 0.05f);

    // Again on the same fixture: the same child, not another.
    r->nudgePosition(1.0f, 0.0f);
    QVERIFY(r->keep());
    QCOMPARE(st->cue(m_cue)->layers.size(), 2);
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.at(1).look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 271, 140 }));

    // A layer nudge leaves the excepted fixture where its exception has it.
    r->setScope(RefineSession::LayerScope);
    r->nudgePosition(10.0f, 0.0f);
    const QHash<QPair<int, int>, uchar> w2 = r->pendingWrites();
    QVERIFY(sentDegrees(m_doc, w2, m_spots.at(0), pan, tilt));
    QVERIFY(qAbs(pan - 280.0f) < 0.05f);
    QVERIFY(!w2.contains(qMakePair(0, int(m_doc->fixture(m_spots.at(1))->address()))));
    r->revert();

    // X: the exception off; the child goes with its last look. (The
    // fixture picked is kept across a change of scope.)
    r->setScope(RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), m_spots.at(1));
    QVERIFY(r->clearException());
    QCOMPARE(st->cue(m_cue)->layers.size(), 1);
    QVERIFY(!r->clearException());   // nothing left to clear
    r->stop();
}

void RefineSession_Test::zoomAndFocus()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 zoom = st->createInlinePalette(QLCPalette::Zoom, { 20.0 }, "Mid");
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyZoom, { zoom })));
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));

    r->apply(ControllerScheme::ZoomWider);
    r->apply(ControllerScheme::ZoomWider);
    float deg = 0.0f;
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 22.0f);
    // Held to the lens range.
    QVERIFY(r->nudgeZoom(100.0f));
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 40.0f);
    QVERIFY(r->nudgeZoom(-15.0f));
    // Focus from the middle (no Beam look yet).
    r->apply(ControllerScheme::FocusFarther);
    int focus = 0;
    QVERIFY(r->pendingFocus(m_spots.first(), focus));
    QCOMPARE(focus, 133);
    QCOMPARE(int(r->pendingWrites().value(qMakePair(0, int(m_doc->fixture(m_spots.first())->address()) + 6))), 133);
    QVERIFY(r->keep());
    const Layer &l = st->cue(m_cue)->layers.first();
    QCOMPARE(m_doc->palette(l.look(KeyZoom)->palettes.first())->value().toFloat(), 25.0f);
    QCOMPARE(m_doc->palette(l.look(KeyBeam)->palettes.first())->values().first().toInt(), 133);

    // The colour page steps the show's colour palettes; A keeps the one picked.
    QLCPalette *red = new QLCPalette(QLCPalette::Color);
    red->setName("Red");
    red->setValue(QColor(Qt::red).name());
    m_doc->addPalette(red);
    QLCPalette *blue = new QLCPalette(QLCPalette::Color);
    blue->setName("Blue");
    blue->setValue(QColor(Qt::blue).name());
    m_doc->addPalette(blue);
    r->apply(ControllerScheme::Alternate);
    QCOMPARE(r->page(), RefineSession::ColourPage);
    r->apply(ControllerScheme::FocusFarther);   // D-pad right: the first, by name
    QVERIFY(r->keep());
    QCOMPARE(st->cue(m_cue)->layers.first().look(KeyColour)->palettes, QList<quint32>({ blue->id() }));
    r->stop();
}

void RefineSession_Test::anAimedLayerAsksForAFixture()
{
    ShowTreeController *st = m_doc->showTree();
    StageTarget *dsc = m_doc->monitorProperties()->addStageTarget();
    dsc->setName("DSC");
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim);
    aim->setName("Aim DSC");
    aim->setStageTargetId(dsc->id());
    m_doc->addPalette(aim);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyPosition, { aim->id() })));

    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(m_cue));
    QVERIFY(!r->nudgePosition(1.0f, 0.0f));
    QVERIFY(said.last().at(0).toString().contains("aimed at a target"));
    // One fixture can still be moved: its exception is a fixed position.
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    const Layer &child = st->cue(m_cue)->layers.at(1);
    QCOMPARE(m_doc->palette(child.look(KeyPosition)->palettes.first())->type(), QLCPalette::PanTilt);
    r->stop();
}

void RefineSession_Test::theOperatorIsToldWhereTheyAre()
{
    // The layer at 80%: the rest of it dims below that.
    QLCPalette *bright = new QLCPalette(QLCPalette::Dimmer);
    bright->setName("80");
    bright->setValue(204);
    m_doc->addPalette(bright);
    QVERIFY(m_doc->showTree()->setLayerLook(m_cue, m_layer, lookOf(KeyDimmer, { bright->id() })));
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    QVERIFY(r->start(m_cue));
    QCOMPARE(said.last().at(0).toString(), QString("Refine, cue 1. AT spots, layer."));
    r->cycleScope();
    QCOMPARE(said.last().at(0).toString(), QString("AT spots, fixture 1 of 4."));
    r->nextItem();
    r->nextItem();
    QCOMPARE(said.last().at(0).toString(), QString("AT spots, fixture 3 of 4."));
    r->apply(ControllerScheme::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("Cue 1, AT spots, fixture 3 of 4."));

    // Only the fixture picked is at the cue's level: the rest of the layer dim.
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    const int dim = qRound(RefineSession::dimLevel() * 255.0f);
    QCOMPARE(int(w.value(qMakePair(0, int(m_doc->fixture(m_spots.at(0))->address()) + 4))), dim);
    QVERIFY(!w.contains(qMakePair(0, int(m_doc->fixture(m_spots.at(2))->address()) + 4)));
    // At Layer scope nothing is dimmed.
    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->pendingWrites().isEmpty());

    // The flash blinks what was picked.
    r->setFlashEnabled(true);
    r->flashSelection();
    QVERIFY(r->isFlashing());
    QTRY_VERIFY_WITH_TIMEOUT(!r->isFlashing(), 2000);
    r->apply(ControllerScheme::Keep);
    r->stop();
}

void RefineSession_Test::oneToolOwnsTheController()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    FocusSession *f = m_doc->programmer()->focusSession();
    f->setFlashEnabled(false);
    StageTarget *dsc = m_doc->monitorProperties()->addStageTarget();
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = dsc->id();
    QVERIFY(f->start(o));
    QVERIFY(r->start(m_cue));
    QVERIFY(!f->isActive());
    QVERIFY(f->start(o));
    QVERIFY(!r->isActive());
    f->stop();

    // Exit only asks.
    QVERIFY(r->start(m_cue));
    QSignalSpy exit(r, &RefineSession::exitRequested);
    r->apply(ControllerScheme::verbFor(GamepadRole::Start));
    QCOMPARE(exit.count(), 1);
    QVERIFY(r->isActive());
    // The sticks move the position through the tick.
    r->setStick(RefineSession::Coarse, 1.0f, 0.5f);
    r->tick(0.5);
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.first(), pan, tilt));
    QVERIFY(pan > 272.0f);
    r->stop();
}

void RefineSession_Test::operateHoldsWhatIsKept()
{
    ShowTreeController *st = m_doc->showTree();
    m_doc->setMode(Doc::Operate);
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QString why;
    QVERIFY2(r->start(m_cue, invalidId(), &why), qPrintable(why));
    // The show runs on: Refine does not preview the cue in Operate.
    QVERIFY(st->previewedCue() != m_cue);
    QVERIFY(r->nudgePosition(4.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(said.last().at(0).toString(), QString("Kept. It plays from the next GO."));
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 274, 135 }));
    // Held on the rig until the next GO plays it.
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(!r->hasPending());
    QVERIFY(sentDegrees(m_doc, r->pendingWrites(), m_spots.at(2), pan, tilt));
    QVERIFY(qAbs(pan - 274.0f) < 0.05f);
    // Back in Design the cue is previewed with it, and nothing is held.
    m_doc->setMode(Doc::Design);
    QVERIFY(r->isActive());
    QCOMPARE(st->previewedCue(), m_cue);
    QVERIFY(r->pendingWrites().isEmpty());
    r->stop();
}

void RefineSession_Test::dimsOnlyWhatIsBrighter()
{
    ShowTreeController *st = m_doc->showTree();
    QLCPalette *bright = new QLCPalette(QLCPalette::Dimmer);
    bright->setName("80");
    bright->setValue(204);
    m_doc->addPalette(bright);
    QLCPalette *low = new QLCPalette(QLCPalette::Dimmer);
    low->setName("5");
    low->setValue(13);
    m_doc->addPalette(low);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyDimmer, { bright->id() })));
    // Spot 1 has its own, low level.
    Target one;
    one.fixtures << m_spots.at(0);
    const quint32 child = st->addChildLayer(m_cue, m_layer, one);
    QVERIFY(child != invalidId());
    QVERIFY(st->setLayerLook(m_cue, child, lookOf(KeyDimmer, { low->id() })));

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->nextItem();
    QCOMPARE(r->currentFixture(), m_spots.at(2));
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    const int dim = qRound(RefineSession::dimLevel() * 255.0f);
    auto dimmer = [this](int i) {
        return qMakePair(0, int(m_doc->fixture(m_spots.at(i))->address()) + 4);
    };
    // Brighter than the dim level: down to it.
    QCOMPARE(int(w.value(dimmer(1))), dim);
    QCOMPARE(int(w.value(dimmer(3))), dim);
    // Below it already: never raised.
    QVERIFY(!w.contains(dimmer(0)));
    // The one picked: as the cue has it.
    QVERIFY(!w.contains(dimmer(2)));
    r->stop();
}

void RefineSession_Test::groupScopeKeepsTheFan()
{
    ShowTreeController *st = m_doc->showTree();
    // Two groups on the layer: AT left (spots 1, 2), AT right (spots 3, 4).
    FixtureGroup *left = new FixtureGroup(m_doc);
    left->setName("AT left");
    left->setSize(QSize(2, 1));
    left->assignFixture(m_spots.at(0), QLCPoint(0, 0));
    left->assignFixture(m_spots.at(1), QLCPoint(1, 0));
    m_doc->addFixtureGroup(left);
    FixtureGroup *right = new FixtureGroup(m_doc);
    right->setName("AT right");
    right->setSize(QSize(2, 1));
    right->assignFixture(m_spots.at(2), QLCPoint(0, 0));
    right->assignFixture(m_spots.at(3), QLCPoint(1, 0));
    m_doc->addFixtureGroup(right);
    Target both;
    both.groups << left->id() << right->id();
    QVERIFY(st->setLayerTarget(m_cue, m_layer, both));
    // A fan inside AT left: spot 1 already points elsewhere.
    Target one;
    one.fixtures << m_spots.at(0);
    const quint32 own = st->addChildLayer(m_cue, m_layer, one);
    const quint32 pos = st->createInlinePalette(QLCPalette::PanTilt, { 250, 135 }, "Own");
    QVERIFY(st->setLayerLook(m_cue, own, lookOf(KeyPosition, { pos })));

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::GroupScope);
    QCOMPARE(r->scopeFixtures(), QList<quint32>({ m_spots.at(0), m_spots.at(1) }));
    QVERIFY(r->nudgePosition(5.0f, 0.0f));
    // Each from its own position: the fan is kept.
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.at(0), pan, tilt));
    QCOMPARE(pan, 255.0f);
    QVERIFY(r->pendingPosition(m_spots.at(1), pan, tilt));
    QCOMPARE(pan, 275.0f);
    QVERIFY(!r->pendingPosition(m_spots.at(2), pan, tilt));

    QVERIFY(r->keep());
    auto positionOf = [&](quint32 fid) {
        Target t;
        t.fixtures << fid;
        for (const Layer &l : st->cue(m_cue)->layers)
            if (l.parent == m_layer && l.target == t && l.look(KeyPosition) != nullptr)
                return m_doc->palette(l.look(KeyPosition)->palettes.first())->values().first().toInt();
        return -1;
    };
    QCOMPARE(positionOf(m_spots.at(0)), 255);
    QCOMPARE(positionOf(m_spots.at(1)), 275);
    QCOMPARE(positionOf(m_spots.at(2)), -1);
    // No group-wide position was made.
    Target leftT;
    leftT.groups << left->id();
    for (const Layer &l : st->cue(m_cue)->layers)
        QVERIFY(!(l.target == leftT && l.look(KeyPosition) != nullptr));
    // The layer's own look is as it was.
    QCOMPARE(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes, QList<quint32>({ m_position }));

    // X at group scope: the group's position exceptions go.
    QVERIFY(r->clearException());
    QCOMPARE(positionOf(m_spots.at(0)), -1);
    QCOMPARE(positionOf(m_spots.at(1)), -1);
    r->stop();
}

void RefineSession_Test::aBuildSceneIsRefinedTheSameWay()
{
    // A Build scene: the AT spots group, the Centre position palette.
    Scene *s = new Scene(m_doc);
    s->setName("Ballad look");
    s->addFixtureGroup(m_group);
    s->addPalette(m_position);
    QVERIFY(m_doc->addFunction(s));

    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QString why;
    QVERIFY2(r->startScene({ s->id() }, s->id(), &why), qPrintable(why));
    QVERIFY(r->isSceneMode());
    QCOMPARE(said.last().at(0).toString(), QString("Refine, scene Ballad look. Ballad look, layer."));
    QCOMPARE(r->layers(), QList<quint32>({ s->id() }));
    QCOMPARE(r->layerFixtures(), m_spots);
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->cuePosition(m_spots.at(1), pan, tilt));
    QCOMPARE(pan, 270.0f);

    // Fixture scope: the exception -- out of the palette, the scene's own values.
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgePosition(3.0f, 0.0f));
    QVERIFY(r->keep());
    QVERIFY(s->excludedFixtures(m_position).contains(m_spots.at(1)));
    QVERIFY(!s->excludedFixtures(m_position).contains(m_spots.at(0)));
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));
    QVERIFY(r->cuePosition(m_spots.at(1), pan, tilt));
    QVERIFY(qAbs(pan - 273.0f) < 0.05f);
    QVERIFY(qAbs(tilt - 135.0f) < 0.05f);
    QVERIFY(m_doc->paletteOps()->canUndo());

    // X: back in the palette, its own values gone.
    QVERIFY(r->clearException());
    QVERIFY(!s->excludedFixtures(m_position).contains(m_spots.at(1)));
    QVERIFY(r->cuePosition(m_spots.at(1), pan, tilt));
    QCOMPARE(pan, 270.0f);
    bool raw = false;
    for (const SceneValue &sv : s->values())
        raw = raw || sv.fxi == m_spots.at(1);
    QVERIFY(!raw);

    // Layer scope: the scene's palette moves (as the Look Editor does), one step.
    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(2.0f, -1.0f));
    QVERIFY(r->keep());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 272, 134 }));
    QVERIFY(r->undo());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));

    // Zoom, with no zoom palette: the scene's own values on all of it.
    QVERIFY(r->nudgeZoom(-5.0f));
    QVERIFY(r->keep());
    int zoomed = 0;
    for (const SceneValue &sv : s->values())
        if (sv.channel == 5)
            ++zoomed;
    QCOMPARE(zoomed, m_spots.size());
    r->stop();
    QVERIFY(!r->isSceneMode());
}

QTEST_GUILESS_MAIN(RefineSession_Test)
