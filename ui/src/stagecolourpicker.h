/*
  Q Light Controller Plus
  stagecolourpicker.h

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

#ifndef STAGECOLOURPICKER_H
#define STAGECOLOURPICKER_H

#include <QObject>
#include <QColor>

class QColorDialog;

/**
 * Makes a QColorDialog a stage colour picker: its "Basic colors" grid
 * becomes the stage colours (PaletteNaming::stageColours(): Flesh, Blacklight,
 * the fluorescents, Tungsten...), hovering a swatch says its name and hex,
 * and hovering the colour field says the nearest named colour.
 */
class StageColourPicker : public QObject
{
    Q_OBJECT

public:
    /** Install on @a dialog (owned by it). */
    static StageColourPicker *install(QColorDialog *dialog);

    /** "Flesh  #ffcdaa" for a swatch, "(approx. sign) Flesh  #fdcba0" for any other colour. */
    static QString describe(const QColor &rgb);

    /** The extra emitters (White=red, Amber=green, UV=blue) a stage colour
     *  needs, when @a rgb is exactly that colour's swatch (Blacklight: UV);
     *  invalid otherwise. */
    static QColor emittersFor(const QColor &rgb);

    /** The dialog's swatch grid and colour field, for tests. */
    QWidget *swatchGrid() const { return m_grid; }
    QWidget *colourField() const { return m_field; }

    /** The swatch under @a pos in the grid (-1: none). */
    int swatchAt(const QPoint &pos) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit StageColourPicker(QColorDialog *dialog);

    QWidget *m_grid = nullptr;
    QWidget *m_field = nullptr;
};

#endif
