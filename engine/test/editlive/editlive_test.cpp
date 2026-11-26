/*
  Q Light Controller Plus - Unit test
  editlive_test.cpp

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
#include <QSettings>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include <QDir>

#include "editlive_test.h"
#include "simmididevice.h"
#include "editlive.h"
#include "surfacepages.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "encoderbank.h"
#include "showtreecontroller.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "qlcfixturedefcache.h"
#include "ioplugincache.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "fixturegroup.h"
#include "showtreemodel.h"
#include "resource_paths.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

const ShowTree::Layer *layerOn(const ShowTree::Cue *c, const Target &t)
{
    if (c == nullptr)
        return nullptr;
    for (const ShowTree::Layer &l : c->layers)
        if (l.target == t)
            return &l;
    return nullptr;
}
}

void EditLive_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void EditLive_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
    m_doc = new Doc(this);
    QDir fx(INTERNAL_FIXTUREDIR);
    fx.setFilter(QDir::Files);
    fx.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(fx));
    QDir dir(QStringLiteral("../iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();

    m_apc = new SimMidiDevice(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc,
                              QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/Akai-APC40-mkII.qxi"));
    QVERIFY2(m_apc->plug(), qPrintable(m_apc->lastError()));
    m_live = m_doc->controlMap()->editLive();
    QVERIFY(m_live != nullptr);
    m_live->setSelectionProbe([this]() { return m_selection; });
    QVERIFY(show());
}

void EditLive_Test::cleanup()
{
    if (m_doc != nullptr)
        for (Function *f : m_doc->functions())
            f->stop(FunctionParent::master());
    tick(3);
    delete m_apc;
    m_apc = nullptr;
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    m_live = nullptr;
    m_chaser = nullptr;
    m_selection.clear();
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
}

/* Two RGB pars and a Sharpy. Cue 1: the pars red at full, the spot at
   Front at full. Cue 2: the pars blue; the spot tracks from cue 1. Cue 3:
   the pars red. */
bool EditLive_Test::show()
{
    QLCFixtureDef *spot = m_doc->fixtureDefCache()->fixtureDef("Clay Paky", "Sharpy");
    QLCFixtureDef *par = m_doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    if (spot == nullptr || par == nullptr)
        return false;
    quint32 address = 0;
    auto fixture = [&](QLCFixtureDef *def, const QString &mode) {
        Fixture *f = new Fixture(m_doc);
        f->setName(def->model());
        f->setFixtureDefinition(def, def->mode(mode));
        f->setAddress(address);
        address += f->channels();
        m_doc->addFixture(f);
        return f->id();
    };
    auto group = [&](const QString &name, const QList<quint32> &fixtures) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        for (quint32 f : fixtures)
            g->assignFixture(f);
        return g->id();
    };
    auto palette = [&](QLCPalette::PaletteType type, const QString &name, const QVariantList &values) {
        QLCPalette *p = new QLCPalette(type, m_doc);
        p->setName(name);
        p->setValues(values);
        m_doc->addPalette(p);
        return p->id();
    };
    m_par1 = fixture(par, "RGB Dimmer");
    m_par2 = fixture(par, "RGB Dimmer");
    m_spot = fixture(spot, "Standard");
    m_pars = group("Pars", { m_par1, m_par2 });
    m_spots = group("Spot", { m_spot });
    m_spot2 = fixture(spot, "Standard");
    m_spot3 = fixture(spot, "Standard");
    m_movers = group("Movers", { m_spot2, m_spot3 });
    m_par1Only = group("Par 1", { m_par1 });
    m_red = palette(QLCPalette::Color, "Red", { QLCPalette::colorToString(Qt::red, Qt::black) });
    m_blue = palette(QLCPalette::Color, "Blue", { QLCPalette::colorToString(Qt::blue, Qt::black) });
    m_full = palette(QLCPalette::Dimmer, "Full", { 255 });
    m_front = palette(QLCPalette::PanTilt, "Front", { 90, 45 });
    m_half = palette(QLCPalette::Dimmer, "Half", { 128 });
    // A fan: the movers 40 degrees apart, and their levels 255 to 127.
    m_fan = palette(QLCPalette::PanTilt, "Fan", { 90, 45 });
    m_doc->palette(m_fan)->setFanningType(QLCPalette::Linear);
    m_doc->palette(m_fan)->setFanningValue(40);
    m_dimFan = palette(QLCPalette::Dimmer, "Dim fan", { 255 });
    m_doc->palette(m_dimFan)->setFanningType(QLCPalette::Linear);
    m_doc->palette(m_dimFan)->setFanningValue(127);

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Edit Live");
    const quint32 list = st->addCueList(prod, "Main");
    auto set = [&](quint32 cue, quint32 g, quint32 p) {
        return st->setTargetLook(cue, groupTarget(g), st->lookForPalette(p));
    };
    m_c1 = st->addCue(list, -1, "Open");
    set(m_c1, m_pars, m_red);
    set(m_c1, m_pars, m_full);
    set(m_c1, m_spots, m_front);
    set(m_c1, m_spots, m_full);
    set(m_c1, m_movers, m_fan);
    set(m_c1, m_movers, m_dimFan);
    // Par 1 again on a layer of its own that sets only its level.
    set(m_c1, m_par1Only, m_half);
    m_c2 = st->addCue(list, 0, "Blue");
    set(m_c2, m_pars, m_blue);
    m_c3 = st->addCue(list, 1, "Red");
    set(m_c3, m_pars, m_red);
    // Cue 4 has two sub-cues: Hit (pars red), Out (pars blue).
    m_c4 = st->addCue(list, 2, "Subs");
    m_s1 = st->addSubCue(m_c4, "Hit");
    m_s2 = st->addSubCue(m_c4, "Out");
    set(m_s1, m_pars, m_red);
    set(m_s1, m_pars, m_full);
    set(m_s2, m_pars, m_blue);
    const CueList *l = st->cueList(list);
    if (l == nullptr)
        return false;
    m_chaser = qobject_cast<Chaser *>(m_doc->function(l->chaser));
    st->setCurrentProduction(prod);
    return m_chaser != nullptr;
}

void EditLive_Test::tick(int n)
{
    if (m_doc == nullptr)
        return;
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

QByteArray EditLive_Test::frame()
{
    tick(3);
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const QByteArray out = ua.isEmpty() ? QByteArray() : ua.first()->preGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

void EditLive_Test::go()
{
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(m_chaser->id());
    ShowControl::Request r;
    r.command = ShowControl::Go;
    r.chaser = m_chaser->id();
    sc->execute(r);
    tick(10);
    QCoreApplication::processEvents();
    if (m_live != nullptr)
        m_live->checkCue();
}

/** The spot's pan channels (coarse, fine) on the universe. */
QList<int> EditLive_Test::spotPan() const
{
    Fixture *f = m_doc->fixture(m_spot);
    QList<int> out;
    for (int cb : { int(QLCChannel::MSB), int(QLCChannel::LSB) })
    {
        const quint32 ch = f->channelNumber(QLCChannel::Pan, cb);
        if (ch != QLCChannel::invalid())
            out << int(f->address() + ch);
    }
    return out;
}

QByteArray EditLive_Test::postFrame()
{
    tick(3);
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    const QByteArray out = ua.isEmpty() ? QByteArray() : *ua.first()->postGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

/** The child layer of cue @p cue's layer on @p group that lights only @p fixture. */
const ShowTree::Layer *EditLive_Test::childOf(quint32 cue, quint32 group, quint32 fixture) const
{
    const ShowTree::Cue *c = m_doc->showTree()->cue(cue);
    const ShowTree::Layer *parent = layerOn(c, groupTarget(group));
    if (parent == nullptr)
        return nullptr;
    for (const ShowTree::Layer &l : c->layers)
        if (l.parent == parent->id && l.target.fixtures == QList<quint32>({ fixture }))
            return &l;
    return nullptr;
}

/** The first value (@p index) of the palette @p layer's @p key look names. */
int EditLive_Test::lookValue(const ShowTree::Layer *layer, const QString &key, int index) const
{
    const Look *look = layer != nullptr ? layer->look(key) : nullptr;
    if (look == nullptr || look->palettes.isEmpty())
        return -1000;
    const QLCPalette *p = m_doc->palette(look->palettes.first());
    if (p == nullptr || p->values().isEmpty())
        return -1000;
    if (p->type() == QLCPalette::Color)
    {
        QColor rgb, wauv;
        QLCPalette::stringToColor(p->values().at(0).toString(), rgb, wauv);
        return index == 0 ? rgb.red() : index == 1 ? rgb.green() : rgb.blue();
    }
    if (p->values().size() <= index)
        return -1000;
    return p->values().at(index).toInt();
}

/** Fader 1 (red) to @p from (picked up there), then to @p to (MIDI 0-127). */
void EditLive_Test::trimRed(int from, int to)
{
    QVERIFY(m_apc->set(QStringLiteral("#7"), from));
    QVERIFY(m_apc->set(QStringLiteral("#7"), to));
}

/*****************************************************************************
 * The tests
 *****************************************************************************/

void EditLive_Test::offNothingHappens()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QCOMPARE(m_live->liveCue(), m_c2);
    m_selection = { m_spot };
    const QByteArray before = frame();
    QSignalSpy turned(m_doc->controlMap()->surfacePages(), &SurfacePages::liveAdjusted);
    QVERIFY(m_apc->set(QStringLiteral("#16"), 10));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 20));
    QCOMPARE(turned.size(), 0);
    QVERIFY(!m_live->hasTrims());
    QCOMPARE(frame(), before);
    // Turned straight from the code, off: refused too.
    QVERIFY(!m_live->adjust(EncoderBank::PositionBank, 0, 5, false));
    QCOMPARE(frame(), before);
}

void EditLive_Test::onlyInOperate()
{
    QString why;
    QVERIFY(!m_live->setOn(true, &why));
    QVERIFY(!why.isEmpty());
    QVERIFY(!m_live->isOn());
    QVERIFY(!m_doc->controlMap()->surfacePages()->isLive());
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_live->setOn(true, &why));
    QVERIFY(m_doc->controlMap()->surfacePages()->isLive());
    // The board's page has its knobs again (the show does not hear them).
    QVERIFY(m_doc->inputOutputMap()->isPageControl(0, 16));
}

void EditLive_Test::aTrimMovesOnlyThatAttribute()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    QCOMPARE(m_live->cue(), m_c2);
    m_selection = { m_spot };
    const QByteArray before = frame();
    const QList<int> pan = spotPan();
    QVERIFY(!pan.isEmpty());

    // Device knob 1 on the Move page is pan: the first move after Edit Live
    // went on is no step, the next four are.
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(!m_live->hasTrims());
    QVERIFY(m_apc->set(QStringLiteral("#16"), 64));
    QVERIFY(m_live->hasTrims());
    const QByteArray after = frame();
    QCOMPARE(after.size(), before.size());
    bool panMoved = false;
    for (int i = 0; i < before.size(); ++i)
    {
        if (pan.contains(i))
            panMoved = panMoved || after.at(i) != before.at(i);
        else
            QVERIFY2(after.at(i) == before.at(i), qPrintable(QStringLiteral("channel %1 moved").arg(i)));
    }
    QVERIFY(panMoved);
    const QList<EditLive::Trim> trims = m_live->trims();
    QCOMPARE(trims.size(), 1);
    QCOMPARE(trims.first().fixture, m_spot);
    QCOMPARE(trims.first().bank, int(EncoderBank::PositionBank));
    QCOMPARE(trims.first().slot, 0);
    QVERIFY(trims.first().to > trims.first().from);
    QVERIFY(m_live->trimText(trims.first()).contains(QStringLiteral("Pan")));
    // The show is not touched.
    const ShowTree::Layer *s1 = layerOn(m_doc->showTree()->cue(m_c1), groupTarget(m_spots));
    QVERIFY(s1 != nullptr);
    QCOMPARE(s1->look(KeyPosition)->palettes, QList<quint32>({ m_front }));
}

void EditLive_Test::storeWritesOnlyTheTouchedAttributeIntoTheTrackingSource()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 70));
    QVERIFY(m_live->hasTrims());
    const QByteArray trimmed = frame();

    ShowTreeController *st = m_doc->showTree();
    const QList<quint32> parsLooks1 = layerOn(st->cue(m_c1), groupTarget(m_pars))->look(KeyColour)->palettes;
    QSignalSpy stored(m_live, &EditLive::stored);
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    QCOMPARE(stored.size(), 1);
    // The note names the look and the cue it was set in.
    QVERIFY(m_live->lastNote().contains(QStringLiteral("Position")));
    QVERIFY(m_live->lastNote().contains(QStringLiteral("Open")));
    QVERIFY(!m_live->hasTrims());

    // Into cue 1's spot layer, where the position cue 2 plays is set.
    const ShowTree::Layer *s1 = layerOn(st->cue(m_c1), groupTarget(m_spots));
    QVERIFY(s1 != nullptr);
    const Look *pos = s1->look(KeyPosition);
    QVERIFY(pos != nullptr);
    QCOMPARE(pos->palettes.size(), 1);
    QVERIFY(pos->palettes.first() != m_front);
    const QLCPalette *np = m_doc->palette(pos->palettes.first());
    QVERIFY(np != nullptr);
    QCOMPARE(np->type(), QLCPalette::PanTilt);
    QVERIFY(np->values().at(0).toInt() > 90);
    QCOMPARE(np->values().at(1).toInt(), 45);       // tilt as it was
    // Nothing else changed: its dimmer, the pars, cue 2 still tracks.
    QCOMPARE(s1->look(KeyDimmer)->palettes, QList<quint32>({ m_full }));
    QCOMPARE(layerOn(st->cue(m_c1), groupTarget(m_pars))->look(KeyColour)->palettes, parsLooks1);
    QVERIFY(layerOn(st->cue(m_c2), groupTarget(m_spots)) == nullptr);
    // Still on the rig until the cue changes (cue 2 plays it from its next GO).
    QCOMPARE(frame(), trimmed);

    // One undo step.
    QVERIFY(st->undo());
    QCOMPARE(layerOn(st->cue(m_c1), groupTarget(m_spots))->look(KeyPosition)->palettes,
             QList<quint32>({ m_front }));
}

void EditLive_Test::aFaderPicksUpThenTrims()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_par1, m_par2 };
    Fixture *p1 = m_doc->fixture(m_par1);
    const int red = int(p1->address() + p1->channelNumber(QLCChannel::Red, QLCChannel::MSB));
    const int blue = int(p1->address() + p1->channelNumber(QLCChannel::Blue, QLCChannel::MSB));
    const QByteArray before = frame();
    QCOMPARE(uchar(before.at(red)), uchar(0));
    QCOMPARE(uchar(before.at(blue)), uchar(255));

    // Fader 1 is red: far up, it waits for the red on stage (none).
    QVERIFY(m_apc->set(QStringLiteral("#7"), 100));
    QVERIFY(!m_live->hasTrims());
    QCOMPARE(frame(), before);
    // Down through it: picked up, then it sets red on both pars.
    QVERIFY(m_apc->set(QStringLiteral("#7"), 0));
    QVERIFY(m_apc->set(QStringLiteral("#7"), 64));
    QVERIFY(m_live->hasTrims());
    const QByteArray after = frame();
    QVERIFY(qAbs(int(uchar(after.at(red))) - 128) <= 3);
    Fixture *p2 = m_doc->fixture(m_par2);
    QCOMPARE(after.at(int(p2->address() + p2->channelNumber(QLCChannel::Red, QLCChannel::MSB))), after.at(red));
    QCOMPARE(uchar(after.at(blue)), uchar(255));

    // Stored into cue 2's own pars layer (it sets their colour itself).
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    ShowTreeController *st = m_doc->showTree();
    const Look *c2 = layerOn(st->cue(m_c2), groupTarget(m_pars))->look(KeyColour);
    QVERIFY(c2->palettes.first() != m_blue);
    QColor rgb, wauv;
    QVERIFY(QLCPalette::stringToColor(m_doc->palette(c2->palettes.first())->values().at(0).toString(), rgb, wauv));
    QVERIFY(qAbs(rgb.red() - 128) <= 3);
    QCOMPARE(rgb.blue(), 255);
    QCOMPARE(layerOn(st->cue(m_c1), groupTarget(m_pars))->look(KeyColour)->palettes, QList<quint32>({ m_red }));
}

void EditLive_Test::dropPutsTheCueBack()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    const QByteArray before = frame();
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 66));
    QVERIFY(frame() != before);
    m_live->drop();
    QVERIFY(!m_live->hasTrims());
    QCOMPARE(frame(), before);
}

void EditLive_Test::goDropsTheTrimsAndTheNoticeStoresToTheRightCue()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    const QByteArray untrimmed = frame();
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 68));
    QVERIFY(m_live->hasTrims());
    QSignalSpy left(m_live, &EditLive::leftBehind);

    // GO is not held: cue 3 plays, the trims went with cue 2.
    go();
    QCOMPARE(m_live->liveCue(), m_c3);
    QCOMPARE(left.size(), 1);
    QCOMPARE(left.first().at(0).toUInt(), m_c2);
    QVERIFY(!m_live->hasTrims());
    QVERIFY(m_live->hasLeftBehind());
    QCOMPARE(m_live->leftBehindCue(), m_c2);
    QCOMPARE(m_live->leftBehindCount(), 1);
    // The spot is back where cue 3 has it (Front, tracked from cue 1).
    const QByteArray now = frame();
    for (int ch : spotPan())
        QCOMPARE(now.at(ch), untrimmed.at(ch));

    // Store to Cue 2: into the look that set it there, cue 1's spot layer.
    QString why;
    QVERIFY2(m_live->storeLeftBehind(&why), qPrintable(why));
    QVERIFY(!m_live->hasLeftBehind());
    ShowTreeController *st = m_doc->showTree();
    const ShowTree::Layer *s1 = layerOn(st->cue(m_c1), groupTarget(m_spots));
    QVERIFY(s1->look(KeyPosition)->palettes.first() != m_front);
    QCOMPARE(s1->look(KeyDimmer)->palettes, QList<quint32>({ m_full }));
    QVERIFY(layerOn(st->cue(m_c2), groupTarget(m_spots)) == nullptr);
    QVERIFY(layerOn(st->cue(m_c3), groupTarget(m_spots)) == nullptr);
}

void EditLive_Test::theEmergencyStopClearsIt()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 70));
    QVERIFY(m_live->hasTrims());
    m_doc->outputRuleKeeper()->emergencyStop();
    QVERIFY(!m_live->hasTrims());
    QVERIFY(!m_live->isOn());
    QVERIFY(!m_doc->controlMap()->surfacePages()->isLive());
    // Nothing of it on the output: the knob turns nothing now.
    QVERIFY(m_apc->set(QStringLiteral("#16"), 80));
    QVERIFY(!m_live->hasTrims());
}

void EditLive_Test::designTurnsItOff()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 70));
    m_doc->setMode(Doc::Design);
    QVERIFY(!m_live->isOn());
    QVERIFY(!m_live->hasTrims());
    QVERIFY(!m_doc->controlMap()->surfacePages()->isLive());
}

/*****************************************************************************
 * The review of d01a79bfb: each of these fails there
 *****************************************************************************/

void EditLive_Test::aPartOfALayerGetsItsOwnException()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_par1 };
    trimRed(0, 64);
    QVERIFY(m_live->hasTrims());
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(layerOn(st->cue(m_c2), groupTarget(m_pars))->look(KeyColour)->palettes, QList<quint32>({ m_blue }));
    const ShowTree::Layer *child = childOf(m_c2, m_pars, m_par1);
    QVERIFY(child != nullptr);
    QVERIFY(qAbs(lookValue(child, KeyColour, 0) - 128) <= 3);
    QCOMPARE(lookValue(child, KeyColour, 2), 255);
}

void EditLive_Test::anEvenMoveOfAFanKeepsTheFan()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot2, m_spot3 };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 64));
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    const ShowTree::Layer *movers = layerOn(m_doc->showTree()->cue(m_c1), groupTarget(m_movers));
    const QLCPalette *p = m_doc->palette(movers->look(KeyPosition)->palettes.first());
    QVERIFY(p->id() != m_fan);
    QCOMPARE(p->fanningType(), QLCPalette::Linear);
    QCOMPARE(p->fanningValue().toInt(), 40);
    QVERIFY(p->values().at(0).toInt() > 90);
    QVERIFY(childOf(m_c1, m_movers, m_spot3) == nullptr);
}

void EditLive_Test::anUnevenMoveOfAFanStoresEachHead()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    // Both a little, then the first one more: no one offset fits both.
    m_selection = { m_spot2, m_spot3 };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 62));
    m_selection = { m_spot2 };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 70));
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    const ShowTree::Layer *movers = layerOn(m_doc->showTree()->cue(m_c1), groupTarget(m_movers));
    QCOMPARE(movers->look(KeyPosition)->palettes, QList<quint32>({ m_fan }));
    const int pan2 = lookValue(childOf(m_c1, m_movers, m_spot2), KeyPosition, 0);
    const int pan3 = lookValue(childOf(m_c1, m_movers, m_spot3), KeyPosition, 0);
    QVERIFY2(pan2 > 90, qPrintable(QString::number(pan2)));
    // The second head keeps its place in the fan (40 degrees on).
    QVERIFY2(pan3 >= 125, qPrintable(QString::number(pan3)));
}

void EditLive_Test::oneHeadOfAFanStoresWhereItIs()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot3 };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 64));
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    const int pan = lookValue(childOf(m_c1, m_movers, m_spot3), KeyPosition, 0);
    // Where it was in the fan (130) and the trim, not the fan's first value.
    QVERIFY2(pan >= 125, qPrintable(QString::number(pan)));
    QVERIFY(childOf(m_c1, m_movers, m_spot2) == nullptr);
}

void EditLive_Test::mixedLevelsStoreEachHead()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot2, m_spot3 };
    QVERIFY(m_live->adjust(EncoderBank::IntensityBank, 0, -5, false));
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    const int l2 = lookValue(childOf(m_c1, m_movers, m_spot2), KeyDimmer);
    const int l3 = lookValue(childOf(m_c1, m_movers, m_spot3), KeyDimmer);
    QVERIFY2(l2 > 0 && l2 < 255, qPrintable(QString::number(l2)));
    QVERIFY2(l3 > 0 && l2 - l3 > 100, qPrintable(QStringLiteral("%1 %2").arg(l2).arg(l3)));
}

void EditLive_Test::theLayerThatSetsTheKeyIsTheOneStored()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_par1 };
    trimRed(127, 64);
    QVERIFY(m_live->hasTrims());
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    ShowTreeController *st = m_doc->showTree();
    // Not onto the Par 1 layer, which sets only its level.
    QVERIFY(layerOn(st->cue(m_c1), groupTarget(m_par1Only))->look(KeyColour) == nullptr);
    QVERIFY(qAbs(lookValue(childOf(m_c1, m_pars, m_par1), KeyColour, 0) - 128) <= 3);
}

void EditLive_Test::theListMasterScalesTheTrimsAndNotTheStore()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    m_chaser->adjustAttribute(0.5, Function::Intensity);
    tick(5);
    QVERIFY(m_live->setOn(true));
    QCOMPARE(m_live->listIntensity(), 0.5);
    m_selection = { m_par1, m_par2 };
    Fixture *p1 = m_doc->fixture(m_par1);
    const int red = int(p1->address() + p1->channelNumber(QLCChannel::Red, QLCChannel::MSB));
    trimRed(0, 64);
    // Never brighter than the cue would play it: half of the trim.
    QVERIFY2(qAbs(int(uchar(frame().at(red))) - 64) <= 3, qPrintable(QString::number(uchar(frame().at(red)))));
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    // The look keeps the look's level, not the output's.
    QVERIFY(qAbs(lookValue(layerOn(m_doc->showTree()->cue(m_c2), groupTarget(m_pars)), KeyColour, 0) - 128) <= 3);
    // The master up again: the held trim follows it.
    m_chaser->adjustAttribute(1.0, Function::Intensity);
    QTest::qWait(300);
    QVERIFY(qAbs(int(uchar(frame().at(red))) - 128) <= 3);
}

void EditLive_Test::theGrandMasterStillAppliesOverTrims()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_par1 };
    Fixture *p1 = m_doc->fixture(m_par1);
    const int red = int(p1->address() + p1->channelNumber(QLCChannel::Red, QLCChannel::MSB));
    trimRed(0, 64);
    QVERIFY(qAbs(int(uchar(postFrame().at(red))) - 128) <= 3);
    m_doc->inputOutputMap()->setGrandMasterValue(127);
    QVERIFY(qAbs(int(uchar(postFrame().at(red))) - 64) <= 3);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
}

void EditLive_Test::stopAllDropsTheTrims()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_par1 };
    trimRed(0, 64);
    QVERIFY(m_live->hasTrims());
    // Stop All, as the tests can (MasterTimer::stopAllFunctions() waits on a
    // timer thread these tests don't run): every function stopped.
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    m_live->checkCue();
    QVERIFY(!m_live->hasTrims());
    QVERIFY(m_live->hasLeftBehind());
    Fixture *p1 = m_doc->fixture(m_par1);
    QCOMPARE(uchar(frame().at(int(p1->address() + p1->channelNumber(QLCChannel::Red, QLCChannel::MSB)))), uchar(0));
}

void EditLive_Test::closingTheShowTurnsItOff()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 66));
    QVERIFY(m_live->hasTrims());
    m_doc->clearContents();
    QVERIFY(!m_live->isOn());
    QVERIFY(!m_live->hasTrims());
    QVERIFY(!m_doc->controlMap()->surfacePages()->isLive());
}

void EditLive_Test::undoOfAStoreLetsGoOfTheStage()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    const QByteArray before = frame();
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 68));
    const QByteArray trimmed = frame();
    QVERIFY(trimmed != before);
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    QVERIFY(m_live->isHolding());
    QCOMPARE(frame(), trimmed);
    QVERIFY(m_doc->showTree()->undo());
    QCoreApplication::processEvents();
    QVERIFY(!m_live->isHolding());
    QCOMPARE(frame(), before);
}

void EditLive_Test::dropAfterStoreLetsGoOfTheStage()
{
    m_doc->setMode(Doc::Operate);
    go();
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    const QByteArray before = frame();
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 68));
    QString why;
    QVERIFY2(m_live->store(&why), qPrintable(why));
    QVERIFY(!m_live->hasTrims());
    QVERIFY(m_live->canDrop());
    m_live->drop();
    QVERIFY(!m_live->isHolding());
    QCOMPARE(frame(), before);
}

void EditLive_Test::thePmjsFadersNeverTrim()
{
    const QString pmjName = QStringLiteral("PMJ Black 1");
    SimMidiDevice pmj(m_doc, m_stub, pmjName, 1, pmjName,
                      QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    pages->setOverlaid(pmjName);        // the PMJ overlay (UI) reads its faders
    int fix = -1;
    for (int i = 0; i < pages->pageSet(1).pages().size(); ++i)
        if (pages->pageSet(1).pages().at(i).name == QLatin1String("Fix Cont"))
            fix = i;
    QVERIFY(fix >= 0);
    pages->setPage(1, fix);
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_par1 };
    QSignalSpy level(pages, &SurfacePages::liveLevelSet);
    QVERIFY(pmj.set(QStringLiteral("Ch 1"), 0));
    QVERIFY(pmj.set(QStringLiteral("Ch 1"), 100));
    QCOMPARE(level.size(), 0);
    QVERIFY(!m_live->hasTrims());
}

void EditLive_Test::aSubCueGoDropsTheTrims()
{
    m_doc->setMode(Doc::Operate);
    for (int i = 0; i < 4; ++i)
        go();
    QCOMPARE(m_live->liveCue(), m_s1);
    QVERIFY(m_live->setOn(true));
    QCOMPARE(m_live->cue(), m_s1);
    m_selection = { m_par1 };
    trimRed(127, 64);
    QVERIFY(m_live->hasTrims());
    ShowControl::Request r;
    r.command = ShowControl::SubGo;
    r.chaser = m_chaser->id();
    m_doc->showControl()->execute(r);
    tick(10);
    QCoreApplication::processEvents();
    m_live->checkCue();
    QCOMPARE(m_live->liveCue(), m_s2);
    QVERIFY(!m_live->hasTrims());
    QCOMPARE(m_live->leftBehindCue(), m_s1);
}

void EditLive_Test::anAttributeMovedBackIsNotPending()
{
    m_doc->setMode(Doc::Operate);
    go();
    QVERIFY(m_live->setOn(true));
    m_selection = { m_spot };
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#16"), 64));
    QVERIFY(m_apc->set(QStringLiteral("#20"), 60));
    QVERIFY(m_apc->set(QStringLiteral("#20"), 64));
    QCOMPARE(m_live->trims().size(), 2);
    // Pan back where it was: only tilt is pending.
    QVERIFY(m_apc->set(QStringLiteral("#16"), 60));
    const QList<EditLive::Trim> trims = m_live->trims();
    QCOMPARE(trims.size(), 1);
    QCOMPARE(trims.first().slot, 1);
}

void EditLive_Test::aRefusedControlStaysTheShows()
{
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_live->setOn(true));
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->isPageControl(0, 16));          // pan: Edit Live's
    QVERIFY(!iom->isPageControl(0, 22));         // gobo: still the show's
}

QTEST_MAIN(EditLive_Test)
