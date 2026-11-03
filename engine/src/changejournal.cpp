/*
  Q Light Controller Plus
  changejournal.cpp

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

#include <QDateTime>
#include <algorithm>

#include "changejournal.h"
#include "showtreecontroller.h"
#include "paletteops.h"
#include "doc.h"

static const int kMaxAreas = 2000;

ChangeJournal::ChangeJournal(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

void ChangeJournal::markSaved()
{
    m_savedStamp = PaletteOps::nextUndoStamp();
    m_meta.clear();
    m_areas.clear();
    m_areasDropped = 0;
    m_explained = m_scanMods = m_mods.loadRelaxed();
}

void ChangeJournal::noteModified()
{
    m_mods.fetchAndAddRelaxed(1);
    m_lastMod.storeRelaxed(QDateTime::currentMSecsSinceEpoch());
}

void ChangeJournal::noteArea(Kind kind, const QString &what, Jump jump, quint32 id)
{
    if (m_areas.count() >= kMaxAreas)
    {
        ++m_areasDropped;
        return;
    }
    Area a;
    a.kind = kind;
    a.what = what;
    a.subject.jump = jump;
    a.subject.id = id;
    a.when = QDateTime::currentMSecsSinceEpoch();
    m_areas << a;
    // The modification that follows is this one, not an unknown one.
    ++m_explained;
    ++m_scanMods;
}

QList<ChangeJournal::Step> ChangeJournal::currentSteps() const
{
    QList<Step> out;
    if (ShowTreeController *tree = m_doc->showTree())
        for (const ShowTreeController::StepInfo &s : tree->undoSteps())
            if (s.stamp > m_savedStamp)
                out << Step { s.stamp, s.label, Subject(), true };
    if (PaletteOps *ops = m_doc->paletteOps())
        for (const PaletteOps::StepInfo &s : ops->undoSteps())
            if (s.stamp > m_savedStamp)
            {
                Subject sub;
                if (s.paletteId != 0xFFFFFFFF)
                {
                    sub.jump = ToPalette;
                    sub.id = s.paletteId;
                    if (const QLCPalette *p = m_doc->palette(s.paletteId))
                        sub.text = p->name();
                }
                else if (s.functionId != 0xFFFFFFFF)
                {
                    sub.jump = ToFunction;
                    sub.id = s.functionId;
                    if (const Function *f = m_doc->function(s.functionId))
                        sub.text = f->name();
                }
                out << Step { s.stamp, s.label, sub, false };
            }
    std::sort(out.begin(), out.end(), [](const Step &a, const Step &b) { return a.stamp > b.stamp; });
    return out;
}

/* Learn when each new step was taken, and what it was about, the first time
   it is seen: the stacks' changed() signals call this while the edit is
   fresh (the view's selection is still the thing edited). */
void ChangeJournal::scan()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const Step &s : currentSteps())
    {
        if (m_meta.contains(s.stamp))
            continue;
        Meta m;
        m.when = now;
        m.subject = s.subject;
        m.kind = kindOf(s.label, s.tree);
        if (s.tree && m_hint)
            m.subject = m_hint();
        m_meta.insert(s.stamp, m);
    }
    if (m_meta.count() > 2000)
    {
        QHash<quint64, Meta> keep;
        for (const Step &s : currentSteps())
            keep.insert(s.stamp, m_meta.value(s.stamp));
        m_meta = keep;
    }
}

/* A stack changed (a step taken, undone or redone): the modification that
   came with it, within a moment, is its own and not an unknown one. */
void ChangeJournal::stepsChanged()
{
    scan();
    const int mods = m_mods.loadRelaxed();
    if (mods != m_scanMods && QDateTime::currentMSecsSinceEpoch() - m_lastMod.loadRelaxed() < 200)
        m_explained += mods - m_scanMods;
    m_scanMods = mods;
}

ChangeJournal::Kind ChangeJournal::kindOf(const QString &label, bool tree)
{
    if (!tree)
        return Look;
    const QString l = label.toLower();
    if (l.contains(QLatin1String("to layer")) || l.contains(QLatin1String("from layer"))
        || l.contains(QLatin1String("target")) || l.contains(QLatin1String("a set from")))
        return Target;
    if (l.contains(QLatin1String("layer")))
        return Layer;
    if (l.startsWith(QLatin1String("set ")) || l.contains(QLatin1String("look")))
        return Look;
    if (l.contains(QLatin1String("numbering")) || l.contains(QLatin1String("timecode source"))
        || l.contains(QLatin1String("main list")) || l.contains(QLatin1String("production"))
        || l.contains(QLatin1String("running order")) || l.contains(QLatin1String("folder")))
        return Settings;
    return Cue;
}

QString ChangeJournal::actionOf(const QString &label)
{
    if (label.startsWith(QLatin1String("Set cue ")))
        return label.mid(8);
    if (label.startsWith(QLatin1String("Set ")))
        return label.mid(4);
    return label;
}

QString ChangeJournal::kindName(Kind kind)
{
    switch (kind)
    {
    case Cue: return tr("Cue");
    case Layer: return tr("Layer");
    case Look: return tr("Look");
    case Target: return tr("Target");
    case Fixture: return tr("Fixture");
    case Settings: return tr("Settings");
    default: return tr("Other");
    }
}

int ChangeJournal::editCount()
{
    scan();
    return currentSteps().count() + m_areas.count() + m_areasDropped;
}

int ChangeJournal::stepCount()
{
    return currentSteps().count();
}

int ChangeJournal::otherCount() const
{
    return qMax(0, m_mods.loadRelaxed() - m_explained);
}

QList<ChangeJournal::Entry> ChangeJournal::entries()
{
    scan();
    QList<Entry> out;
    QHash<QString, int> at;

    auto add = [&](Kind kind, const QString &title, const QString &action, qint64 when, const Subject &sub) {
        const QString key = QString::number(int(kind)) + QLatin1Char('|')
                + (sub.jump != NoJump ? QString::number(sub.id) + QLatin1Char('/') + QString::number(sub.id2) : title);
        const auto found = at.constFind(key);
        if (found == at.constEnd())
        {
            Entry e;
            e.kind = kind;
            e.title = title;
            e.what = action;
            e.edits = 1;
            e.when = when;
            e.subject = sub;
            at.insert(key, out.count());
            out << e;
            return;
        }
        Entry &e = out[*found];
        ++e.edits;
        e.when = qMax(e.when, when);
        if (!action.isEmpty() && !e.what.split(QStringLiteral(", ")).contains(action))
            e.what += (e.what.isEmpty() ? QString() : QStringLiteral(", ")) + action;
    };

    const QList<Step> steps = currentSteps();
    for (const Step &s : steps)
    {
        const Meta m = m_meta.value(s.stamp);
        const Subject sub = m.subject.jump != NoJump || !m.subject.text.isEmpty() ? m.subject : s.subject;
        const QString action = actionOf(s.label);
        // A subject-less step is its own line, named by what was done.
        add(kindOf(s.label, s.tree), sub.text.isEmpty() ? s.label : sub.text,
            sub.text.isEmpty() ? QString() : action, m.when, sub);
    }
    for (const Area &a : m_areas)
        add(a.kind, a.what, QString(), a.when, a.subject);

    std::stable_sort(out.begin(), out.end(), [](const Entry &a, const Entry &b) { return a.when > b.when; });

    if (const int n = otherCount())
    {
        Entry e;
        e.kind = Other;
        e.title = tr("Other changes (settings, programmer values, patch details…)");
        e.edits = n;
        e.itemised = false;
        e.when = m_lastMod.loadRelaxed();
        out << e;
    }
    return out;
}

QString ChangeJournal::summary()
{
    const int n = editCount();
    const int other = otherCount();
    if (other > 0)
        return n > 0 ? (n == 1 ? tr("1 change listed; some may be listed by area only")
                               : tr("%1 changes listed; some may be listed by area only").arg(n))
                     : tr("Changes are listed by area only");
    return n == 1 ? tr("1 change") : tr("%1 changes").arg(n);
}

QString ChangeJournal::tooltip()
{
    const int n = editCount() + (otherCount() > 0 ? 1 : 0);
    return n == 1 ? tr("1 unsaved change, click to see it")
                  : n > 1 ? tr("%1 unsaved changes, click to see them").arg(n)
                          : tr("Unsaved changes, click to see them");
}
