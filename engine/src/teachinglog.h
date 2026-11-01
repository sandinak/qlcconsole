/*
  Q Light Controller Plus
  teachinglog.h

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

#ifndef TEACHINGLOG_H
#define TEACHINGLOG_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "showcontrol.h"
#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Teaching show's "what just happened" log (docs/features/teaching.md):
 * one line in crew words for every GO, Back and hit, e.g. "GO \u2192 cue 2.1
 * 'Verse lights' (fade in 3 s): Backs \u2192 Full; Fronts tracked".
 *
 * Fed by ShowControl::cueTaken(), so every route (key, Run tab, pad, OSC,
 * the web page) lands here, and by the Virtual Console button for a hit.
 * The line is built on the main thread when the command arrives, from the
 * resolved cue (ShowTreeController::goLogLine()); nothing runs per frame.
 * With Teaching off it does nothing at all and keeps nothing.
 */
class QLC_ENGINE_EXPORT GoLog final : public QObject
{
    Q_OBJECT

public:
    explicit GoLog(Doc *doc);

    /** Listen to the show's ShowControl (Doc, once both exist). */
    void attach();

    /** The log is on: the show is a Teaching show. */
    bool active() const;

    /** The lines, oldest first (at most kMaxLines). */
    QStringList lines() const { return m_lines; }
    QString last() const { return m_lines.isEmpty() ? QString() : m_lines.last(); }
    void clear();

    /** A hit fired (a Virtual Console button, hit cue @a hitId). */
    void hit(quint32 hitId);

    static const int kMaxLines = 200;

signals:
    /** A line was added (@a line has its time in front). */
    void added(const QString &line);
    void cleared();

private slots:
    void slotCueTaken(ShowControl::Command command, quint32 chaserId, int fromStep, int fromSub,
                      int step, int sub);

private:
    void add(const QString &text);

    Doc *m_doc;
    QStringList m_lines;
};

/** @} */

#endif
