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
#include <algorithm>

#include "createfixturegroup.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
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

    CreateFixtureGroup cfg(parent);
    cfg.setSize(suggestGrid(doc, fixtureIds));
    if (cfg.exec() != QDialog::Accepted)
        return NULL;

    FixtureGroup *grp = new FixtureGroup(doc);
    grp->setName(cfg.name());
    grp->setSize(cfg.size());
    doc->addFixtureGroup(grp);

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
        grp->assignFixture(fid);

    doc->setModified();
    return grp;
}
