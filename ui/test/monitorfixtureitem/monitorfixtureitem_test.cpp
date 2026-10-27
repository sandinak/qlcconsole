/*
  Q Light Controller Plus
  monitorfixtureitem_test.cpp

  Copyright (C) Jano Svitok

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

#include <QLabel>
#include <QApplication>
#include <QInputDialog>
#include <QTimer>
#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QStyleOptionGraphicsItem>

#define protected public
#define private public
#include "monitorfixtureitem.h"
#undef protected
#undef private

#include "monitorfixtureitem_test.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfile.h"
#include "qlcmacros.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "monitorproperties.h"
#include "programmercontroller.h"
#include "venuecorrections.h"
#include "truss.h"
#include "doc.h"

#include "../../../engine/test/common/resource_paths.h"

void MonitorFixtureItem_Test::initTestCase()
{
    m_doc = new Doc(this);

    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void MonitorFixtureItem_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void MonitorFixtureItem_Test::cleanup()
{
    m_doc->clearContents();
}

void MonitorFixtureItem_Test::computeAlpha()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    QFETCH(quint32, masterDimmer);
    QFETCH(quint32, head1Dimmer);
    QFETCH(quint32, head2Dimmer);

    Fixture* fxi = new Fixture(m_doc);
    fxi->setName("Test Fixture");

    QLCFixtureDef* def = m_doc->fixtureDefCache()->fixtureDef(manufacturer, model);
    QVERIFY(def != NULL);
    QVERIFY(def != NULL);
    QVERIFY(def->channels().size() > 0);
    QLCFixtureMode* m = def->mode(mode);
    QVERIFY(m != NULL);

    fxi->setFixtureDefinition(def, m);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    m_doc->addFixture(fxi);

    MonitorFixtureItem* mfi = new MonitorFixtureItem(m_doc, fxi->id());
    const FixtureHead* h1 = mfi->m_heads.at(0);
    const FixtureHead* h2 = mfi->m_heads.at(1);

    QVERIFY(h1 != NULL);
    QVERIFY(h2 != NULL);

    if (masterDimmer != QLCChannel::invalid())
    {
        QCOMPARE(h1->m_masterDimmer, masterDimmer);
        QCOMPARE(h2->m_masterDimmer, masterDimmer);
    }
    else
    {
        QCOMPARE(h1->m_masterDimmer, QLCChannel::invalid());
        QCOMPARE(h2->m_masterDimmer, QLCChannel::invalid());
    }

    if (head1Dimmer != QLCChannel::invalid())
    {
        QCOMPARE(h1->m_dimmer, head1Dimmer);
    }
    else
    {
        QCOMPARE(h1->m_dimmer, QLCChannel::invalid());
    }

    if (head2Dimmer != QLCChannel::invalid())
    {
        QCOMPARE(h2->m_dimmer, head2Dimmer);
    }
    else
    {
        QCOMPARE(h2->m_dimmer, QLCChannel::invalid());
    }

    if (masterDimmer == QLCChannel::invalid() && head1Dimmer == QLCChannel::invalid())
    {
       {
           QByteArray values(fxi->channels(), (char)255);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)255u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)255u);
       }

       {
           QByteArray values(fxi->channels(), 127);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)255u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)255u);
       }

       {
           QByteArray values(fxi->channels(), 0);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)255u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)255u);
       }
    }
    else if (masterDimmer != QLCChannel::invalid() && head1Dimmer == QLCChannel::invalid())
    {
       {
           QByteArray values(fxi->channels(), (char)255);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)255u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)255u);
       }

       {
           QByteArray values(fxi->channels(), 127);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)127u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)127u);
       }

       {
           QByteArray values(fxi->channels(), 0);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)0u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)0u);
       }
    }
    else if (masterDimmer == QLCChannel::invalid() && head1Dimmer != QLCChannel::invalid())
    {
       {
           QByteArray values(fxi->channels(), (char)255);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)255u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)255u);
       }

       {
           QByteArray values(fxi->channels(), 127);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)127u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)127u);
       }

       {
           QByteArray values(fxi->channels(), 0);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)0u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)0u);
       }
    }
    else // masterDimmer != QLCChannel::invalid() && head1Dimmer != QLCChannel::invalid()
    {
       // change both master & head dimmer

       {
           QByteArray values(fxi->channels(), (char)255);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)255u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)255u);
       }

       {
           QByteArray values(fxi->channels(), 127);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)63u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)63u);
       }

       {
           QByteArray values(fxi->channels(), 0);
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)0u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)0u);
       }

       // change only master dimmer

       {
           QByteArray values(fxi->channels(), (char)255);
           values[masterDimmer] = 127;
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)127u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)127u);
       }

       {
           QByteArray values(fxi->channels(), (char)255);
           values[masterDimmer] = 0;
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)0u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)0u);
       }

       // change only head dimmer

       {
           QByteArray values(fxi->channels(), (char)255);
           values[head1Dimmer] = 100;
           values[head2Dimmer] = (char)200;
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)100u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)200u);
       }

       {
           QByteArray values(fxi->channels(), (char)255);
           values[head1Dimmer] = 0;
           values[head2Dimmer] = 0;
           QCOMPARE(mfi->computeAlpha(h1, values), (uchar)0u);
           QCOMPARE(mfi->computeAlpha(h2, values), (uchar)0u);
       }
    }
}

void MonitorFixtureItem_Test::computeAlpha_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<quint32>("masterDimmer");
    QTest::addColumn<quint32>("head1Dimmer");
    QTest::addColumn<quint32>("head2Dimmer");

    QTest::newRow("5 heads with dimmer")
        << "Showtec"
        << "Sunstrip Active"
        << "5 Channels Mode"
        << QLCChannel::invalid()
        << 0u
        << 1u;

    QTest::newRow("4 heads with dimmer and RGB")
        << "American DJ"
        << "Quad Scan LED"
        << "28 Channels mode"
        << QLCChannel::invalid()
        << 5u
        << 12u;

    QTest::newRow("Master dimmer and 4 heads with dimmer")
        << "American DJ"
        << "Event Bar LED"
        << "25 Channel Mode"
        << 24u
        << 4u
        << 9u;

    QTest::newRow("Master dimmer and 3 heads with RGB")
        << "American DJ"
        << "Mega Bar LED"
        << "11 Channels"
        << 10u
        << QLCChannel::invalid()
        << QLCChannel::invalid();

    QTest::newRow("Master dimmer and 4 heads with dimmer and RGB")
        << "ADB"
        << "ALC4"
        << "Extended 21 Channels (CT Linear)"
        << 0u
        << 1u
        << 6u;

     QTest::newRow("4 RGB heads")
        << "American DJ"
        << "Dotz Bar 1.4"
        << "12 Channel"
        << QLCChannel::invalid()
        << QLCChannel::invalid()
        << QLCChannel::invalid();
}

/* Scramble practice: "the views draw where the scrambled lights land
   (practiceViewProps())". The 2D plot's own item draws a moving head's
   pan and tilt gauges from its DMX with the calibration taken out; in a
   practice that is the light's RIGHT calibration (where it really points),
   not the scrambled one applied -- so the gauges look as they did before
   the practice, and as they do after it. Drawn from the scrambled offsets
   they would look different. */
void MonitorFixtureItem_Test::aPracticeDrawsTheGaugesWhereTheLightPoints()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(dir.filePath("Tour.qlcc"), false);
    auto noShow = qScopeGuard([venues]() { venues->setShowFile(QString(), false); });

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Practice Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    QLCFixtureHead head;
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    // The fixture does not own its definition; like the other tests, it outlives the Doc.

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Practised");
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    QVERIFY(m_doc->addFixture(fxi));
    const quint32 fid = fxi->id();
    MonitorProperties *props = m_doc->monitorProperties();
    props->setFixturePosition(fid, 0, 0, QVector3D(3000, 2000, 5));
    FixtureRigProps right = props->fixtureRigProps(fid);
    right.panOffsetDeg = 0.0f;
    right.tiltOffsetDeg = 0.0f;
    props->setFixtureRigProps(fid, right);

    QScopedPointer<MonitorFixtureItem> mfi(new MonitorFixtureItem(m_doc, fid));
    mfi->setSize(QSize(40, 40));
    QVERIFY(!mfi->m_heads.isEmpty());
    FixtureHead *h = mfi->m_heads.first();
    QVERIFY(h->m_panChannel != UINT_MAX && h->m_tiltChannel != UINT_MAX);
    // The head pointed well off centre, so the gauges' arcs are long.
    h->m_panDegrees = 120.0;
    h->m_tiltDegrees = 60.0;

    // Drawn ten times up, so a few degrees of arc are many pixels.
    auto render = [&]() {
        QImage img(800, 800, QImage::Format_ARGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.translate(200, 200);
        p.scale(10.0, 10.0);
        QStyleOptionGraphicsItem opt;
        mfi->paint(&p, &opt, nullptr);
        p.end();
        return img;
    };
    const QImage before = render();

    QString why;
    QVERIFY2(!venues->startPractice({ fid }, 7, 8.0f, 12.0f, &why).isEmpty(), qPrintable(why));
    QVERIFY(venues->isPracticeActive());
    const FixtureRigProps scrambled = props->fixtureRigProps(fid);
    QVERIFY(qAbs(scrambled.panOffsetDeg) >= 7.9f && qAbs(scrambled.tiltOffsetDeg) >= 7.9f);
    const QImage during = render();
    QVERIFY2(during == before, "in practice the gauges are drawn from the scrambled calibration");

    QVERIFY(venues->endPractice());
    QVERIFY(!venues->isPracticeActive());
    QVERIFY(render() == before);

    // The same head with the scrambled offsets applied for real (no practice):
    // its gauges are drawn differently, so the comparison above can tell.
    props->setFixtureRigProps(fid, scrambled);
    QVERIFY2(render() != before, "the gauges do not show the calibration at all");
    mfi.reset();
    m_doc->deleteFixture(fid);
}

void MonitorFixtureItem_Test::mountingHeightIsInTheShowsUnits()
{
    MonitorProperties *props = m_doc->monitorProperties();
    const MonitorProperties::GridUnits before = props->gridUnits();
    const quint32 fid = 4242;
    props->setFixturePosition(fid, 0, 0, QVector3D(1000.0f, 2000.0f, 3048.0f));

    // Answers the dialog: what it asked and showed, then the typed value.
    QString asked;
    double shown = -1.0;
    auto answerWith = [&](double value) {
        QTimer::singleShot(0, [&asked, &shown, value]() {
            QInputDialog *d = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (d == nullptr)
                return;
            asked = d->labelText();
            shown = d->doubleValue();
            d->setDoubleValue(value);
            d->accept();
        });
    };

    // A feet show: 3048 mm is 10 ft; 12 typed is 12 ft.
    props->setGridUnits(MonitorProperties::Feet);
    QVERIFY2(MonitorFixtureItem::mountingHeightText(props, fid).contains("10.00 ft"),
             qPrintable(MonitorFixtureItem::mountingHeightText(props, fid)));
    answerWith(12.0);
    QVERIFY(MonitorFixtureItem::askMountingHeight(nullptr, props, fid));
    QVERIFY2(asked.contains("feet"), qPrintable(asked));
    QCOMPARE(shown, 10.0);
    QCOMPARE(qRound(props->fixturePosition(fid, 0, 0).z()), qRound(12 * 304.8));
    // Where it is on the floor is left alone.
    QCOMPARE(props->fixturePosition(fid, 0, 0).x(), 1000.0f);

    // A metric show: metres.
    props->setGridUnits(MonitorProperties::Meters);
    props->setFixturePosition(fid, 0, 0, QVector3D(1000.0f, 2000.0f, 2500.0f));
    QVERIFY(MonitorFixtureItem::mountingHeightText(props, fid).contains("2.50 m"));
    answerWith(4.0);
    QVERIFY(MonitorFixtureItem::askMountingHeight(nullptr, props, fid));
    QVERIFY2(asked.contains("metres"), qPrintable(asked));
    QCOMPARE(shown, 2.5);
    QCOMPARE(qRound(props->fixturePosition(fid, 0, 0).z()), 4000);

    props->removeFixture(fid);
    props->setGridUnits(before);
}

QTEST_MAIN(MonitorFixtureItem_Test)
