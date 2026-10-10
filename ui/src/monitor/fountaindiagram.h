/*
  Q Light Controller Plus
  fountaindiagram.h

  The picture beside Add Fountain's fields: the fountain from above and cut
  through its front, each measurement drawn where it is taken and labelled
  with its value, the one being edited picked out.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FOUNTAINDIAGRAM_H
#define FOUNTAINDIAGRAM_H

#include <QWidget>
#include <QHash>

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class FountainDiagram final : public QWidget
{
    Q_OBJECT

public:
    explicit FountainDiagram(QWidget *parent = nullptr);

    /** The measurements, metres: "width", "depth", "base", "outer", "inner",
     *  "thickness", "gap". */
    void setValue(const QString &key, double metres, const QString &label);
    /** Whether the flame is drawn in the middle. */
    void setFlame(bool on);
    /** The measurement being edited (empty: none). */
    void setHighlight(const QString &key);
    QString highlight() const { return m_highlight; }

    QSize sizeHint() const override { return QSize(440, 520); }
    QSize minimumSizeHint() const override { return QSize(360, 440); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double value(const QString &key) const { return m_values.value(key, 0.0); }

    QHash<QString, double> m_values;
    QHash<QString, QString> m_labels;
    QString m_highlight;
    bool m_flame = true;
};

/** @} */

#endif
