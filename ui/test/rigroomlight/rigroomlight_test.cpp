/*
  Q Light Controller Plus
  rigroomlight_test.cpp

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

#include <QApplication>
#include <QTextStream>
#include <QPainter>
#include <QImage>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "app.h"
#include "structurestudioview.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "stageplatform.h"
#include "scenery.h"
#include "fixture.h"
#include "truss.h"
#include "tower.h"
#include "doc.h"

#include "rigroomlight_test.h"

namespace
{

void settle(int ms = 20)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

/* Red enough to read as a red line: red well above both green and blue. */
bool redDominant(const QColor &c)
{
    return c.isValid() && c.alpha() > 0 && c.red() >= 90
           && c.red() > c.green() + 50 && c.red() > c.blue() + 50;
}

QString colourText(const QColor &c)
{
    return QStringLiteral("(%1,%2,%3,a%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

}

void RigRoomLight_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
}

void RigRoomLight_Test::cleanupTestCase()
{
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

void RigRoomLight_Test::userShowShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    const QString show = QStringLiteral(RIGROOM_SRC_DIR "/repro/show.qlcc");
    if (dir.isEmpty() || !QFile::exists(show))
        QSKIP("QLC_SHOT_DIR not set, or no repro/show.qlcc");
    const QString state = qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after"));

    QLCFixtureDefCache *cache = m_app->doc()->fixtureDefCache();
    cache->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    m_app->doc()->resetModified();
    settle(200);

    Doc *doc = m_app->doc();
    MonitorProperties *props = doc->monitorProperties();
    /* The drape as the user looks at it: open. Its curtain fixture is at
       zero with nothing playing, which closes it under Live; the file's own
       state (Open) is what we want here. */
    foreach (Scenery *sc, props->sceneries())
        if (sc != nullptr && sc->hasState())
        {
            sc->setDriverFixture(Scenery::invalidId());
            sc->setState(Scenery::Open);
        }

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.setPlane(StructureStudioView::Angled);
    view.setStandOnCentreStage(true);
    view.setLocked(true);
    view.setLiveValues(true);
    view.setBeams(true);
    view.reload();

    QFile diag(QDir(dir).filePath(QStringLiteral("%1-rig45-diagnostics.txt").arg(state)));
    QVERIFY(diag.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&diag);

    const struct { const char *name; double level; } rooms[] = {
        { "blackout", 0.0 }, { "house", 0.62 } };
    const QSize sizes[] = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (const auto &room : rooms)
    {
        view.setAmbient(room.level);
        for (const QSize &sz : sizes)
        {
            view.resize(sz);
            view.reload();
            settle(50);
            const QImage img = view.grab().toImage();
            QVERIFY(img.save(QDir(dir).filePath(QStringLiteral("%1-rig45-%2-%3x%4.png")
                        .arg(state, QLatin1String(room.name)).arg(sz.width()).arg(sz.height()))));
        }

        /* Every object's own primitives, at this room level: which draw a
           red-dominant edge (pen) or face (fill), and in what colour. */
        out << "== room " << room.name << " (ambient " << room.level << ")\n";
        QImage canvas(64, 64, QImage::Format_ARGB32);
        QPainter p(&canvas);
        typedef QPair<StructureStudioView::Kind, quint32> KindId;
        auto report = [&](const QString &what) {
            QMap<QString, int> pens, fills;
            for (const auto &o : view.m_ops)
            {
                if (redDominant(o.pen) && o.penWidth > 0.0)
                    pens[colourText(o.pen)]++;
                if (redDominant(o.fill) && !o.light)
                    fills[colourText(o.fill)]++;
            }
            if (pens.isEmpty() && fills.isEmpty())
                return;
            out << what << ":";
            for (auto it = pens.cbegin(); it != pens.cend(); ++it)
                out << " pen" << it.key() << "x" << it.value();
            for (auto it = fills.cbegin(); it != fills.cend(); ++it)
                out << " fill" << it.key() << "x" << it.value();
            out << "\n";
        };
        foreach (const KindId &ki, view.everyStructure())
        {
            view.m_ops.clear();
            view.m_collecting = true;
            view.drawOneStructure(p, ki.first, ki.second);
            view.m_collecting = false;
            QString name;
            if (ki.first == StructureStudioView::PlatformKind && props->platform(ki.second))
                name = QStringLiteral("platform ") + props->platform(ki.second)->name();
            else if (ki.first == StructureStudioView::SceneryKind && props->scenery(ki.second))
                name = QStringLiteral("scenery ") + props->scenery(ki.second)->name();
            else if (ki.first == StructureStudioView::TrussKind && props->truss(ki.second))
                name = QStringLiteral("truss ") + props->truss(ki.second)->name();
            else if (ki.first == StructureStudioView::TowerKind && props->tower(ki.second))
                name = QStringLiteral("tower ") + props->tower(ki.second)->name();
            else
                name = QStringLiteral("kind %1 id %2").arg(int(ki.first)).arg(ki.second);
            report(name);
        }
        foreach (quint32 fid, view.mountedFixtures())
        {
            view.m_ops.clear();
            view.m_collecting = true;
            view.drawOneFixture(p, fid, false);
            view.m_collecting = false;
            Fixture *fx = doc->fixture(fid);
            report(QStringLiteral("fixture %1%2").arg(fx ? fx->name() : QString::number(fid),
                   view.m_overlappingFixtures.contains(fid) ? QStringLiteral(" [overlap]") : QString()));
        }
        view.m_ops.clear();
        view.m_collecting = true;
        view.drawSocks(p);
        view.drawTowerFaces(p);
        view.drawStageFigures(p);
        view.m_collecting = false;
        report(QStringLiteral("socks/tower faces/figures"));
        view.m_ops.clear();
    }
    out << "overlapping fixtures: " << view.m_overlappingFixtures.size() << "\n";
    diag.close();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    RigRoomLight_Test test;
    return QTest::qExec(&test, argc, argv);
}
