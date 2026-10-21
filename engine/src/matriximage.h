/*
  Q Light Controller Plus
  matriximage.h

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

#ifndef MATRIXIMAGE_H
#define MATRIXIMAGE_H

#include <QSharedPointer>
#include <QByteArray>
#include <QVector>
#include <QString>

class QJSEngine;
class QJSValue;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The picture a Matrix Image effect plays: a still or an animated GIF,
 * kept IN the show. An "image" effect setting holds the file itself as a
 * data URI --
 *
 *     data:image/gif;name=logo.gif;base64,R0lGODlh...
 *
 * -- so the show carries its pictures wherever it goes (another desk, a
 * backup, a .qlcc), with nothing to relink. The file is kept as it was
 * picked (up to kMaxFileBytes); it is decoded, once, into frames no bigger
 * than kMaxSide on a side (a box rig is a few hundred pixels, not a
 * screen), at most kMaxFrames of them, each composited on black. Decoded
 * pictures are shared, by the setting's text, between every effect that
 * shows one (thread-safe: the effects decode on the DMX thread).
 */
namespace MatrixImage
{
    /** Largest file a show takes. */
    const qint64 kMaxFileBytes = 2 * 1024 * 1024;
    /** Decoded frames fit inside kMaxSide x kMaxSide. */
    const int kMaxSide = 160;
    /** Frames kept of an animation (the rest are dropped). */
    const int kMaxFrames = 240;

    struct Frames
    {
        QString name;              ///< the file's name, as picked
        int width = 0, height = 0; ///< every frame's size
        QVector<QByteArray> rgba;  ///< width * height * 4 bytes a frame, premultiplied (on black)
        QVector<int> delaysMs;     ///< how long each frame shows (0 for a still)
        int totalMs = 0;           ///< one time through
        QString key;               ///< a short hash of the setting: same picture, same key
    };

    /** The setting's text for the file at @p path, or empty (and why in
     *  @p error) when it isn't a picture Qt can read or is too big. */
    QString fromFile(const QString &path, QString *error = nullptr);

    /** The setting's text for @p bytes named @p name (as fromFile). */
    QString fromBytes(const QByteArray &bytes, const QString &name, QString *error = nullptr);

    /** Is @p value an image setting (a data URI)? */
    bool isImage(const QString &value);

    /** The file in an image setting: its bytes, name and media type. */
    bool unpack(const QString &value, QByteArray *bytes, QString *name = nullptr,
                QString *mimeType = nullptr);

    /** The decoded frames of an image setting, decoding it the first time
     *  (cached, thread-safe); null when it isn't one or can't be read. */
    QSharedPointer<const Frames> decode(const QString &value);

    /** @p f as a script sees it, in @p engine: { name, width, height,
     *  frames: [ArrayBuffer of RGBA], delays: [ms], totalMs, key }. */
    QJSValue toScript(QJSEngine *engine, const Frames &f);

    /** A line for people: "logo.gif · 64 × 32 · 24 frames · 180 KB". */
    QString describe(const QString &value);

    /** Forget every decoded picture (tests). */
    void clearCache();
}

/** @} */

#endif
