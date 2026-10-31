/*
  Q Light Controller Plus
  teachinglog.cpp

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

#include <QTime>

#include "teachingsettings.h"
#include "showtreecontroller.h"
#include "teachinglog.h"
#include "doc.h"

GoLog::GoLog(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

void GoLog::attach()
{
    if (m_doc->showControl() != nullptr)
        connect(m_doc->showControl(), &ShowControl::cueTaken, this, &GoLog::slotCueTaken);
    if (m_doc->teachingSettings() != nullptr)
        connect(m_doc->teachingSettings(), &TeachingSettings::teachingChanged, this, [this]() {
            if (!active())
                clear();
        });
}

bool GoLog::active() const
{
    return m_doc->teachingSettings() != nullptr && m_doc->teachingSettings()->enabled();
}

void GoLog::clear()
{
    if (m_lines.isEmpty())
        return;
    m_lines.clear();
    emit cleared();
}

void GoLog::add(const QString &text)
{
    const QString line = QTime::currentTime().toString(QStringLiteral("HH:mm:ss")) + QLatin1Char(' ') + text;
    m_lines << line;
    while (m_lines.count() > kMaxLines)
        m_lines.removeFirst();
    emit added(line);
}

void GoLog::hit(quint32 hitId)
{
    if (!active() || m_doc->showTree() == nullptr)
        return;
    add(m_doc->showTree()->hitLogLine(hitId));
}

void GoLog::slotCueTaken(ShowControl::Command command, quint32 chaserId, int fromStep, int fromSub,
                         int step, int sub)
{
    if (!active() || m_doc->showTree() == nullptr)
        return;
    ShowTreeController *st = m_doc->showTree();
    quint32 listId = ShowTree::invalidId();
    if (st->productionOfChaser(chaserId, &listId) == ShowTree::invalidId())
        return;                         // a plain chaser: no cue to describe
    const quint32 to = st->cueOfStep(listId, step, sub);
    if (to == ShowTree::invalidId())
        return;
    const quint32 from = fromStep < 0 ? ShowTree::invalidId() : st->cueOfStep(listId, fromStep, fromSub);
    const QString verb = command == ShowControl::Back ? tr("Back") : tr("GO");
    add(st->goLogLine(verb, from, to));
}
