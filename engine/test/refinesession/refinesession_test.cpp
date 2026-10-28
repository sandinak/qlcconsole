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
#include <QtCore>

#define protected public                // Universe::processFaders()
#include "universe.h"
#undef protected
#define private public                  // MasterTimer::timerTick()
#include "mastertimer.h"
#undef private
#include "refinesession_test.h"
#include "showcontrol.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "controllerscheme.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlccapability.h"
#include "refinesession.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scenevalue.h"
#include "paletteops.h"
#include "inputoutputmap.h"
#include "iopluginstub.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const int kChannels = 7;   // pan, pan fine, tilt, tilt fine, dimmer, zoom, focus

/* A 540/270 spot with 16-bit pan and tilt, a dimmer, a 10-40 degree zoom and
   a focus channel, placed at (x, 2 m), hung at 5 m. */
Fixture *makeSpot(Doc *doc, const QString &name, float x, int panMax = 540)
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
    ph.setFocusPanMax(panMax);
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

/* A spot with a gobo wheel (Open, Dots, Star) and a dimmer, at @p address. */
Fixture *makeGoboSpot(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Refine Gobo Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer");
    dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim);
    mode->insertChannel(dim, 0);
    QLCChannel *gobo = new QLCChannel();
    gobo->setName("Gobo");
    gobo->setGroup(QLCChannel::Gobo);
    gobo->addCapability(new QLCCapability(0, 9, "Open"));
    gobo->addCapability(new QLCCapability(10, 19, "Dots"));
    gobo->addCapability(new QLCCapability(20, 29, "Star"));
    def->addChannel(gobo);
    mode->insertChannel(gobo, 1);
    QLCFixtureHead head;
    head.addChannel(0);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(address);
    doc->addFixture(fxi);
    return fxi;
}

Look lookOf(const QString &key, const QList<quint32> &palettes)
{
    Look l;
    l.key = key;
    l.palettes = palettes;
    return l;
}

/* A light whose focus is its first channel, then a dimmer, at @p address. */
Fixture *makeFocusFirst(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Refine Focus First");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    QLCChannel *focus = new QLCChannel();
    focus->setName("Focus");
    focus->setGroup(QLCChannel::Beam);
    focus->setPreset(QLCChannel::BeamFocusNearFar);
    def->addChannel(focus);
    mode->insertChannel(focus, 0);
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer");
    dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim);
    mode->insertChannel(dim, 1);
    QLCFixtureHead head;
    head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(address);
    doc->addFixture(fxi);
    return fxi;
}

quint32 addDimmerPalette(Doc *doc, const QString &name, int value)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer);
    p->setName(name);
    p->setValue(value);
    doc->addPalette(p);
    return p->id();
}

quint32 addColourPalette(Doc *doc, const QString &name, const QColor &colour)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color);
    p->setName(name);
    p->setValue(colour.name());
    doc->addPalette(p);
    return p->id();
}

/* A PanTilt palette fanned linearly across the rig. */
QLCPalette *addFannedPosition(Doc *doc, int pan, int tilt, QLCPalette::FanningLayout layout,
                              int amount, int fanBy)
{
    QLCPalette *p = new QLCPalette(QLCPalette::PanTilt);
    p->setName("Fan");
    p->setValues({ pan, tilt });
    p->setFanningType(QLCPalette::Linear);
    p->setFanningLayout(layout);
    p->setFanningAmount(amount);
    p->setFanningValue(fanBy);
    doc->addPalette(p);
    return p;
}

/* Universe 0, @p fid's channel @p ch. */
QPair<int, int> chAt(Doc *doc, quint32 fid, int ch)
{
    return qMakePair(0, int(doc->fixture(fid)->address()) + ch);
}

/* @p values as writes, (universe, address) -> value. */
QHash<QPair<int, int>, uchar> asWrites(Doc *doc, const QList<SceneValue> &values)
{
    QHash<QPair<int, int>, uchar> w;
    for (const SceneValue &sv : values)
        if (Fixture *fx = doc->fixture(sv.fxi))
            w.insert(qMakePair(int(fx->universe()), int(fx->address() + sv.channel)), sv.value);
    return w;
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
    const quint32 empty = st->addCue(st->listOfCue(m_cue)->id, -1, "Empty");
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

    // D-pad down twice: out, wider (up is in).
    r->apply(ControllerScheme::verbFor(GamepadRole::DpadDown));
    r->apply(ControllerScheme::verbFor(GamepadRole::DpadDown));
    float deg = 0.0f;
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 22.0f);
    // Held to the lens range.
    QVERIFY(r->nudgeZoom(100.0f));
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 40.0f);
    QVERIFY(r->nudgeZoom(-15.0f));
    // Focus from the middle (no Beam look yet): right tightens, value up.
    r->apply(ControllerScheme::verbFor(GamepadRole::DpadRight));
    int focus = 0;
    QVERIFY(r->pendingFocus(m_spots.first(), focus));
    QCOMPARE(focus, 133);
    QCOMPARE(int(r->pendingWrites().value(qMakePair(0, int(m_doc->fixture(m_spots.first())->address()) + 6))), 133);
    QVERIFY(r->keep());
    const Layer &l = st->cue(m_cue)->layers.first();
    QCOMPARE(m_doc->palette(l.look(KeyZoom)->palettes.first())->value().toFloat(), 25.0f);
    QCOMPARE(m_doc->palette(l.look(KeyBeam)->palettes.first())->values().first().toInt(), 133);

    // Held, the D-pad repeats: once on the press, then after 0.4 s every 80 ms.
    const ControllerScheme::Verb in = ControllerScheme::verbFor(GamepadRole::DpadUp);
    r->press(in, true);
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 24.0f);
    r->tick(0.3);
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 24.0f);
    r->tick(0.1 + ControllerScheme::HoldRepeat::interval() * 2);
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 21.0f);                   // three more steps in
    r->press(in, false);
    r->tick(1.0);
    QVERIFY(r->pendingZoom(m_spots.first(), deg));
    QCOMPARE(deg, 21.0f);                   // let go: no more
    r->revert();

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
    // Paging palettes does not repeat: held, D-pad right steps once.
    QVERIFY(!r->repeats(ControllerScheme::FocusFarther));
    r->press(ControllerScheme::FocusFarther, true);   // D-pad right: the first, by name
    r->tick(2.0);
    r->press(ControllerScheme::FocusFarther, false);
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
    QCOMPARE(said.last().at(0).toString(), QString("Focusing cue 1. AT spots, layer."));
    r->cycleScope();
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 1, AT 1."));
    r->nextItem();
    r->nextItem();
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 3, AT 3."));
    // Say the light: its name first.
    r->apply(ControllerScheme::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("AT 3. Cue 1, AT spots, fixture 3."));
    // Voice detail: Verbose says the layer and how many; Terse, the name.
    m_doc->programmer()->setVoiceDetail(ControllerScheme::VerboseVoice);
    r->nextItem();
    QCOMPARE(said.last().at(0).toString(), QString("AT spots, fixture 4 of 4, AT 4."));
    r->apply(ControllerScheme::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("AT 4. Cue 1, AT spots, fixture 4 of 4."));
    m_doc->programmer()->setVoiceDetail(ControllerScheme::TerseVoice);
    r->previousItem();
    QCOMPARE(said.last().at(0).toString(), QString("F3 AT 3."));
    r->apply(ControllerScheme::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("AT 3."));
    m_doc->programmer()->setVoiceDetail(ControllerScheme::StandardVoice);

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
    r->setFlashEnabled(true);
    QSignalSpy said(r, &RefineSession::announce);
    // Q130: only the cue running. Nothing is: refused.
    quint32 running = invalidId();
    r->setRunningCueProbe([&running](quint32) { return running; });
    QString why;
    QVERIFY(!r->start(m_cue, invalidId(), &why));
    QVERIFY2(why.startsWith("In Operate only the cue running can be focused"), qPrintable(why));
    running = m_cue;
    QVERIFY2(r->start(m_cue, invalidId(), &why), qPrintable(why));
    // The show runs on: Refine does not preview the cue in Operate...
    QVERIFY(st->previewedCue() != m_cue);
    // ...and is quiet: no blink, no dim, a fixture picked or not.
    QVERIFY(!r->isFlashing());
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(!r->isFlashing());
    const int dimmer2 = int(m_doc->fixture(m_spots.at(2))->address()) + 4;
    QVERIFY(!r->pendingWrites().contains(qMakePair(0, dimmer2)));
    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(4.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(said.last().at(0).toString(), QString("AT spots kept in cue 1. It plays next time it runs."));
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 274, 135 }));
    // Held on the rig until the next GO plays it.
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(!r->hasPending());
    QVERIFY(sentDegrees(m_doc, r->pendingWrites(), m_spots.at(2), pan, tilt));
    QVERIFY(qAbs(pan - 274.0f) < 0.05f);
    // GO: the list moves on -- the hold is let go, and Refine sends nothing.
    running = invalidId() - 1;
    r->tick(0.02);
    QVERIFY(r->isBlind());
    QVERIFY(r->pendingWrites().isEmpty());
    QVERIFY(said.last().at(0).toString().startsWith("Cue 1 has moved on"));
    QVERIFY(r->nudgePosition(2.0f, 0.0f));          // blind: pending, not sent
    QVERIFY(r->pendingWrites().isEmpty());
    r->revert();
    running = m_cue;                                // back on it
    r->tick(0.02);
    QVERIFY(!r->isBlind());
    // Back in Design the cue is previewed with it, and nothing is held.
    m_doc->setMode(Doc::Design);
    QVERIFY(r->isActive());
    QCOMPARE(st->previewedCue(), m_cue);
    QVERIFY(r->pendingWrites().isEmpty());
    r->stop();
    r->setRunningCueProbe({});
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

void RefineSession_Test::aTrackedLayerKeepsHereOrAtItsSource()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->listOfCue(m_cue)->id;
    // Cue 2 sets nothing: AT spots track in from cue 1.
    const quint32 cue2 = st->addCue(list, 0, "Verse");
    QVERIFY(st->cue(cue2)->layers.isEmpty());

    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QString why;
    QVERIFY2(r->start(cue2, invalidId(), &why), qPrintable(why));
    QCOMPARE(r->layers().size(), 1);
    QVERIFY(r->isTrackedLayer());
    QCOMPARE(r->trackedSource(), m_cue);
    // The default, said: a keep goes into this cue.
    QVERIFY2(said.last().at(0).toString().endsWith("AT spots, tracked from 1. Keeps go into this cue."),
             qPrintable(said.last().at(0).toString()));
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->cuePosition(m_spots.at(0), pan, tilt));
    QCOMPARE(pan, 270.0f);
    // An exception needs the layer to be this cue's own first.
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::LayerScope);

    // A keeps it in this cue: its own layer now, cue 1 as it was.
    QVERIFY(r->nudgePosition(4.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(said.last().at(0).toString(), QString("Kept in this cue."));
    QCOMPARE(st->cue(cue2)->layers.size(), 1);
    QCOMPARE(m_doc->palette(st->cue(cue2)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 274, 135 }));
    QCOMPARE(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes, QList<quint32>({ m_position }));
    QVERIFY(!r->isTrackedLayer());                  // stepping on from its own layer
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    r->stop();

    // View + A (Q118): once at the source, the setting left as it was.
    const quint32 cue4 = st->addCue(list, 1, "Middle 8");
    QVERIFY(r->start(cue4));
    QVERIFY(r->isTrackedLayer());
    QVERIFY(!r->keepsToSource());
    QVERIFY(r->nudgePosition(0.0f, 2.0f));
    QVERIFY(r->keepAtSource());
    QCOMPARE(said.last().at(0).toString(), QString("Kept in the cue it tracks from."));
    QVERIFY(!r->keepsToSource());
    QVERIFY(st->cue(cue4)->layers.isEmpty());
    QCOMPARE(m_doc->palette(st->cue(cue2)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 274, 137 }));
    r->stop();

    // Cue 3 tracks from cue 2; this time keep at the source.
    const quint32 cue3 = st->addCue(list, 1, "Bridge");
    QVERIFY(r->start(cue3));
    QVERIFY(r->isTrackedLayer());
    QCOMPARE(r->trackedSource(), cue2);
    r->setKeepToSource(true);
    QCOMPARE(said.last().at(0).toString(), QString("Keeps go into cue 2."));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(said.last().at(0).toString(), QString("Kept in the cue it tracks from."));
    QVERIFY(st->cue(cue3)->layers.isEmpty());       // still only tracks in
    QCOMPARE(m_doc->palette(st->cue(cue2)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 275, 137 }));
    r->stop();
}

void RefineSession_Test::refineTellsYouMoreToo()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    ProgrammerController *pc = m_doc->programmer();
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy buzz(pc, &ProgrammerController::padRumbled);
    QVERIFY(r->start(m_cue));
    QCOMPARE(pc->padLight(), ControllerScheme::savedColour());       // nothing pending: blue
    QVERIFY(!r->nextPending());
    QCOMPARE(said.last().at(0).toString(), QString("All kept."));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QCOMPARE(pc->padLight(), ControllerScheme::thisUnsavedColour()); // pending here: yellow
    r->setScope(RefineSession::LayerScope);
    // Both bumpers: back to the change not kept.
    QVERIFY(r->nextPending());
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), m_spots.at(1));
    QCOMPARE(said.last().at(0).toString(), QString("AT 2, not kept."));
    // Exit with it pending says so; a keep buzzes and says the light.
    r->apply(ControllerScheme::Exit);
    QCOMPARE(said.last().at(0).toString(), QString("1 change not kept."));
    QVERIFY(r->keep());
    QCOMPARE(said.last().at(0).toString(), QString("AT 2 kept in cue 1."));
    QCOMPARE(buzz.last().at(0).toInt(), 1);
    r->stop();
    QVERIFY(!pc->padLight().isValid());
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
    QCOMPARE(said.last().at(0).toString(), QString("Focusing scene Ballad look. Ballad look, layer."));
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

    /* Layer scope: the scene's palette moves (as the Look Editor does). But
       Centre is also the Show Tree cue's (its baked scene): Q116 -- said,
       and only on a second A. */
    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(2.0f, -1.0f));
    QVERIFY(r->nudgeZoom(-5.0f));            // no zoom palette: the scene's own values
    why.clear();
    QVERIFY(!r->keep(&why));
    QVERIFY2(why.startsWith("Centre is also used by 1-Ballad"), qPrintable(why));
    QCOMPARE(r->sharedUsers().size(), 1);
    QCOMPARE(said.last().at(0).toString(), why);
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));   // not yet
    QVERIFY(r->keep());                      // A again: changed everywhere
    QVERIFY(r->sharedWarning().isEmpty());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 272, 134 }));
    int zoomedNow = 0;
    for (const SceneValue &sv : s->values())
        if (sv.channel == 5)
            ++zoomedNow;
    QCOMPARE(zoomedNow, m_spots.size());
    // Q117: the palette and the scene's own values are ONE step.
    QVERIFY(r->undo());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));
    zoomedNow = 0;
    for (const SceneValue &sv : s->values())
        if (sv.channel == 5)
            ++zoomedNow;
    QCOMPARE(zoomedNow, 0);
    // B before the second A: nothing changes, and the warning goes.
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->keep());
    r->revert();
    QVERIFY(r->sharedWarning().isEmpty());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));

    // Zoom, with no zoom palette: the scene's own values on all of it.
    QVERIFY(r->nudgeZoom(-5.0f));
    QVERIFY(r->keep());                      // no shared palette: at once
    int zoomed = 0;
    for (const SceneValue &sv : s->values())
        if (sv.channel == 5)
            ++zoomed;
    QCOMPARE(zoomed, m_spots.size());
    r->stop();
    QVERIFY(!r->isSceneMode());
}

void RefineSession_Test::aHeldANeverAnswersAWarning()
{
    // The pad, through the stub plugin: A on channel 10.
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() > 0);
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 11, "b" },
                             { 16, "back" }, { 17, "start" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto down = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 255, QString()); };
    auto up = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 0, QString()); };
    auto held = [pc](double secs) {
        for (int i = 0; i < qRound(secs / 0.02); ++i)
            pc->applyDesignJoystick();
    };

    // Centre is the scene's and the cue's: a Layer-scope keep warns first.
    Scene *s = new Scene(m_doc);
    s->setName("Ballad look");
    s->addFixtureGroup(m_group);
    s->addPalette(m_position);
    QVERIFY(m_doc->addFunction(s));
    RefineSession *r = pc->refineSession();
    QVERIFY(r->startScene({ s->id() }, s->id()));
    QVERIFY(r->nudgePosition(2.0f, 0.0f));
    down(10);                                   // A: the warning
    QVERIFY(r->hasWarningPending());
    held(1.5);                                  // ...held well past the keep-all hold
    up(10);
    QVERIFY(r->hasWarningPending());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 270, 135 }));   // not changed
    QVERIFY(r->hasPending());
    down(10);                                   // a fresh press answers it
    up(10);
    QVERIFY(!r->hasWarningPending());
    QCOMPARE(m_doc->palette(m_position)->values(), QVariantList({ 272, 135 }));
    r->stop();

    // The keep-all hold is a second: not at 0.8 s, at 1.0 s.
    QCOMPARE(ControllerScheme::keepAllHoldSeconds(), 1.0);
    ControllerScheme::LongPress keepAll(ControllerScheme::keepAllHoldSeconds());
    keepAll.press();
    QVERIFY(!keepAll.due(0.8));
    QVERIFY(keepAll.due(0.2));
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    r->nextItem();
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QSignalSpy kept(r, &RefineSession::kept);
    down(10);                                   // the press keeps
    QCOMPARE(kept.count(), 1);
    QVERIFY(!r->hasPending());
    up(10);
    r->stop();
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
}

void RefineSession_Test::aReusableLayerWarnsFirst()
{
    ShowTreeController *st = m_doc->showTree();
    QString why;
    const quint32 reusable = st->makeLayerReusable(m_cue, m_layer, "Specials", &why);
    QVERIFY2(reusable != invalidId(), qPrintable(why));
    const quint32 list = st->listOfCue(m_cue)->id;
    const quint32 cue2 = st->addCue(list, 0, "Verse");
    QVERIFY(st->insertReusableLayer(cue2, reusable) != invalidId());

    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(3.0f, 0.0f));
    why.clear();
    QVERIFY(!r->keep(&why));                                  // warned, not kept
    QVERIFY2(why.startsWith("Specials is reusable: it is also used in"), qPrintable(why));
    QCOMPARE(said.last().at(1).toInt(), int(ControllerScheme::ImportantPriority));
    QVERIFY(r->hasWarningPending());
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 270, 135 }));
    QVERIFY(r->keep());                                       // A again: in every use
    QVERIFY(!r->hasWarningPending());
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 273, 135 }));
    r->stop();
}

/* CHANGELOG [0.2.2] Refine: "position, zoom, focus, colour and gobo at
   Layer, Group or Fixture scope, previewed on the rig and kept by A as one
   Show Tree step". The gobo page at Layer scope: D-pad right picks the
   show's first gobo palette by name, every light of the layer shows it at
   once, the cue has it only after A, and one undo takes it back. */
void RefineSession_Test::aGoboAtLayerScopeIsPreviewedAndKept()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 base = quint32(m_doc->fixtures().size() * kChannels) + 20;
    Fixture *g1 = makeGoboSpot(m_doc, "Gobo 1", base);
    Fixture *g2 = makeGoboSpot(m_doc, "Gobo 2", base + 2);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Gobo spots");
    grp->setSize(QSize(2, 1));
    grp->assignFixture(g1->id(), QLCPoint(0, 0));
    grp->assignFixture(g2->id(), QLCPoint(1, 0));
    m_doc->addFixtureGroup(grp);
    QLCPalette *star = new QLCPalette(QLCPalette::Gobo);
    star->setName("Star");
    star->setGoboName("Star");
    m_doc->addPalette(star);
    QLCPalette *dots = new QLCPalette(QLCPalette::Gobo);
    dots->setName("Dots");
    dots->setGoboName("Dots");
    m_doc->addPalette(dots);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);

    const quint32 cue = st->addCue(st->addCueList(st->addProduction("Gobos"), "Main"), -1, "Gobo cue");
    Target t;
    t.groups << grp->id();
    const quint32 layer = st->addLayer(cue, t);
    QVERIFY(st->setLayerLook(cue, layer, lookOf(KeyDimmer, { full->id() })));
    QVERIFY(st->cue(cue)->layers.first().look(KeyGobo) == nullptr);

    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(cue));
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    r->apply(ControllerScheme::Alternate);          // colour page
    r->apply(ControllerScheme::Alternate);          // gobo page
    QCOMPARE(r->page(), RefineSession::GoboPage);

    r->press(ControllerScheme::FocusFarther, true);  // D-pad right
    r->press(ControllerScheme::FocusFarther, false);
    QVERIFY(r->hasPending());
    QCOMPARE(said.last().at(0).toString(), QString("Dots"));    // first by name
    // On the rig at once, on both lights of the layer: the Dots slot.
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    for (Fixture *fx : { g1, g2 })
    {
        const QPair<int, int> at(0, int(fx->address()) + 1);
        QVERIFY2(w.contains(at), qPrintable(fx->name()));
        QVERIFY(w.value(at) >= 10 && w.value(at) <= 19);
    }
    // Not in the cue yet.
    QVERIFY(st->cue(cue)->layers.first().look(KeyGobo) == nullptr);

    r->apply(ControllerScheme::Keep);               // A
    QVERIFY(!r->hasPending());
    QVERIFY(r->pendingWrites().isEmpty());
    const Look *gobo = st->cue(cue)->layers.first().look(KeyGobo);
    QVERIFY(gobo != nullptr);
    QCOMPARE(gobo->palettes, QList<quint32>({ dots->id() }));
    QCOMPARE(st->cue(cue)->layers.size(), 1);       // the layer's own, no exception

    // One Show Tree step: one undo, and the gobo is gone, the dimmer stays.
    QVERIFY(st->undo());
    QVERIFY(st->cue(cue)->layers.first().look(KeyGobo) == nullptr);
    QVERIFY(st->cue(cue)->layers.first().look(KeyDimmer) != nullptr);
    r->stop();
}

QTEST_GUILESS_MAIN(RefineSession_Test)

/* The changelog: Refine is "quiet in Operate" -- with a real cue list
   running (ShowControl's GO, the MasterTimer playing it), Refine starts on
   the running cue without previewing, never blinks or dims the rig, shows
   what is moved, holds what A keeps; the cue's own playback takes the kept
   change only at its next GO (refine.md, "Refine during the show"). */
void RefineSession_Test::operateIsQuietOnARunningList()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->listOfCue(m_cue)->id;
    QLCPalette *bright = new QLCPalette(QLCPalette::Dimmer);
    bright->setName("Bright");
    bright->setValue(200);
    m_doc->addPalette(bright);
    QLCPalette *away = new QLCPalette(QLCPalette::PanTilt);
    away->setName("Away");
    away->setValues({ 100, 100 });
    m_doc->addPalette(away);
    Target t;
    t.groups << m_group;
    QVERIFY(st->setTargetLook(m_cue, t, st->lookForPalette(bright->id())));
    const quint32 next = st->addCue(list, 0, "Next");
    QVERIFY(st->setTargetLook(next, t, st->lookForPalette(away->id())));
    QVERIFY(st->setTargetLook(next, t, st->lookForPalette(bright->id())));
    const quint32 chaser = st->cueList(list)->chaser;
    ShowControl *sc = m_doc->showControl();
    auto command = [sc, chaser](ShowControl::Command c) {
        ShowControl::Request r;
        r.command = c;
        r.chaser = chaser;
        return sc->execute(r);
    };
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    auto frames = [this, uni](int n) {
        for (int i = 0; i < n; ++i)
        {
            m_doc->masterTimer()->timerTick();
            QCoreApplication::processEvents();
            uni->processFaders();
        }
    };
    auto rigPan = [this, uni](quint32 fid) {
        Fixture *fx = m_doc->fixture(fid);
        QHash<QPair<int, int>, uchar> w;
        for (int c = 0; c < 4; ++c)
            w.insert(qMakePair(0, int(fx->address()) + c), uchar(uni->preGMValue(int(fx->address()) + c)));
        float pan = -1.0f, tilt = -1.0f;
        sentDegrees(m_doc, w, fid, pan, tilt);
        return pan;
    };
    auto dimmers = [this, uni]() {
        QList<int> d;
        for (quint32 fid : m_spots)
            d << int(uni->preGMValue(int(m_doc->fixture(fid)->address()) + 4));
        return d;
    };
    auto rig = [uni]() {
        QByteArray all;
        for (int c = 0; c < 4 * kChannels; ++c)
            all.append(char(uni->preGMValue(c)));
        return all;
    };

    m_doc->setMode(Doc::Operate);
    QVERIFY(command(ShowControl::Go));
    frames(20);
    QCOMPARE(st->runningCue(list), m_cue);
    QVERIFY2(qAbs(rigPan(m_spots.at(2)) - 270.0f) < 0.05f, qPrintable(QString::number(rigPan(m_spots.at(2)))));
    const QList<int> lit = dimmers();
    QCOMPARE(lit, QList<int>({ 200, 200, 200, 200 }));
    const QByteArray asPlayed = rig();

    // Started on the running cue (no probe: the list as it plays), the rig
    // is left as the show has it -- no preview, nothing sent.
    RefineSession *r = m_doc->programmer()->refineSession();
    r->setFlashEnabled(true);
    QString why;
    QVERIFY2(r->start(m_cue, invalidId(), &why), qPrintable(why));
    QVERIFY(!r->isBlind());
    QVERIFY(st->previewedCue() != m_cue);
    frames(5);
    QCOMPARE(rig(), asPlayed);
    // A fixture picked: no blink on it, no dim on the rest.
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->whereAmI();
    for (int i = 0; i < 10; ++i)
    {
        frames(1);
        QCOMPARE(dimmers(), lit);
    }
    QVERIFY(!r->isFlashing());
    QCOMPARE(rig(), asPlayed);

    // What is moved shows at once; still no blink, no dim.
    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(4.0f, 0.0f));
    frames(3);
    QVERIFY(qAbs(rigPan(m_spots.at(2)) - 274.0f) < 0.05f);
    QCOMPARE(dimmers(), lit);
    // A keeps it: held on the rig.
    QVERIFY(r->keep());
    QVERIFY(!r->hasPending());
    frames(3);
    QVERIFY(qAbs(rigPan(m_spots.at(2)) - 274.0f) < 0.05f);
    QCOMPARE(dimmers(), lit);
    // Leaving Refine lets go of the hold: the cue plays as it was baked --
    // the kept change is not in the running cue until its next GO.
    r->stop();
    frames(3);
    QVERIFY2(qAbs(rigPan(m_spots.at(2)) - 270.0f) < 0.05f, qPrintable(QString::number(rigPan(m_spots.at(2)))));
    QCOMPARE(rig(), asPlayed);

    // GO: the next cue. BACK onto it: it runs again, with the kept change.
    QVERIFY(command(ShowControl::Go));
    frames(20);
    QCOMPARE(st->runningCue(list), next);
    QVERIFY(qAbs(rigPan(m_spots.at(2)) - 100.0f) < 0.05f);
    QVERIFY(command(ShowControl::Back));
    frames(20);
    QCOMPARE(st->runningCue(list), m_cue);
    QVERIFY2(qAbs(rigPan(m_spots.at(2)) - 274.0f) < 0.05f, qPrintable(QString::number(rigPan(m_spots.at(2)))));
    QCOMPARE(dimmers(), lit);

    QVERIFY(command(ShowControl::Stop));
    frames(5);
    m_doc->setMode(Doc::Design);
}

/* The changelog (Q140): both stick clicks hold every light
   (setHolding()): a held Refine takes no stick and no button; released,
   the stick moves it again. */
void RefineSession_Test::holdingTakesNoStick()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setStick(RefineSession::Coarse, 1.0f, 0.5f);
    r->setHolding(true);
    QVERIFY(r->isHolding());
    r->tick(0.5);
    QVERIFY(!r->hasPending());
    QVERIFY(r->pendingWrites().isEmpty());
    // Nor a button: Y, the D-pad, a nudge by hand.
    r->apply(ControllerScheme::CycleScope);
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    r->apply(ControllerScheme::ZoomNarrower);
    QVERIFY(!r->nudgePosition(2.0f, 0.0f));
    QVERIFY(!r->hasPending());
    // Released: the stick (still over) moves the layer.
    r->setHolding(false);
    QVERIFY(!r->isHolding());
    r->tick(0.1);
    QVERIFY(r->hasPending());
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.at(0), pan, tilt));
    QVERIFY(pan > 270.0f);
    // The programmer's hold (the chord, or H) reaches it too.
    r->setStick(RefineSession::Coarse, 0.5f, 0.5f);
    m_doc->programmer()->setHolding(true);
    QVERIFY(r->isHolding());
    float held = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.at(0), held, tilt));
    r->setStick(RefineSession::Coarse, 1.0f, 0.5f);
    r->tick(0.5);
    float after = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.at(0), after, tilt));
    QCOMPARE(after, held);
    m_doc->programmer()->setHolding(false);
    QVERIFY(!r->isHolding());
    r->revert();
    r->stop();
}

void RefineSession_Test::splitIntoTwoGroups(quint32 *left, quint32 *right)
{
    FixtureGroup *l = new FixtureGroup(m_doc);
    l->setName("AT left");
    l->setSize(QSize(2, 1));
    l->assignFixture(m_spots.at(0), QLCPoint(0, 0));
    l->assignFixture(m_spots.at(1), QLCPoint(1, 0));
    m_doc->addFixtureGroup(l);
    FixtureGroup *r = new FixtureGroup(m_doc);
    r->setName("AT right");
    r->setSize(QSize(2, 1));
    r->assignFixture(m_spots.at(2), QLCPoint(0, 0));
    r->assignFixture(m_spots.at(3), QLCPoint(1, 0));
    m_doc->addFixtureGroup(r);
    Target both;
    both.groups << l->id() << r->id();
    QVERIFY(m_doc->showTree()->setLayerTarget(m_cue, m_layer, both));
    *left = l->id();
    *right = r->id();
}

/* G1: into Operate with the cue not running, Refine goes blind at once --
   the Design dims and the pending preview leave the live rig. */
void RefineSession_Test::operateDropsTheDesignFeedback()
{
    QVERIFY(m_doc->showTree()->setLayerLook(m_cue, m_layer,
                                            lookOf(KeyDimmer, { addDimmerPalette(m_doc, "80", 204) })));
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    QVERIFY(w.contains(chAt(m_doc, m_spots.at(1), 4)));    // the rest dimmed
    QVERIFY(w.contains(chAt(m_doc, m_spots.at(0), 0)));    // the nudge

    r->setRunningCueProbe([](quint32) { return invalidId(); });
    m_doc->setMode(Doc::Operate);
    QVERIFY(r->isBlind());
    QVERIFY(r->pendingWrites().isEmpty());
    r->tick(0.02);                                          // and stays so
    QVERIFY(r->pendingWrites().isEmpty());
    m_doc->setMode(Doc::Design);
    r->stop();
    r->setRunningCueProbe({});
}

/* G1: the GO is said once; the next ticks say nothing more. */
void RefineSession_Test::aMovedOnCueIsSaidOnce()
{
    m_doc->setMode(Doc::Operate);
    RefineSession *r = m_doc->programmer()->refineSession();
    quint32 running = m_cue;
    r->setRunningCueProbe([&running](quint32) { return running; });
    QVERIFY(r->start(m_cue));
    QSignalSpy said(r, &RefineSession::announce);
    running = invalidId() - 1;
    for (int i = 0; i < 6; ++i)
        r->tick(0.02);
    int movedOn = 0;
    for (const QList<QVariant> &s : said)
        if (s.at(0).toString().startsWith("Cue 1 has moved on"))
            ++movedOn;
    QCOMPARE(movedOn, 1);
    running = m_cue;
    r->tick(0.02);
    QVERIFY(!r->isBlind());
    r->stop();
    r->setRunningCueProbe({});
    m_doc->setMode(Doc::Design);
}

/* G2: a fixture's kept zoom is found again (its own child) and refined
   from there, in the same session and in a new one. */
void RefineSession_Test::aKeptExceptionIsRefinedAgain()
{
    ShowTreeController *st = m_doc->showTree();
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgeZoom(-1.0f));
    QVERIFY(r->keep());
    QCOMPARE(st->cue(m_cue)->layers.size(), 2);
    QVERIFY(r->nudgeZoom(-1.0f));
    float deg = 0.0f;
    QVERIFY(r->pendingZoom(m_spots.at(1), deg));
    QCOMPARE(deg, 38.0f);
    QVERIFY(r->keep());
    r->stop();

    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgeZoom(-1.0f));
    QVERIFY(r->pendingZoom(m_spots.at(1), deg));
    QCOMPARE(deg, 37.0f);
    QVERIFY(r->keep());
    QCOMPARE(st->cue(m_cue)->layers.size(), 2);
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.at(1).look(KeyZoom)->palettes.first())->value().toFloat(),
             37.0f);
    r->stop();
}

/* G3: the second A answers one warning, not every keep after it; and a
   warning left unanswered goes with the session. */
void RefineSession_Test::theSharedWarningIsAskedEveryKeep()
{
    ShowTreeController *st = m_doc->showTree();
    QString why;
    const quint32 reusable = st->makeLayerReusable(m_cue, m_layer, "Specials", &why);
    QVERIFY2(reusable != invalidId(), qPrintable(why));
    const quint32 cue2 = st->addCue(st->listOfCue(m_cue)->id, 0, "Verse");
    QVERIFY(st->insertReusableLayer(cue2, reusable) != invalidId());

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(3.0f, 0.0f));
    QVERIFY(!r->keep());                    // warned
    QVERIFY(r->keep());                     // A again
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->keep());                    // warned again
    QVERIFY(r->hasWarningPending());
    r->revert();

    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->keep());                    // warned, not answered
    r->stop();
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->keep());                    // a new session asks again
    r->stop();
}

/* G4 (937) and G22: an exception is a child layer in this cue only -- no
   warning for a reusable parent; the warning's users are listed until B. */
void RefineSession_Test::anExceptionOnAReusableLayerIsThisCuesOwn()
{
    ShowTreeController *st = m_doc->showTree();
    QString why;
    const quint32 reusable = st->makeLayerReusable(m_cue, m_layer, "Specials", &why);
    QVERIFY2(reusable != invalidId(), qPrintable(why));
    const quint32 cue2 = st->addCue(st->listOfCue(m_cue)->id, 0, "Verse");
    QVERIFY(st->insertReusableLayer(cue2, reusable) != invalidId());

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(2.0f, 0.0f));
    why.clear();
    QVERIFY2(r->keep(&why), qPrintable(why));
    QVERIFY(!r->hasWarningPending());

    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(2.0f, 0.0f));
    QVERIFY(!r->keep());
    QCOMPARE(r->sharedUsers().size(), 1);
    r->revert();
    QVERIFY(r->sharedUsers().isEmpty());
    QVERIFY(r->sharedWarning().isEmpty());
    r->stop();
}

/* G25 (and G24): X is ONE Show Tree step, so one undo brings back every
   position it took off; a pending change at that scope goes with it, off
   the rig too. */
void RefineSession_Test::clearingAnExceptionIsOneUndoStep()
{
    ShowTreeController *st = m_doc->showTree();
    quint32 left = 0, right = 0;
    splitIntoTwoGroups(&left, &right);
    auto positionOf = [&](quint32 fid) {
        Target t;
        t.fixtures << fid;
        for (const Layer &l : st->cue(m_cue)->layers)
            if (l.parent == m_layer && l.target == t && l.look(KeyPosition) != nullptr)
                return m_doc->palette(l.look(KeyPosition)->palettes.first())->values().first().toInt();
        return -1;
    };

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::GroupScope);
    QVERIFY(r->nudgePosition(5.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(positionOf(m_spots.at(0)), 275);
    QCOMPARE(positionOf(m_spots.at(1)), 275);

    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->pendingWrites().contains(chAt(m_doc, m_spots.at(0), 0)));
    QVERIFY(r->clearException());
    QVERIFY(!r->hasPending());
    QVERIFY(!r->pendingWrites().contains(chAt(m_doc, m_spots.at(0), 0)));
    QCOMPARE(positionOf(m_spots.at(0)), -1);
    QCOMPARE(positionOf(m_spots.at(1)), -1);
    QVERIFY(r->undo());
    QCOMPARE(positionOf(m_spots.at(0)), 275);
    QCOMPARE(positionOf(m_spots.at(1)), 275);
    // A keep after it is a step of its own.
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(positionOf(m_spots.at(0)), 276);
    QVERIFY(r->undo());
    QCOMPARE(positionOf(m_spots.at(0)), 275);

    // At Fixture scope: the child's looks and the child, one step.
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->nextItem();
    QCOMPARE(r->currentFixture(), m_spots.at(2));
    QVERIFY(r->nudgePosition(0.0f, 5.0f));
    QVERIFY(r->nudgeZoom(-1.0f));
    QVERIFY(r->keep());
    const int withChild = st->cue(m_cue)->layers.size();
    QVERIFY(r->clearException());
    QCOMPARE(st->cue(m_cue)->layers.size(), withChild - 1);
    QVERIFY(r->undo());
    QCOMPARE(st->cue(m_cue)->layers.size(), withChild);
    QCOMPARE(positionOf(m_spots.at(2)), 270);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(positionOf(m_spots.at(2)), 271);
    QVERIFY(r->undo());
    QCOMPARE(positionOf(m_spots.at(2)), 270);
    r->stop();
}

/* G26: X on the Beam page takes the beam keys off; the exception's colour
   (another page's) stays, and so does its child. */
void RefineSession_Test::xOnTheBeamPageKeepsTheColour()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 blue = addColourPalette(m_doc, "Blue", Qt::blue);
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(0.0f, 5.0f));
    QVERIFY(r->keep());
    r->apply(ControllerScheme::Alternate);          // colour page
    QVERIFY(r->stepPalette(1));
    QVERIFY(r->keep());
    QCOMPARE(st->cue(m_cue)->layers.size(), 2);
    QVERIFY(st->cue(m_cue)->layers.at(1).look(KeyPosition) != nullptr);
    QVERIFY(st->cue(m_cue)->layers.at(1).look(KeyColour) != nullptr);

    r->apply(ControllerScheme::Alternate);
    r->apply(ControllerScheme::Alternate);          // the Beam page
    QCOMPARE(r->page(), RefineSession::BeamPage);
    QVERIFY(r->clearException());
    QCOMPARE(said.last().at(0).toString(), QString("Exception cleared."));
    QCOMPARE(st->cue(m_cue)->layers.size(), 2);
    const Layer &child = st->cue(m_cue)->layers.at(1);
    QVERIFY(child.look(KeyPosition) == nullptr);
    QVERIFY(child.look(KeyColour) != nullptr);
    QCOMPARE(child.look(KeyColour)->palettes, QList<quint32>({ blue }));
    r->stop();
}

/* G4: at Group scope, what is not a position is kept on the group's child
   (zoom, focus, colour, gobo alone each); a position alone is kept on the
   fixtures and makes no group child. */
void RefineSession_Test::groupScopeKeepsEveryAttribute()
{
    ShowTreeController *st = m_doc->showTree();
    quint32 left = 0, right = 0;
    splitIntoTwoGroups(&left, &right);
    addColourPalette(m_doc, "Red", Qt::red);
    QLCPalette *star = new QLCPalette(QLCPalette::Gobo);
    star->setName("Star");
    star->setGoboName("Star");
    m_doc->addPalette(star);
    Target leftT;
    leftT.groups << left;
    auto groupChild = [&]() -> const Layer * {
        for (const Layer &l : st->cue(m_cue)->layers)
            if (l.parent == m_layer && l.target == leftT)
                return &l;
        return nullptr;
    };

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->cycleScope();
    QCOMPARE(r->scope(), RefineSession::GroupScope);
    QVERIFY(r->nudgePosition(2.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(st->cue(m_cue)->layers.size(), 3);     // the layer, two fixtures
    QVERIFY(groupChild() == nullptr);

    QVERIFY(r->nudgeZoom(-1.0f));
    QVERIFY(r->keep());
    QVERIFY(groupChild() != nullptr);
    QVERIFY(groupChild()->look(KeyZoom) != nullptr);
    QCOMPARE(m_doc->palette(groupChild()->look(KeyZoom)->palettes.first())->value().toFloat(), 39.0f);

    QVERIFY(r->nudgeFocus(5));
    QVERIFY(r->keep());
    QVERIFY(groupChild()->look(KeyBeam) != nullptr);

    r->apply(ControllerScheme::Alternate);          // colour
    QVERIFY(r->stepPalette(1));
    QVERIFY(r->keep());
    QVERIFY(groupChild()->look(KeyColour) != nullptr);

    r->apply(ControllerScheme::Alternate);          // gobo
    QVERIFY(r->stepPalette(1));
    QVERIFY(r->keep());
    QVERIFY(groupChild()->look(KeyGobo) != nullptr);
    QCOMPARE(groupChild()->look(KeyGobo)->palettes, QList<quint32>({ star->id() }));
    r->stop();
}

/* G5: a fanned or aimed layer is read per fixture, from that fixture's own
   DMX -- not the palette's one value, not another fixture's. */
void RefineSession_Test::cuePositionReadsEachFixture()
{
    ShowTreeController *st = m_doc->showTree();
    QLCPalette *fan = addFannedPosition(m_doc, 200, 100, QLCPalette::XAscending, 100, 40);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyPosition, { fan->id() })));
    const QHash<QPair<int, int>, uchar> fanned = asWrites(m_doc, fan->valuesFromFixtures(m_doc, m_spots));

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    float first = 0.0f, last = 0.0f;
    for (quint32 fid : m_spots)
    {
        float pan = 0.0f, tilt = 0.0f, wantPan = 0.0f, wantTilt = 0.0f;
        QVERIFY(r->cuePosition(fid, pan, tilt));
        QVERIFY(sentDegrees(m_doc, fanned, fid, wantPan, wantTilt));
        QVERIFY2(qAbs(pan - wantPan) < 0.05f, qPrintable(QString("%1 %2").arg(pan).arg(wantPan)));
        QVERIFY(qAbs(tilt - wantTilt) < 0.05f);
        if (fid == m_spots.first())
            first = pan;
        last = pan;
    }
    QVERIFY(last - first > 30.0f);                  // a fan
    r->stop();

    // Aimed: a fixture's exception starts from where it is aimed.
    StageTarget *dsc = m_doc->monitorProperties()->addStageTarget();
    dsc->setName("DSC");
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim);
    aim->setName("Aim DSC");
    aim->setStageTargetId(dsc->id());
    m_doc->addPalette(aim);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyPosition, { aim->id() })));
    const QHash<QPair<int, int>, uchar> aimed = asWrites(m_doc, aim->valuesFromFixtures(m_doc, m_spots));
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    float aim0 = 0.0f, aim1 = 0.0f, tilt = 0.0f;
    QVERIFY(sentDegrees(m_doc, aimed, m_spots.at(0), aim0, tilt));
    QVERIFY(sentDegrees(m_doc, aimed, m_spots.at(1), aim1, tilt));
    QVERIFY(qAbs(aim0 - aim1) > 1.0f);
    r->nextItem();
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    float pan = 0.0f;
    QVERIFY(r->pendingPosition(m_spots.at(1), pan, tilt));
    QVERIFY2(qAbs(pan - (aim1 + 1.0f)) < 0.05f, qPrintable(QString("%1 %2").arg(pan).arg(aim1)));
    r->stop();
}

/* G6: a first zoom nudge starts from the wide end when the cue sets none;
   it is sent as absolute DMX; a position nudge sends no zoom; a focus on
   a fixture's first channel is found. */
void RefineSession_Test::zoomAndFocusStartWhereTheyAre()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->pendingWrites().contains(chAt(m_doc, m_spots.at(0), 5)));
    float deg = 0.0f;
    QVERIFY(!r->pendingZoom(m_spots.at(0), deg));
    r->revert();

    QVERIFY(r->nudgeZoom(-1.0f));
    QVERIFY(r->pendingZoom(m_spots.at(0), deg));
    QCOMPARE(deg, 39.0f);
    QVERIFY(r->nudgeZoom(-17.0f));
    QVERIFY(r->pendingZoom(m_spots.at(0), deg));
    QCOMPARE(deg, 22.0f);
    Fixture *fx = m_doc->fixture(m_spots.at(0));
    const QList<SceneValue> zoom = fx->zoomToValues(22.0f, false);
    QCOMPARE(zoom.size(), 1);
    QCOMPARE(r->pendingWrites().value(chAt(m_doc, m_spots.at(0), int(zoom.first().channel))),
             zoom.first().value);
    r->stop();

    ShowTreeController *st = m_doc->showTree();
    Fixture *ff = makeFocusFirst(m_doc, "Focus first", 100);
    const quint32 cue = st->addCue(st->listOfCue(m_cue)->id, -1, "Focus");
    Target t;
    t.fixtures << ff->id();
    QVERIFY(st->addLayer(cue, t) != invalidId());
    QVERIFY(r->start(cue));
    QVERIFY(r->nudgeFocus(5));
    int focus = 0;
    QVERIFY(r->pendingFocus(ff->id(), focus));
    QCOMPARE(focus, 133);
    QCOMPARE(int(r->pendingWrites().value(qMakePair(0, 100))), 133);
    r->stop();
}

/* G7: a layer keep makes a new palette with the old one's fanning. */
void RefineSession_Test::aLayerKeepKeepsTheFan()
{
    ShowTreeController *st = m_doc->showTree();
    QLCPalette *fan = addFannedPosition(m_doc, 200, 100, QLCPalette::YDescending, 60, 40);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyPosition, { fan->id() })));
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(2.0f, 0.0f));
    QVERIFY(r->keep());
    const QLCPalette *kept = m_doc->palette(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes.first());
    QVERIFY(kept != nullptr && kept != fan);
    QCOMPARE(kept->values(), QVariantList({ 202, 100 }));
    QCOMPARE(kept->fanningType(), QLCPalette::Linear);
    QCOMPARE(kept->fanningLayout(), QLCPalette::YDescending);
    QCOMPARE(kept->fanningAmount(), 60);
    QCOMPARE(kept->fanningValue().toInt(), 40);
    r->stop();
}

/* G8: two layers tracked in are two stand-ins; a keep here on the second
   leaves Refine on that layer, now the cue's own; two keeps at the source
   add up there. */
void RefineSession_Test::twoTrackedLayersStayApart()
{
    ShowTreeController *st = m_doc->showTree();
    Fixture *x = makeSpot(m_doc, "X", 12.0f);
    Target xT;
    xT.fixtures << x->id();
    const quint32 layerX = st->addLayer(m_cue, xT, "Special");
    const quint32 away = st->createInlinePalette(QLCPalette::PanTilt, { 100, 100 }, "Away");
    QVERIFY(st->setLayerLook(m_cue, layerX, lookOf(KeyPosition, { away })));
    const quint32 cue2 = st->addCue(st->listOfCue(m_cue)->id, 0, "Verse");

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(cue2));
    QCOMPARE(r->layers().size(), 2);
    QVERIFY(r->layers().at(0) != r->layers().at(1));
    r->nextLayer();
    QVERIFY(r->isTrackedLayer());
    QVERIFY(r->scopeTarget() == xT);
    QVERIFY(r->nudgePosition(3.0f, 0.0f));
    QVERIFY(r->keep());
    QVERIFY(!r->isTrackedLayer());
    QVERIFY(r->scopeTarget() == xT);
    QCOMPARE(st->cue(cue2)->layers.size(), 1);

    // The group still tracks in: kept twice at its source, both count.
    r->previousLayer();
    QVERIFY(r->isTrackedLayer());
    Target groupT;
    groupT.groups << m_group;
    QVERIFY(r->scopeTarget() == groupT);
    r->setKeepToSource(true);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    QCOMPARE(m_doc->palette(st->cue(m_cue)->layers.first().look(KeyPosition)->palettes.first())->values(),
             QVariantList({ 272, 135 }));
    r->stop();
}

/* G17: stick Y tilts; the response's sensitivity scales the stick; a held
   D-pad arm does not repeat into the next session. */
void RefineSession_Test::theSticksAndTheHeldArm()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    float pan = 0.0f, tilt = 0.0f;
    r->setStick(RefineSession::Coarse, 0.5f, 1.0f);
    r->tick(0.5);
    QVERIFY(r->pendingPosition(m_spots.at(0), pan, tilt));
    QCOMPARE(pan, 270.0f);
    QVERIFY(tilt < 134.0f);
    r->revert();

    r->setStick(RefineSession::Coarse, 1.0f, 0.5f);
    r->tick(0.5);
    QVERIFY(r->pendingPosition(m_spots.at(0), pan, tilt));
    const float once = pan - 270.0f;
    QVERIFY(once > 1.0f);
    r->revert();
    r->setStickResponse(0.05f, 2.0f);
    r->tick(0.5);
    QVERIFY(r->pendingPosition(m_spots.at(0), pan, tilt));
    QVERIFY2(qAbs((pan - 270.0f) - 2.0f * once) < 0.1f, qPrintable(QString::number(pan)));
    r->setStick(RefineSession::Coarse, 0.5f, 0.5f);
    r->revert();

    r->press(ControllerScheme::ZoomWider, true);
    QVERIFY(r->hasPending());
    r->stop();
    QVERIFY(r->start(m_cue));
    r->tick(1.0);
    QVERIFY(!r->hasPending());
    r->press(ControllerScheme::ZoomWider, false);
    r->stop();
}

/* G9: a layer that tracks in dark (it plays nothing) is not one to refine. */
void RefineSession_Test::aDarkTrackedLayerIsNotOffered()
{
    ShowTreeController *st = m_doc->showTree();
    Fixture *x = makeSpot(m_doc, "X", 12.0f);
    Target xT;
    xT.fixtures << x->id();
    const quint32 layerX = st->addLayer(m_cue, xT, "Dark");
    Look none = lookOf(KeyPosition, {});
    none.none = true;
    QVERIFY(st->setLayerLook(m_cue, layerX, none));
    const quint32 cue2 = st->addCue(st->listOfCue(m_cue)->id, 0, "Verse");

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QCOMPARE(r->layers().size(), 2);                // its own, dark or not
    r->stop();
    QVERIFY(r->start(cue2));
    QCOMPARE(r->layers().size(), 1);
    QVERIFY(r->scopeTarget().groups.contains(m_group));
    r->stop();
}

/* G10: a fixture with no pan range in its physical maps through 360
   degrees, as Fixture::positionToValues() does. */
void RefineSession_Test::degreesWithoutAPanRange()
{
    Fixture *fx = makeSpot(m_doc, "No range", 12.0f, 0);
    for (float deg : { 0.0f, 90.0f, 200.0f, 359.0f })
    {
        quint16 v = 0;
        for (const SceneValue &sv : fx->positionToValues(QLCChannel::Pan, deg))
            v = sv.channel == 0 ? quint16((v & 0xFF) | (sv.value << 8)) : quint16((v & 0xFF00) | sv.value);
        QVERIFY2(qAbs(RefineSession::dmxToDegrees(fx, QLCChannel::Pan, v) - deg) < 0.02f,
                 qPrintable(QString::number(RefineSession::dmxToDegrees(fx, QLCChannel::Pan, v))));
    }
}

/* G11: the colour page steps on from the colour the cue has, then from
   what it stepped to, round both ways. */
void RefineSession_Test::palettesStepFromTheCuesOwn()
{
    addColourPalette(m_doc, "Amber", QColor(255, 191, 0));
    const quint32 blue = addColourPalette(m_doc, "Blue", Qt::blue);
    addColourPalette(m_doc, "Red", Qt::red);
    QVERIFY(m_doc->showTree()->setLayerLook(m_cue, m_layer, lookOf(KeyColour, { blue })));
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(m_cue));
    r->apply(ControllerScheme::Alternate);
    QVERIFY(r->stepPalette(1));
    QCOMPARE(said.last().at(0).toString(), QString("Red"));
    QVERIFY(r->stepPalette(1));
    QCOMPARE(said.last().at(0).toString(), QString("Amber"));
    QVERIFY(r->stepPalette(1));
    QCOMPARE(said.last().at(0).toString(), QString("Blue"));
    QVERIFY(r->stepPalette(-1));
    QCOMPARE(said.last().at(0).toString(), QString("Amber"));
    QVERIFY(r->stepPalette(-1));
    QCOMPARE(said.last().at(0).toString(), QString("Red"));
    r->stop();
}

/* G12: LT from the first layer goes to the last; a new layer starts at
   its first fixture, and the old layer's dims go. */
void RefineSession_Test::layersStepRoundAndStartFresh()
{
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyDimmer, { addDimmerPalette(m_doc, "80", 204) })));
    Fixture *x = makeSpot(m_doc, "X", 12.0f);
    Fixture *y = makeSpot(m_doc, "Y", 14.0f);
    Target xT, yT;
    xT.fixtures << x->id();
    yT.fixtures << y->id();
    const quint32 layer2 = st->addLayer(m_cue, xT, "Two");
    const quint32 layer3 = st->addLayer(m_cue, yT, "Three");

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QCOMPARE(r->layers().size(), 3);
    r->previousLayer();
    QCOMPARE(r->currentLayer(), layer3);
    r->setLayerIndex(0);
    QCOMPARE(r->currentLayer(), m_layer);
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->nextItem();
    QCOMPARE(r->currentFixture(), m_spots.at(2));
    QVERIFY(r->pendingWrites().contains(chAt(m_doc, m_spots.at(0), 4)));
    r->nextLayer();
    QCOMPARE(r->currentLayer(), layer2);
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    QVERIFY(r->pendingWrites().isEmpty());
    r->cycleScope();
    QCOMPARE(r->currentFixture(), x->id());
    r->stop();
}

/* G13: a fanned Dimmer look is read per fixture (one under the dim level
   is left alone); with no Dimmer look the level comes from the rig, read
   again after a trip through Operate. */
void RefineSession_Test::dimsReadTheFanAndTheRig()
{
    ShowTreeController *st = m_doc->showTree();
    QLCPalette *fan = new QLCPalette(QLCPalette::Dimmer);
    fan->setName("Fan");
    fan->setValue(10);
    fan->setFanningType(QLCPalette::Linear);
    fan->setFanningLayout(QLCPalette::XAscending);
    fan->setFanningAmount(100);
    fan->setFanningValue(255);
    m_doc->addPalette(fan);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyDimmer, { fan->id() })));
    const int dim = qRound(RefineSession::dimLevel() * 255.0f);

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->nextItem();
    QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    QVERIFY(!w.contains(chAt(m_doc, m_spots.at(0), 4)));        // 10: under it
    QCOMPARE(int(w.value(chAt(m_doc, m_spots.at(1), 4))), dim);
    QCOMPARE(int(w.value(chAt(m_doc, m_spots.at(3), 4))), dim);
    r->stop();

    // No Dimmer look: what the rig has.
    const quint32 plain = st->addCue(st->addCueList(st->addProduction("Plain"), "Main"), -1, "Plain");
    Target t;
    t.groups << m_group;
    QVERIFY(st->addLayer(plain, t) != invalidId());
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);
    QVERIFY(uni->write(chAt(m_doc, m_spots.at(0), 4).second, 200));
    QVERIFY(r->start(plain));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->nextItem();
    w = r->pendingWrites();
    QCOMPARE(int(w.value(chAt(m_doc, m_spots.at(0), 4))), dim);
    QVERIFY(!w.contains(chAt(m_doc, m_spots.at(1), 4)));        // at 0
    // Brought up meanwhile: read again after Operate.
    QVERIFY(uni->write(chAt(m_doc, m_spots.at(1), 4).second, 200));
    r->setRunningCueProbe([plain](quint32) { return plain; });
    m_doc->setMode(Doc::Operate);
    m_doc->setMode(Doc::Design);
    QCOMPARE(int(r->pendingWrites().value(chAt(m_doc, m_spots.at(1), 4))), dim);
    r->stop();
    r->setRunningCueProbe({});
}

/* G14: a fixture's row picks it, the first one too; one not in the layer is
   refused; a pick at Fixture scope is said and moves the dims. */
void RefineSession_Test::pickFixtureFromARow()
{
    QVERIFY(m_doc->showTree()->setLayerLook(m_cue, m_layer,
                                            lookOf(KeyDimmer, { addDimmerPalette(m_doc, "80", 204) })));
    Fixture *x = makeSpot(m_doc, "X", 12.0f);
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(m_cue));
    QVERIFY(!r->pickFixture(x->id()));
    QCOMPARE(r->scope(), RefineSession::LayerScope);
    QVERIFY(r->pickFixture(m_spots.first()));
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), m_spots.first());
    QVERIFY(r->pickFixture(m_spots.at(2)));
    QCOMPARE(r->currentFixture(), m_spots.at(2));
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 3, AT 3."));
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    QVERIFY(w.contains(chAt(m_doc, m_spots.at(0), 4)));
    QVERIFY(!w.contains(chAt(m_doc, m_spots.at(2), 4)));
    r->stop();
}

/* G15: the layer, or a fixture, deleted while an edit is pending: the
   preview is rebuilt without them, no crash. */
void RefineSession_Test::aLayerOrFixtureGoneUnderAnEdit()
{
    ShowTreeController *st = m_doc->showTree();
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(st->removeLayer(m_cue, m_layer));
    r->setLightsFeedback(false);                    // a rebuild
    float pan = 0.0f, tilt = 0.0f;
    r->pendingPosition(m_spots.at(0), pan, tilt);
    r->setLightsFeedback(true);
    r->stop();
    QVERIFY(st->undo());

    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgePosition(0.0f, 5.0f));
    const quint32 gone = m_spots.at(1);
    QVERIFY(m_doc->deleteFixture(gone));
    r->setLightsFeedback(false);
    QVERIFY(!r->pendingWrites().contains(qMakePair(0, int(kChannels))));
    r->setLightsFeedback(true);
    r->stop();
    m_spots.removeAll(gone);
}

/* G16: with the flash off nothing blinks; on, a start, a new layer and
   both bumpers blink, on the flash's timer. */
void RefineSession_Test::theFlashIsGatedAndTimed()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->flashSelection();
    QVERIFY(!r->isFlashing());
    r->flashWarning();
    QVERIFY(!r->isFlashing());
    r->stop();

    r->setFlashEnabled(true);
    QTimer *timer = r->findChild<QTimer *>();
    QVERIFY(timer != nullptr);
    QVERIFY(r->start(m_cue));
    QVERIFY(r->isFlashing());
    QVERIFY(timer->isActive());
    auto quiet = [r]() { r->setHolding(true); r->setHolding(false); };
    quiet();
    QVERIFY(!r->isFlashing());
    r->nextLayer();
    QVERIFY(r->isFlashing());
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    quiet();
    QVERIFY(r->nextPending());
    QVERIFY(r->isFlashing());
    r->setFlashEnabled(false);
    r->stop();
}

/* G18: after stop() nothing of the session is left to read. */
void RefineSession_Test::stopLeavesNothingBehind()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->pendingWrites().isEmpty());
    r->stop();
    QVERIFY(r->layers().isEmpty());
    QCOMPARE(r->layerIndex(), -1);
    QCOMPARE(r->cueId(), invalidId());
    QVERIFY(r->pendingWrites().isEmpty());
}

/* G19: the panel and the pad light follow changed(). */
void RefineSession_Test::everyActionSaysChanged()
{
    quint32 left = 0, right = 0;
    splitIntoTwoGroups(&left, &right);
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy changed(r, &RefineSession::changed);
    auto says = [&changed](const std::function<void()> &action) {
        changed.clear();
        action();
        return changed.count() > 0;
    };
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(says([r]() { r->previousItem(); }));
    QVERIFY(says([r]() { r->nudgeZoom(-1.0f); }));
    QVERIFY(says([r]() { r->nudgeFocus(5); }));
    QVERIFY(says([r]() { r->setKeepToSource(false); }));
    r->setScope(RefineSession::LayerScope);
    QVERIFY(says([r]() { r->nextPending(); }));
    QVERIFY(says([r]() { r->keep(); }));
    // X with nothing there, and at Group scope.
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();                                  // off the one just kept
    QVERIFY(says([r]() { QVERIFY(!r->clearException()); }));
    r->setScope(RefineSession::GroupScope);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(r->keep());
    QVERIFY(says([r]() { QVERIFY(r->clearException()); }));
    r->stop();

    // The GO in Operate.
    m_doc->setMode(Doc::Operate);
    quint32 running = m_cue;
    r->setRunningCueProbe([&running](quint32) { return running; });
    QVERIFY(r->start(m_cue));
    running = invalidId() - 1;
    QVERIFY(says([r]() { r->tick(0.02); }));
    r->stop();
    r->setRunningCueProbe({});
    m_doc->setMode(Doc::Design);
}

/* G20, G21: only what is pending is pending; A with nothing pending keeps
   nothing. */
void RefineSession_Test::pendingIsOnlyWhatIsPending()
{
    ShowTreeController *st = m_doc->showTree();
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy kept(r, &RefineSession::kept);
    QVERIFY(r->start(m_cue));
    const QString undoBefore = st->undoText();
    QVERIFY(!r->keep());
    QCOMPARE(kept.count(), 0);
    QCOMPARE(st->undoText(), undoBefore);

    int focus = 0;
    float deg = 0.0f;
    QVERIFY(!r->pendingFocus(m_spots.at(0), focus));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QVERIFY(!r->pendingZoom(m_spots.at(0), deg));
    QVERIFY(!r->pendingFocus(m_spots.at(0), focus));
    r->revert();
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgeZoom(-1.0f));
    QVERIFY(r->pendingZoom(m_spots.at(1), deg));
    QVERIFY(!r->pendingZoom(m_spots.at(0), deg));
    r->stop();
}

/* G23: both bumpers go to the pending edit's layer and fixture, and the
   rig shows that selection. */
void RefineSession_Test::nextPendingLandsOnTheEdit()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 bright = addDimmerPalette(m_doc, "80", 204);
    QVERIFY(st->setLayerLook(m_cue, m_layer, lookOf(KeyDimmer, { bright })));
    QList<quint32> more;
    Target t2;
    for (int i = 0; i < 3; ++i)
    {
        more << makeSpot(m_doc, QString("X%1").arg(i + 1), 12.0f + i)->id();
        t2.fixtures << more.last();
    }
    const quint32 layer2 = st->addLayer(m_cue, t2, "Specials");
    QVERIFY(st->setLayerLook(m_cue, layer2, lookOf(KeyDimmer, { bright })));

    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->nextLayer();
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    r->nextItem();
    QCOMPARE(r->currentFixture(), more.at(2));
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    r->previousLayer();
    r->setScope(RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), m_spots.at(0));
    QVERIFY(r->nextPending());
    QCOMPARE(r->currentLayer(), layer2);
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), more.at(2));
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    QVERIFY(w.contains(chAt(m_doc, more.at(0), 4)));
    QVERIFY(!w.contains(chAt(m_doc, more.at(2), 4)));
    QVERIFY(!w.contains(chAt(m_doc, m_spots.at(1), 4)));
    r->stop();
}

/* G24: X with no exception drops the pending change off the rig too; undo
   rebases what is pending on the look it restored. */
void RefineSession_Test::theRigFollowsXAndUndo()
{
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgePosition(0.0f, 5.0f));
    QVERIFY(r->pendingWrites().contains(chAt(m_doc, m_spots.at(1), 0)));
    QVERIFY(!r->clearException());
    QVERIFY(!r->hasPending());
    QVERIFY(!r->pendingWrites().contains(chAt(m_doc, m_spots.at(1), 0)));

    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(4.0f, 0.0f));
    QVERIFY(r->keep());
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(sentDegrees(m_doc, r->pendingWrites(), m_spots.at(0), pan, tilt));
    QVERIFY(qAbs(pan - 275.0f) < 0.05f);
    QVERIFY(r->undo());
    QVERIFY(r->hasPending());
    QVERIFY(sentDegrees(m_doc, r->pendingWrites(), m_spots.at(0), pan, tilt));
    QVERIFY2(qAbs(pan - 271.0f) < 0.05f, qPrintable(QString::number(pan)));
    r->stop();
}

/* G27: where at Group scope, and the crew's word for a beam press. */
void RefineSession_Test::groupAndCrewWordsAreSaid()
{
    quint32 left = 0, right = 0;
    splitIntoTwoGroups(&left, &right);
    RefineSession *r = m_doc->programmer()->refineSession();
    QSignalSpy said(r, &RefineSession::announce);
    QVERIFY(r->start(m_cue));
    r->cycleScope();
    QCOMPARE(said.last().at(0).toString(), QString("AT left, group 1 of 2."));
    r->press(ControllerScheme::ZoomWider, true);
    r->press(ControllerScheme::ZoomWider, false);
    QCOMPARE(said.last().at(0).toString(), ControllerScheme::crewWord(ControllerScheme::ZoomWider));
    QVERIFY(!said.last().at(0).toString().isEmpty());
    r->stop();
}

/* G28, G30: with lights feedback off nothing is dimmed, and back on it is;
   an unpatched light is never dimmed. */
void RefineSession_Test::lightsFeedbackOffAndUnpatched()
{
    QVERIFY(m_doc->showTree()->setLayerLook(m_cue, m_layer,
                                            lookOf(KeyDimmer, { addDimmerPalette(m_doc, "80", 204) })));
    m_doc->fixture(m_spots.at(3))->setPatched(false);
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(m_cue));
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->pendingWrites().contains(chAt(m_doc, m_spots.at(1), 4)));
    QVERIFY(!r->pendingWrites().contains(chAt(m_doc, m_spots.at(3), 4)));
    r->setLightsFeedback(false);
    QVERIFY(r->pendingWrites().isEmpty());
    r->setLightsFeedback(true);
    QVERIFY(r->pendingWrites().contains(chAt(m_doc, m_spots.at(1), 4)));
    r->stop();
}

/* G29: a Build scene's fixture exception (its own values) is not moved by
   a nudge of the scene as a whole. */
void RefineSession_Test::aSceneLayerNudgeLeavesAnException()
{
    Scene *s = new Scene(m_doc);
    s->setName("Ballad look");
    s->addFixtureGroup(m_group);
    s->addPalette(m_position);
    QVERIFY(m_doc->addFunction(s));
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->startScene({ s->id() }, s->id()));
    r->setScope(RefineSession::FixtureScope);
    r->nextItem();
    QVERIFY(r->nudgePosition(3.0f, 0.0f));
    QVERIFY(r->keep());
    QVERIFY(s->excludedFixtures(m_position).contains(m_spots.at(1)));
    r->setScope(RefineSession::LayerScope);
    QVERIFY(r->nudgePosition(10.0f, 0.0f));
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(sentDegrees(m_doc, w, m_spots.at(0), pan, tilt));
    QVERIFY(qAbs(pan - 280.0f) < 0.05f);
    QVERIFY(!w.contains(chAt(m_doc, m_spots.at(1), 0)));
    r->revert();
    r->stop();
}

/* G30: a light whose last channel would be past 512 gets nothing there --
   not wrapped into the next universe's first channel. */
void RefineSession_Test::outputStaysInTheUniverse()
{
    ShowTreeController *st = m_doc->showTree();
    Fixture *end = makeSpot(m_doc, "End", 12.0f);
    end->setAddress(UNIVERSE_SIZE - kChannels + 1);  // focus (ch 6) at 512
    QCOMPARE(int(end->address()) + 6, UNIVERSE_SIZE);
    const quint32 cue = st->addCue(st->listOfCue(m_cue)->id, -1, "End");
    Target t;
    t.fixtures << end->id();
    QVERIFY(st->addLayer(cue, t) != invalidId());
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->start(cue));
    QVERIFY(r->nudgeFocus(5));
    const QHash<QPair<int, int>, uchar> w = r->pendingWrites();
    for (auto it = w.constBegin(); it != w.constEnd(); ++it)
        QVERIFY(it.key().second < UNIVERSE_SIZE);
    QVERIFY(!w.contains(qMakePair(0, UNIVERSE_SIZE)));
    r->stop();
}
