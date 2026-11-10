/*
  Q Light Controller Plus
  effectrows.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QRegularExpression>
#include <QSet>
#include <cmath>

#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "effectpresetcache.h"
#include "measurementunits.h"
#include "effectdesigner.h"
#include "effectorigin.h"
#include "speedmasters.h"
#include "effectrows.h"
#include "effectruns.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "freerotation.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
QString number(double v)
{
    if (std::fabs(v - std::round(v)) < 1e-6)
        return QString::number(qint64(std::round(v)));
    QString s = QString::number(v, 'f', std::fabs(v) < 1.0 ? 3 : 2);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    return s;
}

bool same(double a, double b)
{
    return std::fabs(a - b) < 1e-4;
}

/* The attribute an effect of @a key drives, as the reach checks it. */
QString attributeOf(const QString &key)
{
    const QString kind = key.mid(KeyEffectPrefix.length());
    if (kind == QLatin1String("Position"))
        return KeyPosition;
    if (kind == QLatin1String("Color") || kind == QLatin1String("Matrix"))
        return KeyColour;
    if (kind == QLatin1String("Dimmer"))
        return KeyDimmer;
    if (kind == QLatin1String("Beam"))
        return KeyBeam;
    return QString();
}
}

QLCPalette *EffectRows::effectPalette(Doc *doc, const Look &look)
{
    if (doc == nullptr || look.none)
        return nullptr;
    for (quint32 pid : look.palettes)
        if (QLCPalette *p = doc->palette(pid))
            if (p->type() == QLCPalette::Effect)
                return p;
    return nullptr;
}

QString EffectRows::paramLabel(const EffectScript::ParamDef &def)
{
    return def.displayLabel();
}

QString EffectRows::valueText(const EffectScript::ParamDef &def, double value)
{
    if (!def.enumValues.isEmpty())
        return def.enumValues.value(qBound(0, int(std::lround(value)), int(def.enumValues.size()) - 1));
    if (def.unit == QLatin1String("cm"))  // units-ok: the script parameter's unit key; the value is shown through MeasurementUnits
        return MeasurementUnits::formatLength(value * 10.0, MeasurementUnits::preferredLength());
    if (def.unit == QLatin1String("cm/s"))  // units-ok: the script parameter's unit key; the value is shown through MeasurementUnits
        return MeasurementUnits::formatSpeed(value / 100.0);
    if (def.unit == QLatin1String("%"))
        return number(value) + QStringLiteral("%");
    if (def.unit == QString(QChar(0x00B0)))     // an angle
        return number(value) + QString(QChar(0x00B0));
    // A switch the script spells out: "Reverse direction (0=fwd, 1=rev)".
    static const QRegularExpression pair(QStringLiteral("(-?\\d+)\\s*=\\s*([A-Za-z][\\w-]*)"));
    if (same(value, std::round(value)))
    {
        QRegularExpressionMatchIterator it = pair.globalMatch(def.description);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            if (m.captured(1).toInt() == int(std::lround(value)))
                return m.captured(2);
        }
    }
    return number(value);
}

QString EffectRows::runsOnText(Doc *doc, const QString &key, const QList<Target> &targets,
                               const QStringList &fixtureTypes)
{
    ShowTreeController *st = doc == nullptr ? nullptr : doc->showTree();
    if (st == nullptr || targets.isEmpty())
        return QString();
    QStringList names;
    QList<quint32> fixtures, misses;
    const QString attr = attributeOf(key);
    // Free Spin runs on heads that turn endlessly, not on any pan/tilt.
    const bool spins = fixtureTypes.contains(QStringLiteral("freerotate"), Qt::CaseInsensitive);
    for (const Target &t : targets)
    {
        names << st->targetLabel(t);
        for (quint32 f : st->targetFixtures(t))
            if (!fixtures.contains(f))
                fixtures << f;
        if (spins)
        {
            for (quint32 f : st->targetFixtures(t))
                if (!FreeRotation::canSpin(doc->fixture(f)) && !misses.contains(f))
                    misses << f;
        }
        else if (!attr.isEmpty())
            for (quint32 f : st->lookMisses(t, attr))
                if (!misses.contains(f))
                    misses << f;
    }
    int heads = 0;
    for (quint32 f : fixtures)
        if (const Fixture *fxi = doc->fixture(f))
            heads += qMax(1, fxi->heads());
    const int total = int(fixtures.count());
    QString count = heads > total ? tr("%1 fixtures, %2 heads").arg(total).arg(heads)
                                  : total == 1 ? tr("1 fixture") : tr("%1 fixtures").arg(total);
    if (!misses.isEmpty())
    {
        Target missed;
        missed.fixtures = misses;
        const QString what = spins ? tr("no free rotation")
                             : attr == KeyPosition ? tr("no pan/tilt")
                             : attr == KeyColour ? tr("no colour")
                             : attr == KeyBeam ? tr("no beam") : tr("no %1").arg(keyLabel(attr).toLower());
        count = tr("%1 of %2 fixtures (%3 %4 %5)")
                .arg(total - misses.count()).arg(total)
                .arg(st->targetLabel(missed), misses.count() == 1 ? tr("has") : tr("have"), what);
    }
    return QStringLiteral("%1 · %2").arg(names.join(QStringLiteral(" + ")), count);
}

QList<EffectRows::Row> EffectRows::rows(Doc *doc, const ResolvedLayer &layer, const QString &key,
                                        const ResolvedValue &v, const QString &carried,
                                        const QList<Target> &span)
{
    QList<Row> out;
    ShowTreeController *st = doc == nullptr ? nullptr : doc->showTree();
    const QLCPalette *p = effectPalette(doc, v.look);
    if (st == nullptr || p == nullptr)
        return out;
    const bool own = v.origin == Origin::Own;
    auto withCarried = [&carried](const QString &from) {
        if (carried.isEmpty())
            return from;
        return from.isEmpty() ? carried : QStringLiteral("%1 · %2").arg(from, carried);
    };

    // What it runs on: the groups, and how many of them can run it.
    {
        Row r;
        r.id = QStringLiteral("runsOn");
        r.label = tr("Runs on");
        QList<Target> targets;
        if (span.count() > 1)
            targets = span;
        else
            targets << layer.target;
        r.text = runsOnText(doc, key, targets, EffectOrigin::scriptFixtureTypes(p->scriptPath()));
        QStringList contents;
        for (const Target &t : targets)
            contents << st->targetContents(t);
        r.tip = tr("The fixtures this effect drives here:\n%1").arg(contents.join(QLatin1Char('\n')));
        if (span.count() > 1)
            r.tip += QStringLiteral("\n") + tr("Set on the cue: one effect over these layers, in step.");
        out << r;
    }

    // What it moves.
    {
        Row r;
        r.id = QStringLiteral("moves");
        r.label = tr("Moves");
        const QString kind = key.mid(KeyEffectPrefix.length());
        r.text = kind == QLatin1String("Position") ? tr("Position (pan and tilt)")
                 : kind == QLatin1String("Color") ? tr("Colour")
                 : kind == QLatin1String("Matrix") ? tr("Colour, pixel by pixel")
                 : kind == QLatin1String("Dimmer") ? tr("Intensity")
                 : kind == QLatin1String("Beam") ? tr("Beam (zoom, focus, iris)")
                 : tr("What its script writes");
        r.tip = tr("What changes on stage while this effect runs. The layer's own %1 is what it moves over.")
                .arg(kind == QLatin1String("Position") ? tr("position")
                     : kind == QLatin1String("Dimmer") ? tr("intensity") : tr("look"));
        out << r;
    }

    // Its colours: its own, else the layer's Colour it plays in.
    const QString kind = key.mid(KeyEffectPrefix.length());
    if (!v.look.own.isEmpty() || kind == QLatin1String("Color") || kind == QLatin1String("Matrix"))
    {
        Row r;
        r.id = QStringLiteral("colours");
        r.label = tr("Colours");
        QStringList names;
        for (quint32 pid : v.look.own)
            if (const QLCPalette *c = doc->palette(pid))
                if (c->type() == QLCPalette::Color)
                    names << c->name();
        if (!names.isEmpty())
        {
            r.text = names.join(QStringLiteral(" + "));
            r.origin = withCarried(tr("its own"));
            r.inherited = !own;
            r.tip = tr("Colours that belong to this effect: they play inside it only (⋯ on the effect's row ▸ "
                       "Its Own Colour / Dimmer).");
        }
        else
        {
            const ResolvedValue col = layer.values.value(KeyColour);
            for (quint32 pid : col.look.none ? QList<quint32>() : col.look.palettes)
                if (const QLCPalette *c = doc->palette(pid))
                    names << c->name();
            r.text = names.isEmpty() ? tr("— (none: it plays white)") : names.join(QStringLiteral(" + "));
            r.origin = tr("the layer's Colour");
            r.inherited = true;
            r.tip = tr("The effect plays in the layer's Colour (colour 1, 2, … in order). Give it colours of its "
                       "own with ⋯ on the effect's row ▸ Its Own Colour / Dimmer.");
        }
        out << r;
    }

    // Each of its settings, and where the value comes from.
    const QMap<QString, double> values = p->effectParamValues();
    const bool oneOff = st->isInlinePalette(p->id());
    EffectPresetCache::Preset preset;
    if (!p->effectPreset().isEmpty() && doc->effectScriptRunner() != nullptr)
        preset = doc->effectScriptRunner()->presetCache()->preset(p->effectPreset());
    const QString scriptName = EffectOrigin::baseName(doc, p);
    EffectScript::ParamDef cycleDef;
    const QString cycle = EffectOrigin::scriptCycleParam(p->scriptPath(), &cycleDef);
    const int speedMaster = int(values.value(QStringLiteral("__speedMaster"), SpeedMasters::Inherit));
    const bool perBeat = SpeedMasters::isMaster(speedMaster) || speedMaster == SpeedMasters::MainClock;
    const QString becomesOwn = oneOff && own
        ? tr("Change it here: it is this cue's own effect, so it changes here (and where this cue's value "
             "tracks on). One undo step.")
        : tr("Change it here and it becomes this cue's own: a one-off copy of “%1” for this cue -- the "
             "palette and its other cues keep theirs. One undo step. (✎ on the effect's row edits the "
             "palette itself, everywhere.)").arg(p->name());
    for (const EffectScript::ParamDef &pd : EffectOrigin::scriptParams(p->scriptPath()))
    {
        if (pd.name.startsWith(QLatin1String("__")) || pd.type == QLatin1String("path")
            || pd.type == QLatin1String("image"))
            continue;
        Row r;
        r.id = pd.name;
        r.label = paramLabel(pd);
        r.def = pd;
        if (pd.type == QLatin1String("text"))
        {
            const QString t = p->effectStringParams().contains(pd.name) ? p->effectStringParam(pd.name)
                                                                         : pd.defaultText;
            r.text = QStringLiteral("“%1”").arg(t);
            r.origin = withCarried(p->effectStringParams().contains(pd.name)
                                   ? (oneOff ? QString() : tr("from Effect “%1”").arg(p->name()))
                                   : tr("the script's default"));
            r.inherited = !r.origin.isEmpty();
            r.tip = pd.description;
            out << r;
            continue;
        }
        const bool set = values.contains(pd.name);
        const bool inPreset = preset.isValid() && preset.params.contains(pd.name);
        r.value = set ? values.value(pd.name) : inPreset ? preset.params.value(pd.name) : pd.defaultValue;
        r.text = valueText(pd, r.value);
        if (perBeat && (pd.name == QLatin1String("speed") || pd.name == QLatin1String("rate")))
            r.text = tr("%1 per beat").arg(r.text);
        else if (pd.name == cycle && r.value > 0.0)
        {
            const double ownRate = values.value(QStringLiteral("__ownRate"), 1.0);
            r.text = tr("%1 · one cycle every %2 s").arg(r.text, number(1.0 / (r.value * qMax(0.0001, ownRate))));
        }
        QString from, why;
        if (inPreset && same(r.value, preset.params.value(pd.name)))
        {
            from = tr("the “%1” preset's").arg(preset.name);
            why = tr("As the “%1” preset sets it.").arg(preset.name);
        }
        else if (!set)
        {
            from = tr("the script's default");
            why = tr("Not set on this effect: %1's own default.").arg(scriptName.isEmpty() ? tr("the script")
                                                                                            : scriptName);
        }
        else if (!oneOff)
        {
            from = tr("from Effect “%1”").arg(p->name());
            why = tr("As the effect palette “%1” sets it: every cue using it plays this.").arg(p->name());
        }
        else
            why = own ? tr("This cue's own value.") : QString();
        r.origin = withCarried(from);
        r.inherited = !r.origin.isEmpty();
        if (!carried.isEmpty())
            why += QStringLiteral(" ") + tr("The effect is carried in (%1).").arg(carried);
        QString range;
        if (pd.enumValues.isEmpty() && pd.max > pd.min && pd.unit != QLatin1String("cm")  // units-ok: the script parameter's unit key; the value is shown through MeasurementUnits
            && pd.unit != QLatin1String("cm/s"))  // units-ok: the script parameter's unit key
            range = tr(" From %1 to %2.").arg(number(pd.min), number(pd.max));
        else if (pd.unit == QLatin1String("cm/s"))  // units-ok: the script parameter's unit key; shown through MeasurementUnits
            range = tr(" From %1 to %2.").arg(MeasurementUnits::formatSpeed(pd.min / 100.0),
                                              MeasurementUnits::formatSpeed(pd.max / 100.0));
        r.tip = QStringLiteral("%1.%2\n%3\n%4").arg(pd.description.isEmpty() ? pd.name : pd.description,
                                                    range, why.trimmed(), becomesOwn);
        r.editable = true;
        out << r;
    }

    // How it runs: Loop, Once, N times (the effect row's ⋯ ▸ Runs sets it).
    {
        bool oneShot = false, release = false;
        EffectOrigin::scriptRunDefaults(p->scriptPath(), &oneShot, &release);
        const EffectRuns::Settings s = EffectRuns::resolve(values, oneShot, release);
        Row r;
        r.id = QStringLiteral("runs");
        r.label = tr("Runs");
        r.text = s.loops() ? tr("Loop") : s.runs == 1 ? tr("Once") : tr("%1 times").arg(s.runs);
        if (!s.loops())
            r.text += QStringLiteral(" · ") + (s.release ? tr("then releases") : tr("then holds"));
        r.origin = withCarried(values.contains(EffectRuns::runsKey())
                               ? (oneOff ? QString() : tr("from Effect “%1”").arg(p->name()))
                               : tr("the script's default"));
        r.inherited = !r.origin.isEmpty();
        r.tip = tr("How it runs: set with ⋯ on the effect's row ▸ Runs (on the effect palette itself).");
        out << r;
    }
    return out;
}

bool EffectRows::setParam(Doc *doc, quint32 cueId, quint32 layerId, const Target &target, const QString &key,
                          const QString &param, double value, bool clear)
{
    ShowTreeController *st = doc == nullptr ? nullptr : doc->showTree();
    if (st == nullptr || !isEffectKey(key) || param.isEmpty())
        return false;
    const CueList *list = st->listOfCue(cueId);
    const Cue *cue = st->cue(cueId);
    if (list == nullptr || cue == nullptr)
        return false;
    Target t = target;
    if (layerId != invalidId())
    {
        const Layer *l = cue->layer(layerId);
        if (l == nullptr)
            return false;
        t = l->target;
    }
    const QList<ResolvedCue> rcs = st->resolved(*list);
    const int at = list->indexOf(cueId);
    const ResolvedLayer *rl = at >= 0 && at < rcs.count() ? rcs.at(at).layer(t) : nullptr;
    if (rl == nullptr || !rl->values.contains(key))
        return false;
    const ResolvedValue v = rl->values.value(key);
    QLCPalette *p = effectPalette(doc, v.look);
    if (p == nullptr)
        return false;
    EffectScript::ParamDef def;
    bool known = false;
    for (const EffectScript::ParamDef &pd : EffectOrigin::scriptParams(p->scriptPath()))
        if (pd.name == param)
        {
            def = pd;
            known = true;
        }
    if (!known)
        return false;
    if (!clear && def.max > def.min && def.enumValues.isEmpty())
        value = qBound(double(def.min), value, double(def.max));
    if (!def.enumValues.isEmpty())
        value = qBound(0, int(std::lround(value)), int(def.enumValues.size()) - 1);
    const bool has = p->effectParamValues().contains(param);
    if ((clear && !has) || (!clear && has && same(p->effectParamValues().value(param), value)))
        return false;

    const QString label = tr("%1 of %2").arg(paramLabel(def), p->name());
    auto change = [param, value, clear](QLCPalette *pp) {
        if (clear)
            pp->removeEffectParamValue(param);
        else
            pp->setEffectParamValue(param, value);
    };
    // This cue's own one-off (on the layer, or the cue's look set here):
    // in place.
    const bool here = v.origin == Origin::Own || (v.origin == Origin::CueLook && v.fromCue == cueId);
    if (st->isInlinePalette(p->id()) && here)
    {
        const quint32 pid = p->id();
        auto apply = [doc, pid, change]() {
            if (QLCPalette *pp = doc->palette(pid))
                change(pp);
            if (doc->effectScriptRunner() != nullptr)
                doc->effectScriptRunner()->updateEffectParams(pid);
        };
        if (doc->paletteOps() != nullptr)
            return doc->paletteOps()->effectParamStep(label, { pid }, apply);
        apply();
        doc->setModified();
        return true;
    }

    // A shared palette, or carried in: a one-off copy for this cue.
    const QString where = QStringLiteral("%1 %2").arg(st->cueLabel(*list, at),
                                                      rl->name.isEmpty() ? st->targetLabel(t) : rl->name);
    st->beginBatch(label);
    const quint32 copyId = st->createInlinePalette(QLCPalette::Effect, QVariantList(),
                                                   QStringLiteral("%1 · %2").arg(p->name(), where.trimmed()));
    QLCPalette *copy = doc->palette(copyId);
    bool ok = copy != nullptr;
    if (ok)
    {
        EffectDesigner::copyEffect(p, copy);
        change(copy);
        Look nl = v.look;
        nl.palettes[nl.palettes.indexOf(p->id())] = copyId;
        if (v.origin == Origin::CueLook && v.fromCue == cueId)
            ok = st->setCueLook(cueId, nl);
        else if (layerId != invalidId())
            ok = st->setLayerLook(cueId, layerId, nl);
        else
            ok = st->setTargetLook(cueId, t, nl);
    }
    st->endBatch(ok);
    if (!ok && copy != nullptr && doc->palette(copyId) != nullptr)
        doc->deletePalette(copyId);
    return ok;
}
