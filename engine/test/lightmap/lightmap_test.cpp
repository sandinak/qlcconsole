/*
  Q Light Controller Plus - Unit test
  lightmap_test.cpp

  The light level at subject height (LightMap): what the Rig Overview's
  light map and Show Review's PWR-14 read.

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
#include <QXmlStreamWriter>
#include <QtMath>
#include <atomic>

#include "lightmap_test.h"
#include "lightmap.h"
#include "measurementunits.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "stageplatform.h"
#include "scenery.h"
#include "fixture.h"
#include "doc.h"

namespace
{
/* E = I / d^2, I = lumens over the beam's solid angle. */
double expectedLux(double lumens, double beamDeg, double distance)
{
    const double omega = 2.0 * M_PI * (1.0 - qCos(qDegreesToRadians(beamDeg / 2.0)));
    return lumens / omega / (distance * distance);
}

bool within(double a, double b, double share)
{
    return qAbs(a - b) <= qAbs(b) * share;
}

Fixture *makePar(Doc *doc, int lumens, int watts, double lensDeg)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QStringLiteral("Par %1 %2 %3").arg(lumens).arg(watts).arg(lensDeg));
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    QLCPhysical ph;
    if (lumens > 0)
        ph.setBulbLumens(lumens);
    if (watts > 0)
        ph.setPowerConsumption(watts);
    if (lensDeg > 0.0)
    {
        ph.setLensDegreesMin(lensDeg);
        ph.setLensDegreesMax(lensDeg);
    }
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setFixtureDefinition(def, mode);
    fx->setAddress(quint32(doc->fixtures().size()));
    if (!doc->addFixture(fx))
        return nullptr;
    return fx;
}
}

/* A 3500 lm, 30-degree light 6 m up, straight down: on its axis at 5' 6"
   the meter reads its lumens over its solid angle over the distance
   squared; half that at the beam angle; nothing past its edge; a quarter
   twice as far; half at half level. */
void LightMap_Test::aKnownLightGivesAKnownLevel()
{
    const QVector3D apex(5.0f, 5.0f, 6.0f);
    const QVector<LightMap::Source> one { LightMap::makeSource(1, apex, QVector3D(0, 0, -1), 30.0, 3500.0, 1.0) };
    const double h = LightMap::defaultHeight();
    QVERIFY(qAbs(h - 1.6764) < 1e-6);                        // 5' 6"
    const double d = 6.0 - h;
    const double onAxis = LightMap::luxAt(one, nullptr, QVector3D(5.0f, 5.0f, float(h)));
    QVERIFY2(within(onAxis, expectedLux(3500.0, 30.0, d), 0.001), qPrintable(QString::number(onAxis)));
    QVERIFY(onAxis > 800.0 && onAxis < 900.0);               // about 875 lx, 81 fc

    // At the beam angle (15 degrees off its axis), the same distance away: half.
    const double a = qDegreesToRadians(15.0);
    const QVector3D atBeam = apex + QVector3D(float(qSin(a)), 0.0f, -float(qCos(a))) * float(d);
    QVERIFY(within(LightMap::luxAt(one, nullptr, atBeam), onAxis * 0.5, 0.01));
    // Past its faded edge (30 degrees off): nothing.
    const double b = qDegreesToRadians(30.0);
    QCOMPARE(LightMap::luxAt(one, nullptr, apex + QVector3D(float(qSin(b)), 0.0f, -float(qCos(b))) * float(d)), 0.0);
    // Behind it: nothing.
    QCOMPARE(LightMap::luxAt(one, nullptr, apex + QVector3D(0, 0, 1)), 0.0);
    // Twice as far, a quarter.
    const QVector<LightMap::Source> high { LightMap::makeSource(1, QVector3D(5.0f, 5.0f, float(h + 2 * d)),
                                                                QVector3D(0, 0, -1), 30.0, 3500.0, 1.0) };
    QVERIFY(within(LightMap::luxAt(high, nullptr, QVector3D(5.0f, 5.0f, float(h))), onAxis / 4.0, 0.001));
    // At half level, half; two of them add up.
    const QVector<LightMap::Source> halfUp { LightMap::makeSource(1, apex, QVector3D(0, 0, -1), 30.0, 3500.0, 0.5) };
    QVERIFY(within(LightMap::luxAt(halfUp, nullptr, QVector3D(5.0f, 5.0f, float(h))), onAxis / 2.0, 0.001));
    QVERIFY(within(LightMap::luxAt(one + one, nullptr, QVector3D(5.0f, 5.0f, float(h))), onAxis * 2.0, 0.001));
    // A wash fades out further than a profile.
    const LightMap::Source wash = LightMap::makeSource(1, apex, QVector3D(0, 0, -1), 30.0, 3500.0, 1.0, 1.0);
    const LightMap::Source profile = LightMap::makeSource(1, apex, QVector3D(0, 0, -1), 30.0, 3500.0, 1.0, 0.0);
    const double c = qCos(qDegreesToRadians(17.5));
    QVERIFY(LightMap::profile(wash, c) > 0.2);
    QCOMPARE(LightMap::profile(profile, c), 0.0);
}

void LightMap_Test::theLumensComeFromTheDefinition()
{
    Doc doc(this);
    bool known = false;
    QCOMPARE(LightMap::fixtureLumens(makePar(&doc, 8000, 300, 25.0), &known), 8000.0);
    QVERIFY(known);
    QCOMPARE(LightMap::fixtureLumens(makePar(&doc, 0, 200, 25.0), &known), 7000.0);   // 35 lm/W
    QVERIFY(known);
    QCOMPARE(LightMap::fixtureLumens(makePar(&doc, 0, 0, 25.0), &known), LightMap::referenceLumens());
    QVERIFY(!known);
    QCOMPARE(LightMap::fixtureLumens(nullptr, &known), 3500.0);

    QLCPhysical ph;
    QCOMPARE(LightMap::beamDegrees(ph), 40.0);       // none declared
    ph.setLensDegreesMin(10);
    ph.setLensDegreesMax(36);
    QCOMPARE(LightMap::beamDegrees(ph), 36.0);       // a zoom at its widest
    ph.setLensDegreesMax(180);
    QCOMPARE(LightMap::beamDegrees(ph), 150.0);      // a flood
}

void LightMap_Test::theGridCoversTheStageAtSubjectHeight()
{
    MonitorProperties mp;
    mp.setGridSize(QVector3D(12, 3, 9));
    const QRectF area = LightMap::stageArea(&mp);
    QVERIFY(area.isValid());
    QCOMPARE(area.width(), 12.0);
    QVERIFY(area.height() > 0.0);
    StagePlatform *riser = mp.addPlatform();
    riser->setOriginX(2.0f);
    riser->setOriginY(2.0f);
    riser->setWidth(2.0f);
    riser->setDepth(2.0f);
    riser->setHeight(0.6f);

    const LightMap::Grid g = LightMap::layout(&mp, area, 1.7, 100.0, 2400);
    QVERIFY(g.isValid());
    QVERIFY(g.cols * g.rows <= 2400 + g.cols + g.rows);
    QVERIFY(g.step >= 0.15);
    QVERIFY(g.area.contains(area.adjusted(0.01, 0.01, -0.01, -0.01)));
    // Subject height above the deck: on the riser, 0.6 m higher.
    const int onRiserCol = int((3.0 - g.area.left()) / g.step), onRiserRow = int((3.0 - g.area.top()) / g.step);
    const int floorCol = int((9.0 - g.area.left()) / g.step);
    QVERIFY(qAbs(g.point(onRiserCol, onRiserRow).z() - 2.3f) < 1e-4f);
    QVERIFY(qAbs(g.point(floorCol, onRiserRow).z() - 1.7f) < 1e-4f);
    // A small stage keeps the closest spacing.
    QCOMPARE(LightMap::layout(&mp, QRectF(0, 0, 1.5, 1.5), 1.7, 100.0, 2400).step, 0.15);
    // No stage, no grid.
    QVERIFY(!LightMap::layout(&mp, QRectF(), 1.7, 100.0).isValid());
}

/* One light over stage right: the point under it is lit, stage left is dark,
   and the dark is one spot round the pool. */
void LightMap_Test::aLitPointAndADarkPointOnTheGrid()
{
    MonitorProperties mp;
    mp.setGridSize(QVector3D(10, 3, 8));
    LightMap::Grid g = LightMap::layout(&mp, LightMap::stageArea(&mp), LightMap::defaultHeight(),
                                        LightMap::defaultDarkLux());
    const QVector<LightMap::Source> one { LightMap::makeSource(1, QVector3D(2.5f, 4.0f, 6.0f), QVector3D(0, 0, -1),
                                                               20.0, 3500.0, 1.0) };
    QVERIFY(LightMap::compute(g, one, BeamOcclusion::ModelPtr()));
    QCOMPARE(g.lux.size(), g.cols * g.rows);
    auto cellAt = [&g](double x, double y) {
        return QPoint(qBound(0, int((x - g.area.left()) / g.step), g.cols - 1),
                      qBound(0, int((y - g.area.top()) / g.step), g.rows - 1));
    };
    const QPoint under = cellAt(2.5, 4.0), across = cellAt(8.0, 4.0);
    QVERIFY2(!g.dark(under.x(), under.y()), qPrintable(QString::number(g.lux.at(g.index(under.x(), under.y())))));
    QVERIFY(g.lux.at(g.index(under.x(), under.y())) > 500.0f);   // a 20-degree 3500 lm from 14' up
    QVERIFY(g.dark(across.x(), across.y()));
    QCOMPARE(g.lux.at(g.index(across.x(), across.y())), 0.0f);
    const QList<QVector<int>> spots = LightMap::darkSpots(g);
    QCOMPARE(spots.size(), 1);
    int lit = 0;
    for (float v : g.lux)
        lit += v >= float(g.darkLux) ? 1 : 0;
    QVERIFY(lit > 0);
    QCOMPARE(spots.first().size(), g.cols * g.rows - lit);
    QVERIFY(!spots.first().contains(g.index(under.x(), under.y())));

    // A second light stage left: two lit pools, still one dark spot round them.
    const QVector<LightMap::Source> two = one + QVector<LightMap::Source> {
        LightMap::makeSource(2, QVector3D(8.0f, 4.0f, 6.0f), QVector3D(0, 0, -1), 20.0, 3500.0, 1.0) };
    QVERIFY(LightMap::compute(g, two, BeamOcclusion::ModelPtr()));
    QVERIFY(!g.dark(across.x(), across.y()));
    // Lit wall to wall but for a strip down the middle: that strip is the one dark spot.
    QVector<LightMap::Source> wash;
    for (float x = 0.5f; x < 10.0f; x += 1.0f)
        if (x < 4.0f || x > 6.0f)
            for (float y = 0.5f; y < 9.0f; y += 1.0f)
                wash << LightMap::makeSource(3, QVector3D(x, y, 6.0f), QVector3D(0, 0, -1), 20.0, 20000.0, 1.0, 0.0);
    QVERIFY(LightMap::compute(g, wash, BeamOcclusion::ModelPtr()));
    const QPoint middle = cellAt(5.0, 4.0);
    QVERIFY(g.dark(middle.x(), middle.y()));
    QVERIFY(!g.dark(cellAt(2.0, 4.0).x(), cellAt(2.0, 4.0).y()));
    const QList<QVector<int>> strip = LightMap::darkSpots(g);
    QCOMPARE(strip.size(), 1);
    QVERIFY(strip.first().contains(g.index(middle.x(), middle.y())));
}

/* Line of sight: a low side light behind a wall lights the near side of it
   and leaves the far side dark. */
void LightMap_Test::aWallDarkensTheAreaBehindIt()
{
    MonitorProperties mp;
    mp.setGridSize(QVector3D(10, 3, 10));
    Scenery *wall = mp.addScenery();
    wall->setKind(Scenery::Wall);
    wall->setName("Garden wall");
    wall->setOriginX(5.0f);
    wall->setOriginY(5.0f);
    wall->setWidth(10.0f);
    wall->setHeight(3.0f);
    wall->setThickness(0.3f);
    const BeamOcclusion::ModelPtr sight = mp.occlusionModel();
    QVERIFY(!sight.isNull() && !sight->isEmpty());

    // From downstage (y = 9), 2 m up, a wide light thrown upstage across the wall.
    const QVector<LightMap::Source> low { LightMap::makeSource(1, QVector3D(5.0f, 9.0f, 2.0f),
                                                               QVector3D(0.0f, -1.0f, -0.05f), 70.0, 10000.0, 1.0) };
    const float h = float(LightMap::defaultHeight());
    const QVector3D inFront(5.0f, 6.5f, h), behind(5.0f, 3.0f, h);
    QVERIFY(LightMap::luxAt(low, nullptr, behind) > LightMap::defaultDarkLux());     // with nothing in the way
    QVERIFY(LightMap::luxAt(low, sight.data(), inFront) > LightMap::defaultDarkLux());
    QCOMPARE(LightMap::luxAt(low, sight.data(), behind), 0.0);

    // The grid the same: every point upstage of the wall is dark, the ones just in front lit.
    LightMap::Grid g = LightMap::layout(&mp, LightMap::stageArea(&mp), LightMap::defaultHeight(),
                                        LightMap::defaultDarkLux());
    QVERIFY(LightMap::compute(g, low, sight));
    for (int r = 0; r < g.rows; ++r)
        for (int c = 0; c < g.cols; ++c)
        {
            const QVector3D p = g.point(c, r);
            if (p.y() < 4.6f)
                QVERIFY2(g.lux.at(g.index(c, r)) == 0.0f,
                         qPrintable(QString("lit behind the wall at %1, %2").arg(p.x()).arg(p.y())));
        }
    const int col = int((5.0 - g.area.left()) / g.step), row = int((6.5 - g.area.top()) / g.step);
    QVERIFY(!g.dark(col, row));
}

void LightMap_Test::aCancelStopsTheWork()
{
    MonitorProperties mp;
    mp.setGridSize(QVector3D(10, 3, 8));
    LightMap::Grid g = LightMap::layout(&mp, LightMap::stageArea(&mp), 1.7, 100.0);
    const QVector<LightMap::Source> one { LightMap::makeSource(1, QVector3D(5, 4, 6), QVector3D(0, 0, -1), 40, 3500, 1) };
    std::atomic<bool> cancel(true);
    QVERIFY(!LightMap::compute(g, one, BeamOcclusion::ModelPtr(), &cancel));
    cancel = false;
    QVERIFY(LightMap::compute(g, one, BeamOcclusion::ModelPtr(), &cancel));
}

void LightMap_Test::theSettingsAreSavedWithTheShow()
{
    MonitorProperties mp;
    QCOMPARE(double(mp.lightMapHeight()), double(float(LightMap::defaultHeight())));
    QCOMPARE(double(mp.lightMapDarkLux()), double(float(LightMap::defaultDarkLux())));
    {
        // The defaults write nothing.
        QByteArray buf;
        QXmlStreamWriter writer(&buf);
        mp.saveXML(&writer, nullptr);
        QVERIFY(!buf.contains("LightMap"));
    }
    mp.setLightMapHeight(1.5f);
    mp.setLightMapDarkLux(250.0f);
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    MonitorProperties back;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(back.loadXML(reader, nullptr));
    QCOMPARE(back.lightMapHeight(), 1.5f);
    QCOMPARE(back.lightMapDarkLux(), 250.0f);
    // Kept in sense.
    back.setLightMapHeight(-3.0f);
    QVERIFY(back.lightMapHeight() > 0.0f);
}

void LightMap_Test::lightLevelsFollowTheUnits()
{
    typedef MeasurementUnits MU;
    QCOMPARE(MU::formatIlluminance(LightMap::defaultDarkLux(), MU::Feet), QStringLiteral("10 fc"));
    QCOMPARE(MU::formatIlluminance(LightMap::defaultDarkLux(), MU::Inches), QStringLiteral("10 fc"));
    QCOMPARE(MU::formatIlluminance(LightMap::defaultDarkLux(), MU::Metres), QStringLiteral("108 lx"));
    QCOMPARE(MU::formatIlluminance(500.0, MU::Millimetres), QStringLiteral("500 lx"));
    QCOMPARE(MU::formatIlluminance(30.0, MU::Feet), QStringLiteral("2.8 fc"));
    QCOMPARE(MU::formatIlluminance(5000.0, MU::Feet), QStringLiteral("465 fc"));
    QCOMPARE(MU::illuminanceSymbol(MU::Centimetres), QStringLiteral("lx"));
    QVERIFY(qAbs(MU::luxPer(MU::Feet) - 10.7639) < 1e-3);
    QCOMPARE(MU::luxPer(MU::Metres), 1.0);
}

QTEST_GUILESS_MAIN(LightMap_Test)
