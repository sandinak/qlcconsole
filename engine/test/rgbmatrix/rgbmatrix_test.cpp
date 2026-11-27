/*
  Q Light Controller Plus - Unit tests
  rgbmatrix_test.cpp

  Copyright (C) Heikki Junnila
                Massimo Callegari

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
#include <QTemporaryDir>

#include "../common/stdprelude.h"
#define protected public
#define private public
#include "rgbscriptscache.h"
#include "rgbmatrix_test.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "rgbmatrix.h"
#include "rgbaudio.h"
#include "rgbtext.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "rgbalgorithm.h"
#undef private
#undef protected

#include "../common/resource_paths.h"

void RGBMatrix_Test::initTestCase()
{
    m_doc = new Doc(this);

    QDir fxiDir(INTERNAL_FIXTUREDIR);
    fxiDir.setFilter(QDir::Files);
    fxiDir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(fxiDir) == true);

    QLCFixtureDef* def = m_doc->fixtureDefCache()->fixtureDef("Stairville", "LED PAR56");
    QVERIFY(def != NULL);
    QLCFixtureMode* mode = def->modes().first();
    QVERIFY(mode != NULL);

    FixtureGroup* grp = new FixtureGroup(m_doc);
    grp->setName("Test Group");
    grp->setSize(QSize(5, 5));
    m_doc->addFixtureGroup(grp);

    for (int i = 0; i < 25; i++)
    {
        Fixture* fxi = new Fixture(m_doc);
        fxi->setFixtureDefinition(def, mode);
        fxi->setAddress(i * fxi->channels());
        m_doc->addFixture(fxi);

        grp->assignFixture(fxi->id());
    }

    QVERIFY(m_doc->rgbScriptsCache()->load(QDir(INTERNAL_SCRIPTDIR)));
    QVERIFY(m_doc->rgbScriptsCache()->names().size() != 0);
}

void RGBMatrix_Test::cleanupTestCase()
{
    delete m_doc;
}

void RGBMatrix_Test::initial()
{
    RGBMatrix mtx(m_doc);
    QCOMPARE(mtx.type(), Function::RGBMatrixType);
    QCOMPARE(mtx.fixtureGroup(), FixtureGroup::invalidId());
    QCOMPARE(mtx.getColor(0), QColor(Qt::red));
    QCOMPARE(mtx.getColor(1), QColor());
    QCOMPARE(mtx.m_fadersMap.count(), 0);
    QCOMPARE(mtx.m_stepHandler->currentStepIndex(), 0);
    QCOMPARE(mtx.name(), tr("New RGB Matrix"));
    QCOMPARE(mtx.duration(), uint(500));
    QCOMPARE(mtx.totalDuration(), uint(0));
    QVERIFY(mtx.algorithm() != NULL);
    QCOMPARE(mtx.algorithm()->name(), QString("Stripes"));
    QCOMPARE(mtx.components().size(), 0);
}

void RGBMatrix_Test::group()
{
    RGBMatrix mtx(m_doc);
    mtx.setFixtureGroup(0);
    QCOMPARE(mtx.fixtureGroup(), uint(0));

    mtx.setFixtureGroup(15);
    QCOMPARE(mtx.fixtureGroup(), uint(15));

    mtx.setFixtureGroup(FixtureGroup::invalidId());
    QCOMPARE(mtx.fixtureGroup(), FixtureGroup::invalidId());
}

void RGBMatrix_Test::totalDurationWithoutSteps()
{
    RGBMatrix mtx(m_doc);
    mtx.setFixtureGroup(m_doc->fixtureGroups().first()->id());

    RGBText *text = new RGBText(m_doc);
    text->setText(QString());
    text->setAnimationStyle(RGBText::StaticLetters);
    mtx.setAlgorithm(text);
    QCOMPARE(text->rgbMapStepCount(QSize(5, 5)), 0);

    // must not divide by zero
    mtx.setTotalDuration(1000);
    QCOMPARE(mtx.duration(), uint(1000));
    QCOMPARE(mtx.totalDuration(), quint32(0));
}

void RGBMatrix_Test::color()
{
    RGBMatrix mtx(m_doc);
    mtx.setColor(0, Qt::blue);
    QCOMPARE(mtx.getColor(0), QColor(Qt::blue));

    mtx.setColor(0, QColor());
    QCOMPARE(mtx.getColor(0), QColor());

    mtx.setColor(1, Qt::green);
    QCOMPARE(mtx.getColor(1), QColor(Qt::green));

    mtx.setColor(1, QColor());
    QCOMPARE(mtx.getColor(1), QColor());
}

void RGBMatrix_Test::copy()
{
    RGBMatrix mtx(m_doc);
    mtx.setColor(0, Qt::magenta);
    mtx.setColor(1, Qt::yellow);
    mtx.setFixtureGroup(0);
    mtx.setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(mtx.algorithm() != NULL);

    RGBMatrix* copyMtx = qobject_cast<RGBMatrix*> (mtx.createCopy(m_doc));
    QVERIFY(copyMtx != NULL);
    QCOMPARE(copyMtx->getColor(0), QColor(Qt::magenta));
    QCOMPARE(copyMtx->getColor(1), QColor(Qt::yellow));
    QCOMPARE(copyMtx->fixtureGroup(), uint(0));
    QVERIFY(copyMtx->algorithm() != NULL);
    QVERIFY(copyMtx->algorithm() != mtx.algorithm()); // Different object pointer!
    QCOMPARE(copyMtx->algorithm()->name(), QString("Stripes"));
}

void RGBMatrix_Test::previewMaps()
{
    RGBMatrix mtx(m_doc);
    RGBMatrixStep handler;
    QVERIFY(mtx.algorithm() != NULL);
    QCOMPARE(mtx.algorithm()->name(), QString("Stripes"));

    int steps = mtx.stepsCount();
    QCOMPARE(steps, 0);

    mtx.previewMap(0, &handler);
    QCOMPARE(handler.m_map.size(), 0); // No fixture group

    mtx.setFixtureGroup(0);
    steps = mtx.stepsCount();
    QCOMPARE(steps, 5);
    QCOMPARE(mtx.components().size(), 25);
    QCOMPARE(mtx.totalDuration(), uint(2500));

    mtx.setTotalDuration(8000);
    QCOMPARE(mtx.totalDuration(), uint(8000));

    mtx.previewMap(0, &handler);
    QCOMPARE(handler.m_map.size(), 5);

    for (int z = 0; z < steps; z++)
    {
        mtx.previewMap(z, &handler);
        for (int y = 0; y < 5; y++)
        {
            for (int x = 0; x < 5; x++)
            {
                if (x == z)
                    QCOMPARE(handler.m_map[y][x], QColor(Qt::black).rgb());
                else
                    QCOMPARE(handler.m_map[y][x], uint(0));
            }
        }
    }
}

void RGBMatrix_Test::property()
{
    RGBMatrix mtx(m_doc);
    QVERIFY(mtx.algorithm() != NULL);
    QCOMPARE(mtx.algorithm()->name(), QString("Stripes"));

    // check on invalid property
    QCOMPARE(mtx.property("foo"), QString());

    // check a valid property
    QCOMPARE(mtx.property("orientation"), QString("Horizontal"));

    mtx.setProperty("orientation", "Vertical");

    QCOMPARE(mtx.property("orientation"), QString("Vertical"));
}

/* A matrix whose script isn't installed on this computer (here "Plasma
   (Colors)", from an older QLC+) used to lose its algorithm on the first
   save and reopen as Stripes, without a word. It now loads with no
   algorithm, says so in the load log, and saves the <Algorithm> element
   back exactly as it came, along with its properties. */
void RGBMatrix_Test::anUninstalledScriptSurvivesASave()
{
    const QString in = QStringLiteral(
        "<Function ID=\"727\" Type=\"RGBMatrix\" Name=\"Safe-R/W Plazma\" Path=\"Safe\">"
        "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"50\"/>"
        "<Direction>Forward</Direction>"
        "<RunOrder>Loop</RunOrder>"
        "<Algorithm Type=\"Script\">Plasma (Colors)</Algorithm>"
        "<ControlMode>RGB</ControlMode>"
        "<FixtureGroup>0</FixtureGroup>"
        "<Property Name=\"ramp\" Value=\"15\"/>"
        "<Property Name=\"color1Index\" Value=\"Dim Red\"/>"
        "</Function>");

    m_doc->clearErrorLog();
    QXmlStreamReader reader(in);
    reader.readNextStartElement();
    RGBMatrix mtx(m_doc);
    mtx.setName("Safe-R/W Plazma");
    QVERIFY(mtx.loadXML(reader));
    QVERIFY(mtx.algorithm() == NULL);
    QVERIFY(mtx.unknownAlgorithmXML().contains("Plasma (Colors)"));
    QVERIFY(m_doc->errorLog().contains("Plasma (Colors)"));
    QVERIFY(m_doc->errorLog().contains("Safe-R/W Plazma"));
    m_doc->clearErrorLog();
    QCOMPARE(mtx.property("ramp"), QString("15"));

    // The rest of the load carried on after the algorithm
    QCOMPARE(mtx.fixtureGroup(), quint32(0));
    QCOMPARE(mtx.controlMode(), RGBMatrix::ControlModeRgb);

    auto save = [](const RGBMatrix &m) {
        QString out;
        QXmlStreamWriter w(&out);
        m.saveXML(&w);
        return out;
    };
    const QString out = save(mtx);
    QVERIFY2(out.contains("<Algorithm Type=\"Script\">Plasma (Colors)</Algorithm>"), qPrintable(out));
    QVERIFY(out.contains("<Property Name=\"ramp\" Value=\"15\"/>"));
    QVERIFY(out.contains("<Property Name=\"color1Index\" Value=\"Dim Red\"/>"));
    QCOMPARE(out.count("<Algorithm"), 1);

    // ... and the saved form reads back the same, again and again
    QXmlStreamReader reader2(out);
    reader2.readNextStartElement();
    RGBMatrix again(m_doc);
    again.setName(mtx.name());
    QVERIFY(again.loadXML(reader2));
    m_doc->clearErrorLog();
    QVERIFY(again.algorithm() == NULL);
    QCOMPARE(save(again), out);

    // A copy keeps it too
    RGBMatrix copy(m_doc);
    QVERIFY(copy.copyFrom(&again));
    QVERIFY(save(copy).contains("<Algorithm Type=\"Script\">Plasma (Colors)</Algorithm>"));

    // Choosing an algorithm replaces it: no second <Algorithm>
    again.setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(again.unknownAlgorithmXML().isEmpty());
    const QString chosen = save(again);
    QCOMPARE(chosen.count("<Algorithm"), 1);
    QVERIFY(chosen.contains("Stripes"));
    QVERIFY(chosen.contains("Plasma") == false);

    // An installed script still loads as itself, with nothing kept aside
    QXmlStreamReader reader3(QString(in).replace("Plasma (Colors)", "Stripes"));
    reader3.readNextStartElement();
    RGBMatrix known(m_doc);
    QVERIFY(known.loadXML(reader3));
    QVERIFY(known.algorithm() != NULL);
    QCOMPARE(known.algorithm()->name(), QString("Stripes"));
    QVERIFY(known.unknownAlgorithmXML().isEmpty());
    QVERIFY(m_doc->errorLog().isEmpty());
}

/* A saved show declares a default namespace on <Workspace>, so the reader
   gives every element in it that namespace. The kept <Algorithm> of an
   uninstalled script came out of the copy as <n1:Algorithm xmlns:n1=...>
   and then saved as nothing: the second save of a show lost the effect
   (found by the 0.4.0 beta matrix on a stress show). Save, reload, save
   again must be identical, with the element still there. */
void RGBMatrix_Test::anUninstalledScriptSurvivesInAShowWithANamespace()
{
    const QString in = QStringLiteral(
        "<Function xmlns=\"http://www.qlcplus.org/Workspace\" ID=\"3\" Type=\"RGBMatrix\" Name=\"NS\">"
        "<Speed FadeIn=\"0\" FadeOut=\"0\" Duration=\"50\"/>"
        "<Direction>Forward</Direction>"
        "<RunOrder>Loop</RunOrder>"
        "<Algorithm Type=\"Script\">No Such Script</Algorithm>"
        "<ControlMode>RGB</ControlMode>"
        "<FixtureGroup>0</FixtureGroup>"
        "</Function>");
    auto save = [](const RGBMatrix &m) {
        QString out;
        QXmlStreamWriter w(&out);
        m.saveXML(&w);
        return out;
    };

    QXmlStreamReader reader(in);
    reader.readNextStartElement();
    RGBMatrix first(m_doc);
    first.setName("NS");
    QVERIFY(first.loadXML(reader));
    const QString a = save(first);
    QVERIFY2(a.contains("<Algorithm Type=\"Script\">No Such Script</Algorithm>"), qPrintable(a));

    // As the app writes it: the element inside a document with the namespace
    QXmlStreamReader reader2(QStringLiteral("<Workspace xmlns=\"http://www.qlcplus.org/Workspace\">") + a
                             + QStringLiteral("</Workspace>"));
    reader2.readNextStartElement();
    reader2.readNextStartElement();
    RGBMatrix second(m_doc);
    second.setName("NS");
    QVERIFY(second.loadXML(reader2));
    const QString b = save(second);
    QCOMPARE(b, a);
    m_doc->clearErrorLog();
}

void RGBMatrix_Test::loadSave()
{
    RGBMatrix* mtx = new RGBMatrix(m_doc);
    mtx->setColor(0, Qt::magenta);
    mtx->setColor(1, Qt::blue);
    mtx->setColor(2, Qt::green);
    mtx->setColor(3, Qt::red);
    mtx->setColor(4, Qt::yellow);
    mtx->setControlMode(RGBMatrix::ControlModeRgb);
    mtx->setFixtureGroup(42);
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(mtx->algorithm() != NULL);
    QCOMPARE(mtx->algorithm()->name(), QString("Stripes"));

    mtx->setName("Xyzzy");
    mtx->setDirection(Function::Backward);
    mtx->setRunOrder(Function::PingPong);
    mtx->setDuration(1200);
    mtx->setFadeInSpeed(10);
    mtx->setFadeOutSpeed(20);
    m_doc->addFunction(mtx);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    QVERIFY(mtx->saveXML(&xmlWriter) == true);

    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QCOMPARE(xmlReader.name().toString(), QString("Function"));
    QCOMPARE(xmlReader.attributes().value("Type").toString(), QString("RGBMatrix"));
    QCOMPARE(xmlReader.attributes().value("ID").toString(), QString::number(mtx->id()));
    QCOMPARE(xmlReader.attributes().value("Name").toString(), QString("Xyzzy"));

    int speed = 0, dir = 0, run = 0, algo = 0, grp = 0, color1 = 0, color2 = 0, color3 = 0, color4 = 0, color5 = 0, colormode = 0;

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name().toString() == "Speed")
        {
            QCOMPARE(xmlReader.attributes().value("FadeIn").toString(), QString("10"));
            QCOMPARE(xmlReader.attributes().value("FadeOut").toString(), QString("20"));
            QCOMPARE(xmlReader.attributes().value("Duration").toString(), QString("1200"));
            speed++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Direction")
        {
            QCOMPARE(xmlReader.readElementText(), QString("Backward"));
            dir++;
        }
        else if (xmlReader.name().toString() == "RunOrder")
        {
            QCOMPARE(xmlReader.readElementText(), QString("PingPong"));
            run++;
        }
        else if (xmlReader.name().toString() == "Algorithm")
        {
            // RGBAlgorithms take care of Algorithm tag's contents
            algo++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Color")
        {
            bool ok = false;
            int colorNum = xmlReader.attributes().value("Index").toInt(&ok);
            QVERIFY(ok);

            switch (colorNum)
            {
                case 0:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::magenta).rgb());
                    color1++;
                break;
                case 1:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::blue).rgb());
                    color2++;
                break;
                case 2:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::green).rgb());
                    color3++;
                break;
                case 3:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::red).rgb());
                    color4++;
                break;
                case 4:
                    QCOMPARE(xmlReader.readElementText().toUInt(), QColor(Qt::yellow).rgb());
                    color5++;
                break;
                default:
                    // The color number can be between 1 and MAXINT, but here we expect only 5.
                    QVERIFY(colorNum > 0 && colorNum <= 5);
                break;
            }
        }
        else if (xmlReader.name().toString() == "FixtureGroup")
        {
            QCOMPARE(xmlReader.readElementText(), QString("42"));
            grp++;
        }
        else if (xmlReader.name().toString() == "ControlMode")
        {
            QCOMPARE(xmlReader.readElementText(), QString("RGB"));
            colormode++;
        }
        else
        {
            QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
        }
    }

    QCOMPARE(speed, 1);
    QCOMPARE(dir, 1);
    QCOMPARE(run, 1);
    QCOMPARE(algo, 1);
    QCOMPARE(color1, 1);
    QCOMPARE(color2, 1);
    QCOMPARE(color3, 1);
    QCOMPARE(color4, 1);
    QCOMPARE(color5, 1);
    QCOMPARE(grp, 1);
    QCOMPARE(colormode, 1);

    xmlReader.setDevice(NULL);
    buffer.seek(0);
    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    RGBMatrix mtx2(m_doc);
    QVERIFY(mtx2.loadXML(xmlReader) == true);
    QCOMPARE(mtx2.direction(), Function::Backward);
    QCOMPARE(mtx2.runOrder(), Function::PingPong);
    QCOMPARE(mtx2.getColor(0), QColor(Qt::magenta));
    QCOMPARE(mtx2.getColor(1), QColor(Qt::blue));
    QCOMPARE(mtx2.controlMode(), RGBMatrix::ControlModeRgb);
    QCOMPARE(mtx2.fixtureGroup(), uint(42));
    QVERIFY(mtx2.algorithm() != NULL);
    QCOMPARE(mtx2.algorithm()->name(), mtx->algorithm()->name());
    QCOMPARE(mtx2.duration(), uint(1200));
    QCOMPARE(mtx2.fadeInSpeed(), uint(10));
    QCOMPARE(mtx2.fadeOutSpeed(), uint(20));

    buffer.close();
    buffer.setData(QByteArray());

    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    xmlWriter.setDevice(&buffer);

    // Put some extra garbage in
    xmlWriter.writeStartElement("Foo");
    xmlWriter.writeEndElement();

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    QVERIFY(mtx2.loadXML(xmlReader) == false); // Not a function node

    xmlReader.setDevice(NULL);
    buffer.close();
    buffer.setData(QByteArray());

    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    xmlWriter.setDevice(&buffer);

    // Put some extra garbage in
    xmlWriter.writeStartElement("Function");
    xmlWriter.writeAttribute("Type", "Scene");
    xmlWriter.writeEndElement();

    buffer.close();
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();
    QVERIFY(mtx2.loadXML(xmlReader) == false); // Not an RGBMatrix node

}

static int level(Doc *doc, int address)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    ua.first()->processFaders();
    const int v = uchar(ua.first()->preGMValues().at(address));
    doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

static void tick(Doc *doc, int n)
{
    for (int i = 0; i < n; i++)
        doc->masterTimer()->timerTick();
}

void RGBMatrix_Test::aRunningMatrixWritesItsColour()
{
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Plain Color"));
    QVERIFY(mtx->algorithm() != NULL);
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(MasterTimer::tick() * 2);
    m_doc->addFunction(mtx);
    auto tidy = qScopeGuard([this, mtx]() {
        mtx->stop(FunctionParent::master());
        tick(m_doc, 3);
        m_doc->deleteFunction(mtx->id());
    });

    mtx->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 4);
    QVERIFY(mtx->isRunning());
    // 25 PAR56s, 5 channels each: Mode, Red, Green, Blue, Speed
    for (int f = 0; f < 25; f++)
    {
        QVERIFY2(level(m_doc, f * 5 + 1) == 255, qPrintable(QString("fixture %1 red").arg(f)));
        QCOMPARE(level(m_doc, f * 5 + 2), 0);
        QCOMPARE(level(m_doc, f * 5 + 3), 0);
        QCOMPARE(level(m_doc, f * 5 + 0), 0);          // not a colour channel
    }

    // A new colour while it runs is what it writes from its next step on
    mtx->setColor(0, QColor(0, 0, 200));
    tick(m_doc, 6);
    QCOMPARE(level(m_doc, 1), 0);
    QCOMPARE(level(m_doc, 3), 200);
    QCOMPARE(level(m_doc, 24 * 5 + 3), 200);

    // Stopped: it lets go of every channel
    mtx->stop(FunctionParent::master());
    tick(m_doc, 4);
    QVERIFY(!mtx->isRunning());
    for (int f = 0; f < 25; f++)
        QCOMPARE(level(m_doc, f * 5 + 3), 0);
}

void RGBMatrix_Test::aScriptedMatrixStepsOnItsDuration()
{
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    QVERIFY(mtx->algorithm() != NULL);
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(MasterTimer::tick() * 4);
    m_doc->addFunction(mtx);
    auto tidy = qScopeGuard([this, mtx]() {
        mtx->stop(FunctionParent::master());
        tick(m_doc, 3);
        m_doc->deleteFunction(mtx->id());
    });

    mtx->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 2);
    const int first = mtx->m_stepHandler->currentStepIndex();
    QList<int> reds;
    for (int f = 0; f < 25; f++)
        reds << level(m_doc, f * 5 + 1);
    QVERIFY(reds.contains(255));                        // a stripe is lit...
    QVERIFY(reds.contains(0));                          // ...and not all of it

    tick(m_doc, 5);
    QVERIFY(mtx->m_stepHandler->currentStepIndex() != first);
    QList<int> next;
    for (int f = 0; f < 25; f++)
        next << level(m_doc, f * 5 + 1);
    QVERIFY(next != reds);                              // the stripe moved
}

/* Item 8: "changing settings on an effect in Build while it's playing on
   the rig doesn't alter the impact". A running matrix takes a new
   direction and a new speed on the next steps, without a restart. */
void RGBMatrix_Test::aRunningMatrixTakesEditsAtOnce()
{
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(MasterTimer::tick() * 2);
    m_doc->addFunction(mtx);
    auto tidy = qScopeGuard([this, mtx]() {
        mtx->stop(FunctionParent::master());
        tick(m_doc, 3);
        m_doc->deleteFunction(mtx->id());
    });
    QVERIFY(mtx->stepsCount() > 3);
    mtx->start(m_doc->masterTimer(), FunctionParent::master());
    tick(m_doc, 2);
    const int a = mtx->m_stepHandler->currentStepIndex();
    tick(m_doc, 2);
    const int b = mtx->m_stepHandler->currentStepIndex();
    QCOMPARE(b, (a + 1) % mtx->stepsCount());          // forward, a step every 2 ticks

    // Backward while it runs: it turns round from where it is.
    mtx->setDirection(Function::Backward);
    tick(m_doc, 2);
    const int c = mtx->m_stepHandler->currentStepIndex();
    QCOMPARE(c, (b - 1 + mtx->stepsCount()) % mtx->stepsCount());
    QVERIFY(mtx->isRunning());

    // Slower while it runs: a step every 6 ticks from now.
    mtx->setDuration(MasterTimer::tick() * 6);
    tick(m_doc, 2);
    const int d = mtx->m_stepHandler->currentStepIndex();
    tick(m_doc, 2);
    QCOMPARE(mtx->m_stepHandler->currentStepIndex(), d);
}

/* Building an RGB matrix makes and destroys a temporary RGBAudio
   (RGBAlgorithm::algorithm()); its destructor asked the Doc for the audio
   capture, which made one -- an FFT plan and a beat tracker per show
   loaded, never used (TEST_MATRIX L2). */
void RGBMatrix_Test::aMatrixDoesNotStartTheAudioCapture()
{
    QVERIFY(m_doc->existingAudioInputCapture().isNull());
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    delete RGBAlgorithm::algorithm(m_doc, "Audio Spectrum");
    delete mtx;
    {
        RGBAudio audio(m_doc);
    }
    QVERIFY2(m_doc->existingAudioInputCapture().isNull(),
             "an RGB matrix that never listened started the audio capture");
}

QTEST_MAIN(RGBMatrix_Test)

namespace
{
/* One frame of the running timer (not a tick by hand, which never maps
   ahead): the functions, then the universe's faders; the frame's bytes */
QByteArray timerFrame(Doc *doc)
{
    doc->masterTimer()->timerTick(true);
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    ua.first()->processFaders();
    const QByteArray bytes = ua.first()->preGMValues().left(25 * 5);
    doc->inputOutputMap()->releaseUniverses(false);
    return bytes;
}
}

void RGBMatrix_Test::mapsAheadSendTheSameFrames()
{
    auto restore = qScopeGuard([] { RGBMatrix::setMapsAhead(true); });
    for (const char *script : { "Plasma", "Stripes" })
    {
        QList<QByteArray> runs[2];
        for (int ahead = 0; ahead < 2; ++ahead)
        {
            RGBMatrix::setMapsAhead(ahead == 1);
            RGBMatrix *mtx = new RGBMatrix(m_doc);
            mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
            mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, script));
            QVERIFY(mtx->algorithm() != NULL);
            mtx->setColor(0, QColor(255, 40, 0));
            mtx->setColor(1, QColor(0, 60, 255));
            mtx->setDuration(MasterTimer::tick() * 3);
            m_doc->addFunction(mtx);
            const quint64 late = RGBMatrix::mapsLate();
            mtx->start(m_doc->masterTimer(), FunctionParent::master());
            for (int f = 0; f < 60; ++f)
            {
                runs[ahead] << timerFrame(m_doc);
                // On time: the scripts' thread done before the next frame
                RGBScript::waitForScriptThread();
                if (ahead == 1 && f == 10)
                    QVERIFY2(!mtx->m_nextMap.isNull(), "no map asked for ahead");
            }
            QCOMPARE(RGBMatrix::mapsLate(), late);
            mtx->stop(FunctionParent::master());
            tick(m_doc, 3);
            m_doc->deleteFunction(mtx->id());
        }
        QCOMPARE(runs[1].size(), runs[0].size());
        // Ahead, a run's first map is made off the tick too: its first
        // frame has nothing yet; from the next, frame for frame the same
        QCOMPARE(runs[1].first(), QByteArray(runs[1].first().size(), char(0)));
        for (int f = 1; f < runs[0].size(); ++f)
            QVERIFY2(runs[1].at(f) == runs[0].at(f),
                     qPrintable(QString("%1: frame %2 differs").arg(script).arg(f)));
        QVERIFY2(runs[0].first() != runs[0].last() || runs[0].at(4) != runs[0].at(8),
                 "the matrix never changed: nothing compared");
    }
}

void RGBMatrix_Test::aLateMapKeepsTheLastOneUntilReady()
{
    RGBMatrix::setMapsAhead(true);
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
    mtx->setAlgorithm(RGBAlgorithm::algorithm(m_doc, "Stripes"));
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(MasterTimer::tick() * 2);
    m_doc->addFunction(mtx);
    QSemaphore hold, holding;
    auto tidy = qScopeGuard([&]() {
        hold.release(10);
        RGBScript::waitForScriptThread();
        mtx->stop(FunctionParent::master());
        tick(m_doc, 3);
        m_doc->deleteFunction(mtx->id());
    });
    mtx->start(m_doc->masterTimer(), FunctionParent::master());
    timerFrame(m_doc);                      // step 0 begins: its map asked for
    RGBScript::waitForScriptThread();
    const QByteArray step0 = timerFrame(m_doc);     // ...out; step 1's asked for
    RGBScript::waitForScriptThread();
    // The scripts' thread busy from here: step 2's map, asked for at step 1,
    // waits behind it
    QVERIFY(RGBScript::runOnScriptThread([&]() { holding.release(); hold.acquire(); }));
    QVERIFY(holding.tryAcquire(1, 5000));
    const QByteArray step1 = timerFrame(m_doc);     // on time
    QVERIFY(step1 != step0);
    const quint64 late = RGBMatrix::mapsLate();
    QByteArray f = timerFrame(m_doc);
    QCOMPARE(f, step1);                     // still step 1's frame...
    f = timerFrame(m_doc);                  // step 2 begins, its map not ready
    QCOMPARE(RGBMatrix::mapsLate(), late + 1);
    QCOMPARE(f, step1);                     // ...the last map stays out
    hold.release();
    RGBScript::waitForScriptThread();
    f = timerFrame(m_doc);                  // ready: out on this frame
    QVERIFY(f != step1);
}

namespace
{
/* A script that takes @a ms to make each map (a moving dot, 8 steps) */
RGBScript *slowScript(Doc *doc, int ms, QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("slow%1.js").arg(ms));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return nullptr;
    f.write(QStringLiteral(
        "(function() {\n"
        "  var algo = new Object;\n"
        "  algo.apiVersion = 2;\n"
        "  algo.name = \"Slow %1\";\n"
        "  algo.author = \"test\";\n"
        "  algo.properties = new Array();\n"
        "  algo.rgbMapStepCount = function(width, height) { return 8; };\n"
        "  algo.rgbMap = function(width, height, rgb, step) {\n"
        "    var until = Date.now() + %1;\n"
        "    while (Date.now() < until) {}\n"
        "    var map = new Array(height);\n"
        "    for (var y = 0; y < height; y++) {\n"
        "      map[y] = new Array(width);\n"
        "      for (var x = 0; x < width; x++)\n"
        "        map[y][x] = ((y * width + x) % 8 == step) ? rgb : 0;\n"
        "    }\n"
        "    return map;\n"
        "  };\n"
        "  return algo;\n"
        "})()\n").arg(ms).toUtf8());
    f.close();
    RGBScript *script = new RGBScript(doc);
    if (!script->load(path))
    {
        delete script;
        return nullptr;
    }
    return script;
}
}

void RGBMatrix_Test::aScriptSlowerThanItsStepsStillMoves()
{
    RGBMatrix::setMapsAhead(true);
    QTemporaryDir dir;
    RGBScript *script = slowScript(m_doc, int(MasterTimer::tick()) * 6, dir);   // 3 steps' time
    QVERIFY(script != nullptr);
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
    mtx->setAlgorithm(script);
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(MasterTimer::tick() * 2);
    m_doc->addFunction(mtx);
    auto tidy = qScopeGuard([&]() {
        mtx->stop(FunctionParent::master());
        tick(m_doc, 3);
        m_doc->deleteFunction(mtx->id());
    });
    mtx->start(m_doc->masterTimer(), FunctionParent::master());
    // Frames at their pace, for 40 steps' time: each map comes three
    // steps late, and the newest one made goes out
    QSet<QByteArray> seen;
    const quint64 late = RGBMatrix::mapsLate();
    for (int f = 0; f < 80; ++f)
    {
        seen.insert(timerFrame(m_doc));
        QThread::msleep(MasterTimer::tick());
    }
    QVERIFY2(seen.size() >= 4, qPrintable(QString("only %1 different frames").arg(seen.size())));
    QVERIFY(RGBMatrix::mapsLate() > late);
}

void RGBMatrix_Test::aMatrixDeletedMidMapWaitsForIt()
{
    RGBMatrix::setMapsAhead(true);
    QTemporaryDir dir;
    RGBScript *script = slowScript(m_doc, 300, dir);
    QVERIFY(script != nullptr);
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(m_doc->fixtureGroups().first()->id());
    mtx->setAlgorithm(script);
    mtx->setColor(0, QColor(255, 0, 0));
    mtx->setDuration(MasterTimer::tick() * 2);
    m_doc->addFunction(mtx);
    const int before = RGBScript::deletedWhileMapping();
    mtx->start(m_doc->masterTimer(), FunctionParent::master());
    timerFrame(m_doc);                      // a map asked for: 300 ms
    timerFrame(m_doc);
    // ...and being made on the scripts' thread
    QTRY_VERIFY_WITH_TIMEOUT(script->mapping(), 2000);
    // Deleted while it runs (Doc::deleteFunction: removed, postRun, deleted)
    mtx->stop(FunctionParent::master());
    timerFrame(m_doc);
    QVERIFY(m_doc->deleteFunction(mtx->id()));
    RGBScript::waitForScriptThread();
    QCOMPARE(RGBScript::deletedWhileMapping(), before);
}

