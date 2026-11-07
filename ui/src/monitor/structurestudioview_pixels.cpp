/*
  Q Light Controller Plus
  structurestudioview_pixels.cpp

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

/* The rig view's pixel paths, wands and the light on a fountain's flame
   (docs/design/FOUNTAIN_WAND.md): kept out of structurestudioview.cpp. */

#include <QtMath>

#include "structurestudioview.h"
#include "fixturevisualtraits.h"
#include "monitorproperties.h"
#include "pixelpathdecor.h"
#include "lanternpixels.h"
#include "pixelpath.h"
#include "fountain.h"
#include "righeads.h"
#include "fixture.h"
#include "doc.h"

QColor StructureStudioView::fountainUplight(quint32 flameFid, double *level) const
{
    /* What lights the silk: the lights in its fountain (its pars inside the
       inner wall, uplighting it), added up by colour and level. A par in
       the gap between the walls washes the inner wall, not the silk. */
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 gid = props->fixtureGroup(flameFid);
    double r = 0.0, g = 0.0, b = 0.0;
    if (gid != 0)
    {
        const Fountain::Parts parts = Fountain::ofGroup(props, gid);
        foreach (quint32 fid, props->fixtureItemsID())
        {
            if (fid == flameFid || props->fixtureGroup(fid) != gid)
                continue;
            Fixture *fx = m_doc->fixture(fid);
            if (fx == nullptr || Fountain::isFlame(fx) || props->fixtureRigProps(fid).pixelPath != 0)
                continue;
            const QVector3D at = props->fixtureRigPosition(fid);
            if (Fountain::inGap(parts, double(at.x()), double(at.y())))
                continue;
            bool complete = true;
            const QByteArray vals = valuesForDrawing(fx, &complete);
            QColor c;
            uchar dim = 0;
            if (vals.isEmpty() || !fixtureLiveState(fx, vals, c, dim, complete) || dim < 3)
                continue;
            const double l = dim / 255.0;
            r += c.redF() * l; g += c.greenF() * l; b += c.blueF() * l;
        }
    }
    const double peak = qMax(r, qMax(g, b));
    if (level != nullptr)
        *level = qMin(1.0, peak);
    if (peak <= 0.004)
        return QColor();
    return QColor::fromRgbF(float(r / peak), float(g / peak), float(b / peak));
}

bool StructureStudioView::drawPixelPath(QPainter &p, quint32 fid, Fixture *fx,
                                        const QByteArray &vals, bool valsComplete) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.pixelPath == PixelPath::None || fx == nullptr)
        return false;
    const int n = PixelPathDecor::ledCount(m_doc, fid);
    const QVector<QVector3D> pts = PixelPath::positions(m_doc, props, fid, n);
    if (pts.isEmpty())
        return false;

    // A wand: its staff, thin and dark, butt to tip.
    QVector3D b, t;
    if (rp.pixelPath == PixelPath::Wand && PixelPath::wandEnds(props, fid, b, t))
    {
        const double af = 0.30 + 0.70 * m_ambient;
        const QColor staff(qRound(78 * af), qRound(70 * af), qRound(60 * af));
        const double px = QLineF(w2s(b), w2s(b + QVector3D(0.025f, 0.0f, 0.0f))).length();
        emitLine(w2s(b), w2s(t), viewDepth(b), viewDepth(t), staff, qMax(1.5, px), p);
    }

    // Each LED's colour: its head's, or a WLED effect's LEDs.
    QVector<QColor> cols(pts.size(), QColor(Qt::black));
    if (!vals.isEmpty())
    {
        if (fx->heads() > 1)
        {
            for (int i = 0; i < pts.size(); ++i)
            {
                QColor c;
                uchar dim = 0;
                if (fixtureHeadBeamState(fx, vals, i, c, dim, valsComplete))
                    cols[i] = QColor::fromRgbF(float(c.redF() * dim / 255.0), float(c.greenF() * dim / 255.0),
                                               float(c.blueF() * dim / 255.0));
            }
        }
        else
        {
            QColor live;
            uchar dim = 0;
            if (fixtureLiveState(fx, vals, live, dim, valsComplete) && dim > 0)
            {
                const LanternPixels::Frame f =
                    m_lanternPixels->frame(fx, vals, pts.size(), live, dim / 255.0, valsComplete);
                for (int i = 0; i < pts.size() && i < f.leds.size(); ++i)
                    cols[i] = f.leds.at(i);
            }
        }
    }

    /* Small points of light, each with a glow as big as it is bright. Light
       from them lights nothing else. */
    const double unitPx = QLineF(w2s(pts.first()), w2s(pts.first() + QVector3D(0.01f, 0, 0))).length();
    const double dotR = qBound(1.0, unitPx * 0.6, 4.0);
    for (int i = 0; i < pts.size(); ++i)
    {
        const QColor &c = cols.at(i);
        const double level = qMax(c.redF(), qMax(c.greenF(), c.blueF()));
        const QPointF s = w2s(pts.at(i));
        const double z = viewDepth(pts.at(i));
        if (level > 0.01)
        {
            QColor hue = QColor::fromRgbF(float(c.redF() / level), float(c.greenF() / level),
                                          float(c.blueF() / level));
            // A soft glow: light, fading from the LED to nothing round it.
            const double gr = dotR * (1.5 + 2.5 * level);
            const int SEG = 10;
            for (int k = 0; k < SEG; ++k)
            {
                const double a0 = 2.0 * M_PI * k / SEG, a1 = 2.0 * M_PI * (k + 1) / SEG;
                emitLightTri(s, s + QPointF(qCos(a0), qSin(a0)) * gr, s + QPointF(qCos(a1), qSin(a1)) * gr,
                             z + 1e-4, z + 1e-4, z + 1e-4, 0.55 * level, 0.0, 0.0, hue, p);
            }
            emitDot(s, z + 2e-4, dotR, hue.lighter(100 + int(40 * level)), p);
        }
        else
            emitDot(s, z + 2e-4, dotR * 0.8, QColor(52, 50, 48), p);
    }
    return true;
}
