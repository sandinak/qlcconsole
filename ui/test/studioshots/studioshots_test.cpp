#include <QDockWidget>
#include <QDir>
#include <QtTest>
#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "structurestudioview.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "fixture.h"
#include "doc.h"

#include "studioshots_test.h"

namespace
{
Fixture *makeFixture(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Wash");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    QLCFixtureHead hd;
    for (quint32 c = 0; c < 6; ++c)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(c));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
        hd.addChannel(c);
    }
    mode->insertHead(-1, hd);
    QLCPhysical ph;
    ph.setWidth(200); ph.setHeight(250); ph.setDepth(200);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setFixtureDefinition(def, mode);
    f->setAddress(address);
    doc->addFixture(f);
    return f;
}
}

void StudioShots_Test::picturesForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    Doc doc(nullptr);
    doc.setMode(Doc::Design);
    QWidget parent;
    Monitor mon(&parent, &doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    MonitorProperties *props = doc.monitorProperties();
    QList<Fixture *> fxs;
    const char *names[] = { "FOH Wash 1", "FOH Wash 2", "FOH Wash 3", "FOH Wash 4" };
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = makeFixture(&doc, QString::fromLatin1(names[i]), quint32(i * 10));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(float(3000 + i * 3500), 4000.0f, 0.0f));
        gv->addFixture(f->id(), QPointF(3000 + i * 3500, 4000.0));
        fxs << f;
    }
    // Wash 2 sits on Wash 1's channels; Wash 4 runs past 512.
    fxs.at(1)->setAddress(3);
    fxs.at(3)->setAddress(510);
    gv->showFixturesLabels(true);
#ifdef STUDIO_NEW
    for (int i = 0; i < 4; ++i)
    {
        props->setFixtureUnit(fxs.at(i)->id(), 11 + i);
        props->setFixtureChannel(fxs.at(i)->id(), 101 + i);
    }
    props->setLabelFields(MonitorProperties::LabelName | MonitorProperties::LabelUnit
                          | MonitorProperties::LabelChannel | MonitorProperties::LabelAddress);
    gv->refreshFixtureLabels();
#endif
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        parent.resize(sz);
        mon.resize(sz);
        QApplication::processEvents();
        mon.applyMapView(Monitor::ViewNormal);
        QApplication::processEvents();
        parent.grab().save(QString("%1/plot-labels-%2x%3.png").arg(dir).arg(sz.width()).arg(sz.height()));
        mon.applyMapView(Monitor::ViewDMX);
        QApplication::processEvents();
        parent.grab().save(QString("%1/plot-dmx-overlay-%2x%3.png").arg(dir).arg(sz.width()).arg(sz.height()));
    }
    mon.applyMapView(Monitor::ViewNormal);

    mon.toggleStageOverview();
    QVERIFY(mon.m_overviewDlg != nullptr);
    for (const QSize &sz : sizes)
    {
        mon.m_overviewDlg->resize(sz);
        QApplication::processEvents();
        mon.m_overviewDlg->grab().save(QString("%1/rig-overview-%2x%3.png").arg(dir).arg(sz.width()).arg(sz.height()));
    }
}

QTEST_MAIN(StudioShots_Test)
