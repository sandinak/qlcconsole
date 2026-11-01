/*
  Q Light Controller Plus
  qrencoder.h

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

#ifndef QRENCODER_H
#define QRENCODER_H

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVector>
#include "qlcui_export.h"

/** @addtogroup ui_io
 * @{
 */

/**
 * A small QR Code encoder (ISO/IEC 18004): byte mode, error correction M,
 * versions 1 to 10 (up to 213 bytes: a URL). Self-contained, no library.
 * Used for the address of the phone Run page (Show Control page).
 */
class QLC_UI_EXPORT QrEncoder final
{
public:
    /** Longest text (UTF-8 bytes) encode() takes. */
    static const int MaxBytes = 213;

    /** The modules of the code: size x size, row by row, 1 = dark. Empty
     *  when @a text is empty or longer than MaxBytes. @a mask: -1 picks the
     *  best of the eight (tests set one). */
    static QVector<quint8> encode(const QString &text, int *size, int mask = -1, int *version = nullptr);

    /** The code as a black-on-white image, @a scale pixels a module, with a
     *  four-module quiet zone. Null when encode() is. */
    static QImage image(const QString &text, int scale = 4);
};

/** @} */

#endif
