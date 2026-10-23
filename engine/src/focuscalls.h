/*
  Q Light Controller Plus
  focuscalls.h

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

#ifndef FOCUSCALLS_H
#define FOCUSCALLS_H

#include <QObject>
#include <QString>
#include <QList>

#include "qlcengine_export.h"
#include "focussession.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCFocusCalls QStringLiteral("FocusCalls")

/**
 * A show's focus calls (FOCUS_REFINE_DESIGN.md section 4): the order the rig
 * is focused in when the show moves into a new venue, saved with the show.
 * Each call is some fixture groups (and loose fixtures) and the point they
 * focus to -- "DH Tspots -> DSC", then "AT spots -> Soloist DS". Run, they
 * become a FocusSession that steps call by call (the triggers) and fixture
 * by fixture within a call (the bumpers), saving to the venue applied.
 */
struct QLC_ENGINE_EXPORT FocusCall
{
    QString name;                   ///< empty: named from the groups and target
    QList<quint32> groups;          ///< FixtureGroup ids
    QList<quint32> fixtures;        ///< loose fixtures, besides the groups'
    FocusSession::Reference target; ///< where they focus to
    QString note;                   ///< a word for the crew ("check the shutter cut")

    bool operator==(const FocusCall &o) const
    {
        return name == o.name && groups == o.groups && fixtures == o.fixtures
            && target.kind == o.target.kind && target.id == o.target.id && note == o.note;
    }
};

class QLC_ENGINE_EXPORT FocusCalls final : public QObject
{
    Q_OBJECT

public:
    explicit FocusCalls(Doc *doc, QObject *parent = nullptr);

    QList<FocusCall> calls() const { return m_calls; }
    int count() const { return m_calls.size(); }
    FocusCall call(int index) const { return m_calls.value(index); }

    void setCalls(const QList<FocusCall> &calls);
    int append(const FocusCall &call);
    bool replace(int index, const FocusCall &call);
    bool remove(int index);
    /** Move call @p from to @p to (the order they are run in). */
    bool move(int from, int to);
    void clear();

    /** The fixtures @p call focuses: its groups' members in group order,
     *  then its loose fixtures, each once, those still in the show. */
    static QList<quint32> fixturesOf(const Doc *doc, const FocusCall &call);
    /** What the call is called: its name, else "<groups> -> <target>". */
    static QString displayName(const Doc *doc, const FocusCall &call);
    /** Who the call focuses, by name ("DH Tspots", "AT spots, Spot 9"). */
    static QString whoName(const Doc *doc, const FocusCall &call);
    /** The target alone, by name ("DSC", "Aim of 01-02.03-Ballad"). */
    static QString targetName(const Doc *doc, const FocusSession::Reference &target);

    /** The calls as Focus mode runs them. */
    QList<FocusSession::Call> sessionCalls() const;

    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

signals:
    void changed();

private:
    void touched();

private:
    Doc *m_doc;
    QList<FocusCall> m_calls;
};

/** @} */

#endif
