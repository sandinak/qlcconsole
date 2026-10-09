/*
  Q Light Controller Plus
  colourswatchbutton.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef COLOURSWATCHBUTTON_H
#define COLOURSWATCHBUTTON_H

#include <QWidget>
#include <QColor>

class QPushButton;
class QToolButton;

/** @addtogroup ui UI
 * @{
 */

/**
 * The one way a colour is picked: a button showing the colour and its hex
 * value, which opens the colour dialog; and, where a colour can go back to
 * something (a gel to none, a sock to white), a Reset beside it. Emits
 * colourChanged() when the user changes it -- the caller applies it at once,
 * as every rig dialog does.
 */
class ColourSwatchButton final : public QWidget
{
    Q_OBJECT

public:
    /** @p pickerTitle names the colour dialog ("Sock colour"). */
    ColourSwatchButton(const QColor &colour, const QString &pickerTitle,
                       QWidget *parent = nullptr);

    QColor colour() const { return m_colour; }
    /** Show @p colour (invalid = none), without signalling. */
    void setColour(const QColor &colour);

    /** Offer a Reset back to @p colour; an invalid colour means "none" (a
     *  fixture with no gel). Not called: no Reset. */
    void setResetColour(const QColor &colour);

    /** Open the picker, as a click does (also for tests and shortcuts). */
    void pick();

signals:
    void colourChanged(const QColor &colour);

private:
    void showColour();
    void change(const QColor &colour);

    QColor m_colour;
    QColor m_resetColour;
    QString m_title;
    QPushButton *m_swatch;
    QToolButton *m_reset;
};

/** @} */

#endif
