/*
  Q Light Controller Plus
  stagemarklabel.h

  A small label on the plot -- a stage mark's name or number, or where a
  target is against the stage marks: text on a dark backing, so it reads on
  the plot's near-black and on a light background picture alike. The same
  size on screen whatever the zoom, and never turned with the page (it is
  not a QGraphicsSimpleTextItem, which the view's label counter-rotation
  would turn).

  Header-only and free of Q_OBJECT, so it needs no moc pass or CMake entry.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEMARKLABEL_H
#define STAGEMARKLABEL_H

#include <QGraphicsItem>
#include <QFontMetricsF>
#include <QPainter>
#include <QFont>

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class StageMarkLabel final : public QGraphicsItem
{
public:
    /** @p align says which side of its position the label sits: AlignTop is
     *  below it, AlignLeft to its right, centred on it otherwise. */
    StageMarkLabel(const QString &text, const QColor &colour, Qt::Alignment align,
                   QGraphicsItem *parent = nullptr)
        : QGraphicsItem(parent), m_colour(colour), m_align(align)
    {
        setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        setAcceptedMouseButtons(Qt::NoButton);
        m_font.setPointSizeF(8.5);
        setText(text);
    }

    QString text() const { return m_text; }
    void setText(const QString &text)
    {
        if (text == m_text && !m_rect.isNull())
            return;
        prepareGeometryChange();
        m_text = text;
        const QFontMetricsF fm(m_font);
        const QSizeF sz(fm.horizontalAdvance(m_text) + 6.0, fm.height() + 2.0);
        qreal x = -sz.width() / 2.0, y = -sz.height() / 2.0;
        if (m_align & Qt::AlignLeft)   x = 3.0;
        if (m_align & Qt::AlignRight)  x = -sz.width() - 3.0;
        if (m_align & Qt::AlignTop)    y = 3.0;          // below the anchor
        if (m_align & Qt::AlignBottom) y = -sz.height() - 3.0;
        m_base = QRectF(QPointF(x, y), sz);
        m_rect = m_base.translated(0.0, m_stack);
        update();
    }

    /** Shift the label down @p dy screen px (stacked under another), and its
     *  box before any shift. */
    void setStackOffset(qreal dy)
    {
        if (qFuzzyCompare(dy + 1.0, m_stack + 1.0))
            return;
        prepareGeometryChange();
        m_stack = dy;
        m_rect = m_base.translated(0.0, m_stack);
        update();
    }
    qreal stackOffset() const { return m_stack; }
    QRectF baseRect() const { return m_base; }

    QRectF boundingRect() const override { return m_rect; }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 150));
        p->drawRoundedRect(m_rect, 3.0, 3.0);
        p->setFont(m_font);
        p->setPen(m_colour);
        p->drawText(m_rect, Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
    QColor m_colour;
    Qt::Alignment m_align;
    QFont m_font;
    QRectF m_rect;
    QRectF m_base;
    qreal m_stack = 0.0;
};

/** @} */

#endif
