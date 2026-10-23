/*
  Q Light Controller Plus - Test Unit
  itemicons_test.cpp

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
#include <QXmlStreamReader>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QSplitter>
#include <QSettings>
#include <QImage>
#include <QPainter>
#include <QFile>

#define protected public
#define private public
#include "programmingmanager.h"
#include "programmingoutline.h"
#include "functionstreewidget.h"
#include "showtreeview.h"
#include "fixturetreewidget.h"
#include "fixturegroupsource.h"
#include "monitor/monitorlayerspanel.h"
#include "monitor/monitorinventorypanel.h"
#undef private
#undef protected

#include "qlcfixturedefcache.h"
#include "monitorproperties.h"
#include "showtreecontroller.h"
#include "showembedding.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "scenery.h"
#include "itemicons.h"
#include "fixture.h"
#include "scene.h"
#include "app.h"
#include "doc.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
#include "monitor/fixturevisualtraits.h"
#include <QSignalSpy>

#include "itemicons_test.h"

using namespace ItemIcons;

void ItemIcons_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
}

void ItemIcons_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

/* A fixture of a made-up definition: @p channels as (group, preset) pairs,
   preset -1 for none. A Shutter channel gets closed / open / strobe
   capabilities at 0-9, 10-19 and 20-255. */
Fixture *ItemIcons_Test::makeFixture(int type, const QString &model,
                                     const QList<QPair<int, int>> &channels)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(model);
    def->setType(QLCFixtureDef::FixtureType(type));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("All"));
    quint32 i = 0;
    for (const auto &c : channels)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QStringLiteral("ch%1").arg(i));
        if (c.second >= 0)
            ch->setPreset(QLCChannel::Preset(c.second));
        ch->setGroup(QLCChannel::Group(c.first));
        if (c.first == QLCChannel::Shutter && c.second < 0)
        {
            // -1: closed / open / strobe; -2: closed / strobe (no open).
            QLCCapability *closed = new QLCCapability(0, 9, QStringLiteral("Closed"));
            closed->setPreset(QLCCapability::ShutterClose);
            ch->addCapability(closed);
            if (c.second == -1)
            {
                QLCCapability *open = new QLCCapability(10, 19, QStringLiteral("Open"));
                open->setPreset(QLCCapability::ShutterOpen);
                ch->addCapability(open);
            }
            QLCCapability *strobe = new QLCCapability(c.second == -1 ? 20 : 10, 255,
                                                      QStringLiteral("Strobe slow to fast"));
            strobe->setPreset(QLCCapability::StrobeSlowToFast);
            ch->addCapability(strobe);
        }
        def->addChannel(ch);
        mode->insertChannel(ch, i++);
    }
    QLCFixtureHead head;
    for (quint32 c = 0; c < i; ++c)
        head.addChannel(c);
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    m_doc->fixtureDefCache()->addFixtureDef(def);

    Fixture *f = new Fixture(m_doc);
    f->setName(model);
    f->setFixtureDefinition(def, mode);
    f->setAddress(m_doc->fixtures().size() * 32);
    m_doc->addFixture(f);
    return f;
}

Fixture *ItemIcons_Test::makeCurtain(int type, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(name);
    def->setType(QLCFixtureDef::FixtureType(type));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("Open"));
    QLCChannel *open = new QLCChannel();
    open->setName(QStringLiteral("Open"));
    open->setGroup(QLCChannel::Intensity);
    def->addChannel(open);
    mode->insertChannel(open, 0);
    QLCPhysical phys;
    phys.setEmission(QLCPhysical::NoEmission);
    mode->setPhysical(phys);
    def->addMode(mode);
    m_doc->fixtureDefCache()->addFixtureDef(def);

    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(def, mode);
    f->setAddress(m_doc->fixtures().size() * 32);
    m_doc->addFixture(f);
    return f;
}

static bool isLight(QRgb px)
{
    // The icons' light (255, 204, 51), fully opaque.
    return qAlpha(px) > 240 && qAbs(qRed(px) - 255) < 12 && qAbs(qGreen(px) - 204) < 14
           && qAbs(qBlue(px) - 51) < 20;
}

static int lightPixels(const QIcon &icon, int side = 64, qreal dpr = 1.0)
{
    const QImage img = icon.pixmap(QSize(side, side), dpr).toImage();
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isLight(img.pixel(x, y)))
                ++n;
    return n;
}

static int yellowPixels(const QIcon &icon, int side = 64)
{
    // Any of the light, the beam's fade included.
    const QImage img = icon.pixmap(QSize(side, side), 1.0).toImage();
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb px = img.pixel(x, y);
            if (qAlpha(px) > 100 && qRed(px) > 200 && qGreen(px) > 150 && qBlue(px) < 120)
                ++n;
        }
    return n;
}

void ItemIcons_Test::thePieIsTheLevel()
{
    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer);
    dim->setValue(115);                 // 45 %
    m_doc->addPalette(dim);
    QCOMPARE(intensityPercent(dim), 45);
    const QIcon icon = paletteIcon(m_doc, dim);
    QCOMPARE(icon.name(), QString("pie:45"));
    // Drawn per value, the same one each time it is asked for.
    QCOMPARE(intensityIcon(45).name(), icon.name());
    QVERIFY(intensityIcon(46).name() != icon.name());

    const int full = lightPixels(intensityIcon(100));
    const int part = lightPixels(intensityIcon(45));
    const int quarter = lightPixels(intensityIcon(25));
    QVERIFY(full > 1000);
    QCOMPARE(lightPixels(intensityIcon(0)), 0);
    const qreal share = qreal(part) / full, q = qreal(quarter) / full;
    QVERIFY2(qAbs(share - 0.45) < 0.05, qPrintable(QString::number(share)));
    QVERIFY2(qAbs(q - 0.25) < 0.05, qPrintable(QString::number(q)));
}

void ItemIcons_Test::aShutterLookShowsItsState()
{
    QLCPalette *sh = new QLCPalette(QLCPalette::Shutter);
    sh->setValue(5);
    m_doc->addPalette(sh);
    QCOMPARE(shutterState(m_doc, sh), ShutterOpen);       // no shutter: nothing to say

    // A shutter that cannot open: the look's value is what it gets.
    Fixture *noOpen = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("No open"),
                                  { { QLCChannel::Intensity, QLCChannel::IntensityRed },
                                    { QLCChannel::Shutter, -2 } });
    QCOMPARE(shutterState(m_doc, sh), ShutterClosed);
    QCOMPARE(paletteIcon(m_doc, sh).name(), shutterIcon(ShutterClosed).name());
    sh->setValue(100);
    QCOMPARE(shutterState(m_doc, sh), ShutterStrobe);
    sh->setValue(0);                                      // the engine's 255
    QCOMPARE(shutterState(m_doc, sh), ShutterStrobe);

    /* One with an Open capability. The engine used to send that whatever
       the look said, so a Closed look showed open (and was); a Closed look
       now closes it and shows closed (QLCPalette::shutterIntent()). */
    QVERIFY(m_doc->deleteFixture(noOpen->id()));
    makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par"),
                { { QLCChannel::Intensity, QLCChannel::IntensityRed },
                  { QLCChannel::Shutter, -1 } });
    sh->setValue(5);
    QCOMPARE(shutterState(m_doc, sh), ShutterClosed);
    QCOMPARE(paletteIcon(m_doc, sh).name(), shutterIcon(ShutterClosed).name());
    sh->setValue(15);
    QCOMPARE(shutterState(m_doc, sh), ShutterOpen);
    QCOMPARE(paletteIcon(m_doc, sh).name(), shutterIcon(ShutterOpen).name());
    sh->setValue(100);
    QCOMPARE(shutterState(m_doc, sh), ShutterStrobe);
    sh->setValue(0);                                      // a look made before Closed: open
    QCOMPARE(shutterState(m_doc, sh), ShutterOpen);

    QVERIFY(shutterIcon(ShutterOpen).name() != shutterIcon(ShutterClosed).name());
    // Open shows light through the gate; closed shows none.
    QVERIFY(lightPixels(shutterIcon(ShutterOpen)) > 500);
    QCOMPARE(lightPixels(shutterIcon(ShutterClosed)), 0);

    // A strobe look: a flash, its ring lit by its rate.
    QLCPalette *st = new QLCPalette(QLCPalette::Strobe);
    st->setValue(0.25f);
    m_doc->addPalette(st);
    QCOMPARE(shutterState(m_doc, st), ShutterStrobe);
    QCOMPARE(paletteIcon(m_doc, st).name(), QString("shutter:%1:25").arg(int(ShutterStrobe)));
    QVERIFY(lightPixels(shutterIcon(ShutterStrobe, 90)) > lightPixels(shutterIcon(ShutterStrobe, 25)));
    QVERIFY(!paletteIcon(m_doc, st).isNull());     // it had no icon at all before
}

void ItemIcons_Test::aTargetShowsItsKind()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *point = mp->addStageTarget();
    StageTarget *line = mp->addStageTarget();
    line->setKind(StageTarget::Kind::Line);
    QCOMPARE(targetIcon(point).name(), QString("target:point"));
    QCOMPARE(targetIcon(line).name(), QString("target:line"));
    QCOMPARE(targetIcon(nullptr).name(), QString("target:point"));

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim);
    m_doc->addPalette(aim);
    aim->setStageTargetId(line->id());
    QCOMPARE(paletteIcon(m_doc, aim).name(), QString("target:line"));
    aim->setStageTargetId(point->id());
    QCOMPARE(paletteIcon(m_doc, aim).name(), QString("target:point"));
    // The point target is yellow.
    QVERIFY(lightPixels(pointTargetIcon()) > 600);
    QVERIFY(lightPixels(lineTargetIcon()) > 200);
}

void ItemIcons_Test::aFixtureShowsWhatItIs()
{
    const int pan = QLCChannel::Pan, tilt = QLCChannel::Tilt, none = -1;
    Fixture *par = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par"),
                               { { QLCChannel::Intensity, QLCChannel::IntensityRed },
                                 { QLCChannel::Intensity, QLCChannel::IntensityGreen } });
    Fixture *spot = makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("Spot"),
                                { { pan, none }, { tilt, none }, { QLCChannel::Gobo, none },
                                  { QLCChannel::Beam, QLCChannel::BeamZoomSmallBig } });
    Fixture *prism = makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("Prism"),
                                 { { pan, none }, { tilt, none }, { QLCChannel::Prism, none } });
    Fixture *wash = makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("Wash"),
                                { { pan, none }, { tilt, none },
                                  { QLCChannel::Beam, QLCChannel::BeamZoomSmallBig } });
    Fixture *ledWash = makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("LED wash"),
                                   { { pan, none }, { tilt, none },
                                     { QLCChannel::Intensity, QLCChannel::IntensityRed } });
    Fixture *beam = makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("Beam"),
                                { { pan, none }, { tilt, none }, { QLCChannel::Colour, none } });
    Fixture *bar = makeFixture(QLCFixtureDef::LEDBarPixels, QStringLiteral("Bar"),
                               { { QLCChannel::Intensity, QLCChannel::IntensityRed } });
    Fixture *dimmer = makeFixture(QLCFixtureDef::Dimmer, QStringLiteral("Dimmer"),
                                  { { QLCChannel::Intensity, QLCChannel::IntensityDimmer } });

    QCOMPARE(fixtureKind(par), FixedWash);
    QCOMPARE(fixtureKind(spot), MovingSpot);
    QCOMPARE(fixtureKind(prism), MovingSpot);
    QCOMPARE(fixtureKind(wash), MovingWash);
    QCOMPARE(fixtureKind(ledWash), MovingWash);
    QCOMPARE(fixtureKind(beam), MovingHead);
    QCOMPARE(fixtureKind(bar), OtherKind);
    QCOMPARE(fixtureKind(dimmer), OtherKind);

    QCOMPARE(fixtureIcon(par).name(), QString("kind:%1:0").arg(int(FixedWash)));
    QCOMPARE(fixtureIcon(spot).name(), QString("kind:%1:0").arg(int(MovingSpot)));
    QCOMPARE(fixtureIcon(wash).name(), QString("kind:%1:0").arg(int(MovingWash)));
    QSet<QString> names;
    for (FixtureKind k : { FixedWash, MovingSpot, MovingWash, MovingHead })
        names << kindIcon(k).name() << kindIcon(k, true).name();
    QCOMPARE(names.size(), 8);
    // Anything else keeps its definition type's icon.
    QVERIFY(!fixtureIcon(bar).isNull());
    QVERIFY(fixtureIcon(bar).name().isEmpty());
    QVERIFY(!fixtureIcon(dimmer).isNull());

    // A spot's beam is narrower than a wash's: less light drawn.
    QVERIFY(yellowPixels(kindIcon(MovingSpot)) < yellowPixels(kindIcon(MovingHead)));
    QVERIFY(yellowPixels(kindIcon(MovingHead)) < yellowPixels(kindIcon(MovingWash)));
}

void ItemIcons_Test::aStageLookKeepsTheOutputRulesBadge()
{
    Fixture *dimmer = makeFixture(QLCFixtureDef::Dimmer, QStringLiteral("Relay"),
                                  { { QLCChannel::Intensity, QLCChannel::IntensityDimmer } });
    dimmer->setStageLook(Fixture::FresnelLook);
    const QImage plain = fixtureIcon(dimmer).pixmap(32, 32).toImage();
    QVERIFY(!plain.isNull());

    // The look sets the picture; the rules still badge it.
    dimmer->setOutputRules(Fixture::ExemptBlackout | Fixture::Switch);
    const QImage badged = fixtureIcon(dimmer).pixmap(32, 32).toImage();
    QVERIFY(!badged.isNull());
    QVERIFY(badged != plain);

    dimmer->setOutputRules(Fixture::OutputRules());
    QCOMPARE(fixtureIcon(dimmer).pixmap(32, 32).toImage(), plain);
}

static QImage iconImage(const QIcon &icon)
{
    return icon.pixmap(32, 32).toImage();
}

void ItemIcons_Test::anAutoFixtureWearsItsClosestLookWhenTheShowSaysSo()
{
    const int none = -1;
    Fixture *par = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par"),
                               { { QLCChannel::Intensity, QLCChannel::IntensityRed },
                                 { QLCChannel::Intensity, QLCChannel::IntensityGreen },
                                 { QLCChannel::Intensity, QLCChannel::IntensityBlue } });
    Fixture *spot = makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("Spot"),
                                { { QLCChannel::Pan, none }, { QLCChannel::Tilt, none },
                                  { QLCChannel::Gobo, none } });
    QCOMPARE(par->autoStageLook(), Fixture::LedParLook);
    QCOMPARE(spot->autoStageLook(), Fixture::MovingSpotLook);
    MonitorProperties *props = m_doc->monitorProperties();
    QVERIFY(!props->autoStageLooks());

    // Off: today's icons, exactly -- the kind icons.
    QCOMPARE(fixtureIcon(par).name(), QString("kind:%1:0").arg(int(FixedWash)));
    QCOMPARE(fixtureIcon(spot).name(), QString("kind:%1:0").arg(int(MovingSpot)));
    const QImage parToday = iconImage(fixtureIcon(par));

    // What the closest look's icon is, set explicitly.
    par->setStageLook(Fixture::LedParLook);
    const QImage parLook = iconImage(fixtureIcon(par));
    QVERIFY(!parLook.isNull());
    QVERIFY(parLook != parToday);
    par->setStageLook(Fixture::AutoLook);

    // On: the Auto fixture wears it, and stays Auto.
    QSignalSpy relay(m_doc, &Doc::stageLooksChanged);
    props->setAutoStageLooks(true);
    QCOMPARE(relay.count(), 1);
    QCOMPARE(iconImage(fixtureIcon(par)), parLook);
    QCOMPARE(par->stageLook(), Fixture::AutoLook);
    // A set look still wins.
    par->setStageLook(Fixture::StrobeLook);
    QCOMPARE(iconImage(fixtureIcon(par)), iconImage(QIcon(":/look_strobe.png")));
    par->setStageLook(Fixture::AutoLook);
    // The output-rules badge still applies.
    par->setOutputRules(Fixture::ExemptBlackout);
    const QImage badged = iconImage(fixtureIcon(par));
    QVERIFY(!badged.isNull());
    QVERIFY(badged != parLook);
    par->setOutputRules(Fixture::OutputRules());

    // A group of one look wears it, badged as a group; a mixed one as before.
    FixtureGroup *pars = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(pars);
    Fixture *par2 = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par 2"),
                                { { QLCChannel::Intensity, QLCChannel::IntensityRed },
                                  { QLCChannel::Intensity, QLCChannel::IntensityGreen },
                                  { QLCChannel::Intensity, QLCChannel::IntensityBlue } });
    QVERIFY(pars->assignFixture(par->id()));
    QVERIFY(pars->assignFixture(par2->id()));
    QCOMPARE(groupStageLook(m_doc, pars), Fixture::LedParLook);
    QCOMPARE(groupIcon(m_doc, pars).name(), lookGroupIcon(Fixture::LedParLook).name());
    QVERIFY(!iconImage(groupIcon(m_doc, pars)).isNull());
    QVERIFY(pars->assignFixture(spot->id()));
    QCOMPARE(groupStageLook(m_doc, pars), Fixture::AutoLook);
    QVERIFY(groupIcon(m_doc, pars).name().isEmpty());          // mixed: the stock group icon

    // Off again: exactly today's, fixtures and groups.
    props->setAutoStageLooks(false);
    QCOMPARE(relay.count(), 2);
    QCOMPARE(fixtureIcon(par).name(), QString("kind:%1:0").arg(int(FixedWash)));
    QCOMPARE(iconImage(fixtureIcon(par)), parToday);
    pars->resignFixture(spot->id());
    QCOMPARE(groupIcon(m_doc, pars).name(), QString("kind:%1:1").arg(int(FixedWash)));

    // No show to ask: its own look, nothing else.
    Fixture loose;
    QCOMPARE(drawnStageLook(&loose), Fixture::AutoLook);
    QVERIFY(!fixtureIcon(&loose).isNull());
}

void ItemIcons_Test::aGroupOfOneKindShowsIt()
{
    const int none = -1;
    QList<Fixture *> spots;
    for (int i = 0; i < 3; ++i)
        spots << makeFixture(QLCFixtureDef::MovingHead, QStringLiteral("Spot %1").arg(i),
                             { { QLCChannel::Pan, none }, { QLCChannel::Tilt, none },
                               { QLCChannel::Gobo, none } });
    Fixture *par = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par"),
                               { { QLCChannel::Intensity, QLCChannel::IntensityRed } });

    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(QStringLiteral("Spots"));
    m_doc->addFixtureGroup(g);
    for (Fixture *f : spots)
        QVERIFY(g->assignFixture(f->id()));
    QCOMPARE(groupKind(m_doc, g), MovingSpot);
    QCOMPARE(groupIcon(m_doc, g).name(), QString("kind:%1:1").arg(int(MovingSpot)));
    QCOMPARE(targetsIcon(m_doc, { g->id() }, {}).name(), groupIcon(m_doc, g).name());
    QCOMPARE(targetsIcon(m_doc, {}, { par->id() }).name(), fixtureIcon(par).name());
    // A Workbench cue-layer row, by its target's signature.
    QCOMPARE(signatureIcon(m_doc, QString("g:%1|f:").arg(g->id())).name(), groupIcon(m_doc, g).name());
    QCOMPARE(signatureIcon(m_doc, QString("g:|f:%1").arg(par->id())).name(), fixtureIcon(par).name());

    QVERIFY(g->assignFixture(par->id()));
    QCOMPARE(groupKind(m_doc, g), OtherKind);
    QVERIFY(groupIcon(m_doc, g).name().isEmpty());     // the stock group icon
    QVERIFY(!groupIcon(m_doc, g).isNull());

    FixtureGroup *empty = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(empty);
    QCOMPARE(groupKind(m_doc, empty), OtherKind);
}

void ItemIcons_Test::aCurtainShowsAsACurtain()
{
    Fixture *curtain = makeCurtain(QLCFixtureDef::Other, QStringLiteral("Main tabs"));
    // A dimmer-type definition whose Auto would be an ellipsoidal.
    Fixture *relay = makeCurtain(QLCFixtureDef::Dimmer, QStringLiteral("Traveller"));
    QVERIFY(Scenery::isCurtainFixture(curtain));
    QVERIFY(Scenery::isCurtainFixture(relay));
    QCOMPARE(relay->autoStageLook(), Fixture::EllipsoidalLook);

    const QImage drape = iconImage(curtainIcon());
    QVERIFY(!drape.isNull());
    QCOMPARE(drape, iconImage(QIcon(QStringLiteral(":/curtain.png"))));
    QVERIFY(!QIcon(QStringLiteral(":/curtain.png")).isNull());
    QVERIFY(QFile::exists(QStringLiteral(":/curtain@2x.png")));
    // Not any other icon: not the type's, not a look's.
    QVERIFY(drape != iconImage(QIcon(curtain->typeIconResource())));
    // (The Curtain look is this very icon: a fixture marked as a curtain.)
    for (Fixture::StageLook look : Fixture::stageLooks())
        if (look != Fixture::AutoLook && look != Fixture::CurtainLook)
            QVERIFY(drape != iconImage(QIcon(Fixture::stageLookIconResource(look))));
    QCOMPARE(iconImage(QIcon(Fixture::stageLookIconResource(Fixture::CurtainLook))), drape);

    MonitorProperties *props = m_doc->monitorProperties();
    for (bool on : { false, true })
    {
        props->setAutoStageLooks(on);
        QCOMPARE(iconImage(fixtureIcon(curtain)), drape);
        QCOMPARE(iconImage(fixtureIcon(relay)), drape);      // not an ellipsoidal
        QCOMPARE(iconImage(autoLookIcon(relay)), drape);     // the picker's Auto entry
    }

    // A group of curtains: the group variant; with anything else, not.
    FixtureGroup *tabs = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(tabs);
    QVERIFY(tabs->assignFixture(curtain->id()));
    QVERIFY(tabs->assignFixture(relay->id()));
    QCOMPARE(groupIcon(m_doc, tabs).name(), curtainIcon(true).name());
    QCOMPARE(targetsIcon(m_doc, { tabs->id() }, {}).name(), curtainIcon(true).name());
    QCOMPARE(groupStageLook(m_doc, tabs), Fixture::AutoLook);
    const QImage drapes = iconImage(curtainIcon(true));
    QVERIFY(!drapes.isNull());
    QVERIFY(drapes != drape);
    props->setAutoStageLooks(false);
    QCOMPARE(groupIcon(m_doc, tabs).name(), curtainIcon(true).name());

    // The output-rules badge, as on anything else.
    curtain->setOutputRules(Fixture::ExemptBlackout);
    QVERIFY(iconImage(fixtureIcon(curtain)) != drape);
    curtain->setOutputRules(Fixture::OutputRules());

    // The LD's own look still wins -- and the group is no longer all curtains.
    relay->setStageLook(Fixture::FresnelLook);
    QCOMPARE(iconImage(fixtureIcon(relay)), iconImage(QIcon(":/look_fresnel.png")));
    QVERIFY(groupIcon(m_doc, tabs).name() != curtainIcon(true).name());
    relay->setStageLook(Fixture::AutoLook);

    Fixture *par = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par"),
                               { { QLCChannel::Intensity, QLCChannel::IntensityRed } });
    QVERIFY(tabs->assignFixture(par->id()));
    QVERIFY(groupIcon(m_doc, tabs).name().isEmpty());       // mixed: the stock group icon

    // A hazer is no curtain: its own hazer icon, setting on or off.
    Fixture *hazer = makeFixture(QLCFixtureDef::Hazer, QStringLiteral("Hazer"),
                                 { { QLCChannel::Maintenance, -1 } });
    QVERIFY(!Scenery::isCurtainFixture(hazer));
    QCOMPARE(hazer->autoStageLook(), Fixture::AutoLook);
    for (bool on : { false, true })
    {
        props->setAutoStageLooks(on);
        QCOMPARE(iconImage(fixtureIcon(hazer)), iconImage(QIcon(":/hazer.png")));
    }
    props->setAutoStageLooks(false);
}

void ItemIcons_Test::aFixtureMarkedCurtainShowsAsACurtain()
{
    // A dimmer whose Auto is an ellipsoidal, and a real curtain.
    Fixture *dim = makeFixture(QLCFixtureDef::Dimmer, QStringLiteral("Relay"),
                               { { QLCChannel::Intensity, QLCChannel::IntensityDimmer } });
    Fixture *curtain = makeCurtain(QLCFixtureDef::Other, QStringLiteral("Main tabs"));
    const QImage drape = iconImage(curtainIcon());
    MonitorProperties *props = m_doc->monitorProperties();
    QVERIFY(iconImage(fixtureIcon(dim)) != drape);

    dim->setStageLook(Fixture::CurtainLook);
    for (bool on : { false, true })
    {
        props->setAutoStageLooks(on);
        QCOMPARE(iconImage(fixtureIcon(dim)), drape);
        // The picker's Auto entry still says what Auto would be: not a curtain.
        QVERIFY(iconImage(autoLookIcon(dim)) != drape);
    }
    props->setAutoStageLooks(false);

    // With a real curtain: a group of curtains.
    FixtureGroup *tabs = new FixtureGroup(m_doc);
    m_doc->addFixtureGroup(tabs);
    QVERIFY(tabs->assignFixture(curtain->id()));
    QVERIFY(tabs->assignFixture(dim->id()));
    QCOMPARE(groupIcon(m_doc, tabs).name(), curtainIcon(true).name());

    // Unmarked: its own icon again, and the group is no longer all curtains.
    dim->setStageLook(Fixture::AutoLook);
    QVERIFY(iconImage(fixtureIcon(dim)) != drape);
    QVERIFY(groupIcon(m_doc, tabs).name() != curtainIcon(true).name());
}

/* The icon in @p item's column 0 as an image (none: a null image). */
static QImage rowImage(const QTreeWidgetItem *item)
{
    return item == nullptr ? QImage() : iconImage(item->icon(0));
}

/* The first row of @p tree whose @p kindRole is @p kind and @p idRole @p id
   (@p kindRole -1: any row with that id that has no @p skipRole). */
static QTreeWidgetItem *findRow(QTreeWidget *tree, int idRole, quint32 id,
                                int kindRole = -1, const QVariant &kind = QVariant(),
                                int skipRole = -1)
{
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (!item->data(0, idRole).isValid() || item->data(0, idRole).toUInt() != id)
            continue;
        if (kindRole >= 0 && item->data(0, kindRole) != kind)
            continue;
        if (skipRole >= 0 && item->data(0, skipRole).isValid())
            continue;
        return item;
    }
    return nullptr;
}

void ItemIcons_Test::theSameIconEverywhere()
{
    // An explicit look, an Auto PAR (its closest look only when the show
    // says so), a curtain and a fixture with output rules.
    Fixture *fresnel = makeFixture(QLCFixtureDef::Dimmer, QStringLiteral("Fresnel"),
                                   { { QLCChannel::Intensity, QLCChannel::IntensityDimmer } });
    fresnel->setStageLook(Fixture::FresnelLook);
    const QList<QPair<int, int>> rgb = { { QLCChannel::Intensity, QLCChannel::IntensityRed },
                                         { QLCChannel::Intensity, QLCChannel::IntensityGreen },
                                         { QLCChannel::Intensity, QLCChannel::IntensityBlue } };
    Fixture *par = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par 1"), rgb);
    Fixture *par2 = makeFixture(QLCFixtureDef::ColorChanger, QStringLiteral("Par 2"), rgb);
    Fixture *curtain = makeCurtain(QLCFixtureDef::Other, QStringLiteral("Main tabs"));
    Fixture *house = makeFixture(QLCFixtureDef::Dimmer, QStringLiteral("House"),
                                 { { QLCChannel::Intensity, QLCChannel::IntensityDimmer } });
    house->setOutputRules(Fixture::ExemptBlackout | Fixture::Switch);
    const QList<Fixture *> fixtures = { fresnel, par, par2, curtain, house };

    auto group = [this](const QString &name, const QList<Fixture *> &members) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        m_doc->addFixtureGroup(g);
        for (Fixture *f : members)
            g->assignFixture(f->id());
        return g;
    };
    const QList<FixtureGroup *> groups = { group(QStringLiteral("Pars"), { par, par2 }),
                                           group(QStringLiteral("Tabs"), { curtain }),
                                           group(QStringLiteral("Mixed"), { fresnel, house }) };

    /* The plot: every fixture placed (the layers panel), and unpatched --
       the inventory lists what is still to be hung. A curtain is on the plot
       as its drape, so the inventory leaves it out (by design). */
    MonitorProperties *props = m_doc->monitorProperties();
    for (Fixture *f : fixtures)
    {
        props->setFixturePosition(f->id(), 0, 0, QVector3D(100.0f * f->id(), 100, 0));
        QVERIFY(m_doc->unpatchFixture(f->id()));
    }

    FixtureTreeWidget manager(m_doc, FixtureTreeWidget::UniverseNumber | FixtureTreeWidget::ShowGroups);
    manager.updateTree();
    FixtureGroupSource build(m_doc);
    ShowTreeView showTree(nullptr, m_doc);
    for (Fixture *f : fixtures)
        showTree.m_pinned << qMakePair(int(ShowTreeView::LibFixture), f->id());
    for (FixtureGroup *g : groups)
        showTree.m_pinned << qMakePair(int(ShowTreeView::LibGroup), g->id());
    showTree.reloadLibrary();
    MonitorInventoryPanel inventory(m_doc, nullptr);
    MonitorLayersPanel layers(m_doc, nullptr);
    layers.reload();

    const int kLayersId = Qt::UserRole + 1, kLayersKind = Qt::UserRole + 2, kLayersType = Qt::UserRole;
    auto check = [&](const char *when) {
        QCoreApplication::processEvents();          // the deferred reloads
        for (Fixture *f : fixtures)
        {
            const QImage want = iconImage(fixtureIcon(f));
            QVERIFY(!want.isNull());
            const QString what = QStringLiteral("%1, %2").arg(f->name(), QLatin1String(when));
            QTreeWidgetItem *mgr = findRow(&manager, PROP_ID, f->id(), -1, QVariant(), PROP_HEAD);
            QVERIFY2(mgr != nullptr, qPrintable(what));
            QVERIFY2(iconImage(mgr->icon(0)) == want, qPrintable("Fixture Manager: " + what));
            QTreeWidgetItem *b = findRow(&build, FixtureGroupSource::IdRole, f->id(),
                                         FixtureGroupSource::KindRole, int(FixtureGroupSource::FixtureNode));
            QVERIFY2(b != nullptr, qPrintable(what));
            QVERIFY2(rowImage(b) == want, qPrintable("Build: " + what));
            QTreeWidgetItem *lib = findRow(showTree.library(), ShowTreeView::IdRole, f->id(),
                                           ShowTreeView::KindRole, int(ShowTreeView::LibFixture));
            QVERIFY2(lib != nullptr, qPrintable(what));
            QVERIFY2(rowImage(lib) == want, qPrintable("Show Tree Library: " + what));
            QTreeWidgetItem *lay = findRow(layers.m_tree, kLayersId, f->id(), kLayersKind,
                                           QStringLiteral("fixture"));
            QVERIFY2(lay != nullptr, qPrintable(what));
            QVERIFY2(rowImage(lay) == want, qPrintable("Layers: " + what));
            QTreeWidgetItem *inv = findRow(inventory.tree(), Qt::UserRole, f->id());
            if (f == curtain)
                QVERIFY(inv == nullptr);
            else
            {
                QVERIFY2(inv != nullptr, qPrintable(what));
                QVERIFY2(rowImage(inv) == want, qPrintable("Inventory: " + what));
            }
        }
        for (FixtureGroup *g : groups)
        {
            const QImage want = iconImage(groupIcon(m_doc, g));
            const QString what = QStringLiteral("group %1, %2").arg(g->name(), QLatin1String(when));
            QTreeWidgetItem *mgr = nullptr;
            for (QTreeWidgetItemIterator it(&manager); *it != nullptr && mgr == nullptr; ++it)
                if (!(*it)->data(0, PROP_ID).isValid() && (*it)->data(0, PROP_GROUP).isValid()
                    && (*it)->data(0, PROP_GROUP).toUInt() == g->id())
                    mgr = *it;
            QVERIFY2(mgr != nullptr, qPrintable(what));
            QVERIFY2(rowImage(mgr) == want, qPrintable("Fixture Manager: " + what));
            QTreeWidgetItem *b = findRow(&build, FixtureGroupSource::IdRole, g->id(),
                                         FixtureGroupSource::KindRole, int(FixtureGroupSource::GroupNode));
            QVERIFY2(b != nullptr, qPrintable(what));
            QVERIFY2(rowImage(b) == want, qPrintable("Build: " + what));
            QTreeWidgetItem *lib = findRow(showTree.library(), ShowTreeView::IdRole, g->id(),
                                           ShowTreeView::KindRole, int(ShowTreeView::LibGroup));
            QVERIFY2(lib != nullptr, qPrintable(what));
            QVERIFY2(rowImage(lib) == want, qPrintable("Show Tree Library: " + what));
            QTreeWidgetItem *lay = findRow(layers.m_tree, kLayersId, g->id(), kLayersType,
                                           int(MonitorLayersPanel::NodeFixtureGroup));
            QVERIFY2(lay != nullptr, qPrintable(what));
            QVERIFY2(rowImage(lay) == want, qPrintable("Layers: " + what));
        }
    };

    check("setting off");
    QVERIFY(QTest::currentTestFailed() == false);
    // The Auto PAR is a kind icon now; on, its closest look.
    const QImage parOff = iconImage(fixtureIcon(par));
    props->setAutoStageLooks(true);
    QVERIFY(iconImage(fixtureIcon(par)) != parOff);
    check("setting on");
    QVERIFY(QTest::currentTestFailed() == false);
    // A look and output rules changed after the lists were built.
    par->setStageLook(Fixture::StrobeLook);
    fresnel->setOutputRules(Fixture::ExemptGrandMaster);
    check("look and rules changed");
    QVERIFY(QTest::currentTestFailed() == false);
    props->setAutoStageLooks(false);
    check("setting off again");
}

void ItemIcons_Test::iconsAreCrispAtTwoX()
{
    for (const QIcon &icon : { intensityIcon(45), shutterIcon(ShutterOpen), pointTargetIcon(),
                               lineTargetIcon(), kindIcon(MovingSpot) })
    {
        const QPixmap one = icon.pixmap(QSize(16, 16), 1.0);
        const QPixmap two = icon.pixmap(QSize(16, 16), 2.0);
        QCOMPARE(one.size(), QSize(16, 16));
        QCOMPARE(two.size(), QSize(32, 32));
        QCOMPARE(two.devicePixelRatio(), 2.0);
        // Drawn at 2x, not scaled up: the 1x picture blown up is not it.
        const QImage up = one.toImage().scaled(32, 32).convertToFormat(QImage::Format_ARGB32);
        QVERIFY2(up != two.toImage().convertToFormat(QImage::Format_ARGB32), qPrintable(icon.name()));
        QVERIFY(lightPixels(icon, 16, 2.0) > lightPixels(icon, 16, 1.0));
    }
    // Disabled: the same picture, faded.
    const QImage on = pointTargetIcon().pixmap(QSize(32, 32), 1.0).toImage();
    const QImage dis = pointTargetIcon().pixmap(QSize(32, 32), 1.0, QIcon::Disabled).toImage();
    QVERIFY2(qAlpha(dis.pixel(16, 16)) < qAlpha(on.pixel(16, 16)) * 0.6,
             qPrintable(QString("%1 vs %2").arg(qAlpha(dis.pixel(16, 16))).arg(qAlpha(on.pixel(16, 16)))));
}

static bool loadCopy(Doc *doc, const QString &path, QTemporaryDir &dir)
{
    // Read a copy: the show itself is never touched.
    const QString copy = dir.filePath(QStringLiteral("show.") + QFileInfo(path).suffix());
    if (!QFile::copy(path, copy))
        return false;
    doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    doc->setWorkspacePath(dir.path());
    if (doc->showEmbedding()->readWorkspace(copy))
        doc->showEmbedding()->apply(dir.path());
    QFile file(copy);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QXmlStreamReader r(&file);
    while (!r.atEnd())
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            return doc->loadXML(r, false);
    return false;
}

void ItemIcons_Test::pictures()
{
    const QString show = qEnvironmentVariable("QLC_ICON_SHOW");
    const QString shots = qEnvironmentVariable("QLC_ICON_SHOTS");
    if (show.isEmpty() || shots.isEmpty())
        QSKIP("set QLC_ICON_SHOW (a show, read from a copy) and QLC_ICON_SHOTS (a folder)");
    QTemporaryDir dir;
    QVERIFY(loadCopy(m_doc, show, dir));
    QSettings().remove(QStringLiteral("showtree/splitter2"));

    const QPalette native = qApp->palette();
    const QList<QPair<int, QString>> themes = { { App::ThemeDefault, QStringLiteral("default") },
                                                { App::ThemeVSCodeDark, QStringLiteral("dark") } };
    // The scene the Workbench shows: the one with the most targets and looks.
    quint32 busiest = Function::invalidId();
    int most = -1;
    for (Function *f : m_doc->functions())
        if (Scene *sc = qobject_cast<Scene *>(f))
        {
            const int n = sc->fixtureGroups().size() + sc->fixtures().size() + sc->palettes().size();
            if (n > most)
            {
                most = n;
                busiest = sc->id();
            }
        }
    for (const auto &theme : themes)
    {
        qApp->setPalette(App::themePalette(theme.first, native));
        {
            // Every icon, at 16 px (1x and 2x) and 32 px, blown up 4x.
            QList<QIcon> icons;
            for (int pct : { 0, 25, 45, 75, 100 })
                icons << intensityIcon(pct);
            icons << shutterIcon(ShutterOpen) << shutterIcon(ShutterClosed) << shutterIcon(ShutterStrobe, 30)
                  << shutterIcon(ShutterStrobe, 90) << pointTargetIcon() << lineTargetIcon();
            for (FixtureKind k : { FixedWash, MovingSpot, MovingWash, MovingHead })
                icons << kindIcon(k) << kindIcon(k, true);
            // A column each: 16 px at 1x and at 2x, then 32 px; all shown 64 wide.
            QImage sheet(72 * icons.size() + 8, 3 * 72 + 8, QImage::Format_ARGB32);
            sheet.fill(qApp->palette().color(QPalette::Base));
            QPainter p(&sheet);
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            for (int i = 0; i < icons.size(); ++i)
            {
                const int x = 8 + 72 * i;
                p.drawImage(QRect(x, 8, 64, 64), icons.at(i).pixmap(QSize(16, 16), 1.0).toImage());
                p.drawImage(QRect(x, 80, 64, 64), icons.at(i).pixmap(QSize(16, 16), 2.0).toImage());
                p.drawImage(QRect(x, 152, 64, 64), icons.at(i).pixmap(QSize(32, 32), 1.0).toImage());
            }
            p.end();
            QVERIFY(sheet.save(shots + QStringLiteral("/icons-sheet-%1.png").arg(theme.second)));
        }
        {
            ProgrammingManager mgr(nullptr, m_doc);
            mgr.setFixedSize(1500, 950);
            mgr.show();
            QVERIFY(QTest::qWaitForWindowExposed(&mgr));
            mgr.setSection(ProgrammingManager::ScenesSection);
            mgr.m_outline->rebuild();
            mgr.m_outline->expandAll();
            if (busiest != Function::invalidId())
                mgr.showFunction(busiest);
            QTest::qWait(100);
            QVERIFY(mgr.grab().save(shots + QStringLiteral("/icons-build-scenes-%1.png").arg(theme.second)));
            mgr.setSection(ProgrammingManager::PalettesSection);
            mgr.m_paletteTree->expandAll();
            QTest::qWait(100);
            QVERIFY(mgr.grab().save(shots + QStringLiteral("/icons-palettes-%1.png").arg(theme.second)));
        }
        {
            ShowTreeView view(nullptr, m_doc);
            view.setFixedSize(1500, 950);
            view.show();
            QVERIFY(QTest::qWaitForWindowExposed(&view));
            view.tree()->expandAll();
            view.reloadLibrary();
            view.library()->expandAll();
            QTest::qWait(100);
            QVERIFY(view.grab().save(shots + QStringLiteral("/icons-showtree-%1.png").arg(theme.second)));
        }
    }
    qApp->setPalette(native);
}

QTEST_MAIN(ItemIcons_Test)
