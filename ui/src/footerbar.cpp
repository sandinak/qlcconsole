/*
  Q Light Controller Plus - qlcconsole
  footerbar.cpp

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

#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QSizeGrip>
#include <QStyleOption>

#include "footerbar.h"

FooterBar::FooterBar(QWidget *parent)
    : QStatusBar(parent)
{
    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("footerMessage"));
    m_label->setAutoFillBackground(true);
    m_label->setBackgroundRole(QPalette::Window);
    m_label->setContentsMargins(4, 0, 4, 0);
    m_label->setTextFormat(Qt::PlainText);
    m_label->hide();
    connect(this, &QStatusBar::messageChanged, this, &FooterBar::showText);
}

void FooterBar::showText(const QString &text)
{
    m_text = text;
    if (text.isEmpty())
    {
        m_label->hide();
        m_permanent.clear();
        return;
    }
    /* QStatusBar has just hidden its other chips for the message: the ones
       still shown are the permanent ones, which the message stops short of. */
    m_permanent.clear();
    for (QObject *o : children())
    {
        QWidget *w = qobject_cast<QWidget *>(o);
        if (w == nullptr || w == m_label || qobject_cast<QSizeGrip *>(w) != nullptr || w->isWindow())
            continue;
        if (w->isVisibleTo(this))
            m_permanent << w;
    }
    place();
    m_label->show();
    m_label->raise();
}

void FooterBar::place()
{
    if (m_text.isEmpty())
        return;
    int right = width() - 2;
    for (const QPointer<QWidget> &w : std::as_const(m_permanent))
        if (w != nullptr && w->isVisibleTo(this))
            right = qMin(right, w->geometry().left() - 6);
    const int left = 2;
    const QRect r(left, 0, qMax(0, right - left), height());
    m_label->setGeometry(r);
    const int room = r.width() - m_label->contentsMargins().left() - m_label->contentsMargins().right();
    m_label->setText(m_label->fontMetrics().elidedText(m_text, Qt::ElideRight, qMax(0, room)));
    m_label->setToolTip(m_label->text() != m_text ? m_text : QString());
    m_label->raise();
}

QSize FooterBar::minimumSizeHint() const
{
    return QSize(0, QStatusBar::minimumSizeHint().height());
}

void FooterBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    /* The bar's panel only: never QStatusBar's own message text, which it
       draws across the whole bar (under the permanent chips). */
    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_PanelStatusBar, &opt, &p, this);
}

void FooterBar::resizeEvent(QResizeEvent *event)
{
    QStatusBar::resizeEvent(event);
    place();
}

bool FooterBar::event(QEvent *event)
{
    const bool r = QStatusBar::event(event);
    // A chip came, went or changed size: the message's room follows.
    if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::ChildAdded)
        place();
    return r;
}
