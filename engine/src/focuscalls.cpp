/*
  Q Light Controller Plus
  focuscalls.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "monitorproperties.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "focuscalls.h"
#include "function.h"
#include "fixture.h"
#include "doc.h"

namespace
{
const QString kCall = QStringLiteral("Call");
const QString kGroup = QStringLiteral("Group");
const QString kFixture = QStringLiteral("Fixture");

QString kindKey(FocusSession::Reference::Kind kind)
{
    switch (kind)
    {
        case FocusSession::Reference::Target:   return QStringLiteral("Target");
        case FocusSession::Reference::SceneAim: return QStringLiteral("SceneAim");
        case FocusSession::Reference::None:     break;
    }
    return QString();
}

FocusSession::Reference::Kind kindOf(const QString &key)
{
    if (key == QLatin1String("Target"))
        return FocusSession::Reference::Target;
    if (key == QLatin1String("SceneAim"))
        return FocusSession::Reference::SceneAim;
    return FocusSession::Reference::None;
}
}

FocusCalls::FocusCalls(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
}

void FocusCalls::touched()
{
    if (m_doc != nullptr)
        m_doc->setModified();
    emit changed();
}

void FocusCalls::setCalls(const QList<FocusCall> &calls)
{
    if (calls == m_calls)
        return;
    m_calls = calls;
    touched();
}

int FocusCalls::append(const FocusCall &call)
{
    m_calls << call;
    touched();
    return m_calls.size() - 1;
}

bool FocusCalls::replace(int index, const FocusCall &call)
{
    if (index < 0 || index >= m_calls.size())
        return false;
    if (m_calls.at(index) == call)
        return true;
    m_calls[index] = call;
    touched();
    return true;
}

bool FocusCalls::remove(int index)
{
    if (index < 0 || index >= m_calls.size())
        return false;
    m_calls.removeAt(index);
    touched();
    return true;
}

bool FocusCalls::move(int from, int to)
{
    if (from < 0 || from >= m_calls.size() || to < 0 || to >= m_calls.size())
        return false;
    if (from == to)
        return true;
    m_calls.move(from, to);
    touched();
    return true;
}

void FocusCalls::clear()
{
    if (m_calls.isEmpty())
        return;
    m_calls.clear();
    emit changed();
}

QList<quint32> FocusCalls::fixturesOf(const Doc *doc, const FocusCall &call)
{
    QList<quint32> out;
    if (doc == nullptr)
        return out;
    for (quint32 gid : call.groups)
        if (const FixtureGroup *grp = doc->fixtureGroup(gid))
            for (quint32 fid : grp->fixtureList())
                if (!out.contains(fid) && doc->fixture(fid) != nullptr)
                    out << fid;
    for (quint32 fid : call.fixtures)
        if (!out.contains(fid) && doc->fixture(fid) != nullptr)
            out << fid;
    return out;
}

QString FocusCalls::targetName(const Doc *doc, const FocusSession::Reference &target)
{
    if (doc == nullptr)
        return QString();
    MonitorProperties *props = const_cast<Doc *>(doc)->monitorProperties();
    if (target.kind == FocusSession::Reference::Target && props != nullptr)
    {
        const StageTarget *t = props->stageTarget(target.id);
        return t != nullptr ? t->name() : tr("a missing target");
    }
    if (target.kind == FocusSession::Reference::SceneAim)
    {
        const Function *f = doc->function(target.id);
        return f != nullptr ? tr("Aim of %1").arg(f->name()) : tr("a missing scene");
    }
    return tr("where they land");
}

QString FocusCalls::whoName(const Doc *doc, const FocusCall &call)
{
    QStringList who;
    if (doc != nullptr)
    {
        for (quint32 gid : call.groups)
            if (const FixtureGroup *grp = doc->fixtureGroup(gid))
                who << grp->name();
        for (quint32 fid : call.fixtures)
            if (const Fixture *fx = doc->fixture(fid))
                who << fx->name();
    }
    return who.isEmpty() ? tr("Nothing") : who.join(QStringLiteral(", "));
}

QString FocusCalls::displayName(const Doc *doc, const FocusCall &call)
{
    if (!call.name.trimmed().isEmpty())
        return call.name.trimmed();
    return tr("%1 \342\206\222 %2").arg(whoName(doc, call), targetName(doc, call.target));
}

QList<FocusSession::Call> FocusCalls::sessionCalls() const
{
    QList<FocusSession::Call> out;
    for (const FocusCall &c : m_calls)
    {
        FocusSession::Call sc;
        sc.name = displayName(m_doc, c);
        sc.who = whoName(m_doc, c);
        sc.targetName = targetName(m_doc, c.target);
        sc.fixtures = fixturesOf(m_doc, c);
        sc.target = c.target;
        sc.note = c.note;
        out << sc;
    }
    return out;
}

bool FocusCalls::saveXML(QXmlStreamWriter *doc) const
{
    // Nothing to say: nothing written, so a show without them is unchanged.
    if (m_calls.isEmpty())
        return true;
    doc->writeStartElement(KXMLQLCFocusCalls);
    for (const FocusCall &c : m_calls)
    {
        doc->writeStartElement(kCall);
        if (!c.name.isEmpty())
            doc->writeAttribute(QStringLiteral("Name"), c.name);
        if (c.target.isValid())
        {
            doc->writeAttribute(QStringLiteral("TargetKind"), kindKey(c.target.kind));
            doc->writeAttribute(QStringLiteral("TargetID"), QString::number(c.target.id));
        }
        if (!c.note.isEmpty())
            doc->writeAttribute(QStringLiteral("Note"), c.note);
        for (quint32 gid : c.groups)
        {
            doc->writeStartElement(kGroup);
            doc->writeAttribute(QStringLiteral("ID"), QString::number(gid));
            doc->writeEndElement();
        }
        for (quint32 fid : c.fixtures)
        {
            doc->writeStartElement(kFixture);
            doc->writeAttribute(QStringLiteral("ID"), QString::number(fid));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}

bool FocusCalls::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFocusCalls)
        return false;
    QList<FocusCall> calls;
    while (root.readNextStartElement())
    {
        if (root.name() != kCall)
        {
            root.skipCurrentElement();
            continue;
        }
        FocusCall c;
        const QXmlStreamAttributes a = root.attributes();
        c.name = a.value(QStringLiteral("Name")).toString();
        c.note = a.value(QStringLiteral("Note")).toString();
        c.target.kind = kindOf(a.value(QStringLiteral("TargetKind")).toString());
        if (c.target.isValid())
            c.target.id = a.value(QStringLiteral("TargetID")).toUInt();
        while (root.readNextStartElement())
        {
            const quint32 id = root.attributes().value(QStringLiteral("ID")).toUInt();
            if (root.name() == kGroup)
                c.groups << id;
            else if (root.name() == kFixture)
                c.fixtures << id;
            root.skipCurrentElement();
        }
        calls << c;
    }
    m_calls = calls;
    emit changed();
    return true;
}
