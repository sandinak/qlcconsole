/*
  Q Light Controller Plus - Test Unit
  networknodeitem_test.cpp

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
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QApplication>
#include <QPainter>
#include <QImage>
#include <QMenu>
#include <functional>
#include <memory>

#include "networknodeitem.h"
#include "networktopology.h"

#include "networknodeitem_test.h"
#include "apputil.h"

/* Paint @p item alone into an image, the item's local origin at @p origin,
 * optionally selected and turned by @p turnDeg (as a rotated Studio view does). */
static QImage paintItem(QGraphicsItem *item, bool selected = false, qreal turnDeg = 0,
                        QPointF origin = QPointF(10, 10))
{
    QImage img(120, 60, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    p.translate(origin);
    p.rotate(turnDeg);
    QStyleOptionGraphicsItem opt;
    if (selected)
        opt.state |= QStyle::State_Selected;
    item->paint(&p, &opt, nullptr);
    p.end();
    return img;
}

/* True when two colours match to within anti-aliasing noise. */
static bool nearPt(QColor a, QColor b)
{
    return qAbs(a.red() - b.red()) < 12 && qAbs(a.green() - b.green()) < 12
           && qAbs(a.blue() - b.blue()) < 12;
}

void NetworkNodeItem_Test::eachTransportPaintsItsOwnColour()
{
    const QList<QPair<int, QColor>> expected = {
        { NetworkNode::ArtNet,      QColor(70, 140, 220) },
        { NetworkNode::sACN,        QColor(160, 100, 210) },
        { NetworkNode::Interface,   QColor(150, 150, 150) },
        { NetworkNode::WirelessDMX, QColor(235, 140, 40) },
    };
    for (const auto &e : expected)
    {
        NetworkNodeItem item(0, "N", e.first);
        const QImage img = paintItem(&item);
        // Inside the marker, clear of the glyph and the name text.
        const QColor fill = img.pixelColor(10 + 4, 10 + 21);
        QVERIFY2(nearPt(fill, e.second),
                 qPrintable(QString("transport %1 painted %2, want %3")
                            .arg(e.first).arg(fill.name(), e.second.name())));
    }
}

void NetworkNodeItem_Test::aLockedNodeHasARedBorderAndCannotMove()
{
    NetworkNodeItem open(0, "Open", NetworkNode::ArtNet, false);
    NetworkNodeItem locked(0, "Locked", NetworkNode::ArtNet, true);
    const QColor openEdge = paintItem(&open).pixelColor(10 + 36, 10);
    const QColor lockedEdge = paintItem(&locked, true).pixelColor(10 + 36, 10);
    QVERIFY2(lockedEdge.red() > 150 && lockedEdge.blue() < 120, qPrintable(lockedEdge.name()));
    QVERIFY2(openEdge.red() < 100, qPrintable(openEdge.name()));

    QVERIFY(locked.locked() && !open.locked());
    open.setMovable(false);
    QVERIFY(!(open.flags() & QGraphicsItem::ItemIsMovable));
    QCOMPARE(open.cursor().shape(), Qt::ArrowCursor);
    open.setMovable(true);
    QVERIFY(open.flags() & QGraphicsItem::ItemIsMovable);
    QCOMPARE(open.cursor().shape(), AppUtil::nativeCursorShape(Qt::SizeAllCursor));
}

void NetworkNodeItem_Test::labelsStayUprightInARotatedView()
{
    // A turned view counter-rotates the label; the marker itself turns with
    // the view. Either way the paint must land and not leave the image blank.
    NetworkNodeItem node(0, "Turned", NetworkNode::sACN);
    const QImage img = paintItem(&node, false, 90, QPointF(60, 5));
    int lit = 0;
    for (int y = 0; y < img.height(); y++)
        for (int x = 0; x < img.width(); x++)
            if (img.pixelColor(x, y) != QColor(Qt::black))
                lit++;
    QVERIFY(lit > 500);

    NetworkReceiverItem rx(0, 0, "RX");
    const QImage rimg = paintItem(&rx, false, 45, QPointF(30, 30));
    QVERIFY(rimg.pixelColor(30, 30 + 7) != QColor(Qt::black));
}

void NetworkNodeItem_Test::aReceiverPaintsAnAmberDisc()
{
    NetworkReceiverItem rx(2, 3, "Rig RX");
    QCOMPARE(rx.nodeIndex(), 2);
    QCOMPARE(rx.portIndex(), 3);
    QVERIFY(rx.toolTip().contains("Rig RX") && rx.toolTip().contains("port 4"));

    const QImage img = paintItem(&rx, true, 0, QPointF(30, 30));
    // Below the "RX" text, inside the disc; outside the disc stays black.
    QVERIFY2(nearPt(img.pixelColor(30, 30 + 7), QColor(235, 140, 40)),
             qPrintable(img.pixelColor(30, 37).name()));
    QCOMPARE(img.pixelColor(30 + 15, 30 + 15), QColor(Qt::black));
}

void NetworkNodeItem_Test::releasingADragReportsTheDrop()
{
    QGraphicsScene scene;
    NetworkNodeItem *node = new NetworkNodeItem(4, "Drag me", NetworkNode::ArtNet);
    scene.addItem(node);
    QSignalSpy spy(node, &NetworkNodeItem::itemDropped);

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setButton(Qt::LeftButton);
    scene.sendEvent(node, &release);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().value<NetworkNodeItem *>(), node);
    QCOMPARE(node->nodeIndex(), 4);
}

/* Open @p item's context menu, pick the action whose text starts with
 * @p pick (or none), and return every action text seen. */
static QStringList runContextMenu(QGraphicsScene &scene, QGraphicsItem *item, const QString &pick)
{
    QStringList texts;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&texts, pick, poll]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        QAction *chosen = nullptr;
        foreach (QAction *a, m->actions())
        {
            texts << a->text();
            if (!pick.isEmpty() && a->text().startsWith(pick))
                chosen = a;
        }
        // exec() only reports an action picked through the menu itself.
        if (chosen != nullptr)
        {
            m->setActiveAction(chosen);
            QTest::keyClick(m, Qt::Key_Return);
        }
        else
        {
            m->close();
        }
    };
    QTimer::singleShot(0, *poll);
    QGraphicsSceneContextMenuEvent ev(QEvent::GraphicsSceneContextMenu);
    ev.setScreenPos(QPoint(100, 100));
    scene.sendEvent(item, &ev);
    return texts;
}

void NetworkNodeItem_Test::theContextMenuTogglesTheLock()
{
    QGraphicsScene scene;
    NetworkNodeItem *open = new NetworkNodeItem(1, "Open", NetworkNode::ArtNet, false);
    NetworkNodeItem *locked = new NetworkNodeItem(2, "Locked", NetworkNode::ArtNet, true);
    scene.addItem(open);
    scene.addItem(locked);
    QSignalSpy openSpy(open, &NetworkNodeItem::lockToggleRequested);
    QSignalSpy lockedSpy(locked, &NetworkNodeItem::lockToggleRequested);

    // Dismissed: nothing toggles.
    QStringList texts = runContextMenu(scene, open, QString());
    QCOMPARE(texts, QStringList({ "Lock Network Node" }));
    QCOMPARE(openSpy.count(), 0);

    texts = runContextMenu(scene, locked, "Unlock");
    QCOMPARE(texts, QStringList({ "Unlock Network Node" }));
    QCOMPARE(lockedSpy.count(), 1);
    QCOMPARE(lockedSpy.first().first().toInt(), 2);
}

void NetworkNodeItem_Test::aRunIsHittableAlongItsLengthOnly()
{
    QPainterPath path(QPointF(0, 0));
    path.lineTo(100, 0);
    path.lineTo(100, 50);
    NetworkRunItem run(path, QColor(Qt::green), "U1: 3 fixtures, 12.0 m", 5, 6);
    QCOMPARE(run.nodeIndex(), 5);
    QCOMPARE(run.portIndex(), 6);
    QCOMPARE(run.stats(), QString("U1: 3 fixtures, 12.0 m"));
    QVERIFY(run.pen().isCosmetic());
    QCOMPARE(run.acceptedMouseButtons(), Qt::NoButton);

    QVERIFY(run.shape().contains(QPointF(50, 3)));      // on the first leg
    QVERIFY(run.shape().contains(QPointF(98, 25)));     // on the second
    QVERIFY(!run.shape().contains(QPointF(50, 25)));    // inside the corner, not on the cable
}

QTEST_MAIN(NetworkNodeItem_Test)
