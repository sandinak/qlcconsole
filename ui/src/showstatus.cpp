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
                           const QString &detail, const QStringList &items,
                           const QVariantList &itemData, const QStringList &itemGroups)
{
    Entry e;
    e.key = key;
    e.severity = severity;
    e.summary = summary;
    e.detail = detail;
    e.items = items;
    e.itemData = itemData;
    e.itemGroups = itemGroups;

    const QMap<QString, Entry>::const_iterator existing = m_entries.constFind(key);
    const bool isChange = (existing == m_entries.constEnd())
        || existing.value().severity != severity
        || existing.value().summary != summary
        || existing.value().detail != detail
        || existing.value().items != items
        || existing.value().itemData != itemData
        || existing.value().itemGroups != itemGroups;

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

void ShowStatus::setAction(const QString &key, const QString &label, const Action &action)
{
    QList<QPair<QString, Action> > list;
    if (!label.isEmpty() && action)
        list << qMakePair(label, action);
    m_actions.insert(key, list);
}

void ShowStatus::addAction(const QString &key, const QString &label, const Action &action)
{
    if (!label.isEmpty() && action)
        m_actions[key] << qMakePair(label, action);
}

QString ShowStatus::actionLabel(const QString &key) const
{
    const QList<QPair<QString, Action> > list = m_actions.value(key);
    return list.isEmpty() ? QString() : list.first().first;
}

QStringList ShowStatus::actionLabels(const QString &key) const
{
    QStringList out;
    foreach (const auto &a, m_actions.value(key))
        out << a.first;
    return out;
}

bool ShowStatus::runAction(const QString &key, QWidget *parent, const QVariantList &items, int index) const
{
    const QList<QPair<QString, Action> > list = m_actions.value(key);
    if (index < 0 || index >= list.size() || !list.at(index).second)
        return false;
    list.at(index).second(parent, items);
    return true;
}
