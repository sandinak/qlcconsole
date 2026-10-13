/*
  Q Light Controller Plus
  lookstackcheck.cpp

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

#include <QSet>

#include "lookstackcheck.h"
#include "effectinstance.h"
#include "qlcfixturemode.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
/* What a static look sets: the attributes the "lower wins" rule compares. */
QSet<QString> attributesOf(QLCPalette::PaletteType type)
{
    switch (type)
    {
        case QLCPalette::Dimmer:  return { QStringLiteral("dimmer") };
        case QLCPalette::Pan:     return { QStringLiteral("pan") };
        case QLCPalette::Tilt:    return { QStringLiteral("tilt") };
        case QLCPalette::PanTilt:
        case QLCPalette::Aim:     return { QStringLiteral("pan"), QStringLiteral("tilt") };
        case QLCPalette::Gobo:    return { QStringLiteral("gobo") };
        case QLCPalette::Shutter:
        case QLCPalette::Strobe:  return { QStringLiteral("shutter") };
        case QLCPalette::Zoom:    return { QStringLiteral("zoom") };
        case QLCPalette::Beam:    return { QStringLiteral("beam") };
        default:                  return {};
    }
}

QString attributeWords(const QSet<QString> &a)
{
    if (a.contains(QStringLiteral("pan")) && a.contains(QStringLiteral("tilt")))
        return QObject::tr("position");
    QStringList w = a.values();
    std::sort(w.begin(), w.end());
    return w.join(QStringLiteral(", "));
}

/* What an effect drives, for "the lower wins": a colour, a dimmer (which
   multiplies, so never covers), a position, or the beam (gobo, iris, zoom,
   shutter -- different channels, so two beam effects can well work together;
   not compared). */
QString kindOf(const QStringList &ft)
{
    if (ft.contains(QStringLiteral("dimmer")))
        return QStringLiteral("Dimmer");
    if (ft.contains(QStringLiteral("beam")) || ft.contains(QStringLiteral("spot"))
        || ft.contains(QStringLiteral("shutter")))
        return QStringLiteral("Beam");
    if (ft.contains(QStringLiteral("rgb")) || ft.contains(QStringLiteral("rgbw"))
        || ft.contains(QStringLiteral("color")) || ft.contains(QStringLiteral("colorwheel")))
        return QStringLiteral("Color");
    if (ft.contains(QStringLiteral("moving")) || ft.contains(QStringLiteral("mover"))
        || ft.contains(QStringLiteral("pantilt")))
        return QStringLiteral("Position");
    return QStringLiteral("Other");
}

QString nameOf(QLCPalette *p)
{
    return p->name().isEmpty() ? QLCPalette::typeToString(p->type()) : p->name();
}
}

QList<LookStackCheck::Issue> LookStackCheck::check(Doc *doc, const Scene *scene)
{
    QList<Issue> out;
    if (doc == nullptr || scene == nullptr)
        return out;
    const QList<quint32> pals = scene->palettes();
    if (pals.isEmpty())
        return out;

    // The look's fixtures, through its groups too.
    QSet<quint32> targets;
    foreach (quint32 fid, scene->fixtures())
        targets.insert(fid);
    foreach (quint32 gid, scene->fixtureGroups())
    {
        if (FixtureGroup *g = doc->fixtureGroup(gid))
        {
            foreach (quint32 fid, g->fixtureList())
                targets.insert(fid);
        }
    }
    if (targets.isEmpty())
    {
        out << Issue{ QLCPalette::invalidId(), Issue::Warning,
                      QObject::tr("No targets: nothing in this look lights anything until "
                                  "fixtures or groups are added to it.") };
        return out;
    }

    // What each effect is and drives, from the engine's own reading of it.
    struct Fx { QLCPalette *pal; int index; QString kind; QSet<quint32> heads; bool takesColours; };
    QList<Fx> effects;
    bool anyPosition = false;
    for (int i = 0; i < pals.size(); ++i)
    {
        QLCPalette *p = doc->palette(pals.at(i));
        if (p == nullptr)
            continue;
        if (p->type() == QLCPalette::Aim || p->type() == QLCPalette::PanTilt
            || p->type() == QLCPalette::Pan || p->type() == QLCPalette::Tilt)
            anyPosition = true;
        if (p->type() != QLCPalette::Effect)
            continue;
        Fx fx{ p, i, QString(), {}, QLCPalette::effectUsesLookColours(p) };
        if (!p->wledEffect().isEmpty() || p->scriptPath().isEmpty())
            fx.kind = QStringLiteral("Color");
        else
        {
            EffectInstance inst(doc, scene->id(), p->id());
            if (inst.isValid())
            {
                fx.kind = kindOf(inst.scriptFixtureTypes());
                foreach (quint32 fid, inst.effectiveFixtureIds())
                    fx.heads.insert(fid);
                if (fx.heads.isEmpty())
                    out << Issue{ p->id(), Issue::Warning,
                                  QObject::tr("Does nothing: none of this look's fixtures is the "
                                              "kind it is for (%1).")
                                      .arg(inst.scriptFixtureTypes().join(QStringLiteral(", "))) };
            }
        }
        effects << fx;
    }

    // 1. A static look the ones below it always cover (lower wins).
    for (int i = 0; i < pals.size(); ++i)
    {
        QLCPalette *p = doc->palette(pals.at(i));
        if (p == nullptr || QLCPalette::isEffectScoped(doc, pals, p->id()))
            continue;
        const QSet<QString> mine = attributesOf(p->type());
        if (mine.isEmpty())
            continue;
        QSet<QString> below;
        QStringList who;
        for (int j = i + 1; j < pals.size(); ++j)
        {
            QLCPalette *q = doc->palette(pals.at(j));
            if (q == nullptr || QLCPalette::isEffectScoped(doc, pals, q->id()))
                continue;
            const QSet<QString> theirs = attributesOf(q->type()) & mine;
            if (!theirs.isEmpty())
            {
                below += theirs;
                who << nameOf(q);
            }
        }
        if (below.contains(mine))
            out << Issue{ p->id(), Issue::Warning,
                          QObject::tr("Never shows: %1 below it sets the same %2, and the lower "
                                      "one wins. Move it below, or remove one.")
                              .arg(who.join(QStringLiteral(", ")), attributeWords(mine)) };
    }

    // 2. An effect the one below it covers: the same thing, on the same heads.
    for (int a = 0; a < effects.size(); ++a)
    {
        const Fx &up = effects.at(a);
        if (up.kind != QLatin1String("Color") && up.kind != QLatin1String("Position"))
            continue;                               // dimmers multiply; beam/other: can't tell
        for (int b = a + 1; b < effects.size(); ++b)
        {
            const Fx &down = effects.at(b);
            if (down.kind != up.kind)
                continue;
            const QSet<quint32> shared = up.heads & down.heads;
            if (shared.isEmpty())
                continue;
            const QString what = up.kind == QLatin1String("Position") ? QObject::tr("position")
                                                                      : QObject::tr("colour");
            out << Issue{ up.pal->id(), Issue::Warning,
                          shared.size() == up.heads.size()
                              ? QObject::tr("Covered: %1 below it also sets the %2, on all its "
                                            "heads, and the lower one wins.").arg(nameOf(down.pal), what)
                              : QObject::tr("Covered on %1 of its %2 heads: %3 below it also "
                                            "sets the %4 there, and the lower one wins.")
                                    .arg(shared.size()).arg(up.heads.size())
                                    .arg(nameOf(down.pal), what) };
            break;
        }
    }

    // 3. Colours and dimmers under an effect: whose they are, and who misses out.
    bool baseDimmer = false, baseColour = false;
    foreach (quint32 pid, pals)
    {
        QLCPalette *p = doc->palette(pid);
        if (p == nullptr || QLCPalette::isEffectScoped(doc, pals, pid))
            continue;
        baseDimmer = baseDimmer || p->type() == QLCPalette::Dimmer;
        baseColour = baseColour || p->type() == QLCPalette::Color;
    }
    for (const Fx &fx : effects)
    {
        QList<QLCPalette *> own;
        for (int j = fx.index + 1; j < pals.size(); ++j)
        {
            QLCPalette *q = doc->palette(pals.at(j));
            if (q == nullptr || q->type() == QLCPalette::Effect)
                break;
            if (q->type() == QLCPalette::Color || q->type() == QLCPalette::Dimmer)
                own << q;
        }
        const int missing = targets.size() - fx.heads.size();
        for (QLCPalette *q : own)
        {
            if (q->type() == QLCPalette::Dimmer && !baseDimmer && !fx.takesColours
                && !fx.heads.isEmpty() && missing > 0)
                out << Issue{ q->id(), Issue::Warning,
                              QObject::tr("%1 belongs to %2: only the %n head(s) it drives get it. "
                                          "The look's other %3 have no dimmer -- add one above the "
                                          "effects for them.", "", fx.heads.size())
                                  .arg(nameOf(q), nameOf(fx.pal)).arg(missing) };
        }
        // The look's colour replaced by this effect's own on its heads.
        bool ownColour = false;
        for (QLCPalette *q : own)
            ownColour = ownColour || q->type() == QLCPalette::Color;
        if (fx.kind == QLatin1String("Color") && ownColour && baseColour)
        {
            foreach (quint32 pid, pals)
            {
                QLCPalette *p = doc->palette(pid);
                if (p == nullptr || p->type() != QLCPalette::Color
                    || QLCPalette::isEffectScoped(doc, pals, pid))
                    continue;
                out << Issue{ p->id(), Issue::Note,
                              QObject::tr("Replaced by %1's own colour on the heads it lights: "
                                          "this colour shows only where it leaves a head dark.")
                                  .arg(nameOf(fx.pal)) };
            }
        }
    }

    // 4. A movement effect with nothing to move round.
    if (!anyPosition)
    {
        for (const Fx &fx : effects)
        {
            if (fx.kind == QLatin1String("Position") && !fx.heads.isEmpty())
                out << Issue{ fx.pal->id(), Issue::Note,
                              QObject::tr("No Aim or Pan/Tilt in the look: it moves round the "
                                          "middle of each head's range (straight down, on a hung "
                                          "mover). Add one to place it.") };
        }
    }

    // 5. A second colour nothing can show.
    bool colourUser = false;
    for (const Fx &fx : effects)
        colourUser = colourUser || fx.takesColours;
    int maxSets = 1;
    foreach (quint32 fid, targets)
    {
        if (Fixture *f = doc->fixture(fid))
        {
            for (int h = 0; h < f->heads(); ++h)
                maxSets = qMax(maxSets, int(f->rgbChannelSets(h).size()));
        }
    }
    int colourNo = 0;
    foreach (quint32 pid, pals)
    {
        QLCPalette *p = doc->palette(pid);
        if (p == nullptr || p->type() != QLCPalette::Color || QLCPalette::isEffectScoped(doc, pals, pid))
            continue;
        ++colourNo;
        if (colourNo > maxSets && !colourUser)
            out << Issue{ p->id(), Issue::Note,
                          QObject::tr("Colour %1 of the look: none of its fixtures has a colour "
                                      "set %1, and no effect here uses it, so it shows nowhere.")
                              .arg(colourNo) };
    }
    return out;
}
