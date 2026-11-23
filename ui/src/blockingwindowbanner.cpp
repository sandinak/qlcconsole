/*
  Q Light Controller Plus
  blockingwindowbanner.cpp

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

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QWindow>

#include "blockingwindowbanner.h"

namespace
{
const int kMargin = 8;

QString titleOf(const QWidget *w)
{
    QString t = w->windowTitle();
    t.remove(QStringLiteral("[*]"));
    t = t.trimmed();
    return t.isEmpty() ? QObject::tr("A window") : t;
}
} // namespace

BlockingWindowBanner *BlockingWindowBanner::whileOpen(QWidget *heldArea, QWidget *blocker)
{
    if (heldArea == nullptr || blocker == nullptr)
        return nullptr;
    return new BlockingWindowBanner(heldArea, blocker);
}

BlockingWindowBanner *BlockingWindowBanner::bannerOn(const QWidget *heldArea)
{
    if (heldArea == nullptr)
        return nullptr;
    const auto list = heldArea->findChildren<BlockingWindowBanner *>(QString(), Qt::FindDirectChildrenOnly);
    for (BlockingWindowBanner *b : list)
        if (b->m_filtering)
            return b;
    return nullptr;
}

BlockingWindowBanner::BlockingWindowBanner(QWidget *heldArea, QWidget *blocker)
    : QFrame(heldArea)
    , m_area(heldArea)
    , m_blocker(blocker)
{
    setObjectName(QStringLiteral("blockingWindowBanner"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    /* Amber with dark text: it reads as "waiting on something" in light and
       dark themes alike, and stands off the plot under it. */
    setStyleSheet(QStringLiteral(
        "#blockingWindowBanner { background: #f2c14e; border: 1px solid #9a6b00; border-radius: 4px; }"
        "#blockingWindowBanner QLabel { color: #1f1600; }"));

    QHBoxLayout *hl = new QHBoxLayout(this);
    hl->setContentsMargins(10, 4, 4, 4);
    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("blockingWindowText"));
    m_label->setText(tr("<b>%1 is open.</b> This view waits until it closes.")
                     .arg(titleOf(blocker).toHtmlEscaped()));
    m_label->setWordWrap(true);
    hl->addWidget(m_label, 1);
    m_show = new QPushButton(tr("Show it"), this);
    m_show->setObjectName(QStringLiteral("blockingWindowShow"));
    m_show->setToolTip(tr("Bring %1 to the front, on this screen").arg(titleOf(blocker)));
    m_show->setFocusPolicy(Qt::NoFocus);       // a click; never a keyboard stop in a held view
    hl->addWidget(m_show);
    connect(m_show, &QPushButton::clicked, this, &BlockingWindowBanner::showBlocker);

    if (QDialog *dlg = qobject_cast<QDialog *>(blocker))
        connect(dlg, &QDialog::finished, this, &BlockingWindowBanner::finish);
    connect(blocker, &QObject::destroyed, this, &BlockingWindowBanner::finish);

    /* After the lock's filter, so this one runs first: a click on Show it
       must reach it while the lock drops every other click here. */
    qApp->installEventFilter(this);
    m_filtering = true;

    place();
    show();
    raise();
}

BlockingWindowBanner::~BlockingWindowBanner()
{
    if (m_filtering)
        qApp->removeEventFilter(this);
}

QString BlockingWindowBanner::text() const
{
    return m_label->text();
}

void BlockingWindowBanner::place()
{
    if (m_area == nullptr)
        return;
    const int w = qMax(0, m_area->width() - 2 * kMargin);
    const int h = qMax(sizeHint().height(), heightForWidth(w));
    setGeometry(kMargin, kMargin, w, h);
    raise();
}

void BlockingWindowBanner::finish()
{
    if (m_filtering)
    {
        qApp->removeEventFilter(this);
        m_filtering = false;
    }
    hide();
    deleteLater();
}

void BlockingWindowBanner::showBlocker()
{
    bringBack(m_blocker, m_area);
}

void BlockingWindowBanner::bringBack(QWidget *window, QWidget *near)
{
    if (window == nullptr)
        return;
    window = window->window();
    if (window->isMinimized())
        window->showNormal();
    QWidget *anchor = near != nullptr ? near->window() : nullptr;
    QScreen *screen = anchor != nullptr ? anchor->screen() : nullptr;
    if (screen != nullptr)
    {
        const QRect room = screen->availableGeometry();
        QRect r = window->frameGeometry();
        if (window->screen() != screen || !room.contains(r))
        {
            r.moveCenter(anchor->frameGeometry().center());
            r.moveLeft(qBound(room.left(), r.left(), qMax(room.left(), room.right() - r.width())));
            r.moveTop(qBound(room.top(), r.top(), qMax(room.top(), room.bottom() - r.height())));
            if (window->windowHandle() != nullptr && window->windowHandle()->screen() != screen)
                window->windowHandle()->setScreen(screen);
            window->move(r.topLeft());
        }
    }
    window->show();
    window->raise();
    window->activateWindow();
}

bool BlockingWindowBanner::onShowButton(QObject *obj) const
{
    for (QObject *o = obj; o != nullptr; o = o->parent())
        if (o == m_show)
            return true;
    return false;
}

bool BlockingWindowBanner::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_area && e->type() == QEvent::Resize)
    {
        place();
        return false;
    }
    const QEvent::Type t = e->type();
    if (t != QEvent::MouseButtonPress && t != QEvent::MouseButtonRelease
        && t != QEvent::MouseButtonDblClick)
        return false;
    if (!m_filtering || !isVisible() || !m_show->isVisible())
        return false;

    bool over = false;
    auto *me = static_cast<QMouseEvent *>(e);
    if (obj->isWindowType())
    {
        /* A real screen hands the click to the window first: answer it
           there, so nothing between the window and the button (a lock that
           drops clicks outside its editor) can keep it from Show it. */
        if (m_show->window()->windowHandle() != obj)
            return false;
        const QPoint at = m_show->mapFrom(m_show->window(), me->position().toPoint());
        over = m_show->rect().contains(at);
        if (!over && !m_pressed)
            return false;
    }
    else if (onShowButton(obj))
    {
        over = m_show->rect().contains(m_show->mapFrom(static_cast<QWidget *>(obj),
                                                       me->position().toPoint()));
    }
    else
    {
        return false;
    }

    if (me->button() != Qt::LeftButton)
        return true;
    if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonDblClick)
    {
        m_pressed = true;
        m_show->setDown(true);
    }
    else if (m_pressed)
    {
        m_pressed = false;
        m_show->setDown(false);
        if (over)
            showBlocker();
    }
    return true;
}
