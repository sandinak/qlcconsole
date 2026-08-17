/*
  Q Light Controller Plus - Test Unit
  powerdistribution_test.cpp

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

#include "powerdistribution.h"
#include "powerdistribution_test.h"

void PowerDistribution_Test::defaults()
{
    PowerCircuit cir;
    QCOMPARE(cir.name, QString("Circuit"));
    QCOMPARE(cir.ratedAmps, 20.0);
    QCOMPARE(cir.deratePercent, 80);
    QCOMPARE(cir.deratedLimit(), 16.0); // 20A * 80%
    QCOMPARE(cir.effectiveVoltage(120.0), 120.0); // voltage==0 -> inherit
    cir.voltage = 240.0;
    QCOMPARE(cir.effectiveVoltage(120.0), 240.0); // explicit overrides source

    PowerSource src;
    QCOMPARE(src.name, QString("Source"));
    QCOMPARE(src.type, int(PowerSource::Distro));
    QCOMPARE(src.voltage, 120.0);
    QCOMPARE(src.isUPS(), false);
    QCOMPARE(src.isWallSocket(), false);

    PowerDistribution pd;
    QCOMPARE(pd.sources().count(), 0);
}

void PowerDistribution_Test::sourcesAndCircuits()
{
    PowerDistribution pd;

    PowerSource src;
    src.name = "Distro A";
    pd.sources().append(src);
    QCOMPARE(pd.sources().count(), 1);

    PowerCircuit c1;
    c1.name = "Circuit 1";
    PowerCircuit c2;
    c2.name = "Circuit 2";
    pd.sources()[0].circuits.append(c1);
    pd.sources()[0].circuits.append(c2);
    QCOMPARE(pd.sources()[0].circuits.count(), 2);

    pd.reset();
    QCOMPARE(pd.sources().count(), 0);
}

void PowerDistribution_Test::assignUnassignFixture()
{
    PowerDistribution pd;
    PowerSource src;
    src.circuits.append(PowerCircuit());
    src.circuits.append(PowerCircuit());
    pd.sources().append(src);

    int s = -1, c = -1;
    pd.circuitOf(42, s, c);
    QCOMPARE(s, -1);
    QCOMPARE(c, -1);

    pd.assignFixture(42, 0, 1);
    pd.circuitOf(42, s, c);
    QCOMPARE(s, 0);
    QCOMPARE(c, 1);
    QVERIFY(pd.sources()[0].circuits[1].fixtures.contains(42));
    QVERIFY(pd.sources()[0].circuits[0].fixtures.contains(42) == false);

    pd.unassignFixture(42);
    pd.circuitOf(42, s, c);
    QCOMPARE(s, -1);
    QCOMPARE(c, -1);
    QVERIFY(pd.sources()[0].circuits[1].fixtures.contains(42) == false);

    // Unassigning something never assigned is a safe no-op.
    pd.unassignFixture(999);

    // Assigning to an out-of-range source is a safe no-op.
    pd.assignFixture(42, 5, 0);
    pd.circuitOf(42, s, c);
    QCOMPARE(s, -1);
}

void PowerDistribution_Test::reassignMovesFixture()
{
    // A fixture already on one circuit, assigned to another, ends up on
    // exactly the new one — not both.
    PowerDistribution pd;
    PowerSource src;
    src.circuits.append(PowerCircuit());
    src.circuits.append(PowerCircuit());
    pd.sources().append(src);

    pd.assignFixture(7, 0, 0);
    pd.assignFixture(7, 0, 1);

    QVERIFY(pd.sources()[0].circuits[0].fixtures.contains(7) == false);
    QVERIFY(pd.sources()[0].circuits[1].fixtures.contains(7));
    int s = -1, c = -1;
    pd.circuitOf(7, s, c);
    QCOMPARE(s, 0);
    QCOMPARE(c, 1);
}

void PowerDistribution_Test::directSourceAutoCreatesCircuit()
{
    // A fresh source (wall socket / single-outlet UPS / distro with no
    // circuits yet) gets its first circuit created on demand when a fixture
    // is assigned directly to index 0 — the "(direct)" case in the
    // right-click/tree "Add to power circuit" UI.
    PowerDistribution pd;
    PowerSource src;
    src.name = "Wall Socket";
    src.type = PowerSource::WallSocket;
    pd.sources().append(src);

    QCOMPARE(pd.sources()[0].circuits.count(), 0);
    pd.assignFixture(3, 0, 0);
    QCOMPARE(pd.sources()[0].circuits.count(), 1);
    QVERIFY(pd.sources()[0].circuits[0].fixtures.contains(3));
}

void PowerDistribution_Test::xmlRoundTrip()
{
    PowerDistribution pd;

    PowerSource src;
    src.name = "House Distro";
    src.voltage = 208.0;
    PowerCircuit c1;
    c1.name = "SR Circuit";
    c1.ratedAmps = 20.0;
    c1.fixtures << 1 << 2;
    PowerCircuit c2;
    c2.name = "SL Circuit";
    c2.ratedAmps = 30.0;
    c2.fixtures << 3;
    src.circuits << c1 << c2;
    pd.sources().append(src);

    PowerSource ups;
    ups.name = "Backup UPS";
    ups.type = PowerSource::Battery;
    ups.vaRating = 1500.0;
    pd.sources().append(ups);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    QVERIFY(pd.saveXML(&writer));
    writer.writeEndDocument();

    PowerDistribution pd2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("PowerDistribution"))
            QVERIFY(pd2.loadXML(reader, nullptr));
        else
            reader.skipCurrentElement();
    }

    QCOMPARE(pd2.sources().count(), 2);
    QCOMPARE(pd2.sources()[0].name, QString("House Distro"));
    QCOMPARE(pd2.sources()[0].voltage, 208.0);
    QCOMPARE(pd2.sources()[0].circuits.count(), 2);
    QCOMPARE(pd2.sources()[0].circuits[0].name, QString("SR Circuit"));
    QCOMPARE(pd2.sources()[0].circuits[0].ratedAmps, 20.0);
    QCOMPARE(pd2.sources()[0].circuits[0].fixtures, (QList<quint32>{1, 2}));
    QCOMPARE(pd2.sources()[0].circuits[1].fixtures, (QList<quint32>{3}));
    QCOMPARE(pd2.sources()[1].name, QString("Backup UPS"));
    QCOMPARE(pd2.sources()[1].isUPS(), true);
    QCOMPARE(pd2.sources()[1].vaRating, 1500.0);

    // An empty model writes nothing (no <PowerDistribution> element at all)
    // and a fresh load of that is simply a no-op, not an error state.
    PowerDistribution empty;
    QByteArray buf2;
    QXmlStreamWriter w2(&buf2);
    w2.writeStartDocument();
    QVERIFY(empty.saveXML(&w2));
    w2.writeEndDocument();
    QVERIFY(!buf2.contains("PowerDistribution"));
}

QTEST_APPLESS_MAIN(PowerDistribution_Test)
