/*
  Q Light Controller Plus
  studiowins_test.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QToolButton>
#include <QComboBox>
#include <QDockWidget>
#include <QMenu>
#include <QtTest>
#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "structurestudioview.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "contextmenus.h"
#include "fixturevisualtraits.h"
#include "beamoptics.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "fixture.h"
#include "truss.h"
#include "plotnumbering.h"
#include "qlcfixturemode.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "doc.h"

#include "studiowins_test.h"

namespace
{
QLCChannel *addCh(QLCFixtureDef *def, QLCFixtureMode *mode, int at, const char *name,
                  QLCChannel::Group group, QLCChannel::PrimaryColour colour = QLCChannel::NoColour)
{
    QLCChannel *ch = new QLCChannel();
    ch->setName(QString::fromLatin1(name));
    ch->setGroup(group);
    ch->setColour(colour);
    def->addChannel(ch);
    mode->insertChannel(ch, quint32(at));
    return ch;
}

/** A red par: 0 Red, 1 Dimmer, 2 Shutter (0-9 closed, 10-19 open, 20-255
 *  strobe slow to fast), 3 Frost, 4 CTO, 5 CTB. Hung 4 m up, pointing down. */
Fixture *makePar(Doc *doc, quint32 address = 0)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Frost par");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    addCh(def, mode, 0, "Red", QLCChannel::Intensity, QLCChannel::Red);
    addCh(def, mode, 1, "Dimmer", QLCChannel::Intensity);
    QLCChannel *sh = addCh(def, mode, 2, "Shutter", QLCChannel::Shutter);
    QLCCapability *c0 = new QLCCapability(0, 9, "Closed");
    c0->setPreset(QLCCapability::ShutterClose);
    sh->addCapability(c0);
    QLCCapability *c1 = new QLCCapability(10, 19, "Open");
    c1->setPreset(QLCCapability::ShutterOpen);
    sh->addCapability(c1);
    QLCCapability *c2 = new QLCCapability(20, 255, "Strobe");
    c2->setPreset(QLCCapability::StrobeSlowToFast);
    sh->addCapability(c2);
    addCh(def, mode, 3, "Frost", QLCChannel::Beam);
    addCh(def, mode, 4, "CTO", QLCChannel::Colour);
    addCh(def, mode, 5, "CTB", QLCChannel::Colour);
    QLCPhysical ph;
    ph.setWidth(200); ph.setHeight(250); ph.setDepth(200);
    ph.setLensDegreesMin(25); ph.setLensDegreesMax(25);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (quint32 c = 0; c < 6; ++c)
        hd.addChannel(c);
    mode->insertHead(-1, hd);
    def->addMode(mode);
    Fixture *f = new Fixture(doc);
    f->setName("Par");
    f->setFixtureDefinition(def, mode);
    f->setAddress(address);
    if (!doc->addFixture(f))
        return nullptr;
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(5000, 5000, 4000));
    FixtureRigProps rp;
    rp.fixedAim = true;
    rp.aimDown = 90.0f;
    doc->monitorProperties()->setFixtureRigProps(f->id(), rp);
    return f;
}

QByteArray vals(int red, int dim, int shutter, int frost = 0, int cto = 0, int ctb = 0)
{
    QByteArray v(6, 0);
    v[0] = char(red); v[1] = char(dim); v[2] = char(shutter);
    v[3] = char(frost); v[4] = char(cto); v[5] = char(ctb);
    return v;
}

/** Pixels the beam lights: red well above the dark canvas. */
int redPixels(const QImage &img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb q = img.pixel(x, y);
            if (qRed(q) > 110 && qGreen(q) < 70 && qBlue(q) < 70)
                ++n;
        }
    return n;
}

QImage frame(StructureStudioView &v, double seconds)
{
    v.m_opticsTime = seconds;
    return v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}
MonitorProperties::CameraView sampleView(const QString &name, double az)
{
    MonitorProperties::CameraView v;
    v.name = name;
    v.plane = StructureStudioView::Angled;
    v.rotation = 1;
    v.azimuth = az;
    v.elevation = 41.5;
    v.zoomed = true;
    v.scale = 123.25;
    v.originX = 31.5;
    v.originY = -12.25;
    v.width = 1100;
    v.height = 700;
    return v;
}
}

void StudioWins_Test::savedViewsRoundTripWithTheShow()
{
    MonitorProperties mp;
    mp.setCameraView(sampleView(QStringLiteral("From FOH"), 12.5));
    mp.setCameraView(sampleView(QStringLiteral("Director's seat"), 301.0));

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));

    const QList<MonitorProperties::CameraView> views = mp2.cameraViews();
    QCOMPARE(views.size(), 2);
    QCOMPARE(views.at(0).name, QStringLiteral("From FOH"));
    QCOMPARE(views.at(1).name, QStringLiteral("Director's seat"));
    QCOMPARE(views.at(1).azimuth, 301.0);
    QCOMPARE(views.at(0).elevation, 41.5);
    QCOMPARE(views.at(0).scale, 123.25);
    QCOMPARE(views.at(0).originX, 31.5);
    QCOMPARE(views.at(0).originY, -12.25);
    QVERIFY(views.at(0).zoomed);
    QCOMPARE(views.at(0).rotation, 1);

    // Same name replaces; rename and delete.
    mp2.setCameraView(sampleView(QStringLiteral("from foh"), 99.0));
    QCOMPARE(mp2.cameraViews().size(), 2);
    QVERIFY(mp2.renameCameraView(QStringLiteral("From FOH"), QStringLiteral("Booth")));
    QVERIFY(!mp2.renameCameraView(QStringLiteral("Booth"), QStringLiteral("Director's seat")));
    QVERIFY(mp2.removeCameraView(QStringLiteral("Booth")));
    QCOMPARE(mp2.cameraViews().size(), 1);
}

void StudioWins_Test::theViewsMenuAddsAppliesRenamesAndDeletes()
{
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    QWidget parent;
    Monitor mon(&parent, &doc);
    mon.toggleStageOverview();
    QVERIFY(mon.m_overviewView != nullptr);
    StructureStudioView *view = mon.m_overviewView;
    QToolButton *btn = mon.m_overviewDlg->findChild<QToolButton *>(QStringLiteral("rigSavedViews"));
    QVERIFY(btn != nullptr);

    view->setAngledView(77.0, 33.0);
    view->setRotation(2);
    view->m_zoomed = true;
    view->m_scale = 140.0;
    view->m_originPx = QPointF(55.0, -20.0);
    mon.saveCurrentView(view, QStringLiteral("From FOH"));
    view->setAngledView(200.0, 10.0);
    view->setRotation(0);
    view->m_zoomed = false;
    mon.saveCurrentView(view, QStringLiteral("Director's seat"));
    QCOMPARE(doc.monitorProperties()->cameraViews().size(), 2);
    QVERIFY(doc.isModified());

    // The menu lists them, then applying one puts the camera back.
    QMenu *menu = btn->menu();
    emit menu->aboutToShow();
    QStringList names;
    for (QAction *a : menu->actions())
        names << a->text();
    QVERIFY(names.contains(QStringLiteral("From FOH")));
    QVERIFY(names.contains(QStringLiteral("Director's seat")));
    QVERIFY(names.contains(QStringLiteral("Add View…")));

    QVERIFY(mon.applySavedView(view, QStringLiteral("From FOH")));
    QCOMPARE(view->angledAzimuth(), 77.0);
    QCOMPARE(view->angledElevation(), 33.0);
    QCOMPARE(view->rotation(), 2);
    QVERIFY(view->m_zoomed);
    QCOMPARE(view->m_scale, 140.0);
    QCOMPARE(view->m_originPx, QPointF(55.0, -20.0));

    QVERIFY(doc.monitorProperties()->renameCameraView(QStringLiteral("From FOH"), QStringLiteral("Booth")));
    QVERIFY(!mon.applySavedView(view, QStringLiteral("From FOH")));
    QVERIFY(doc.monitorProperties()->removeCameraView(QStringLiteral("Booth")));
    QCOMPARE(doc.monitorProperties()->cameraViews().size(), 1);
}

void StudioWins_Test::frostSoftensTheEdgeAndFadesTheGoboMask()
{
    BeamOptics::Optics clear;
    clear.mask = QImage(BeamOptics::kMaskSize, BeamOptics::kMaskSize, QImage::Format_Grayscale8);
    clear.mask.fill(255);
    // A gobo: black bars across, the holes clear.
    for (int y = 0; y < clear.mask.height(); ++y)
        for (int x = 0; x < clear.mask.width(); ++x)
            clear.mask.scanLine(y)[x] = ((x / 8) % 2) ? 0 : 255;
    BeamOptics::Optics open;

    const BeamOptics::Optics f0 = BeamOptics::frosted(clear, 0.0);
    const BeamOptics::Optics f1 = BeamOptics::frosted(clear, 1.0);
    QCOMPARE(BeamOptics::maskContrast(f0.mask), BeamOptics::maskContrast(clear.mask));
    QVERIFY2(BeamOptics::maskContrast(f1.mask) < BeamOptics::maskContrast(clear.mask) * 0.6,
             "a frosted gobo is blurred: its picture loses contrast");
    QVERIFY(f1.softness > f0.softness);

    const BeamOptics::Optics o0 = BeamOptics::frosted(open, 0.0);
    const BeamOptics::Optics o1 = BeamOptics::frosted(open, 1.0);
    QVERIFY(o1.softness > o0.softness + 0.5);
    QVERIFY(frostWiden(1.0) > frostWiden(0.0));
}

void StudioWins_Test::ctoWarmsAndCtbCoolsTheBeam()
{
    const QColor white(255, 255, 255);
    const QColor warm = tintByWarmth(white, 1.0);
    const QColor cool = tintByWarmth(white, -1.0);
    QVERIFY(warm.red() > warm.blue() + 60);
    QVERIFY(warm.hueF() > 0.0 && warm.hue() < 50);           // orange
    QVERIFY(cool.blue() > cool.red() + 40);
    QCOMPARE(tintByWarmth(white, 0.0), white);
}

void StudioWins_Test::frostAndCtoAreReadFromTheChannels()
{
    Doc doc(nullptr);
    Fixture *fx = makePar(&doc);
    QVERIFY(fx != nullptr);
    BeamExtras e = beamExtras(fx, vals(255, 255, 15, 0, 0, 0));
    QCOMPARE(e.frost, 0.0);
    QCOMPARE(e.warmth, 0.0);
    QVERIFY(!e.shutterClosed);
    QCOMPARE(e.strobeHz, 0.0);
    e = beamExtras(fx, vals(255, 255, 15, 255, 255, 0));
    QCOMPARE(e.frost, 1.0);
    QCOMPARE(e.warmth, 1.0);
    e = beamExtras(fx, vals(255, 255, 15, 0, 0, 128));
    QVERIFY(e.warmth < -0.4);
    e = beamExtras(fx, vals(255, 255, 5));
    QVERIFY(e.shutterClosed);
    e = beamExtras(fx, vals(255, 255, 255));
    QVERIFY(e.strobeHz > 5.0);
    e = beamExtras(fx, vals(255, 255, 20));
    QVERIFY(e.strobeHz >= 1.0 && e.strobeHz < 1.5);
    QVERIFY(strobeLit(2.0, 0.0) && !strobeLit(2.0, 0.3) && strobeLit(2.0, 0.55 * 1.0 - 0.05 + 0.5));
}

void StudioWins_Test::aClosedShutterDrawsNoBeamInTheRigOverview()
{
    Doc doc(nullptr);
    Fixture *fx = makePar(&doc);
    QVERIFY(fx != nullptr);
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    v.setBeams(true);
    v.setAmbient(0.0);
    v.setPlane(StructureStudioView::Top);
    v.resize(900, 600);
    v.reload();

    fx->setOwnChannelValues(vals(255, 255, 15));
    const int open = redPixels(frame(v, 0.0));
    QVERIFY2(open > 300, qPrintable(QString("an open shutter lights %1 px").arg(open)));
    fx->setOwnChannelValues(vals(255, 255, 5));
    const int closed = redPixels(frame(v, 0.0));
    QCOMPARE(closed, 0);
}

void StudioWins_Test::aStrobeBlinksInTheRigOverview()
{
    Doc doc(nullptr);
    Fixture *fx = makePar(&doc);
    QVERIFY(fx != nullptr);
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    v.setBeams(true);
    v.setAmbient(0.0);
    v.setPlane(StructureStudioView::Top);
    v.resize(900, 600);
    v.reload();

    fx->setOwnChannelValues(vals(255, 255, 20));            // the slowest strobe, about 1 Hz
    const int on = redPixels(frame(v, 0.1));
    const int off = redPixels(frame(v, 0.7));
    QVERIFY2(on > 300, qPrintable(QString("lit half: %1 px").arg(on)));
    QCOMPARE(off, 0);
    QVERIFY(redPixels(frame(v, 1.1)) > 300);   // and again a second on
}

void StudioWins_Test::numberingFollowsThePlotOrderAndOneUndoRestores()
{
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    QWidget parent;
    Monitor mon(&parent, &doc);
    // Six fixtures at shuffled x (mm): stage right is the low x.
    const int xs[] = { 4000, 1000, 6000, 2000, 5000, 3000 };
    QList<quint32> ids;
    for (int i = 0; i < 6; ++i)
    {
        Fixture *fx = makePar(&doc, quint32(i * 6));
        QVERIFY(fx != nullptr);
        doc.monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(float(xs[i]), 2000.0f, 3000.0f));
        ids << fx->id();
    }
    const QList<quint32> order = PlotNumbering::ordered(&doc, ids, PlotNumbering::StageRightToLeft);
    QList<int> got;
    for (quint32 fid : order)
        got << int(doc.monitorProperties()->fixturePosition(fid, 0, 0).x());
    QCOMPARE(got, QList<int>({ 1000, 2000, 3000, 4000, 5000, 6000 }));
    // US to DS is the y order.
    doc.monitorProperties()->setFixturePosition(ids.at(0), 0, 0, QVector3D(4000, 500, 3000));
    QCOMPARE(PlotNumbering::ordered(&doc, ids, PlotNumbering::UpstageToDownstage).first(), ids.at(0));

    const QByteArray before = mon.rigSnapshot();
    const QList<PlotNumbering::Row> rows = PlotNumbering::plan(
        &doc, ids, PlotNumbering::StageRightToLeft, true, 10, true, 101, false);
    QVERIFY(PlotNumbering::apply(&doc, rows));
    mon.pushRigUndo(before);
    // The fixture at x = 1000 (index 1) is unit 10, the one at 6000 (index 2) unit 15.
    QCOMPARE(doc.monitorProperties()->fixtureUnit(ids.at(1)), 10);
    QCOMPARE(doc.monitorProperties()->fixtureUnit(ids.at(3)), 11);
    QCOMPARE(doc.monitorProperties()->fixtureUnit(ids.at(5)), 12);
    QCOMPARE(doc.monitorProperties()->fixtureUnit(ids.at(0)), 13);
    QCOMPARE(doc.monitorProperties()->fixtureUnit(ids.at(4)), 14);
    QCOMPARE(doc.monitorProperties()->fixtureUnit(ids.at(2)), 15);
    QCOMPARE(doc.monitorProperties()->fixtureChannel(ids.at(2)), 106);

    mon.undo();                                         // one step
    for (quint32 fid : ids)
    {
        QCOMPARE(doc.monitorProperties()->fixtureUnit(fid), 0);
        QCOMPARE(doc.monitorProperties()->fixtureChannel(fid), 0);
    }
}

void StudioWins_Test::numberingCanPatchInThatOrderIntoFreeAddresses()
{
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    const int xs[] = { 3000, 1000, 2000 };
    QList<quint32> ids;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fx = makePar(&doc, quint32(100 + i * 6));
        QVERIFY(fx != nullptr);
        doc.monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(float(xs[i]), 2000.0f, 3000.0f));
        ids << fx->id();
    }
    // Another fixture holds 120-125; the three start at the lowest of theirs, 100.
    Fixture *other = makePar(&doc, 0);
    QVERIFY(other != nullptr);
    QVERIFY(doc.patchFixtures({ Doc::FixturePatch{ other->id(), 0, 120 } }));
    const QList<PlotNumbering::Row> rows = PlotNumbering::plan(
        &doc, ids, PlotNumbering::StageRightToLeft, true, 1, false, 1, true);
    QVERIFY(PlotNumbering::apply(&doc, rows));
    // x order: ids[1], ids[2], ids[0]; 6 channels each from 100; 120-125 is taken.
    QCOMPARE(doc.fixture(ids.at(1))->address(), quint32(100));
    QCOMPARE(doc.fixture(ids.at(2))->address(), quint32(106));
    QCOMPARE(doc.fixture(ids.at(0))->address(), quint32(112));
}

void StudioWins_Test::theNumberItemIsOnTheSelectionMenuOfThePlot()
{
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    QWidget parent;
    Monitor mon(&parent, &doc);
    QList<quint32> ids;
    for (int i = 0; i < 2; ++i)
        ids << makePar(&doc, quint32(i * 6))->id();
    for (bool offer : { true, false })
    {
        QMenu menu;
        ContextMenu cm(&menu);
        ContextMenus::RigPlace rig;
        rig.offerNumber = offer;
        ContextMenus::rig(cm, &doc, QList<quint32>(), ids, rig);
        QCOMPARE(menu.findChild<QAction *>(QStringLiteral("ctxNumber")) != nullptr, offer);
    }
}

void StudioWins_Test::theDmxOverlayOutlinesPatchClashesAndFixingClearsThem()
{
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    QWidget parent;
    Monitor mon(&parent, &doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    // a and b overlap (0 and 3 of 6 channels); c is fine; d runs past 512; e is unpatched.
    Fixture *a = makePar(&doc, 0);
    Fixture *b = makePar(&doc, 100);
    Fixture *c = makePar(&doc, 200);
    Fixture *d = makePar(&doc, 300);
    Fixture *e = makePar(&doc, 400);
    QVERIFY(a && b && c && d && e);
    // The doc refuses these placements, so write them as a loaded show could hold them.
    b->setAddress(3);
    d->setAddress(510);
    doc.unpatchFixture(e->id());
    int x = 1000;
    for (Fixture *f : { a, b, c, d, e })
    {
        doc.monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(float(x), 2000.0f, 0.0f));
        if (gv->fixtureItemForId(f->id()) == nullptr)
            gv->addFixture(f->id(), QPointF(x, 2000.0));
        x += 1500;
    }

    mon.applyMapView(Monitor::ViewDMX);
    auto item = [&](Fixture *f) { return gv->fixtureItemForId(f->id()); };
    QVERIFY(item(a)->hasPatchClash());
    QVERIFY(item(b)->hasPatchClash());
    QVERIFY(item(a)->patchClashReason().contains(QStringLiteral("share channels")));
    QVERIFY(item(d)->hasPatchClash());
    QVERIFY(item(d)->patchClashReason().contains(QStringLiteral("past the end")));
    QVERIFY(item(e)->hasPatchClash());
    QVERIFY(item(e)->toolTip().contains(QStringLiteral("patched")));
    QVERIFY(!item(c)->hasPatchClash());
    QVERIFY(item(a)->toolTip().contains(item(a)->patchClashReason().section('\n', 0, 0)));

    // Fix the patch: the outlines go.
    b->setAddress(100);
    d->setAddress(300);
    e->setPatched(true);
    e->setUniverse(0);
    e->setAddress(400);
    mon.applyMapView(Monitor::ViewDMX);
    for (Fixture *f : { a, b, c, d, e })
        QVERIFY2(!item(f)->hasPatchClash(), qPrintable(item(f)->patchClashReason()));

    // And another view never draws them.
    b->setAddress(3);
    mon.applyMapView(Monitor::ViewDMX);
    QVERIFY(item(a)->hasPatchClash());
    mon.applyMapView(Monitor::ViewNormal);
    QVERIFY(!item(a)->hasPatchClash());
}

void StudioWins_Test::labelFieldsRoundTripWithTheShow()
{
    MonitorProperties mp;
    QCOMPARE(mp.labelFields(), int(MonitorProperties::LabelName));
    mp.setLabelFields(MonitorProperties::LabelName | MonitorProperties::LabelUnit | MonitorProperties::LabelOwner);
    mp.setFixtureUnit(7, 12);
    mp.setFixtureChannel(7, 101);
    mp.setFixturePurpose(7, QStringLiteral("Front wash"));
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    QCOMPARE(mp2.labelFields(), mp.labelFields());
    QCOMPARE(mp2.fixtureUnit(7), 12);
    QCOMPARE(mp2.fixtureChannel(7), 101);
    QCOMPARE(mp2.fixturePurpose(7), QStringLiteral("Front wash"));
}

void StudioWins_Test::theLabelSaysTheChosenFieldsAroundTheIcon()
{
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    QWidget parent;
    Monitor mon(&parent, &doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    MonitorProperties *props = doc.monitorProperties();
    QList<Fixture *> fxs;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = makePar(&doc, quint32(i * 10));
        QVERIFY(f != nullptr);
        f->setName(QStringLiteral("Wash %1").arg(i + 1));
        props->setFixtureUnit(f->id(), 20 + i);
        props->setFixtureChannel(f->id(), 100 + i);
        props->setFixturePurpose(f->id(), QStringLiteral("Front"));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(float(2000 + i * 3000), 4000.0f, 0.0f));
        gv->addFixture(f->id(), QPointF(2000 + i * 3000, 4000.0));
        fxs << f;
    }
    gv->showFixturesLabels(true);

    // Name only, as every show always had it.
    MonitorFixtureItem *it = gv->fixtureItemForId(fxs.first()->id());
    QVERIFY(it != nullptr);
    QCOMPARE(it->labelBelow(), QStringLiteral("Wash 1"));
    QCOMPARE(it->labelAbove(), QString());

    const int all = MonitorProperties::LabelName | MonitorProperties::LabelUnit
                    | MonitorProperties::LabelChannel | MonitorProperties::LabelAddress
                    | MonitorProperties::LabelPurpose | MonitorProperties::LabelMode
                    | MonitorProperties::LabelOwner;
    props->setLabelFields(all);
    gv->refreshFixtureLabels();
    QCOMPARE(it->labelAbove(), QStringLiteral("U20 · Ch 100 · 1.1"));
    const QStringList below = it->labelBelow().split(QLatin1Char('\n'));
    QCOMPARE(below.value(0), QStringLiteral("Wash 1"));
    QCOMPARE(below.value(1), QStringLiteral("Front"));
    QCOMPARE(below.value(2), fxs.first()->fixtureMode()->name());
    QCOMPARE(below.size(), 4);                      // + the owner

    // Only the unit number: nothing below at all.
    props->setLabelFields(MonitorProperties::LabelUnit);
    gv->refreshFixtureLabels();
    QCOMPARE(it->labelAbove(), QStringLiteral("U20"));
    QCOMPARE(it->labelBelow(), QString());

    // The menu in Show sets it, per field.
    QAction *nameAct = mon.findChild<QAction *>(QStringLiteral("labelFieldName"));
    QVERIFY(nameAct != nullptr);
    nameAct->setChecked(false);                     // the menu starts as the show did: Name on
    nameAct->setChecked(true);
    QCOMPARE(props->labelFields(), int(MonitorProperties::LabelUnit | MonitorProperties::LabelName));
    QCOMPARE(it->labelBelow(), QStringLiteral("Wash 1"));

    // Nothing touches its neighbours, at three plot sizes.
    props->setLabelFields(all);
    gv->refreshFixtureLabels();
    for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        parent.resize(sz);
        mon.resize(sz);
        gv->resize(sz);
        QApplication::processEvents();
        QList<QRectF> rects;
        for (Fixture *f : fxs)
            rects << gv->fixtureItemForId(f->id())->labelSceneRects();
        QVERIFY(rects.size() >= 8);
        for (int a = 0; a < rects.size(); ++a)
            for (int b = a + 1; b < rects.size(); ++b)
            {
                // Two boxes of one fixture sit above and below its icon: never touching.
                QVERIFY2(!rects.at(a).intersects(rects.at(b)),
                         qPrintable(QStringLiteral("labels %1 and %2 overlap at %3x%4")
                                        .arg(a).arg(b).arg(sz.width()).arg(sz.height())));
            }
    }
}

QTEST_MAIN(StudioWins_Test)
