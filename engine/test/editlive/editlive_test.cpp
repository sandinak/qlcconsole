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
#include "mastertimer.h"
#include "resource_paths.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "universe.h"
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

const Layer *layerOn(const Cue *c, const Target &t)
{
    if (c == nullptr)
        return nullptr;
    for (const Layer &l : c->layers)
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
    m_red = palette(QLCPalette::Color, "Red", { QLCPalette::colorToString(Qt::red, Qt::black) });
    m_blue = palette(QLCPalette::Color, "Blue", { QLCPalette::colorToString(Qt::blue, Qt::black) });
    m_full = palette(QLCPalette::Dimmer, "Full", { 255 });
    m_front = palette(QLCPalette::PanTilt, "Front", { 90, 45 });

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
    m_c2 = st->addCue(list, 0, "Blue");
    set(m_c2, m_pars, m_blue);
    m_c3 = st->addCue(list, 1, "Red");
    set(m_c3, m_pars, m_red);
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
    const Layer *s1 = layerOn(m_doc->showTree()->cue(m_c1), groupTarget(m_spots));
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
    const Layer *s1 = layerOn(st->cue(m_c1), groupTarget(m_spots));
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
    const Layer *s1 = layerOn(st->cue(m_c1), groupTarget(m_spots));
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

QTEST_GUILESS_MAIN(EditLive_Test)
