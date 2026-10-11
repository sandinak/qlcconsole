/*
  Q Light Controller Plus
  spreadlayer_test.cpp

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

#include <QJsonDocument>
#include <QJSEngine>
#include <QtTest>
#include <QDir>

#define private public                  // EffectInstance's cells and fixtures
#include "effectinstance.h"
#undef private
#include "fixturegroup.h"
#include "effectscript.h"
#include "spreadlayer.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "spreadlayer_test.h"

static QVector<SpreadLayer::Point> line(int n)
{
    QVector<SpreadLayer::Point> pts;
    for (int i = 0; i < n; ++i)
    {
        SpreadLayer::Point p;
        p.order = i;
        p.col = i;
        pts << p;
    }
    return pts;
}

static SpreadLayer::Settings settings(double amount, SpreadLayer::Direction d = SpreadLayer::Forward,
                                      int wings = 1, int blocks = 1)
{
    SpreadLayer::Settings s;
    s.amount = amount;
    s.direction = d;
    s.wings = wings;
    s.blocks = blocks;
    return s;
}

#define PHASES(...) QVector<double>({ __VA_ARGS__ })

void SpreadLayer_Test::offIsAllTogether()
{
    QCOMPARE(SpreadLayer::phases(line(4), settings(0)), PHASES(0, 0, 0, 0));
    QVERIFY(!settings(0).isActive());
    QVERIFY(SpreadLayer::phases(QVector<SpreadLayer::Point>(), settings(1)).isEmpty());
}

void SpreadLayer_Test::forwardAndReverseRunAcross()
{
    QCOMPARE(SpreadLayer::phases(line(4), settings(1)), PHASES(0, 0.25, 0.5, 0.75));
    QCOMPARE(SpreadLayer::phases(line(4), settings(1, SpreadLayer::Reverse)), PHASES(0.75, 0.5, 0.25, 0));
    // Two cycles across: twice as far each.
    QCOMPARE(SpreadLayer::phases(line(4), settings(2)), PHASES(0, 0.5, 1.0, 1.5));
    QCOMPARE(SpreadLayer::phases(line(1), settings(1)), PHASES(0));
}

void SpreadLayer_Test::fromTheCentreAndFromTheEnds()
{
    // Five: the middle first, then out to both ends together.
    const QVector<double> out = SpreadLayer::phases(line(5), settings(1, SpreadLayer::CentreOut));
    QCOMPARE(out.at(2), 0.0);
    QCOMPARE(out.at(1), out.at(3));
    QCOMPARE(out.at(0), out.at(4));
    QVERIFY(out.at(0) > out.at(1));
    const QVector<double> in = SpreadLayer::phases(line(5), settings(1, SpreadLayer::EndsIn));
    QCOMPARE(in.at(0), 0.0);
    QCOMPARE(in.at(4), 0.0);
    QVERIFY(in.at(2) > in.at(1));
    // Four: the middle two together.
    const QVector<double> four = SpreadLayer::phases(line(4), settings(1, SpreadLayer::CentreOut));
    QCOMPARE(four.at(1), four.at(2));
    QCOMPARE(four.at(0), four.at(3));
}

void SpreadLayer_Test::wingsMirror()
{
    // Two wings of three: each runs out from the middle seam the same way.
    QCOMPARE(SpreadLayer::phases(line(6), settings(1, SpreadLayer::Forward, 2)),
             PHASES(0, 1.0 / 3, 2.0 / 3, 2.0 / 3, 1.0 / 3, 0));
    // More wings than heads: as many as there are.
    QCOMPARE(SpreadLayer::phases(line(2), settings(1, SpreadLayer::Forward, 8)), PHASES(0, 0));
}

void SpreadLayer_Test::blocksShare()
{
    QCOMPARE(SpreadLayer::phases(line(6), settings(1, SpreadLayer::Forward, 1, 2)),
             PHASES(0, 0, 1.0 / 3, 1.0 / 3, 2.0 / 3, 2.0 / 3));
    // Blocks then wings: two wings of two blocks.
    QCOMPARE(SpreadLayer::phases(line(8), settings(1, SpreadLayer::Forward, 2, 2)),
             PHASES(0, 0, 0.5, 0.5, 0.5, 0.5, 0, 0));
}

void SpreadLayer_Test::anAxisPutsThemInItsOwnOrder()
{
    // Listed one way, standing another: stage left to right goes by where they stand.
    QVector<SpreadLayer::Point> pts = line(3);
    pts[0].pos = QVector3D(3, 0, 0);
    pts[1].pos = QVector3D(1, 0, 0);
    pts[2].pos = QVector3D(2, 0, 0);
    SpreadLayer::Settings s = settings(0.75);
    s.axis = SpreadLayer::StageX;
    QCOMPARE(SpreadLayer::phases(pts, s), PHASES(0.5, 0, 0.25));

    // A head layout's columns: the heads of one column share a phase.
    QVector<SpreadLayer::Point> grid;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 2; ++c)
        {
            SpreadLayer::Point p;
            p.order = grid.size();
            p.col = c;
            p.row = r;
            grid << p;
        }
    s = settings(1);
    s.axis = SpreadLayer::Columns;
    QCOMPARE(SpreadLayer::phases(grid, s), PHASES(0, 0.5, 0, 0.5));
    s.axis = SpreadLayer::Rows;
    QCOMPARE(SpreadLayer::phases(grid, s), PHASES(0, 0, 0.5, 0.5));
}

void SpreadLayer_Test::theSettingsComeFromTheEffect()
{
    QCOMPARE(SpreadLayer::fromParams({}), SpreadLayer::Settings());
    QMap<QString, double> p;
    p[SpreadLayer::paramName("Amount")] = 1.5;
    p[SpreadLayer::paramName("Axis")] = SpreadLayer::Rows;
    p[SpreadLayer::paramName("Direction")] = SpreadLayer::EndsIn;
    p[SpreadLayer::paramName("Wings")] = 3;
    p[SpreadLayer::paramName("Blocks")] = 0;          // nonsense: one
    const SpreadLayer::Settings s = SpreadLayer::fromParams(p);
    QCOMPARE(s.amount, 1.5);
    QCOMPARE(s.axis, SpreadLayer::Rows);
    QCOMPARE(s.direction, SpreadLayer::EndsIn);
    QCOMPARE(s.wings, 3);
    QCOMPARE(s.blocks, 1);
}

/* Every shipped script that says it takes the host spread reads fixture.phase:
   with the spread on, heads given different phases come out differently, and
   heads given the same phase come out the same. */
void SpreadLayer_Test::everyScriptThatTakesItUsesIt()
{
    QDir dir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts");
    int checked = 0;
    foreach (const QString &file, dir.entryList({ "*.js" }, QDir::Files))
    {
        EffectScript script;
        if (!script.load(dir.filePath(file)) || !script.takesHostSpread())
            continue;
        QJSEngine *e = script.engine();
        auto run = [&](const QVector<double> &phases) {
            QJSValue fixtures = e->newArray(uint(phases.size()));
            for (int i = 0; i < phases.size(); ++i)
            {
                QJSValue f = e->newObject();
                f.setProperty("id", i + 1);
                f.setProperty("head", 0);
                f.setProperty("heads", 1);
                f.setProperty("name", QString("F%1").arg(i + 1));
                for (const char *cap : { "hasRGB", "hasDimmer", "hasPanTilt", "hasColorWheel",
                                         "hasGobo", "hasShutter", "hasZoom" })
                    f.setProperty(cap, true);
                f.setProperty("panRange", 540);
                f.setProperty("tiltRange", 270);
                f.setProperty("lensMin", 10.0);
                f.setProperty("lensMax", 40.0);
                QJSValue pos = e->newObject();
                pos.setProperty("x", double(i));
                pos.setProperty("y", 0.0);
                pos.setProperty("z", 3.0);
                f.setProperty("pos", pos);
                QJSValue grid = e->newObject();
                grid.setProperty("col", i);
                grid.setProperty("row", 0);
                grid.setProperty("cols", int(phases.size()));
                grid.setProperty("rows", 1);
                f.setProperty("grid", grid);
                f.setProperty("aimAt", e->newObject());
                f.setProperty("phase", phases.at(i));
                fixtures.setProperty(quint32(i), f);
            }
            QJSValue inputs = e->newObject();
            inputs.setProperty("_time", 0.37);
            inputs.setProperty("_beat", 0.37);
            inputs.setProperty("_bpm", 120.0);
            inputs.setProperty("_beatCount", 0);
            inputs.setProperty("_beatPos", 0.37);
            inputs.setProperty("_spread", true);
            QJSValue state = script.newState();
            const QJSValue out = script.callTick(fixtures, inputs, e->newObject(), e->newObject(),
                                                 state, e->newObject());
            QStringList per;
            for (int i = 0; i < phases.size(); ++i)
                per << QString::fromUtf8(QJsonDocument::fromVariant(out.property(quint32(i)).toVariant())
                                            .toJson(QJsonDocument::Compact));
            return per;
        };
        const QStringList same = run({ 0.3, 0.3, 0.3 });
        const QStringList spread = run({ 0.0, 0.3, 0.6 });
        QVERIFY2(!same.at(0).isEmpty() && same.at(0) != QLatin1String("{}"),
                 qPrintable(file + ": no output to compare"));
        QVERIFY2(same.at(0) == same.at(1) && same.at(1) == same.at(2),
                 qPrintable(file + ": heads at one phase differ: " + same.join(" | ")));
        QVERIFY2(spread.at(0) != spread.at(1) || spread.at(1) != spread.at(2),
                 qPrintable(file + ": fixture.phase is ignored: " + spread.join(" | ")));
        checked++;
    }
    QVERIFY2(checked >= 15, qPrintable(QString("only %1 scripts take the host spread").arg(checked)));
}

/* The instance hands each head its phase, and tells the script it's on. */
void SpreadLayer_Test::theHostSpreadReachesTheScript()
{
    Doc doc(this);
    FixtureGroup *grp = new FixtureGroup(&doc);
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fx = new Fixture(&doc);
        fx->setName(QString("Par %1").arg(i + 1));
        fx->setChannels(1);                  // one head each
        fx->setAddress(quint32(i));
        QVERIFY(doc.addFixture(fx));
    }
    doc.addFixtureGroup(grp);
    QLCPalette *pal = new QLCPalette(QLCPalette::Effect, &doc);
    QVERIFY(doc.addPalette(pal));
    Scene *scene = new Scene(&doc);
    foreach (Fixture *f, doc.fixtures())
        scene->addFixture(f->id());
    doc.addFunction(scene);

    EffectInstance inst(&doc, scene->id(), pal->id());
    auto phases = [&]() {
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
        const QJSValue arr = inst.buildFixturesArray(cells, cols, rows);
        QVector<double> out;
        for (int i = 0; i < cells.size(); ++i)
            out << arr.property(quint32(i)).property("phase").toNumber();
        return out;
    };
    QCOMPARE(phases(), PHASES(0, 0, 0, 0));
    QCOMPARE(inst.buildInputsObject().property("_spread").toBool(), false);

    pal->setEffectParamValue(SpreadLayer::paramName("Amount"), 1.0);
    QCOMPARE(phases(), PHASES(0, 0.25, 0.5, 0.75));
    QCOMPARE(inst.buildInputsObject().property("_spread").toBool(), true);
    pal->setEffectParamValue(SpreadLayer::paramName("Direction"), SpreadLayer::Reverse);
    QCOMPARE(phases(), PHASES(0.75, 0.5, 0.25, 0));
}

QTEST_MAIN(SpreadLayer_Test)
