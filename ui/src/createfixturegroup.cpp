/*
  Q Light Controller
  createfixturegroup.cpp

  Copyright (c) Heikki Junnila

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

#include <QSettings>
#include <QtMath>
#include <QVector3D>
#include <QSet>
#include <algorithm>
#include <climits>
#include <functional>

#include "createfixturegroup.h"
#include "headtypesdialog.h"
#include "stagegroupdialog.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
#include "headlayout.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "grouphead.h"
#include "qlcpoint.h"
#include "fixture.h"
#include "doc.h"

#define SETTINGS_GEOMETRY "createfixturegroup/geometry"

CreateFixtureGroup::CreateFixtureGroup(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);

    QSettings settings;
    QVariant geometrySettings = settings.value(SETTINGS_GEOMETRY);
    if (geometrySettings.isValid() == true)
        restoreGeometry(geometrySettings.toByteArray());
}

CreateFixtureGroup::~CreateFixtureGroup()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}

QString CreateFixtureGroup::name() const
{
    Q_ASSERT(m_nameEdit != NULL);
    return m_nameEdit->text();
}

void CreateFixtureGroup::setSize(const QSize& size)
{
    Q_ASSERT(m_widthSpin != NULL);
    Q_ASSERT(m_heightSpin != NULL);
    m_widthSpin->setValue(size.width());
    m_heightSpin->setValue(size.height());
}

QSize CreateFixtureGroup::size() const
{
    Q_ASSERT(m_widthSpin != NULL);
    Q_ASSERT(m_heightSpin != NULL);
    return QSize(m_widthSpin->value(), m_heightSpin->value());
}

QSize CreateFixtureGroup::suggestGrid(Doc *doc, const QList<quint32> &fixtureIds)
{
    if (doc == NULL)
        return QSize(4, 4);

    int headTotal = 0;
    QList<double> ys;
    MonitorProperties *mp = doc->monitorProperties();
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fxi = doc->fixture(fid);
        if (fxi == NULL)
            continue;
        headTotal += qMax(1, fxi->heads());
        if (mp != NULL)
        {
            QVector3D p = mp->fixturePosition(fid, 0, 0);
            if (!p.isNull())
                ys << double(p.y());
        }
    }
    if (headTotal < 1)
        headTotal = 1;

    // Position-based: distinct Y bands = rows, columns derived. A row boundary is
    // a Y gap larger than half the largest gap, so evenly-spaced rows cluster
    // cleanly (e.g. 8 rows of 64 → 8×64) while jitter within a row does not.
    if (ys.size() == fixtureIds.size() && ys.size() >= 2)
    {
        std::sort(ys.begin(), ys.end());
        double maxGap = 0.0;
        for (int i = 1; i < ys.size(); i++)
            maxGap = qMax(maxGap, ys[i] - ys[i - 1]);
        if (maxGap > 1e-6)
        {
            const double tol = maxGap * 0.5;
            int rows = 1;
            for (int i = 1; i < ys.size(); i++)
                if (ys[i] - ys[i - 1] > tol)
                    rows++;
            if (rows < 1) rows = 1;
            const int cols = (headTotal + rows - 1) / rows;
            return QSize(qMax(1, cols), rows);
        }
        return QSize(headTotal, 1);   // one horizontal line
    }

    // Fallback: near-square of the head count.
    int side = int(qCeil(qSqrt(double(headTotal))));
    if (side < 1) side = 1;
    const int rows = (headTotal + side - 1) / side;
    return QSize(side, rows);
}

FixtureGroup *CreateFixtureGroup::createFromFixtures(Doc *doc,
                                                     const QList<quint32> &fixtureIds,
                                                     QWidget *parent)
{
    if (doc == NULL || fixtureIds.isEmpty())
        return NULL;

    /* Fixtures with more than one kind of head (RGB rows and white rows):
       offer a group per kind, laid out from where each head really is. */
    {
        QList<FixtureGroup *> made;
        const int split = HeadTypesDialog::offerSplit(doc, fixtureIds,
                                                      HeadTypesDialog::suggestName(doc, fixtureIds),
                                                      parent, &made);
        if (split < 0)
            return NULL;
        if (split > 0)
            return made.isEmpty() ? NULL : made.first();
    }

    /* On the stage: named, laid out from where each one stands, from the
       front or from above, previewed, and checked for an identical group.
       A grid size typed in and filled in a guessed order made a jumble of a
       rig's worth of pixel bars ("128 x 24 .. didn't create it right"). */
    if (StageGroupDialog::anyOnStage(doc, fixtureIds))
    {
        StageGroupDialog dlg(doc, fixtureIds, HeadTypesDialog::suggestName(doc, fixtureIds), parent);
        if (dlg.exec() != QDialog::Accepted)
            return NULL;
        return dlg.group();
    }

    CreateFixtureGroup cfg(parent);
    cfg.setSize(suggestGrid(doc, fixtureIds));
    if (cfg.exec() != QDialog::Accepted)
        return NULL;

    FixtureGroup *grp = new FixtureGroup(doc);
    grp->setName(cfg.name());
    grp->setSize(cfg.size());
    doc->addFixtureGroup(grp);
    layoutFixturesInGrid(doc, fixtureIds, grp);

    doc->setModified();
    return grp;
}

void CreateFixtureGroup::layoutFixturesInGrid(Doc *doc, const QList<quint32> &fixtureIds,
                                              FixtureGroup *target)
{
    if (doc == NULL || target == NULL)
        return;
    // Order fixtures by physical position (rows top→bottom, columns left→right)
    // so the grid matches the rig — heads then auto-fill row-major. Unplaced
    // fixtures keep selection order at the end.
    MonitorProperties *mp = doc->monitorProperties();
    QList<quint32> ordered = fixtureIds;
    if (mp != NULL)
    {
        std::stable_sort(ordered.begin(), ordered.end(),
            [mp](quint32 a, quint32 b) {
                const QVector3D pa = mp->fixturePosition(a, 0, 0);
                const QVector3D pb = mp->fixturePosition(b, 0, 0);
                if (pa.isNull() != pb.isNull()) return !pa.isNull();   // placed first
                if (pa.isNull()) return false;
                if (qAbs(pa.y() - pb.y()) > 1e-4) return pa.y() > pb.y(); // top first
                return pa.x() < pb.x();                                  // left→right
            });
    }
    foreach (quint32 fid, ordered)
        if (doc->fixture(fid) != NULL)
            target->assignFixture(fid);
}

void CreateFixtureGroup::appendSelection(Doc *doc, FixtureGroup *target,
                                         const QList<quint32> &fixtureIds,
                                         const QList<quint32> &groupIds)
{
    if (doc == NULL || target == NULL)
        return;

    // Start appending below whatever the target already holds.
    int curY = 0;
    {
        QMapIterator<QLCPoint, GroupHead> it(target->headsMap());
        while (it.hasNext()) { it.next(); curY = qMax(curY, it.key().y() + 1); }
    }

    // Each source group goes in as a block (preserving its relative layout),
    // tagged as a sub-group so it stays visually distinct and movable as a unit.
    foreach (quint32 gid, groupIds)
    {
        FixtureGroup *src = doc->fixtureGroup(gid);
        if (src == NULL || src == target)
            continue;

        const QMap<QLCPoint, GroupHead> srcHeads = src->headsMap();
        if (srcHeads.isEmpty())
            continue;

        // Normalise the source to its top-left corner.
        int minX = INT_MAX, minY = INT_MAX;
        QMapIterator<QLCPoint, GroupHead> nit(srcHeads);
        while (nit.hasNext()) { nit.next(); minX = qMin(minX, nit.key().x()); minY = qMin(minY, nit.key().y()); }

        int blockMaxY = curY;
        QMapIterator<QLCPoint, GroupHead> it(srcHeads);
        while (it.hasNext())
        {
            it.next();
            const int x = it.key().x() - minX;
            const int y = curY + (it.key().y() - minY);
            if (target->assignHead(QLCPoint(x, y), it.value()) == true)
            {
                target->setHeadSubGroup(QLCPoint(x, y), FixtureGroup::subGroupTagFor(src->id()));
                blockMaxY = qMax(blockMaxY, y);
            }
        }
        curY = blockMaxY + 1;
    }

    // Loose fixtures go in a row of their own (all heads of each fixture).
    int col = 0;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fxi = doc->fixture(fid);
        if (fxi == NULL)
            continue;
        for (int h = 0; h < fxi->heads(); h++)
        {
            if (target->assignHead(QLCPoint(col, curY), GroupHead(fid, h)) == true)
                col++;
        }
    }

    // Grow the grid to fit everything we placed.
    int maxX = 0, maxY = 0;
    QMapIterator<QLCPoint, GroupHead> sz(target->headsMap());
    while (sz.hasNext()) { sz.next(); maxX = qMax(maxX, sz.key().x()); maxY = qMax(maxY, sz.key().y()); }
    if (maxX + 1 > target->size().width() || maxY + 1 > target->size().height())
        target->setSize(QSize(qMax(maxX + 1, target->size().width()),
                              qMax(maxY + 1, target->size().height())));
}

bool CreateFixtureGroup::layoutGroupsByStage(Doc *doc, const QList<quint32> &groupIds,
                                             FixtureGroup *target, int view, bool ordinal)
{
    if (doc == NULL || target == NULL)
        return false;
    QList<StageBlock> blocks;
    foreach (quint32 gid, groupIds)
    {
        FixtureGroup *src = doc->fixtureGroup(gid);
        if (src == NULL || src == target || src->headsMap().isEmpty())
            continue;
        StageBlock b;
        b.tag = FixtureGroup::subGroupTagFor(gid);
        b.heads = src->headsMap();
        b.fixtures = src->fixtureList();
        blocks << b;
    }
    const int vertical = view == ViewFront ? 1 : view == ViewTop ? 0 : -1;
    return layoutBlocksByStage(doc, blocks, target, vertical, ordinal);
}

bool CreateFixtureGroup::layoutBlocksByStage(Doc *doc, const QList<StageBlock> &sources,
                                             FixtureGroup *target, int vertical, bool ordinal)
{
    if (doc == NULL || target == NULL)
        return false;
    MonitorProperties *mp = doc->monitorProperties();

    struct Block
    {
        quint32 tag = 0;
        QMap<QLCPoint, GroupHead> heads;   // normalised to its own top-left
        int w = 1, h = 1;
        double hint = 0.0;                 // its own cells per metre, when known
        bool placed = false;
        double x0 = 0, x1 = 0, y0 = 0, y1 = 0, z0 = 0, z1 = 0;
        int col = 0, row = 0;
    };
    QList<Block> blocks;
    foreach (const StageBlock &src, sources)
    {
        if (src.heads.isEmpty())
            continue;
        Block b;
        b.tag = src.tag;
        b.hint = src.cellsPerMetre;
        int minX = INT_MAX, minY = INT_MAX, maxX = 0, maxY = 0;
        QMapIterator<QLCPoint, GroupHead> it(src.heads);
        while (it.hasNext())
        {
            it.next();
            minX = qMin(minX, it.key().x()); minY = qMin(minY, it.key().y());
            maxX = qMax(maxX, it.key().x()); maxY = qMax(maxY, it.key().y());
        }
        it.toFront();
        while (it.hasNext())
        {
            it.next();
            b.heads.insert(QLCPoint(it.key().x() - minX, it.key().y() - minY), it.value());
        }
        b.w = maxX - minX + 1;
        b.h = maxY - minY + 1;
        // Where its fixtures are, in the rig.
        bool first = true;
        foreach (quint32 fid, src.fixtures)
        {
            const QVector3D p = (mp != NULL) ? mp->fixtureRigPosition(fid) : QVector3D();
            if (p.isNull())
                continue;
            if (first)
            {
                b.x0 = b.x1 = p.x(); b.y0 = b.y1 = p.y(); b.z0 = b.z1 = p.z();
                first = false;
            }
            b.x0 = qMin(b.x0, double(p.x())); b.x1 = qMax(b.x1, double(p.x()));
            b.y0 = qMin(b.y0, double(p.y())); b.y1 = qMax(b.y1, double(p.y()));
            b.z0 = qMin(b.z0, double(p.z())); b.z1 = qMax(b.z1, double(p.z()));
        }
        b.placed = !first;
        blocks << b;
    }
    if (blocks.isEmpty())
        return false;

    /* Up and down the grid: height when the groups differ in height more
       than in depth, else upstage (top) to downstage. */
    bool byHeight = (vertical == 1);
    if (vertical < 0)
    {
        double minCY = 1e9, maxCY = -1e9, minCZ = 1e9, maxCZ = -1e9;
        foreach (const Block &b, blocks)
            if (b.placed)
            {
                minCY = qMin(minCY, (b.y0 + b.y1) / 2); maxCY = qMax(maxCY, (b.y0 + b.y1) / 2);
                minCZ = qMin(minCZ, (b.z0 + b.z1) / 2); maxCZ = qMax(maxCZ, (b.z0 + b.z1) / 2);
            }
        byHeight = (maxCZ - minCZ) >= (maxCY - minCY);
    }
    /* A block that knows its own size (one fixture's LED grid) covers that
       much of the rig, centred on where it is -- its fixtures alone are one
       point and say nothing about its size. */
    for (Block &b : blocks)
    {
        if (!b.placed || b.hint <= 0.0)
            continue;
        const double halfW = 0.5 * (b.w - 1) / b.hint, halfH = 0.5 * (b.h - 1) / b.hint;
        const double cx = (b.x0 + b.x1) / 2;
        b.x0 = qMin(b.x0, cx - halfW); b.x1 = qMax(b.x1, cx + halfW);
        if (byHeight)
        {
            const double cz = (b.z0 + b.z1) / 2;
            b.z0 = qMin(b.z0, cz - halfH); b.z1 = qMax(b.z1, cz + halfH);
        }
        else
        {
            const double cy = (b.y0 + b.y1) / 2;
            b.y0 = qMin(b.y0, cy - halfH); b.y1 = qMax(b.y1, cy + halfH);
        }
    }
    auto vTop = [byHeight](const Block &b) { return byHeight ? -b.z1 : b.y0; };   // smaller = higher row
    auto vExt = [byHeight](const Block &b) { return byHeight ? b.z1 - b.z0 : b.y1 - b.y0; };

    /* One scale for all of them: cells per metre, from the blocks' own
       layouts (a 16-head bar 1 m long is 15 cells over 1 m). The median, so
       one odd group does not set it. */
    QList<double> scales;
    foreach (const Block &b, blocks)
    {
        if (!b.placed) continue;
        if (b.hint > 0.0) { scales << b.hint; continue; }
        if (b.w > 1 && b.x1 - b.x0 > 0.05) scales << (b.w - 1) / (b.x1 - b.x0);
        if (b.h > 1 && vExt(b) > 0.05)     scales << (b.h - 1) / vExt(b);
    }
    std::sort(scales.begin(), scales.end());
    double s = scales.isEmpty() ? 0.0 : scales.at(scales.size() / 2);

    double minX = 1e9, minV = 1e9;
    int sumW = 0, sumH = 0;
    foreach (const Block &b, blocks)
    {
        sumW += b.w; sumH += b.h;
        if (b.placed) { minX = qMin(minX, b.x0); minV = qMin(minV, vTop(b)); }
    }
    if (s <= 0.0)
    {
        /* Nothing to take a scale from (every group is a single head, or one
           point): spread them so the row of blocks spans the rig at about a
           cell per block, which keeps their order and rough spacing. */
        s = 1.0;
        double span = 0;
        foreach (const Block &b, blocks)
            if (b.placed) span = qMax(span, b.x1 - minX);
        if (span > 0)
            s = double(sumW + blocks.size()) / span;
    }
    // Keep it usable: the gaps between groups need not be life-size.
    double extent = 0, vextent = 0;
    foreach (const Block &b, blocks)
        if (b.placed)
        {
            extent = qMax(extent, (b.x0 - minX) * s + b.w);
            vextent = qMax(vextent, (vTop(b) - minV) * s + b.h);
        }
    const double limit = 3.0 * sumW + blocks.size(), vlimit = 3.0 * sumH + blocks.size();
    if (extent > limit)   s *= limit / extent;
    if (vextent > vlimit) s *= vlimit / vextent;

    for (Block &b : blocks)
        if (b.placed)
        {
            b.col = qRound((b.x0 - minX) * s);
            b.row = qRound((vTop(b) - minV) * s);
        }

    /* In ORDER rather than to scale (closing the gaps): blocks whose stage
       extents overlap side to side share a band of columns, bands in stage
       order, each as wide as its widest block; likewise rows, top to bottom.
       To scale, a step's eight bars 2.5 cm apart all rounded onto one row,
       and two sizes of fixture with nothing to give a scale piled into each
       other -- patching that up afterwards put things out of order. */
    if (ordinal)
    {
        auto bands = [&blocks](std::function<double(const Block &)> lo,
                               std::function<double(const Block &)> hi,
                               std::function<int(const Block &)> size,
                               std::function<void(Block &, int)> set) {
            QList<int> idx;
            for (int i = 0; i < blocks.size(); ++i)
                if (blocks[i].placed) idx << i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b) { return lo(blocks[a]) < lo(blocks[b]); });
            /* "The same place", relative to how far apart things are along
               this axis: a step set 3 cm further across than the one in front
               of it is still in line with it when steps are 1.2 m apart, but
               bars 2.5 cm apart up a step face are not one row, nor bars
               15 cm apart along a truss. A fraction of the typical (median)
               gap between distinct positions. */
            QList<double> gaps;
            for (int k = 1; k < idx.size(); ++k)
            {
                const double g = lo(blocks[idx[k]]) - lo(blocks[idx[k - 1]]);
                if (g > 0.001)
                    gaps << g;
            }
            std::sort(gaps.begin(), gaps.end());
            const double tol = qMax(0.005, gaps.isEmpty() ? 0.0 : 0.15 * gaps.at(gaps.size() / 2));
            int start = 0, bandSize = 0;
            double bandLo = -1e9, bandHi = -1e9;
            bool first = true;
            foreach (int i, idx)
            {
                /* In this band if it starts where the band starts (within the
                   tolerance) or actually overlaps it. The tolerance is never
                   added to the band's far END: steps 30 cm apart are two
                   bands however far apart steps typically are. */
                const bool joins = !first && (lo(blocks[i]) <= bandLo + tol
                                              || lo(blocks[i]) < bandHi - 0.005);
                if (!first && !joins)
                {
                    start += bandSize;           // a new band, after the last
                    bandSize = 0;
                }
                if (!joins)
                {
                    bandLo = lo(blocks[i]);
                    bandHi = hi(blocks[i]);
                }
                else
                    bandHi = qMax(bandHi, hi(blocks[i]));
                first = false;
                set(blocks[i], start);
                bandSize = qMax(bandSize, size(blocks[i]));
            }
        };
        bands([](const Block &b) { return b.x0; }, [](const Block &b) { return b.x1; },
              [](const Block &b) { return b.w; }, [](Block &b, int v) { b.col = v; });
        bands([&vTop](const Block &b) { return vTop(b); },
              [&vTop, &vExt](const Block &b) { return vTop(b) + vExt(b); },
              [](const Block &b) { return b.h; }, [](Block &b, int v) { b.row = v; });
    }

    // No two blocks on the same cells: push a later one right, past whatever
    // it would land on.
    QList<int> order;
    for (int i = 0; i < blocks.size(); ++i)
        if (blocks[i].placed) order << i;
    std::sort(order.begin(), order.end(), [&blocks, &vTop](int a, int b) {
        if (blocks[a].row != blocks[b].row) return blocks[a].row < blocks[b].row;
        if (!qFuzzyCompare(vTop(blocks[a]) + 1e3, vTop(blocks[b]) + 1e3))
            return vTop(blocks[a]) < vTop(blocks[b]);       // higher on stage first
        return blocks[a].col < blocks[b].col;
    });
    QList<int> done;
    auto overlaps = [&blocks](int a, int b) {
        return blocks[a].col < blocks[b].col + blocks[b].w && blocks[b].col < blocks[a].col + blocks[a].w
            && blocks[a].row < blocks[b].row + blocks[b].h && blocks[b].row < blocks[a].row + blocks[a].h;
    };
    /* Two blocks on the same cells are pushed apart the way they ARE apart:
       down when one is below the other on stage, across when beside it. The
       scale comes from the pixel pitch along a bar, so bars stacked up a step
       face a couple of centimetres apart round onto one row -- always pushing
       across laid each step's eight bars out as a line ("didn't create it
       right"). */
    foreach (int i, order)
    {
        for (bool moved = true; moved; )
        {
            moved = false;
            foreach (int j, done)
            {
                if (!overlaps(i, j))
                    continue;
                const double across = qAbs((blocks[i].x0 + blocks[i].x1) - (blocks[j].x0 + blocks[j].x1)) / 2.0;
                const double down = vTop(blocks[i]) - vTop(blocks[j]);
                if (down > 0.0 && down >= across)
                    blocks[i].row = blocks[j].row + blocks[j].h;
                else
                    blocks[i].col = qMax(blocks[i].col, blocks[j].col + blocks[j].w);
                moved = true;
            }
        }
        done << i;
    }
    // Blocks with nothing on the plot go after the rest, on the top row.
    int endCol = 0;
    foreach (int i, done)
        endCol = qMax(endCol, blocks[i].col + blocks[i].w);
    for (Block &b : blocks)
        if (!b.placed)
        {
            b.col = endCol;
            b.row = 0;
            endCol += b.w;
        }

    int maxX = 0, maxY = 0;
    foreach (const Block &b, blocks)
    {
        QMapIterator<QLCPoint, GroupHead> it(b.heads);
        while (it.hasNext())
        {
            it.next();
            const QLCPoint at(b.col + it.key().x(), b.row + it.key().y());
            if (target->assignHead(at, it.value()))
            {
                if (b.tag != 0)
                    target->setHeadSubGroup(at, b.tag);
                maxX = qMax(maxX, at.x());
                maxY = qMax(maxY, at.y());
            }
        }
    }
    target->setSize(QSize(qMax(maxX + 1, target->size().width()),
                          qMax(maxY + 1, target->size().height())));
    return true;
}

QList<CreateFixtureGroup::StageBlock> CreateFixtureGroup::headKindBlocks(Doc *doc,
                                                                         const QList<quint32> &fixtureIds,
                                                                         const QString &kind, int *vertical,
                                                                         int view)
{
    return headBlocks(doc, fixtureIds, [&kind](const QLCFixtureMode *mode, int h) {
        return kind.isEmpty() || HeadKinds::kindOf(mode, h) == kind;
    }, vertical, view);
}

QList<CreateFixtureGroup::StageBlock> CreateFixtureGroup::headTypeBlocks(Doc *doc,
                                                                         const QList<quint32> &fixtureIds,
                                                                         const QString &type, int *vertical,
                                                                         int view)
{
    return headBlocks(doc, fixtureIds, [&type](const QLCFixtureMode *mode, int h) {
        return type.isEmpty() || HeadKinds::typeOf(mode, h) == type;
    }, vertical, view);
}

QList<CreateFixtureGroup::StageBlock> CreateFixtureGroup::headBlocks(Doc *doc,
                                                                     const QList<quint32> &fixtureIds,
                                                                     const std::function<bool(const QLCFixtureMode *, int)> &keep,
                                                                     int *vertical, int view)
{
    QList<StageBlock> out;
    MonitorProperties *mp = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mp == NULL)
        return out;

    // The same choice layoutBlocksByStage() makes, made here so each bar can
    // be turned to the view it will be laid out in.
    double minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
    foreach (quint32 fid, fixtureIds)
    {
        const QVector3D p = mp->fixtureRigPosition(fid);
        if (p.isNull()) continue;
        minY = qMin(minY, double(p.y())); maxY = qMax(maxY, double(p.y()));
        minZ = qMin(minZ, double(p.z())); maxZ = qMax(maxZ, double(p.z()));
    }
    // Asked for a view: from the front is by height, from above by depth.
    const bool byHeight = (view == ViewFront) ? true
                        : (view == ViewTop)   ? false
                                              : (maxZ - minZ) >= (maxY - minY);
    if (vertical != NULL)
        *vertical = byHeight ? 1 : 0;
    // Screen axes: X across; down the page is down (front) or downstage (plan).
    const QVector3D sx(1, 0, 0);
    const QVector3D sy = byHeight ? QVector3D(0, 0, -1) : QVector3D(0, 1, 0);

    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = doc->fixture(fid);
        if (fx == NULL || fx->fixtureMode() == NULL)
            continue;
        const QLCFixtureMode *mode = fx->fixtureMode();
        const HeadLayout map = mp->headLayoutFor(fx);
        // This kind's heads and their cells, rows/columns of other kinds
        // squeezed out (the RGB rows of an XL-450 are 0, 2 and 4: 0, 1, 2 here).
        QList<int> heads;
        QSet<int> rows, cols;
        for (int h = 0; h < mode->heads().size(); ++h)
        {
            if (!keep(mode, h))
                continue;
            const QPoint c = map.isValid() ? map.cellOf(h) : QPoint(h, 0);
            if (c.x() < 0) continue;
            heads << h;
            rows.insert(c.y());
            cols.insert(c.x());
        }
        if (heads.isEmpty())
            continue;
        QList<int> rowList = rows.values(), colList = cols.values();
        std::sort(rowList.begin(), rowList.end());
        std::sort(colList.begin(), colList.end());

        /* Turned the way the bar is rigged: along its length (columns) and
           across it (rows) in the world, then onto the page. A bar hanging
           straight down from a truss is a column of cells, not a row. */
        const FixtureRigProps rp = mp->fixtureRigProps(fid);
        const QVector3D L = rp.longAxis();
        const QVector3D H = rp.acrossAxis();
        QMap<QLCPoint, GroupHead> cells;
        bool clash = false;
        QList<QPair<QPoint, int> > placed;
        int minPX = INT_MAX, minPY = INT_MAX;
        foreach (int h, heads)
        {
            const QPoint c = map.isValid() ? map.cellOf(h) : QPoint(h, 0);
            const QVector3D local = L * float(colList.indexOf(c.x())) + H * float(rowList.indexOf(c.y()));
            const QPoint pt(qRound(QVector3D::dotProduct(local, sx)), qRound(QVector3D::dotProduct(local, sy)));
            placed << qMakePair(pt, h);
            minPX = qMin(minPX, pt.x()); minPY = qMin(minPY, pt.y());
        }
        foreach (const auto &ph, placed)
        {
            const QLCPoint at(ph.first.x() - minPX, ph.first.y() - minPY);
            if (cells.contains(at)) { clash = true; break; }
            cells.insert(at, GroupHead(fid, ph.second));
        }
        if (clash)
        {
            // Turned at an angle the grid cannot show: lay it out as it is built.
            cells.clear();
            foreach (int h, heads)
            {
                const QPoint c = map.isValid() ? map.cellOf(h) : QPoint(h, 0);
                cells.insert(QLCPoint(colList.indexOf(c.x()), rowList.indexOf(c.y())), GroupHead(fid, h));
            }
        }

        StageBlock b;
        b.heads = cells;
        b.fixtures << fid;
        /* Its own size -- unless it is a Generic definition, whose physical
           size is QLC+'s 300 mm-a-pixel placeholder: a 48-pixel washer then
           "measures" 14.4 m, overlaps everything on the rig and cannot be
           told apart from its neighbours. */
        const QLCPhysical &phys = mode->physical();
        const int mapCols = map.isValid() ? map.size.width() : heads.size();
        const bool generic = fx->fixtureDef() == NULL
                             || fx->fixtureDef()->manufacturer() == KXMLFixtureGeneric;
        if (!generic && phys.width() > 0 && mapCols > 1)
            b.cellsPerMetre = (mapCols - 1) / (phys.width() / 1000.0);
        out << b;
    }
    return out;
}

QString CreateFixtureGroup::headKindGroupName(const QString &base, const QString &kind)
{
    const QString b = base.trimmed();
    if (b.isEmpty())
        return kind;
    // "T1-CW-RGB" is already the RGB group's name: do not make it "... RGB RGB".
    if (b.endsWith(kind, Qt::CaseInsensitive)
        && (b.size() == kind.size() || !b.at(b.size() - kind.size() - 1).isLetterOrNumber()))
        return b;
    return QStringLiteral("%1 %2").arg(b, kind);
}

QList<FixtureGroup *> CreateFixtureGroup::createByHeadKind(Doc *doc, const QList<quint32> &fixtureIds,
                                                           const QList<QPair<QString, QString> > &named,
                                                           int view, bool closeGapsToo)
{
    QList<FixtureGroup *> made;
    if (doc == NULL)
        return made;
    for (const QPair<QString, QString> &kn : named)
    {
        const QString &kind = kn.first;
        int vertical = -1;
        const QList<StageBlock> blocks = headKindBlocks(doc, fixtureIds, kind, &vertical, view);
        if (blocks.isEmpty())
            continue;
        FixtureGroup *grp = new FixtureGroup(doc);
        grp->setName(kn.second.trimmed().isEmpty() ? kind : kn.second.trimmed());
        grp->setSize(QSize(1, 1));
        doc->addFixtureGroup(grp);
        grp->blockSignals(true);
        layoutBlocksByStage(doc, blocks, grp, vertical, closeGapsToo);
        if (closeGapsToo)
            closeGaps(grp);
        grp->blockSignals(false);
        grp->notifyChanged();
        made << grp;
    }
    if (!made.isEmpty())
        doc->setModified();
    return made;
}

FixtureGroup *CreateFixtureGroup::createFromGroups(Doc *doc, const QList<quint32> &groupIds,
                                                   QWidget *parent)
{
    if (doc == NULL || groupIds.isEmpty())
        return NULL;

    // Groups of mixed fixtures: a group per kind of head is usually the point.
    {
        QList<quint32> members;
        QStringList groupNames;
        foreach (quint32 gid, groupIds)
            if (FixtureGroup *g = doc->fixtureGroup(gid))
            {
                groupNames << g->name();
                foreach (quint32 fid, g->fixtureList())
                    if (!members.contains(fid))
                        members << fid;
            }
        QList<FixtureGroup *> made;
        const int split = HeadTypesDialog::offerSplit(doc, members, groupNames.join(QStringLiteral(" + ")),
                                                      parent, &made);
        if (split < 0)
            return NULL;
        if (split > 0)
            return made.isEmpty() ? NULL : made.first();
    }

    /* On the stage: named, laid out from the front or from above, previewed,
       and checked for an identical group -- "as on stage" alone did not say
       from where, and from the front is a different picture than from the
       top. */
    {
        QList<quint32> members;
        QStringList names;
        foreach (quint32 gid, groupIds)
            if (FixtureGroup *g = doc->fixtureGroup(gid))
            {
                names << g->name();
                foreach (quint32 fid, g->fixtureList())
                    if (!members.contains(fid))
                        members << fid;
            }
        if (StageGroupDialog::anyOnStage(doc, members))
        {
            StageGroupDialog dlg(doc, members, names.join(QStringLiteral(" + ")), parent, groupIds);
            if (dlg.exec() != QDialog::Accepted)
                return NULL;
            return dlg.group();
        }
    }

    // Worked out first, so the dialog offers the size it will need.
    FixtureGroup probe(doc);
    probe.setSize(QSize(1, 1));
    if (!layoutGroupsByStage(doc, groupIds, &probe))
        return NULL;

    QStringList names;
    foreach (quint32 gid, groupIds)
        if (FixtureGroup *g = doc->fixtureGroup(gid))
            names << g->name();
    CreateFixtureGroup cfg(parent);
    cfg.m_nameEdit->setText(names.join(QStringLiteral(" + ")));
    cfg.setSize(probe.size());
    if (cfg.exec() != QDialog::Accepted)
        return NULL;

    FixtureGroup *grp = new FixtureGroup(doc);
    grp->setName(cfg.name());
    grp->setSize(QSize(1, 1));
    doc->addFixtureGroup(grp);
    grp->blockSignals(true);
    layoutGroupsByStage(doc, groupIds, grp);
    grp->setSize(QSize(qMax(grp->size().width(), cfg.size().width()),
                       qMax(grp->size().height(), cfg.size().height())));
    grp->blockSignals(false);
    grp->notifyChanged();
    doc->setModified();
    return grp;
}

bool CreateFixtureGroup::layoutFixturesByStage(Doc *doc, const QList<quint32> &fixtureIds,
                                               FixtureGroup *target, int view, bool gaps,
                                               const QString &kind)
{
    int vertical = -1;
    const QList<StageBlock> blocks = headKindBlocks(doc, fixtureIds, kind, &vertical, view);
    if (blocks.isEmpty())
        return false;
    if (!layoutBlocksByStage(doc, blocks, target, vertical, gaps))
        return false;
    if (gaps)
        closeGaps(target);
    return true;
}

void CreateFixtureGroup::closeGaps(FixtureGroup *grp)
{
    if (grp == NULL)
        return;
    const QMap<QLCPoint, GroupHead> heads = grp->headsMap();
    const QMap<QLCPoint, quint32> tags = grp->headSubGroupMap();
    QList<int> cols, rows;
    QMapIterator<QLCPoint, GroupHead> it(heads);
    while (it.hasNext())
    {
        it.next();
        if (!cols.contains(it.key().x())) cols << it.key().x();
        if (!rows.contains(it.key().y())) rows << it.key().y();
    }
    std::sort(cols.begin(), cols.end());
    std::sort(rows.begin(), rows.end());
    QMap<QLCPoint, GroupHead> packed;
    QMap<QLCPoint, quint32> packedTags;
    it.toFront();
    while (it.hasNext())
    {
        it.next();
        const QLCPoint at(cols.indexOf(it.key().x()), rows.indexOf(it.key().y()));
        packed.insert(at, it.value());
        if (tags.contains(it.key()))
            packedTags.insert(at, tags.value(it.key()));
    }
    grp->restoreState(QSize(qMax(1, int(cols.size())), qMax(1, int(rows.size()))), packed, packedTags);
}

QString CreateFixtureGroup::fixtureModelKey(const Fixture *fxi)
{
    if (fxi == NULL)
        return QString();
    if (fxi->fixtureDef() == NULL || fxi->fixtureMode() == NULL)
        return QStringLiteral("?|%1|%2").arg(const_cast<Fixture *>(fxi)->typeString()).arg(fxi->channels());
    return fxi->fixtureDef()->manufacturer() + QLatin1Char('|') + fxi->fixtureDef()->model()
           + QLatin1Char('|') + fxi->fixtureMode()->name();
}

QList<FixtureGroup *> CreateFixtureGroup::similarGroups(Doc *doc, const FixtureGroup *fg, bool sameLayout)
{
    QList<FixtureGroup *> out;
    if (doc == NULL || fg == NULL)
        return out;

    // How many fixtures of each model; with the layout, what each cell holds.
    auto models = [doc](const FixtureGroup *g) {
        QMap<QString, int> n;
        foreach (quint32 fid, g->fixtureList())
            n[fixtureModelKey(doc->fixture(fid))]++;
        return n;
    };
    auto cells = [doc](const FixtureGroup *g) {
        QMap<QLCPoint, QPair<QString, int> > c;
        QMapIterator<QLCPoint, GroupHead> it(g->headsMap());
        while (it.hasNext())
        {
            it.next();
            c.insert(it.key(), qMakePair(fixtureModelKey(doc->fixture(it.value().fxi)), it.value().head));
        }
        return c;
    };
    const QMap<QString, int> wantModels = models(fg);
    const QMap<QLCPoint, QPair<QString, int> > wantCells = sameLayout ? cells(fg) : QMap<QLCPoint, QPair<QString, int> >();

    foreach (FixtureGroup *g, doc->fixtureGroups())
    {
        if (g == NULL)
            continue;
        if (g->id() == fg->id())
        {
            out.prepend(g);
            continue;
        }
        if (g->headsMap().size() != fg->headsMap().size() || models(g) != wantModels)
            continue;
        if (sameLayout && (g->size() != fg->size() || cells(g) != wantCells))
            continue;
        out << g;
    }
    return out;
}

FixtureGroup *CreateFixtureGroup::findSameGroup(Doc *doc, const QMap<QLCPoint, GroupHead> &heads,
                                                const FixtureGroup *except)
{
    if (doc == NULL || heads.isEmpty())
        return NULL;
    QSet<QPair<quint32, int> > want;
    foreach (const GroupHead &h, heads)
        want.insert(qMakePair(h.fxi, h.head));
    foreach (FixtureGroup *g, doc->fixtureGroups())
    {
        if (g == NULL || g == except || g->headsMap().size() != heads.size())
            continue;
        QSet<QPair<quint32, int> > have;
        foreach (const GroupHead &h, g->headsMap())
            have.insert(qMakePair(h.fxi, h.head));
        if (have == want)
            return g;
    }
    return NULL;
}
