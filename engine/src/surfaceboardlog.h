/*
  Q Light Controller Plus - qlcconsole
  surfaceboardlog.h

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

#ifndef SURFACEBOARDLOG_H
#define SURFACEBOARDLOG_H

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include "qlcengine_export.h"

class Doc;
class QLCIOPlugin;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What a control surface sent and was sent, per board (universe), for
 * debugging on the real hardware: the last kSize MIDI messages in and out,
 * timestamped and decoded (note, CC, pitch bend, SysEx) with the control's
 * name from the board's profile, and the app's own steps beside them (its
 * init sent or not and why, a profile chosen, the motor fader sent where
 * the Grand Master is). Copied as text for a bug report (text()).
 *
 * In: every value the board's plugin hands over, before the input patch
 * gathers them (a fader's every step, not one per frame). Out: every
 * feedback value (InputOutputMap::feedbackSent) and every message the
 * surface setup sends (logSysExOut()). Owned by SurfaceSetup.
 */
class QLC_ENGINE_EXPORT SurfaceBoardLog final : public QObject
{
    Q_OBJECT

public:
    static constexpr int kSize = 500;

    enum Direction { In, Out, Note };

    struct Entry
    {
        qint64 msecs = 0;               ///< since the epoch
        Direction dir = Note;
        QByteArray bytes;               ///< the MIDI bytes (empty for a note)
        QString text;                   ///< decoded, or the note itself
    };

    explicit SurfaceBoardLog(Doc *doc, QObject *parent = nullptr);

    /** The entries of @p universe's board, oldest first. */
    QList<Entry> entries(quint32 universe) const { return m_entries.value(universe); }
    /** The whole log of @p universe as text: a header naming the board,
     *  its profile and lines, then one line per entry. */
    QString text(quint32 universe) const;
    /** One entry as text ("15:02:11.402  IN   E0 00 5A  Pitch bend ch 1 = 180 (Fader)"). */
    static QString entryText(const Entry &e);

    /** Note an app step for @p universe's board. */
    void note(quint32 universe, const QString &text);
    /** A message the app sent the board outside feedback (an init). */
    void logSysExOut(quint32 universe, const QByteArray &message, bool sent, const QString &why = QString());

    /** Clear @p universe's log. */
    void clear(quint32 universe);

    /** The MIDI bytes of QLC+ channel @p channel at @p value (0-255), as the
     *  MIDI plugin sends or reads them; @p midiChannel 0-15, or -1 for the
     *  channel's own. Empty when it is no channel message. */
    static QByteArray midiBytes(quint32 channel, uchar value, int midiChannel = -1);
    /** "Note 93 on, ch 1, velocity 127", "CC 16, ch 1 = 65",
     *  "Pitch bend, ch 1 = 8192", "SysEx (12 bytes)". */
    static QString decode(const QByteArray &bytes);

    /** The board's plugin is watched for its raw input (tests: the stub). */
    void watchPlugin(QLCIOPlugin *plugin);

signals:
    /** @p universe's log grew. */
    void logged(quint32 universe);

private:
    void add(quint32 universe, Direction dir, const QByteArray &bytes, const QString &text);
    QString controlName(quint32 universe, quint32 channel) const;
    void slotRawInput(QLCIOPlugin *plugin, quint32 input, quint32 channel, uchar value);

    Doc *m_doc;
    QHash<quint32, QList<Entry>> m_entries;
    QList<QPointer<QLCIOPlugin>> m_watched;
};

/** @} */

#endif
