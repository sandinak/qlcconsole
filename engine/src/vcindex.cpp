/*
  Q Light Controller Plus - qlcconsole
  vcindex.cpp

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
#include <QCoreApplication>
#include <QFile>
#include <QHash>

#include "vcindex.h"

static const quint32 kInvalid = 0xFFFFFFFF;

static const QString tagCaption()   { return QStringLiteral("Caption"); }

QList<quint32> VcWidget::allFunctions() const
{
    QList<quint32> out;
    if (function != kInvalid)
        out << function;
    if (chaser != kInvalid)
        out << chaser;
    for (quint32 f : functions)
        if (f != kInvalid)
            out << f;
    return out;
}

QString VcWidget::label() const
{
    return caption.isEmpty() ? type : caption;
}

bool VcWidget::operator==(const VcWidget &o) const
{
    return parent == o.parent && id == o.id && type == o.type && caption == o.caption
           && page == o.page && multipage == o.multipage && action == o.action
           && mode == o.mode && function == o.function && hits == o.hits
           && chaser == o.chaser && showSubGo == o.showSubGo && follows == o.follows && follow == o.follow
           && functions == o.functions && levelChannels == o.levelChannels
           && group == o.group && bindings == o.bindings;
}

/****************************************************************************
 * Reading
 ****************************************************************************/

namespace
{
struct State
{
    VcIndex *index = nullptr;
    int serial = 0;
    /** The widget being read, by index in index->widgets. */
    int widget = -1;
    /** Per open element under the widget: its serial (the widget itself 0). */
    QList<int> serials;
    QStringList path;
    QHash<int, int> bindingOf;      //!< element serial -> index in the widget's bindings
};

quint32 toId(const QString &s)
{
    bool ok = false;
    const quint32 v = s.trimmed().toUInt(&ok);
    return ok ? v : kInvalid;
}

VcBinding &bindingHere(State &st)
{
    VcWidget &w = st.index->widgets[st.widget];
    const int serial = st.serials.isEmpty() ? 0 : st.serials.last();
    auto it = st.bindingOf.constFind(serial);
    if (it != st.bindingOf.constEnd())
        return w.bindings[it.value()];
    VcBinding b;
    b.role = st.path.isEmpty() ? QString() : st.path.last();
    w.bindings << b;
    st.bindingOf.insert(serial, w.bindings.count() - 1);
    return w.bindings.last();
}

void readWidget(QXmlStreamReader &r, VcIndex &ix, int parent);

bool isFrame(const QString &type)
{
    return type == QLatin1String("Frame") || type == QLatin1String("SoloFrame");
}

void readChildren(QXmlStreamReader &r, State &st)
{
    while (r.readNextStartElement())
    {
        const QString name = r.name().toString();
        const QXmlStreamAttributes a = r.attributes();
        const QString type = st.index->widgets.at(st.widget).type;
        if (st.path.isEmpty() && isFrame(type) && a.hasAttribute(tagCaption()))
        {
            readWidget(r, *st.index, st.widget);
            continue;
        }
        if (name == QLatin1String("Key"))
        {
            const QString key = r.readElementText();
            bindingHere(st).key = key;
            continue;
        }
        if (name == QLatin1String("Input"))
        {
            VcBinding &b = bindingHere(st);
            b.hasInput = true;
            b.universe = a.value(QStringLiteral("Universe")).toUInt();
            b.channel = a.value(QStringLiteral("Channel")).toUInt();
            r.skipCurrentElement();
            continue;
        }

        const QString where = (st.path + QStringList(name)).join(QLatin1Char('/'));
        // Needs an index each time: the list may grow under a nested frame.
        auto w = [&st]() -> VcWidget & { return st.index->widgets[st.widget]; };
        if (type == QLatin1String("Button") && where == QLatin1String("Function"))
        {
            w().function = toId(a.value(QStringLiteral("ID")).toString());
            if (a.hasAttribute(QStringLiteral("Hit")))
            {
                const quint32 h = toId(a.value(QStringLiteral("Hit")).toString());
                if (h != kInvalid)
                    w().hits << h;
            }
        }
        else if (type == QLatin1String("Button") && where == QLatin1String("Function/Hit"))
        {
            const quint32 h = toId(a.value(QStringLiteral("ID")).toString());
            if (h != kInvalid)
                w().hits << h;
        }
        else if (type == QLatin1String("Button") && where == QLatin1String("Action"))
        {
            w().action = r.readElementText();
            continue;
        }
        else if (type == QLatin1String("Matrix") && where == QLatin1String("Function"))
            w().function = toId(a.value(QStringLiteral("ID")).toString());
        else if (type == QLatin1String("Slider") && where == QLatin1String("SliderMode"))
        {
            w().mode = r.readElementText();
            continue;
        }
        else if (type == QLatin1String("Slider") && where == QLatin1String("Playback/Function"))
        {
            w().function = toId(r.readElementText());
            continue;
        }
        else if (type == QLatin1String("Slider") && where == QLatin1String("Level/Channel"))
        {
            const quint32 fx = toId(a.value(QStringLiteral("Fixture")).toString());
            const quint32 ch = r.readElementText().trimmed().toUInt();
            w().levelChannels << qMakePair(fx, ch);
            continue;
        }
        else if (type == QLatin1String("Slider") && where == QLatin1String("GroupMaster"))
            w().group = toId(a.value(QStringLiteral("Group")).toString());
        else if (type == QLatin1String("CueList") && (where == QLatin1String("Chaser") || where == QLatin1String("Function")))
        {
            const quint32 id = toId(r.readElementText());
            if (where == QLatin1String("Chaser") || w().chaser == kInvalid)
                w().chaser = id;
            continue;
        }
        else if (type == QLatin1String("SpeedDial") && where == QLatin1String("Function"))
        {
            const quint32 id = toId(r.readElementText());
            if (id != kInvalid)
                w().functions << id;
            continue;
        }
        else if (type == QLatin1String("Clock") && where == QLatin1String("Schedule"))
        {
            const quint32 id = toId(a.value(QStringLiteral("Function")).toString());
            if (id != kInvalid)
                w().functions << id;
        }
        else if (isFrame(type) && where == QLatin1String("Multipage"))
            w().multipage = true;

        st.path << name;
        st.serials << ++st.serial;
        readChildren(r, st);
        st.path.removeLast();
        st.serials.removeLast();
    }
}

void readWidget(QXmlStreamReader &r, VcIndex &ix, int parent)
{
    VcWidget w;
    w.parent = parent;
    w.type = r.name().toString();
    const QXmlStreamAttributes a = r.attributes();
    w.caption = a.value(tagCaption()).toString();
    w.id = toId(a.value(QStringLiteral("ID")).toString());
    w.page = a.value(QStringLiteral("Page")).toInt();
    if (w.type == QLatin1String("ShowControl"))
        w.function = toId(a.value(QStringLiteral("Function")).toString());
    if (w.type == QLatin1String("CueRunner") && a.hasAttribute(QStringLiteral("SubGo")))
        w.showSubGo = a.value(QStringLiteral("SubGo")).toInt() != 0;
    if (w.type == QLatin1String("CueRunner") && a.hasAttribute(QStringLiteral("Follow")))
    {
        w.follow = toId(a.value(QStringLiteral("Follow")).toString());
        w.follows = w.follow != kInvalid;
    }
    ix.widgets << w;
    State st;
    st.index = &ix;
    st.widget = ix.widgets.count() - 1;
    readChildren(r, st);
    VcWidget &done = ix.widgets[st.widget];
    // Only a Playback slider runs its function.
    if (done.type == QLatin1String("Slider") && done.mode != QLatin1String("Playback"))
        done.function = kInvalid;
}
}

VcIndex VcIndex::read(QXmlStreamReader &r)
{
    VcIndex ix;
    ix.available = true;
    while (r.readNextStartElement())
    {
        if (r.attributes().hasAttribute(tagCaption()))
            readWidget(r, ix, -1);
        else if (r.name() == QLatin1String("Properties"))
        {
            // The Grand Master and the like: only their inputs matter.
            VcWidget w;
            w.type = QStringLiteral("Properties");
            w.id = propertiesId;
            w.caption = QCoreApplication::translate("VcIndex", "Virtual Console");
            ix.widgets << w;
            State st;
            st.index = &ix;
            st.widget = ix.widgets.count() - 1;
            readChildren(r, st);
            if (ix.widgets.last().bindings.isEmpty())
                ix.widgets.removeLast();
        }
        else
            r.skipCurrentElement();
    }
    return ix;
}

VcIndex VcIndex::fromXml(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd())
    {
        if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("VirtualConsole"))
            return read(r);
    }
    VcIndex none;
    none.available = true;          // the file says nothing: an empty Virtual Console
    return none;
}

VcIndex VcIndex::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return VcIndex();
    QXmlStreamReader r(&file);
    while (!r.atEnd())
    {
        if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("VirtualConsole"))
            return read(r);
    }
    VcIndex none;
    none.available = true;
    return none;
}

/****************************************************************************
 * Questions
 ****************************************************************************/

int VcIndex::indexOf(quint32 id) const
{
    if (id == kInvalid)
        return -1;
    for (int i = 0; i < widgets.count(); ++i)
        if (widgets.at(i).id == id)
            return i;
    return -1;
}

const VcWidget *VcIndex::widget(quint32 id) const
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &widgets.at(i);
}

QSet<quint32> VcIndex::functionsUsed() const
{
    QSet<quint32> out;
    for (const VcWidget &w : widgets)
        for (quint32 f : w.allFunctions())
            out.insert(f);
    return out;
}

QSet<quint32> VcIndex::hitsFired() const
{
    QSet<quint32> out;
    for (const VcWidget &w : widgets)
        for (quint32 h : w.hits)
            out.insert(h);
    return out;
}

bool VcIndex::canBeLiveTogether(int a, int b) const
{
    if (a < 0 || b < 0 || a >= widgets.count() || b >= widgets.count())
        return true;
    // The page each widget sits on, in every multi-page frame above it.
    auto pagesOf = [this](int i) {
        QHash<int, int> pages;
        int cur = i;
        while (cur >= 0)
        {
            const int p = widgets.at(cur).parent;
            if (p >= 0 && widgets.at(p).multipage)
                pages.insert(p, widgets.at(cur).page);
            cur = p;
        }
        return pages;
    };
    const QHash<int, int> pa = pagesOf(a), pb = pagesOf(b);
    for (auto it = pa.constBegin(); it != pa.constEnd(); ++it)
    {
        auto other = pb.constFind(it.key());
        if (other != pb.constEnd() && other.value() != it.value())
            return false;
    }
    return true;
}

QString VcIndex::place(int index) const
{
    for (int p = index >= 0 && index < widgets.count() ? widgets.at(index).parent : -1; p >= 0;
         p = widgets.at(p).parent)
    {
        if (!widgets.at(p).caption.isEmpty())
            return QCoreApplication::translate("VcIndex", "in '%1'").arg(widgets.at(p).caption);
    }
    return QCoreApplication::translate("VcIndex", "on the Virtual Console");
}
