/*
  Q Light Controller Plus
  capabilitybar.h

  Fork-owned horizontal value bar (0..255) painted with a dark->bright
  gradient and overlaid with named capability regions (e.g. strobe ranges
  that live in an intensity channel). Click/drag to set the value.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CAPABILITYBAR_H
#define CAPABILITYBAR_H

#include <QWidget>
#include <QList>
#include <QString>

/** @addtogroup ui_functions
 * @{
 */

class CapabilityBar final : public QWidget
{
    Q_OBJECT

public:
    CapabilityBar(QWidget *parent = nullptr);

    struct Region { int min; int max; QString name; };

    int value() const { return m_value; }
    void setValue(int v);

    /** Named regions to mark (from a representative fixture channel). */
    void setRegions(const QList<Region> &regions);

signals:
    void valueChanged(int v);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    QSize sizeHint() const override { return QSize(220, 44); }

private:
    void setFromX(int x);
    int xForValue(int v) const;

    int m_value;
    QList<Region> m_regions;
};

/** @} */

#endif
