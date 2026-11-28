/*
  Q Light Controller Plus
  lightmapview_test.cpp

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
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QToolButton>
#include <QTextStream>
#include <QMenu>
#include <QFile>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "app.h"
#include "monitor.h"
#include "structurestudioview.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "measurementunits.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "lightmap.h"
#include "scenery.h"
#include "fixture.h"
#include "doc.h"

#include "lightmapview_test.h"

namespace
{
/* What the view's map is, for a failure. */
QString state(const StructureStudioView &v)
{
    const QRectF a = LightMap::stageArea(v.m_doc->monitorProperties());
    return QStringLiteral("busy %1 pending %2 grid %3x%4 area %5,%6 %7x%8 sources %9 caption '%10'")
        .arg(v.m_lightBusy).arg(v.m_lightPending).arg(v.lightMapGrid().cols).arg(v.lightMapGrid().rows)
        .arg(a.left()).arg(a.top()).arg(a.width()).arg(a.height()).arg(v.lightSources().size())
        .arg(v.lightMapCaption());
}

void settle(int ms = 20)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

/* A par with a dimmer: 3500 lm, 30 degrees. */
Fixture *addPar(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Par " + name);
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    ch->setColour(QLCChannel::NoColour);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    QLCPhysical ph;
    ph.setBulbLumens(3500);
    ph.setLensDegreesMin(30);
    ph.setLensDegreesMax(30);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    if (!doc->addFixture(fx))
        return nullptr;
    return fx;
}

/* @p fx hung at (@p x, @p y) metres, 6 m up, pointing straight down. */
void hangOver(MonitorProperties *mp, Fixture *fx, float x, float y)
{
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(x * 1000.0f, y * 1000.0f, 6.0f);
    mp->setFixtureProperties(fx->id(), item);
    FixtureRigProps rp = mp->fixtureRigProps(fx->id());
    rp.fixedAim = true;
    rp.panZeroDir = 0;
    rp.aimDown = 90.0f;
    mp->setFixtureRigProps(fx->id(), rp);
}

QPoint cellAt(const LightMap::Grid &g, const QVector3D &p)
{
    return QPoint(qBound(0, int((p.x() - g.area.left()) / g.step), g.cols - 1),
                  qBound(0, int((p.y() - g.area.top()) / g.step), g.rows - 1));
}

/* Every head of the rig up but every third fixture's: some of the stage lit,
   some not; movers at home. */
void lightTwoThirds(Doc *doc)
{
    int i = 0;
    for (Fixture *fx : doc->fixtures())
    {
        QByteArray v(int(fx->channels()), 0);
        const bool on = (i++ % 3) != 0;
        for (quint32 c = 0; c < fx->channels(); ++c)
        {
            const QLCChannel *ch = fx->channel(c);
            if (ch == nullptr)
                continue;
            if (ch->group() == QLCChannel::Intensity && on)
                v[int(c)] = char(255);
            else if (ch->group() == QLCChannel::Pan || ch->group() == QLCChannel::Tilt)
                v[int(c)] = char(128);
        }
        fx->setOwnChannelValues(v);
    }
}
}

void LightMapView_Test::init()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
}

void LightMapView_Test::cleanupTestCase()
{
    if (m_app != nullptr)
    {
        m_app->doc()->resetModified();
        delete m_app;
        m_app = nullptr;
    }
}

void LightMapView_Test::loadStageShow()
{
    if (m_app != nullptr)
        return;
    m_app = new App();
    m_app->init();
    m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(QStringLiteral(STAGE_SHOW)), QFile::NoError);
    m_app->doc()->resetModified();
    // The main drape open: its curtain fixture at zero would close it.
    foreach (Scenery *sc, m_app->doc()->monitorProperties()->sceneries())
        if (sc != nullptr && sc->hasState())
        {
            sc->setDriverFixture(Scenery::invalidId());
            sc->setState(Scenery::Open);
        }
    settle(100);
}

void LightMapView_Test::offWorksNothingOut()
{
    Doc doc(this);
    doc.monitorProperties()->setGridSize(QVector3D(10, 3, 8));
    Fixture *par = addPar(&doc, "1");
    hangOver(doc.monitorProperties(), par, 5.0f, 4.0f);
    par->setOwnChannelValues(QByteArray(1, char(255)));
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.setPlane(StructureStudioView::Top);
    v.setLiveValues(true);
    v.resize(800, 600);
    v.reload();
    v.grab();
    v.liveTick();
    v.grab();
    QVERIFY(!v.lightMap());
    QVERIFY(!v.m_lightStarted.isValid());           // never worked one out
    QVERIFY(!v.m_lightBusy);
    QVERIFY(!v.lightMapGrid().isValid());
    QVERIFY(v.lightMapCaption().isEmpty());
    QCOMPARE(v.lightMapPaintMs(), 0.0);              // and never drew one

    // On and off again: let go of what it had.
    v.setLightMap(true);
    QVERIFY2(v.waitForLightMap(), qPrintable(state(v)));
    QVERIFY(v.lightMapGrid().isValid());
    v.setLightMap(false);
    QVERIFY(!v.lightMapGrid().isValid());
    QVERIFY(v.m_lightImage.isNull());
}

void LightMapView_Test::theMapShowsWhatTheOutputLights()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 8));
    Fixture *par = addPar(&doc, "1");
    hangOver(mp, par, 3.0f, 4.0f);
    par->setOwnChannelValues(QByteArray(1, char(255)));

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.setPlane(StructureStudioView::Top);
    v.setLiveValues(true);
    v.resize(900, 640);
    v.reload();
    v.setLightMap(true);
    QVERIFY2(v.waitForLightMap(), qPrintable(state(v)));

    // The light it reads: the par, pointing down, at its lumens.
    const QVector<LightMap::Source> sources = v.lightSources();
    QCOMPARE(sources.size(), 1);
    const LightMap::Source &s = sources.first();
    QCOMPARE(s.fixture, par->id());
    QVERIFY2(s.dir.z() < -0.99f, qPrintable(QString("pointing %1 %2 %3").arg(s.dir.x()).arg(s.dir.y()).arg(s.dir.z())));
    QVERIFY(s.candela > 10000.0f);

    const LightMap::Grid &g = v.lightMapGrid();
    QVERIFY(g.isValid());
    QVERIFY(qAbs(g.height - LightMap::defaultHeight()) < 1e-4);
    const QPoint under = cellAt(g, QVector3D(s.apex.x(), s.apex.y(), 0));
    const QPoint across = cellAt(g, QVector3D(s.apex.x() + 5.0f, s.apex.y(), 0));
    QVERIFY2(!g.dark(under.x(), under.y()), qPrintable(QString::number(g.lux.at(g.index(under.x(), under.y())))));
    QVERIFY(g.dark(across.x(), across.y()));
    QCOMPARE(v.lightMapDarkSpots(), 1);
    // The legend: in the show's units (feet: 5' 6", foot-candles).
    const QString caption = v.lightMapCaption();
    QVERIFY2(caption.contains(MeasurementUnits::formatMetres(LightMap::defaultHeight()))
             && caption.contains(QStringLiteral("10 fc")) && caption.contains(QStringLiteral("1 dark spot")),
             qPrintable(caption));

    // Drawn: the dark outline's amber, and heat under the light.
    const QImage img = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY(v.lightMapPaintMs() > 0.0);
    // Beside the par's own icon, inside its pool (30 degrees from 14': about 3' 10" across).
    const QPointF litPx = v.worldToScreen(QVector3D(s.apex.x(), s.apex.y() + 0.6f, float(g.height)));
    const QPointF darkPx = v.worldToScreen(QVector3D(s.apex.x() + 5.0f, s.apex.y() + 0.6f, float(g.height)));
    const QColor lit = img.pixelColor(litPx.toPoint()), dark = img.pixelColor(darkPx.toPoint());
    QVERIFY2(lit.green() > dark.green() + 40,
             qPrintable(QString("lit %1, dark %2").arg(lit.name(), dark.name())));
    int amber = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor c = img.pixelColor(x, y);
            amber += (c.red() > 220 && c.green() > 140 && c.green() < 200 && c.blue() < 90) ? 1 : 0;
        }
    QVERIFY2(amber > 40, qPrintable(QString("%1 outline pixels").arg(amber)));

    // The light goes out: the whole stage dark, and the map follows the output.
    par->setOwnChannelValues(QByteArray(1, char(0)));
    v.liveTick();
    QVERIFY2(v.waitForLightMap(), qPrintable(state(v)));
    QVERIFY(v.lightMapGrid().dark(under.x(), under.y()));
    QCOMPARE(v.lightMapDarkSpots(), 1);
    int darkCells = 0;
    for (int i = 0; i < v.lightMapGrid().lux.size(); ++i)
        darkCells += v.lightMapGrid().lux.at(i) < float(v.lightMapGrid().darkLux) ? 1 : 0;
    QCOMPARE(darkCells, v.lightMapGrid().cols * v.lightMapGrid().rows);

    // In an elevation the map cannot be read: it says where it can.
    v.setPlane(StructureStudioView::Front);
    QVERIFY(v.lightMapCaption().contains(QStringLiteral("Top or Angled")));
    v.setPlane(StructureStudioView::Angled);
    QVERIFY(v.lightMapCaption().contains(QStringLiteral("dark under")));
}

void LightMapView_Test::theSettingsAreReadAgain()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 8));
    Fixture *par = addPar(&doc, "1");
    hangOver(mp, par, 5.0f, 4.0f);
    par->setOwnChannelValues(QByteArray(1, char(255)));
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.setPlane(StructureStudioView::Top);
    v.resize(800, 600);
    v.reload();
    v.setLightMap(true);
    QVERIFY2(v.waitForLightMap(), qPrintable(state(v)));
    const QVector3D apex = v.lightSources().first().apex;
    const QPoint under = cellAt(v.lightMapGrid(), apex);
    const float before = v.lightMapGrid().lux.at(v.lightMapGrid().index(under.x(), under.y()));
    QVERIFY(before > 0.0f);

    // A seated subject (3' 6"): further from the light, less light.
    mp->setLightMapHeight(float(42 * 0.0254));
    v.lightMapSettingsChanged();
    QVERIFY2(v.waitForLightMap(), qPrintable(state(v)));
    QVERIFY(qAbs(v.lightMapGrid().height - 42 * 0.0254) < 1e-4);
    const float seated = v.lightMapGrid().lux.at(v.lightMapGrid().index(under.x(), under.y()));
    QVERIFY2(seated < before, qPrintable(QString("%1 seated, %2 standing").arg(seated).arg(before)));
    // Dark under more than it gets: dark there too.
    mp->setLightMapDarkLux(seated * 2.0f);
    v.lightMapSettingsChanged();
    QVERIFY2(v.waitForLightMap(), qPrintable(state(v)));
    QVERIFY(v.lightMapGrid().dark(under.x(), under.y()));
}

void LightMapView_Test::theFooterHasTheLightMap()
{
    QScopedPointer<Doc> docOwner(new Doc(this));
    Doc *doc = docOwner.data();
    MonitorProperties *mp = doc->monitorProperties();
    QWidget parent;
    Monitor mon(&parent, doc);
    mon.showStageOverview();
    QDockWidget *dock = mon.m_overviewDlg;
    QVERIFY(dock != nullptr && mon.m_overviewView != nullptr);
    QToolButton *btn = dock->findChild<QToolButton *>(QStringLiteral("rigLightMap"));
    QVERIFY(btn != nullptr);
    QVERIFY(btn->isCheckable() && !btn->isChecked());
    QVERIFY(!mon.m_overviewView->lightMap());
    btn->setChecked(true);
    QVERIFY(mon.m_overviewView->lightMap());
    btn->setChecked(false);
    QVERIFY(!mon.m_overviewView->lightMap());

    // Its two numbers, in feet and foot-candles, saved with the show.
    QVERIFY(btn->menu() != nullptr);
    QDoubleSpinBox *height = dock->findChild<QDoubleSpinBox *>(QStringLiteral("rigLightMapHeight"));
    QDoubleSpinBox *dark = dock->findChild<QDoubleSpinBox *>(QStringLiteral("rigLightMapDark"));
    QVERIFY(height != nullptr && dark != nullptr);
    QCOMPARE(height->text(), MeasurementUnits::formatLength(1676.4, MeasurementUnits::Feet));
    QVERIFY2(height->text().contains(QStringLiteral("5'")), qPrintable(height->text()));
    QVERIFY2(dark->text().endsWith(QStringLiteral("fc")), qPrintable(dark->text()));
    QCOMPARE(dark->value(), 10.0);
    doc->resetModified();
    height->setValue(1.5);
    QCOMPARE(mp->lightMapHeight(), 1.5f);
    QVERIFY(doc->isModified());
    dark->setValue(20.0);
    QVERIFY(qAbs(double(mp->lightMapDarkLux()) - 20.0 * MeasurementUnits::luxPer(MeasurementUnits::Feet)) < 0.01);
    doc->resetModified();
}

void LightMapView_Test::aRealRigAndWhatItCosts()
{
    loadStageShow();
    Doc *doc = m_app->doc();
    lightTwoThirds(doc);
    const bool moveSpeed = StructureStudioView::moveSpeed();
    StructureStudioView::setMoveSpeed(false);
    auto restore = qScopeGuard([moveSpeed]() { StructureStudioView::setMoveSpeed(moveSpeed); });

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    v.setBeams(true);
    v.setPlane(StructureStudioView::Angled);
    v.setStandOnCentreStage(true);
    v.resize(1440, 900);
    v.reload();
    settle(30);

    // A frame without the map, then with it: what it adds.
    auto frameMs = [&v]() {
        v.grab();                       // the first after a change is the full one
        QElapsedTimer t;
        t.start();
        const int n = 5;
        for (int i = 0; i < n; ++i)
        {
            v.m_qualityFrame = true;
            v.grab();
        }
        return t.nsecsElapsed() / 1e6 / n;
    };
    const double offMs = frameMs();
    v.setLightMap(true);
    QVERIFY2(v.waitForLightMap(20000), qPrintable(state(v)));
    const QVector<LightMap::Source> sources = v.lightSources();
    QVERIFY2(sources.size() > 20, qPrintable(QString("%1 lights lit").arg(sources.size())));
    const LightMap::Grid &g = v.lightMapGrid();
    QVERIFY(g.isValid());
    int lit = 0;
    for (float x : g.lux)
        lit += x >= float(g.darkLux) ? 1 : 0;
    QVERIFY2(lit > 0 && lit < g.lux.size(), qPrintable(QString("%1 of %2 points lit").arg(lit).arg(g.lux.size())));
    const double onMs = frameMs();
    const double overlayMs = v.lightMapPaintMs();
    const double computeMs = v.lightMapComputeMs();
    // The overlay is one image and an outline; the work is off the GUI thread.
    QVERIFY2(overlayMs < 5.0, qPrintable(QString("the overlay took %1 ms a frame").arg(overlayMs)));

    const QString report = QStringLiteral("light map, %1 lights lit, %2 x %3 points (%4 m apart), %5 dark spots: "
                                          "frame %6 ms off, %7 ms on (overlay %8 ms); map worked out in %9 ms "
                                          "on a pool thread")
                               .arg(sources.size()).arg(g.cols).arg(g.rows).arg(g.step, 0, 'f', 2)
                               .arg(v.lightMapDarkSpots()).arg(offMs, 0, 'f', 1).arg(onMs, 0, 'f', 1)
                               .arg(overlayMs, 0, 'f', 2).arg(computeMs, 0, 'f', 1);
    qInfo().noquote() << report;

    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        return;
    QFile f(QDir(dir).filePath(QStringLiteral("lightmap-cost.txt")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        QTextStream(&f) << report << "\n";
    const QString state = qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after"));

    // The Rig Overview itself, the footer under the picture, three sizes, both views the map is read in.
    QWidget host;
    QScopedPointer<Monitor> own;
    Monitor *mon = Monitor::instance();
    if (mon == nullptr)
    {
        own.reset(new Monitor(&host, doc));
        mon = own.data();
    }
    mon->showStageOverview();
    QDockWidget *dock = mon->m_overviewDlg;
    QVERIFY(dock != nullptr);
    dock->setFloating(true);
    dock->show();
    QToolButton *btn = dock->findChild<QToolButton *>(QStringLiteral("rigLightMap"));
    QVERIFY(btn != nullptr);
    btn->setChecked(true);
    StructureStudioView *ov = mon->m_overviewView;
    const QSize sizes[] = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        dock->resize(sz);
        for (int plane : { int(StructureStudioView::Angled), int(StructureStudioView::Top) })
        {
            ov->setPlane(StructureStudioView::Plane(plane));
            if (plane == StructureStudioView::Angled)
                ov->setStandOnCentreStage(true);
            settle(30);
            QVERIFY2(ov->waitForLightMap(20000), qPrintable(state(*ov)));
            ov->m_qualityFrame = true;
            dock->grab().save(QDir(dir).filePath(QStringLiteral("%1-lightmap-%2-%3x%4.png")
                .arg(state, plane == StructureStudioView::Top ? QStringLiteral("top") : QStringLiteral("angled"))
                .arg(sz.width()).arg(sz.height())));
        }
    }
    doc->resetModified();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    LightMapView_Test test;
    return QTest::qExec(&test, argc, argv);
}
