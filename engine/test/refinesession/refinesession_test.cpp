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
