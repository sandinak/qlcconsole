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
#include "doc.h"

#include "studiowins_test.h"

namespace
{
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

QTEST_MAIN(StudioWins_Test)
