/*
  Q Light Controller Plus - qlcconsole
  surfacechips.cpp

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

#include <QToolButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QApplication>
#include <QPalette>

#include "surfacechips.h"
#include "surfacelayout.h"
#include "inputoutputmap.h"
#include "surfacemap.h"
#include "inputpatch.h"
#include "surfacepages.h"
#include "controlmap.h"
#include "doc.h"

/* A generic controller: a board with a fader slot, two knobs and a row of
   keys. Drawn in the footer's text colour, never a brand's mark. */
QIcon SurfaceChips::boardIcon(const QColor &colour, int size)
{
    const qreal dpr = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(colour, 1.2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal u = size / 14.0;
    p.drawRoundedRect(QRectF(1 * u, 2 * u, 12 * u, 10 * u), 1.5 * u, 1.5 * u);
    p.setBrush(colour);
    p.drawRect(QRectF(3 * u, 4 * u, 1.2 * u, 6 * u));      // the fader slot
    p.drawEllipse(QPointF(8 * u, 5.5 * u), 1.1 * u, 1.1 * u);   // two knobs
    p.drawEllipse(QPointF(11 * u, 5.5 * u), 1.1 * u, 1.1 * u);
    p.drawRect(QRectF(7 * u, 8.5 * u, 1.6 * u, 1.6 * u));   // keys
    p.drawRect(QRectF(10 * u, 8.5 * u, 1.6 * u, 1.6 * u));
    p.end();
    return QIcon(pm);
}

SurfaceChips::SurfaceChips(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_box(new QHBoxLayout(this))
{
    setObjectName(QStringLiteral("footerSurfaces"));
    m_box->setContentsMargins(0, 0, 0, 0);
    m_box->setSpacing(0);
    auto queue = [this]() {
        if (m_queued)
            return;
        m_queued = true;
        QTimer::singleShot(0, this, &SurfaceChips::rebuild);
    };
    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::inputPatchChanged, this, queue);
    connect(iom, &InputOutputMap::profileChanged, this, queue);
    connect(iom, &InputOutputMap::universeAdded, this, queue);
    connect(iom, &InputOutputMap::universeRemoved, this, queue);
    connect(m_doc, &Doc::loaded, this, queue);
    // "APC40 · Move": the page on now.
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    connect(pages, &SurfacePages::pageChanged, this, queue);
    connect(pages, &SurfacePages::setsChanged, this, queue);
    rebuild();
}

void SurfaceChips::setChipHeight(int h)
{
    m_height = h;
    for (QToolButton *b : std::as_const(m_chips))
        b->setFixedHeight(h);
}

void SurfaceChips::rebuild()
{
    m_queued = false;
    qDeleteAll(m_chips);
    m_chips.clear();
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u : SurfaceMaps::surfaceUniverses(m_doc))
    {
        const SurfaceLayout layout = SurfaceLayout::forProfile(iom->inputPatch(u)->profileName());
        QToolButton *b = new QToolButton(this);
        b->setObjectName(QStringLiteral("footerSurface%1").arg(u));
        b->setAutoRaise(true);
        // A small controller glyph, so the chip reads as "a board".
        b->setIcon(boardIcon(b->palette().color(QPalette::WindowText)));
        b->setIconSize(QSize(14, 14));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("helpTopic", QStringLiteral("control-surfaces.md#the-surface-map"));
        // In Operate the controls are the show's: the page is not said.
        SurfacePages *pages = m_doc->controlMap()->surfacePages();
        const QString withPage = pages->isActive(u) ? pages->chipText(u) : QString();
        b->setText(withPage.isEmpty() ? layout.shortName() : withPage);
        b->setToolTip(tr("%1 on Universe %2: click for its map (what each control does now).")
                          .arg(layout.name()).arg(u + 1));
        b->setAccessibleName(tr("Map of the %1").arg(layout.name()));
        b->setStyleSheet(QStringLiteral("QToolButton { font-weight: bold; padding: 0 6px 0 4px; border: none; }"));
        if (m_height > 0)
            b->setFixedHeight(m_height);
        connect(b, &QToolButton::clicked, this, [this, u]() { emit mapRequested(u); });
        m_box->addWidget(b);
        m_chips.insert(u, b);
    }
    setVisible(!m_chips.isEmpty());
}
