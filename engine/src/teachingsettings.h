/*
  Q Light Controller Plus
  teachingsettings.h

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

#ifndef TEACHINGSETTINGS_H
#define TEACHINGSETTINGS_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QXmlStreamAttributes>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCTeaching QStringLiteral("Teaching")

/**
 * Teaching mode (docs/features/teaching.md): a flag saved with the show.
 * A show handed to students carries it. It turns on the classroom features
 * (scramble practice and its score), holds real network output until the
 * teacher allows it, and warns when the show is run for real.
 *
 * Saved under the Engine as <Teaching Enabled="1" AllowOutput="1"
 * Lesson=".." Link=".." HandedOut=".."/>, only while Teaching is on, so a
 * show without it is byte-for-byte what it was. QLC+ 4 and older builds skip
 * the element. The instructor PIN is the show's own (ShowOperate): one PIN
 * per show. Unknown attributes of a newer build are kept and written back.
 */
class QLC_ENGINE_EXPORT TeachingSettings final : public QObject
{
    Q_OBJECT

public:
    explicit TeachingSettings(Doc *doc);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);

    /** The teacher let real (network) output through in a Teaching show. */
    bool allowOutput() const { return m_allowOutput; }
    void setAllowOutput(bool on);
    /** Teaching holds real network output now. */
    bool holdsOutput() const { return m_enabled && !m_allowOutput; }

    /** Lesson number ("3.6") and the page of its handout; both optional. */
    QString lesson() const { return m_lesson; }
    QString link() const { return m_link; }
    void setLesson(const QString &lesson, const QString &link);

    /** When Make Student Handout wrote the show (ISO date time), or empty. */
    QString handedOut() const { return m_handedOut; }
    void setHandedOut(const QString &when);

    /** The show as Make Student Handout wrote it (the whole file, deflated),
     *  kept inside the handout so the teacher can put the show back to it
     *  (Reset to handout). Empty in a show that was not made by a handout.
     *  Saved as the text of the Teaching element; never part of itself. */
    QByteArray handoutSnapshot() const { return m_snapshot; }
    bool hasHandoutSnapshot() const { return m_snapshot.isEmpty() == false; }
    /** Without marking the show modified: it travels with the file. */
    void setHandoutSnapshot(const QByteArray &snapshot);
    /** Deflate / inflate the file bytes of a show (the format of the
     *  snapshot); inflate gives empty for a damaged one. */
    static QByteArray packSnapshot(const QByteArray &file);
    static QByteArray unpackSnapshot(const QByteArray &snapshot);

    /** What a copy of the show is written with instead of the open show's
     *  own settings (Make Student Handout): nothing is changed in the open
     *  show, so nothing is held, marked modified or undone. */
    struct SaveOverride
    {
        bool enabled = true;
        bool allowOutput = false;
        QString lesson;
        QString link;
        QString handedOut;
        QByteArray snapshot;    //!< the Reset to handout state, or empty
    };
    /** Set for the duration of one save of the copy, then cleared with
     *  nullptr; the pointer is not owned. */
    void setSaveOverride(const SaveOverride *saveOverride) { m_saveOverride = saveOverride; }

    /** Back to off (a new show). */
    void reset();

    /** Write the element (nothing when off). */
    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);

signals:
    void teachingChanged();

private:
    void changed();
    void applyOutputHold();

private:
    Doc *m_doc;
    bool m_enabled = false;
    bool m_allowOutput = false;
    QString m_lesson;
    QString m_link;
    QString m_handedOut;
    QByteArray m_snapshot;
    QXmlStreamAttributes m_unknown;
    const SaveOverride *m_saveOverride = nullptr;
};

/** @} */

#endif
