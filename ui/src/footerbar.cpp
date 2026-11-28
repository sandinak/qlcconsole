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
#include <QLayout>
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
        return;
    }
    place();
}

namespace
{
/* The row of QStatusBar's layout that holds the stretch between its normal
   and its permanent items (QStatusBar::reformat(): fixed spacings, the
   normal items, one stretch, the permanent items), and where the stretch is. */
QLayout *chipRow(QLayout *l, int &stretchAt)
{
    for (int i = 0; l != nullptr && i < l->count(); i++)
    {
        QLayoutItem *it = l->itemAt(i);
        if (QSpacerItem *sp = it->spacerItem())
        {
            if (sp->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag)
            {
                stretchAt = i;
                return l;
            }
        }
        else if (QLayout *inner = it->layout())
        {
            if (QLayout *found = chipRow(inner, stretchAt))
                return found;
        }
    }
    return nullptr;
}
}

QList<QWidget *> FooterBar::permanentChips() const
{
    QList<QWidget *> out;
    int at = -1;
    QLayout *row = chipRow(layout(), at);
    for (int i = at + 1; row != nullptr && i < row->count(); i++)
        if (QWidget *w = row->itemAt(i)->widget())
            out << w;
    return out;
}

void FooterBar::place()
{
    if (m_text.isEmpty())
        return;
    /* The message stops short of the permanent chips shown now, not of the
       ones shown when it came: Edit Live, shown on going to Operate while a
       message was up, was drawn under it and its click went to the message
       (train/053s). A normal chip shown again meanwhile stays under it. */
    int right = width() - 2;
    for (QWidget *w : permanentChips())
        if (w != m_label && w->isVisibleTo(this))
            right = qMin(right, w->geometry().left() - 6);
    const int left = 2;
    const QRect r(left, 0, qMax(0, right - left), height());
    m_label->setGeometry(r);
    const int room = r.width() - m_label->contentsMargins().left() - m_label->contentsMargins().right();
    m_label->setText(m_label->fontMetrics().elidedText(m_text, Qt::ElideRight, qMax(0, room)));
    m_label->setToolTip(m_label->text() != m_text ? m_text : QString());
    m_label->setVisible(room > 0 && !m_label->text().isEmpty());
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
