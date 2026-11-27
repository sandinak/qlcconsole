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

#include "surfacechips.h"
#include "surfacelayout.h"
#include "inputoutputmap.h"
#include "surfacemap.h"
#include "inputpatch.h"
#include "surfacepages.h"
#include "controlmap.h"
#include "doc.h"
#include <QMouseEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>

bool SurfaceChips::onPageName(quint32 universe, int x) const
{
    QToolButton *b = m_chips.value(universe);
    if (b == nullptr)
        return false;
    const int dot = b->text().indexOf(QStringLiteral(" · "));
    if (dot < 0)
        return false;
    QFont f = b->font();
    f.setBold(true);
    // The name's end, and the 6 px padding either side of the text (the chip's style).
    return x >= 6 + QFontMetrics(f).horizontalAdvance(b->text().left(dot)) + 2;
}

QMenu *SurfaceChips::pageMenu(quint32 universe)
{
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    const PageSet set = pages->pageSet(universe);
    if (!set.isValid())
        return nullptr;
    QMenu *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("footerSurfacePages%1").arg(universe));
    for (int i = 0; i < set.pages().size(); ++i)
    {
        QAction *a = menu->addAction(set.pages().at(i).name);
        a->setCheckable(true);
        a->setChecked(i == pages->page(universe));
        connect(a, &QAction::triggered, this, [pages, universe, i]() { pages->setPage(universe, i); });
    }
    return menu;
}

bool SurfaceChips::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        for (auto it = m_chips.constBegin(); it != m_chips.constEnd(); ++it)
            if (it.value() == watched && me->button() == Qt::LeftButton && onPageName(it.key(), int(me->position().x())))
            {
                if (QMenu *menu = pageMenu(it.key()))
                {
                    menu->setAttribute(Qt::WA_DeleteOnClose);
                    menu->popup(it.value()->mapToGlobal(QPoint(0, it.value()->height())));
                    return true;
                }
            }
    }
    return QWidget::eventFilter(watched, event);
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
    connect(pages, &SurfacePages::pageChanged, this, [this, queue](quint32 u) {
        queue();
        m_flash.insert(u);
        QTimer::singleShot(1000, this, [this, u, queue]() {
            m_flash.remove(u);
            queue();
        });
    });
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
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("helpTopic", QStringLiteral("control-surfaces.md#the-surface-map"));
        // In Operate the controls are the show's: the page is not said.
        SurfacePages *pages = m_doc->controlMap()->surfacePages();
        // The page is always said, in Operate too (where the controls are the show's: the tooltip says so).
        const QString withPage = pages->chipText(u);
        b->setText(withPage.isEmpty() ? layout.shortName() : withPage);
        QString tip = tr("%1 on Universe %2: click for its map (what each control does now).")
                          .arg(layout.name()).arg(u + 1);
        if (!withPage.isEmpty())
            tip += QStringLiteral(" ") + (pages->isActive(u) ? tr("Click the page name to pick a page.")
                                                             : tr("The page is on again in Design: in Operate the controls are the show's."));
        b->setToolTip(tip);
        b->installEventFilter(this);
        b->setAccessibleName(tr("Map of the %1").arg(layout.name()));
        b->setStyleSheet(m_flash.contains(u)
                             ? QStringLiteral("QToolButton { font-weight: bold; padding: 0 6px; border: none; "
                                              "background: #5b4a9e; color: white; border-radius: 3px; }")
                             : QStringLiteral("QToolButton { font-weight: bold; padding: 0 6px; border: none; }"));
        if (m_height > 0)
            b->setFixedHeight(m_height);
        connect(b, &QToolButton::clicked, this, [this, u]() { emit mapRequested(u); });
        m_box->addWidget(b);
        m_chips.insert(u, b);
    }
    setVisible(!m_chips.isEmpty());
}
