/*
  Q Light Controller Plus - Test Unit
  bandtargets_test.cpp

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
#include <QApplication>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDir>
#include <QComboBox>
#include <QAbstractItemView>
#include <QTimer>
#include <QDialog>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "structurestudioview.h"
#include "targetitem.h"
#undef private
#undef protected

#include "bandtargets_test.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "doc.h"

namespace
{

const StageTarget::Kind kBand[] = { StageTarget::Kind::Keyboard, StageTarget::Kind::GrandPiano,
                                    StageTarget::Kind::UprightPiano, StageTarget::Kind::Rack,
                                    StageTarget::Kind::Speaker };

StageTarget *place(MonitorProperties *props, StageTarget::Kind k, float x, float y, const QString &name)
{
    StageTarget *t = props->addStageTarget();
    t->setName(name);
    // QLC_BAND_PLAIN: the same layout as plain marks, for the before pictures.
    t->setKind(k);
    if (qEnvironmentVariableIsSet("QLC_BAND_PLAIN") && StageTarget::isBandKind(k))
        t->setKind(StageTarget::Kind::None);
    t->setPosition(QVector3D(x, y, 0.0f));
    if (t->kind() == k)
        t->setFootprint(StageTarget::defaultFootprint(k));
    return t;
}

/* The showband, between the risers and the cyc: kit, keys, a grand, an upright,
   a rack, two speakers and two people. */
void showband(MonitorProperties *props)
{
    place(props, StageTarget::Kind::DrumKit, 5.0f, 2.2f, "Drums")->setFootprint(QSizeF(2.0, 1.6));
    place(props, StageTarget::Kind::Keyboard, 2.2f, 1.6f, "Keys");
    place(props, StageTarget::Kind::GrandPiano, 8.3f, 2.2f, "Grand");
    place(props, StageTarget::Kind::UprightPiano, 6.6f, 0.9f, "Upright");
    place(props, StageTarget::Kind::Rack, 3.4f, 0.8f, "FX rack");
    place(props, StageTarget::Kind::Speaker, 1.2f, 2.4f, "Wedge L")->setRotation(20.0f);
    place(props, StageTarget::Kind::Speaker, 9.2f, 3.9f, "Wedge R")->setRotation(-30.0f);
    place(props, StageTarget::Kind::Person, 4.0f, 3.4f, "Vocal")->setFootprint(QSizeF());
    place(props, StageTarget::Kind::Person, 6.0f, 3.6f, "Guitar")->setFootprint(QSizeF());
}

struct Plot
{
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;
    explicit Plot(Doc *doc)
    {
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        gv->resize(1000, 700);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(10, 7));
        gv->updatePlatforms();
        gv->updateTargets();
    }
    ~Plot() { delete mon; }
};

} // namespace

void BandTargets_Test::sizesAndHeights()
{
    QCOMPARE(StageTarget::defaultAimHeight(StageTarget::Kind::Keyboard), 1.1f);
    QCOMPARE(StageTarget::defaultAimHeight(StageTarget::Kind::GrandPiano), 1.1f);
    QCOMPARE(StageTarget::defaultAimHeight(StageTarget::Kind::UprightPiano), 1.2f);
    QCOMPARE(StageTarget::defaultAimHeight(StageTarget::Kind::Rack), 1.0f);
    QCOMPARE(StageTarget::defaultAimHeight(StageTarget::Kind::Speaker), 0.8f);
    QCOMPARE(StageTarget::defaultFootprint(StageTarget::Kind::Keyboard), QSizeF(1.4, 0.5));
    QCOMPARE(StageTarget::defaultFootprint(StageTarget::Kind::GrandPiano), QSizeF(1.5, 1.8));
    QCOMPARE(StageTarget::defaultFootprint(StageTarget::Kind::UprightPiano), QSizeF(1.5, 0.6));
    QCOMPARE(StageTarget::defaultFootprint(StageTarget::Kind::Rack), QSizeF(0.6, 0.6));
    QCOMPARE(StageTarget::defaultFootprint(StageTarget::Kind::Speaker), QSizeF(0.55, 0.5));
    QCOMPARE(StageTarget::bodyHeight(StageTarget::Kind::Rack), 1.2f);
    QCOMPARE(StageTarget::bodyHeight(StageTarget::Kind::Speaker), 1.1f);   // 1.0 m box, 0.1 m casters
    for (StageTarget::Kind k : kBand)
    {
        QVERIFY(StageTarget::isBandKind(k));
        QVERIFY(StageTarget::hasFootprintKind(k));
    }
    QVERIFY(!StageTarget::isBandKind(StageTarget::Kind::DrumKit));
    QVERIFY(StageTarget::hasFootprintKind(StageTarget::Kind::DrumKit));
}

void BandTargets_Test::saveAndLoadRoundTrip()
{
    const char *names[] = { "keyboard", "grandpiano", "uprightpiano", "rack", "speaker" };
    for (int i = 0; i < 5; ++i)
    {
        QByteArray buf;
        {
            MonitorProperties mp;
            StageTarget *t = place(&mp, kBand[i], 1.0f, 2.0f, "Gear");
            t->setFootprint(QSizeF(1.1, 0.7));            // resized
            t->setRotation(i == 4 ? 35.0f : 0.0f);
            QXmlStreamWriter w(&buf);
            w.writeStartDocument();
            t->saveXML(&w);
            w.writeEndDocument();
            QVERIFY(buf.contains(QByteArray("Kind=\"") + names[i] + "\""));
        }
        MonitorProperties mp2;
        StageTarget *t2 = mp2.addStageTarget();
        QXmlStreamReader r(buf);
        QVERIFY(r.readNextStartElement());
        QVERIFY(t2->loadXML(r));
        QCOMPARE(int(t2->kind()), int(kBand[i]));
        QCOMPARE(StageTarget::kindToString(kBand[i]), QString::fromLatin1(names[i]));
        QVERIFY(qAbs(t2->footprint().width() - 1.1) < 0.001);
        QVERIFY(qAbs(t2->footprint().height() - 0.7) < 0.001);
        QVERIFY(qAbs(double(t2->rotation()) - (i == 4 ? 35.0 : 0.0)) < 0.01);
        QCOMPARE(t2->effectiveAimHeight(), StageTarget::defaultAimHeight(kBand[i]));
        if (i != 4)
            QVERIFY2(!buf.contains("Rotation="), "an unturned target writes a rotation");
    }
    // A newer build's kind degrades to a plain mark, as the old ones always did.
    QCOMPARE(int(StageTarget::stringToKind("trombone")), int(StageTarget::Kind::None));
}

void BandTargets_Test::aimAtLandsAtTheAimHeight()
{
    MonitorProperties mp;
    StagePlatform *deck = mp.addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(6.0f); deck->setDepth(4.0f); deck->setHeight(0.4f);
    const struct { StageTarget::Kind k; double h; } want[] = {
        { StageTarget::Kind::Keyboard, 1.1 }, { StageTarget::Kind::GrandPiano, 1.1 },
        { StageTarget::Kind::UprightPiano, 1.2 }, { StageTarget::Kind::Rack, 1.0 },
        { StageTarget::Kind::Speaker, 0.8 } };
    for (const auto &w : want)
    {
        StageTarget *t = place(&mp, w.k, 3.0f, 2.0f, "Gear");
        QVERIFY2(qAbs(double(mp.targetAimPoint(t).z()) - (0.4 + w.h)) < 0.005,
                 qPrintable(StageTarget::kindToString(w.k)));
        t->setAimHeightOverride(0.5f);                      // one override still beats the kind
        QVERIFY(qAbs(double(mp.targetAimPoint(t).z()) - 0.9) < 0.005);
    }
}

void BandTargets_Test::drawnOnThePlot()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    showband(props);
    Plot plot(&doc);
    const double ppm = 1000.0 * plot.gv->m_cellPixels / plot.gv->m_unitValue;
    foreach (StageTarget *t, props->stageTargets())
    {
        if (!StageTarget::isBandKind(t->kind()))
            continue;
        TargetItem *ti = plot.gv->m_targetItems.value(t->id(), nullptr);
        QVERIFY(ti != nullptr);
        QVERIFY2(!ti->bandOutline().isEmpty(), qPrintable(t->name()));
        // At its real size from above, and never smaller than a readable box.
        const QRectF r = ti->bandRect();
        QVERIFY2(r.width() >= 16.0 - 1e-6 && r.height() >= 12.0 - 1e-6, qPrintable(t->name()));
        QVERIFY(qAbs(r.width() - qMax(16.0, t->footprint().width() * ppm)) < 0.5);
        QVERIFY(qAbs(r.height() - qMax(12.0, t->footprint().height() * ppm)) < 0.5);
        QVERIFY(ti->boundingRect().contains(ti->bandBounds()));
        QVERIFY(ti->boundingRect().contains(ti->bandOutline().boundingRect()));
    }
    // In an elevation the silhouette stands to its height.
    plot.gv->setViewPOV(MonitorGraphicsView::PovFront);
    plot.gv->updateTargets();
    foreach (StageTarget *t, props->stageTargets())
        if (StageTarget::isBandKind(t->kind()))
        {
            TargetItem *ti = plot.gv->m_targetItems.value(t->id(), nullptr);
            QVERIFY(ti != nullptr);
            QVERIFY2(!ti->bandOutline().isEmpty(), qPrintable(t->name()));
            const QRectF r = ti->bandRect();
            QVERIFY2(qAbs(r.height() - double(StageTarget::bodyHeight(t->kind())) * ppm) < 1.0
                         || r.height() <= 12.0 * 0.7 + 1.0, qPrintable(t->name()));
        }
}

void BandTargets_Test::turnsWithTheBrackets()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *sp = place(props, StageTarget::Kind::Speaker, 3.0f, 3.0f, "Wedge");
    StageTarget *kb = place(props, StageTarget::Kind::Keyboard, 6.0f, 3.0f, "Keys");
    Plot plot(&doc);
    QVERIFY(qFuzzyIsNull(sp->rotation()));
    plot.gv->m_targetItems.value(sp->id())->setSelected(true);
    QCOMPARE(plot.gv->rotateSelectedItems(15.0), 1);
    QVERIFY(qAbs(double(sp->rotation()) - 15.0) < 0.01);
    QVERIFY(qFuzzyIsNull(kb->rotation()));
    plot.gv->m_targetItems.value(sp->id())->setSelected(true);      // a rebuild drops the selection
    plot.gv->rotateSelectedItems(-15.0);
    QVERIFY(qAbs(double(sp->rotation())) < 0.01);
}

void BandTargets_Test::drawnInTheRigView()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(10.0f); deck->setDepth(5.0f); deck->setHeight(0.4f);
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(700, 450);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    const QImage empty = v.grab().toImage();
    for (StageTarget::Kind k : kBand)
    {
        StageTarget *t = place(props, k, 5.0f, 2.5f, "Gear");
        props->notifyStageTargetsChanged();
        v.reload();
        QVERIFY2(v.grab().toImage() != empty, qPrintable(StageTarget::kindToString(k)));
        props->removeStageTarget(t->id());
        props->notifyStageTargetsChanged();
        v.reload();
    }
}

void BandTargets_Test::showbandShots()
{
    const QString dir = qEnvironmentVariable("QLC_BAND_SHOTS");
    if (dir.isEmpty())
        QSKIP("set QLC_BAND_SHOTS to a folder to write the showband pictures");
    QDir().mkpath(dir);
    const QString tag = qEnvironmentVariable("QLC_BAND_TAG", "after");
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(10.0f); deck->setDepth(5.0f); deck->setHeight(0.4f);
    props->setGridSize(QVector3D(11.0f, 6.0f, 5.0f));
    showband(props);
    const QSize sizes[] = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    const char *sn[] = { "1024x640", "1280x800", "1920x1080" };
    for (int i = 0; i < 3; ++i)
    {
        {
            Monitor mon(nullptr, &doc);
            mon.resize(sizes[i]);
            mon.show();
            QTest::qWait(150);
            MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
            gv->updateTargets();
            QTest::qWait(100);
            QVERIFY(mon.grab().save(QStringLiteral("%1/%2-plot-%3.png").arg(dir, tag, sn[i])));
            gv->setViewPOV(MonitorGraphicsView::PovFront);
            gv->updateTargets();
            QTest::qWait(100);
            QVERIFY(mon.grab().save(QStringLiteral("%1/%2-plot-front-%3.png").arg(dir, tag, sn[i])));
        }
        StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
        v.resize(sizes[i]);
        v.show();
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        QTest::qWait(150);
        QVERIFY(v.grab().save(QStringLiteral("%1/%2-rig-%3.png").arg(dir, tag, sn[i])));
    }
}

void BandTargets_Test::editTargetDialogShots()
{
    const QString dir = qEnvironmentVariable("QLC_BAND_SHOTS");
    if (dir.isEmpty())
        QSKIP("set QLC_BAND_SHOTS to a folder to write the pictures");
    QDir().mkpath(dir);
    Doc doc(this);
    QWidget parent;
    Monitor mon(&parent, &doc);
    QTimer::singleShot(0, [&]() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != nullptr);
        QComboBox *kind = nullptr;
        foreach (QComboBox *c, modal->findChildren<QComboBox *>())
            if (c->findText("Speaker (on wheels)") >= 0)
                kind = c;
        QVERIFY(kind != nullptr);
        kind->setCurrentIndex(kind->findText("Speaker (on wheels)"));
        QVERIFY(modal->grab().save(dir + "/after-edit-target-speaker.png"));
        kind->showPopup();
        QTest::qWait(100);
        QWidget *popup = kind->view()->window();
        QVERIFY(popup->grab().save(dir + "/after-edit-target-subject-list.png"));
        kind->hidePopup();
        if (QDialog *d = qobject_cast<QDialog *>(modal))
            d->reject();
    });
    mon.slotAddTarget();
}

QTEST_MAIN(BandTargets_Test)
