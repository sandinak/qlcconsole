/*
  Q Light Controller Plus - qlcconsole
  showstatus.cpp

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

#include <algorithm>

#include "showstatus.h"

ShowStatus::ShowStatus()
    : QObject(nullptr)
{
}

ShowStatus *ShowStatus::instance()
{
    static ShowStatus *inst = new ShowStatus();
    return inst;
}

void ShowStatus::setStatus(const QString &key, Severity severity, const QString &summary,
                           const QString &detail, const QStringList &items)
{
    Entry e;
    e.key = key;
    e.severity = severity;
    e.summary = summary;
    e.detail = detail;
    e.items = items;

    const QMap<QString, Entry>::const_iterator existing = m_entries.constFind(key);
    const bool isChange = (existing == m_entries.constEnd())
        || existing.value().severity != severity
        || existing.value().summary != summary
        || existing.value().detail != detail
        || existing.value().items != items;

    m_entries.insert(key, e);
    if (isChange)
        emit changed();
}

void ShowStatus::clearStatus(const QString &key)
{
    if (m_entries.remove(key) > 0)
        emit changed();
}

ShowStatus::Entry ShowStatus::worst() const
{
    Entry best;
    best.severity = Ok;

    for (QMap<QString, Entry>::const_iterator it = m_entries.constBegin();
         it != m_entries.constEnd(); ++it)
    {
        if (it.value().severity > best.severity)
            best = it.value();
    }

    return best;
}

QList<ShowStatus::Entry> ShowStatus::allEntries() const
{
    QList<Entry> list = m_entries.values();
    std::sort(list.begin(), list.end(), [](const Entry &a, const Entry &b) {
        return a.severity > b.severity;
    });
    return list;
}
