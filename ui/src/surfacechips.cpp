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
#include "surfacesetup.h"
#include "surfaceautopatch.h"
#include "qlcinputprofile.h"
#include "outputpatch.h"
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

QMenu *SurfaceChips::boardMenu(quint32 universe)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom->inputPatch(universe);
    SurfaceSetup *ss = m_doc->surfaceSetup();
    if (ip == nullptr || ip->plugin() == nullptr || ss == nullptr)
        return nullptr;
    SurfaceAutoPatch *ap = ss->autoPatch();
    QMenu *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("footerSurfaceMenu%1").arg(universe));
    menu->setToolTipsVisible(true);

    QAction *map = menu->addAction(tr("Map"));
    map->setObjectName(QStringLiteral("chipMenuMap"));
    connect(map, &QAction::triggered, this, [this, universe]() { emit mapRequested(universe); });

    const SurfaceSetup::Spec *spec = ss->specOf(universe);
    QAction *init = menu->addAction(tr("Send Init Again"));
    init->setObjectName(QStringLiteral("chipMenuInit"));
    init->setEnabled(spec != nullptr && !spec->init.isEmpty());
    init->setToolTip(spec != nullptr && !spec->init.isEmpty()
                         ? tr("Send the board its init again (%1). It is in the Board Log.").arg(spec->initText)
                         : tr("This board needs no init."));
    connect(init, &QAction::triggered, this, [ss, universe]() { ss->sendInit(universe, tr("asked from its chip")); });

    // The profiles that fit this device, the one in use ticked.
    QMenu *prof = menu->addMenu(tr("Profile"));
    prof->setObjectName(QStringLiteral("chipMenuProfile"));
    QStringList names;
    for (const QString &pn : iom->profileNames())
        if (QLCInputProfile *p = iom->profile(pn))
            if (p->deviceMatchScore(ip->inputName()) > 0 || p->asksForDevice(ip->inputName()))
                names << p->name();
    if (!ip->profileName().isEmpty() && !names.contains(ip->profileName()))
        names.prepend(ip->profileName());
    for (const QString &n : std::as_const(names))
    {
        QAction *a = prof->addAction(n);
        a->setCheckable(true);
        a->setChecked(n == ip->profileName());
        connect(a, &QAction::triggered, this, [ap, universe, n]() { ap->setProfile(universe, n); });
    }

    QMenu *fb = menu->addMenu(tr("Feedback"));
    fb->setObjectName(QStringLiteral("chipMenuFeedback"));
    OutputPatch *op = iom->feedbackPatch(universe);
    const bool paired = op != nullptr && op->isPatched();
    if (paired)
    {
        QAction *a = fb->addAction(tr("Paired to %1").arg(op->outputName()));
        a->setCheckable(true);
        a->setChecked(true);
        a->setEnabled(false);
    }
    const InputOutputMap::FeedbackLine line = iom->feedbackLineFor(universe);
    QAction *pair = fb->addAction(line.plugin.isEmpty() ? tr("Pair (no output of this name)")
                                                         : tr("Pair to %1").arg(line.name));
    pair->setObjectName(QStringLiteral("chipMenuPair"));
    pair->setEnabled(!line.plugin.isEmpty() && !(paired && op->outputName() == line.name));
    connect(pair, &QAction::triggered, this, [ap, universe, line]() { ap->setFeedback(universe, line.name); });
    QAction *none = fb->addAction(tr("None"));
    none->setObjectName(QStringLiteral("chipMenuFeedbackNone"));
    none->setCheckable(true);
    none->setChecked(!paired);
    none->setEnabled(paired);
    connect(none, &QAction::triggered, this, [ap, universe]() { ap->setFeedback(universe, QString()); });

    QAction *lamp = menu->addAction(tr("Lamp Test"));
    lamp->setObjectName(QStringLiteral("chipMenuLampTest"));
    lamp->setEnabled(paired);
    lamp->setToolTip(paired ? tr("Light each lamp in turn (its Board Log opens).")
                            : tr("It has no feedback line: Feedback ▸ Pair first."));
    connect(lamp, &QAction::triggered, this, [this, universe]() { emit lampTestRequested(universe); });

    QAction *log = menu->addAction(tr("Board Log…"));
    log->setObjectName(QStringLiteral("chipMenuBoardLog"));
    connect(log, &QAction::triggered, this, [this, universe]() { emit boardLogRequested(universe); });

    menu->addSeparator();
    QAction *setUp = menu->addAction(tr("Set Up…"));
    setUp->setObjectName(QStringLiteral("chipMenuSetUp"));
    connect(setUp, &QAction::triggered, this, [this, universe]() { emit setUpRequested(universe); });
    return menu;
}

void SurfaceChips::popupBoardMenu(quint32 universe)
{
    QToolButton *b = m_chips.value(universe);
    if (b == nullptr)
        return;
    m_lastMenu = boardMenu(universe);
    if (QMenu *menu = m_lastMenu)
    {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(b->mapToGlobal(QPoint(0, b->height())));
    }
}

bool SurfaceChips::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ContextMenu)
    {
        for (auto it = m_chips.constBegin(); it != m_chips.constEnd(); ++it)
            if (it.value() == watched)
            {
                popupBoardMenu(it.key());
                return true;
            }
    }
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
    for (QToolButton *b : std::as_const(m_menuButtons))
        b->setFixedHeight(h);
}

void SurfaceChips::rebuild()
{
    m_queued = false;
    qDeleteAll(m_chips);
    m_chips.clear();
    qDeleteAll(m_menuButtons);
    m_menuButtons.clear();
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
        // The page is always said, in Operate too (where the controls are the show's: the tooltip says so).
        const QString withPage = pages->chipText(u);
        b->setText(withPage.isEmpty() ? layout.shortName() : withPage);
        QString tip = tr("%1 on Universe %2: click for its map (what each control does now); ▾ or a "
                         "right-click for its menu (init, profile, feedback, lamp test, board log).")
                          .arg(layout.name()).arg(u + 1);
        if (!withPage.isEmpty())
            tip += QStringLiteral(" ") + (pages->isActive(u) ? tr("Click the page name to pick a page.")
                                                             : tr("The page is on again in Design: in Operate the controls are the show's."));
        b->setToolTip(tip);
        b->installEventFilter(this);
        b->setAccessibleName(tr("Map of the %1").arg(layout.name()));
        b->setStyleSheet(m_flash.contains(u)
                             ? QStringLiteral("QToolButton { font-weight: bold; padding: 0 6px 0 4px; border: none; "
                                              "background: #5b4a9e; color: white; border-radius: 3px; }")
                             : QStringLiteral("QToolButton { font-weight: bold; padding: 0 6px 0 4px; border: none; }"));
        if (m_height > 0)
            b->setFixedHeight(m_height);
        connect(b, &QToolButton::clicked, this, [this, u]() { emit mapRequested(u); });
        m_box->addWidget(b);
        m_chips.insert(u, b);
        // The ▾ beside it: the board's menu (a right-click on the chip too).
        QToolButton *m = new QToolButton(this);
        m->setObjectName(QStringLiteral("footerSurfaceMenu%1").arg(u));
        m->setAutoRaise(true);
        m->setText(QStringLiteral("\u25BE"));
        m->setCursor(Qt::PointingHandCursor);
        m->setToolTip(tr("%1's menu: send its init again, its profile and feedback, a lamp test, its board log.")
                          .arg(layout.shortName()));
        m->setAccessibleName(tr("Menu of the %1").arg(layout.name()));
        m->setStyleSheet(QStringLiteral("QToolButton { padding: 0 4px 0 0; border: none; }"));
        if (m_height > 0)
            m->setFixedHeight(m_height);
        connect(m, &QToolButton::clicked, this, [this, u]() { popupBoardMenu(u); });
        m_box->addWidget(m);
        m_menuButtons.insert(u, m);
    }
    setVisible(!m_chips.isEmpty());
}
