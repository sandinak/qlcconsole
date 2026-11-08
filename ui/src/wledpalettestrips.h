/*
  Q Light Controller Plus
  wledpalettestrips.h

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

#ifndef WLEDPALETTESTRIPS_H
#define WLEDPALETTESTRIPS_H

#include <QStringList>
#include <QVector>
#include <QPixmap>
#include <QList>
#include <QIcon>

#include "wledpalettes.h"

class QComboBox;
class Doc;

/**
 * The colouring each of a WLED's colour palettes would use, as a small strip
 * beside its name. The stops are the device's own (kept by its WLED output
 * the first time it answers, by MAC), else a bundled copy of WLED's built-in
 * palettes -- so it works offline. Nothing here touches the network; a strip
 * is drawn once into a pixmap, never in a paint event.
 */
namespace WledPaletteStrips
{
    struct Set
    {
        QStringList names;                          ///< by WLED palette number
        QVector<WledPalettes::Palette> palettes;    ///< same indexes
        bool fromDevice = false;                    ///< else the bundled built-ins
    };

    /** WLED's built-in palettes, from the resource. */
    Set bundled();

    /** What is kept for the device (its MAC, else its address), else bundled. */
    Set forDevice(const QString &mac, const QString &address);

    /** The first WLED among @p fixtureIds that has palettes kept, else bundled. */
    Set forFixtures(const Doc *doc, const QList<quint32> &fixtureIds);

    /** A strip of palette @p index of @p set, its colour slots as given. */
    QPixmap strip(const Set &set, int index, QRgb c1, QRgb c2, QRgb c3,
                  const QSize &size = QSize(72, 12));

    /** @p box filled with one row per palette: its strip and name; the item
     *  data is the WLED palette number. */
    void fill(QComboBox *box, const Set &set, QRgb c1, QRgb c2, QRgb c3);
}

#endif
