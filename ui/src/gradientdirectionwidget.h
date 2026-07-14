/*
  Q Light Controller Plus
  gradientdirectionwidget.h

  Visual editor for an EffectScript "direction" parameter — a compass-style
  control that sets the angle a spatial gradient travels across the rig and
  previews the result. The widget fills itself with a live gradient from the
  look's start colour to its end colour, oriented along the chosen angle, with
  a draggable arrow pointing toward the END colour. Four quick buttons set the
  cardinal screen directions (→ ↓ ← ↑) that match the 2D Monitor's view.

  The angle is a screen-space bearing in degrees: 0 = left→right, 90 = top→
  bottom, 180 = right→left, 270 = bottom→top. The consuming script flips stage
  Y so this reads WYSIWYG against the Monitor (upstage = top).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef GRADIENTDIRECTIONWIDGET_H
#define GRADIENTDIRECTIONWIDGET_H

#include <QWidget>
#include <QColor>
#include <QList>

class QPushButton;

/** @addtogroup ui_functions
 * @{
 */

class GradientDirectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GradientDirectionWidget(QWidget *parent = nullptr);

    /** Current bearing in degrees [0, 360). */
    double angle() const { return m_angle; }
    /** Set the bearing (degrees). Does NOT emit angleChanged. */
    void   setAngle(double deg);
    /** Set the preview's start/end colours (the look's two gradient ends). */
    void   setColors(const QColor &start, const QColor &end);
    /** Set the full multi-stop gradient (the look's Colour palettes, in order).
     *  Overrides setColors() for the preview when it has 2+ entries. */
    void   setStops(const QList<QColor> &stops);

    QSize sizeHint() const override { return QSize(200, 230); }
    QSize minimumSizeHint() const override { return QSize(150, 180); }

signals:
    void angleChanged(double deg);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QRect   compassRect() const;       //!< square preview area above the buttons
    void    setAngleFromPos(const QPointF &p, bool snap);
    void    reposition();

    static const int kBtnH = 24;       //!< quick-button strip height

    double  m_angle = 0.0;             //!< screen bearing, degrees
    QColor  m_start = QColor(255, 0, 0);
    QColor  m_end   = QColor(0, 0, 255);
    QList<QColor> m_stops;             //!< full multi-stop gradient (2+ → used)

    QPushButton *m_right;   // →   0°
    QPushButton *m_down;    // ↓  90°
    QPushButton *m_left;    // ←  180°
    QPushButton *m_up;      // ↑  270°
};

/** @} */

#endif // GRADIENTDIRECTIONWIDGET_H
