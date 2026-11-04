/*
  Q Light Controller Plus
  pixelpathdecor.cpp

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

#include <QPainterPath>
#include <QtMath>

#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "monitorproperties.h"
#include "pixelpathdecor.h"
#include "pixelpath.h"
#include "righeads.h"
#include "fountain.h"
#include "fixture.h"
#include "doc.h"

int PixelPathDecor::ledCount(Doc *doc, quint32 fid)
{
    Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    if (fx == nullptr)
        return 0;
    if (fx->heads() > 1)
        return fx->heads();
    return qMax(1, RigHeads::lightLeds(doc, fid));
}

void PixelPathDecor::apply(const MonitorGraphicsView *view, Doc *doc, MonitorFixtureItem *item,
                           quint32 fid)
{
    if (view == nullptr || doc == nullptr || item == nullptr)
        return;
    MonitorProperties *props = doc->monitorProperties();
    Fixture *fx = doc->fixture(fid);
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    auto toItem = [view, item](const QVector3D &w) {
        return item->mapFromScene(view->projectMm(double(w.x()) * 1000.0, double(w.y()) * 1000.0,
                                                  double(w.z()) * 1000.0));
    };

    QVector<QPointF> pixels;
    QLineF staff;
    QPainterPath flame;
    if (rp.pixelPath != PixelPath::None)
    {
        /* Its body is only the controller: a small box where it is, the LEDs
           along the path instead of a strip as long as it would be. */
        const QPointF mid = item->mapToScene(QPointF(item->cellSize().width() / 2.0,
                                                     item->cellSize().height() / 2.0));
        const int side = 12;
        if (item->cellSize() != QSize(side, side))
        {
            item->setSize(QSize(side, side));
            item->setPos(mid - QPointF(side / 2.0, side / 2.0));
        }
        foreach (const QVector3D &w, PixelPath::positions(doc, props, fid, ledCount(doc, fid)))
            pixels << toItem(w);
        QVector3D b, t;
        if (rp.pixelPath == PixelPath::Wand && PixelPath::wandEnds(props, fid, b, t))
            staff = QLineF(toItem(b), toItem(t));
    }
    if (Fountain::isFlame(fx))
    {
        /* The silk: from where it starts (the inner wall's top), its fire
           height up, as wide as the unit at the foot and coming to a tip. */
        const QVector3D at = props->fixtureRigPosition(fid);
        const float z0 = Fountain::flameBottom(doc, props, fid);
        const float h = Fountain::flameHeight(doc, props, fid);
        const float r = float(Fountain::flameUnitSize(fx).x() / 2.0);
        if (view->isElevation())
        {
            // Across the view: X in Front, Y in Side.
            const QPointF side = (view->projectMm(1000.0, 0, 0) - view->projectMm(0, 0, 0));
            const QVector3D across = qAbs(side.x()) > 0.5 ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
            const QVector3D foot(at.x(), at.y(), z0);
            flame.moveTo(toItem(foot - across * r));
            flame.cubicTo(toItem(foot - across * (r * 1.1f) + QVector3D(0, 0, h * 0.45f)),
                          toItem(foot - across * (r * 0.25f) + QVector3D(0, 0, h * 0.75f)),
                          toItem(foot + QVector3D(0, 0, h)));
            flame.cubicTo(toItem(foot + across * (r * 0.25f) + QVector3D(0, 0, h * 0.75f)),
                          toItem(foot + across * (r * 1.1f) + QVector3D(0, 0, h * 0.45f)),
                          toItem(foot + across * r));
            flame.closeSubpath();
        }
        else
        {
            // From above: the silk's round, the tongues a star in it.
            const QPointF c = toItem(QVector3D(at.x(), at.y(), z0));
            const QPointF e = toItem(QVector3D(at.x() + r, at.y(), z0));
            const qreal rp2 = qMax(4.0, QLineF(c, e).length());
            flame.addEllipse(c, rp2, rp2);
            for (int k = 0; k < 6; ++k)
            {
                const double a = k * M_PI / 3.0;
                flame.moveTo(c);
                flame.lineTo(c + QPointF(qCos(a), qSin(a)) * rp2 * 0.7);
            }
        }
    }
    item->setPathDecor(pixels, staff, flame);
}

bool MonitorGraphicsView::dropIntoFountain(quint32 fid, const QPointF &mmCentre)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const double x = mmCentre.x() / 1000.0, y = mmCentre.y() / 1000.0;
    const Fountain::Parts f = Fountain::at(props, x, y);
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (!f.isValid())
    {
        // Out of the fountain it stood in: no longer one of its pieces.
        const Fountain::Parts was = Fountain::ofBase(props, rp.deckPlatformId);
        if (was.isValid() && props->fixtureGroup(fid) == was.groupId)
            props->setFixtureGroup(fid, 0);
        return false;       // the deck rule takes it off the base
    }
    if (rp.deckPlatformId != f.base->id() || props->fixtureGroup(fid) != f.groupId)
        return Fountain::mountIn(m_doc, props, fid, x, y);
    // Moved within it: on the inner wall's top, or on the base.
    rp.deckHeightOffset = Fountain::onInnerWall(f, x, y)
        ? qMax(0.0f, Fountain::innerTop(f) - Fountain::baseTop(props, f)) : 0.0f;
    props->setFixtureRigProps(fid, rp);
    return true;
}
