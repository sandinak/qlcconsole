/*
  Q Light Controller Plus
  effectorigin.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <cmath>

#include "effectscriptrunner.h"
#include "effectpresetcache.h"
#include "effectscriptcache.h"
#include "effectorigin.h"
#include "effectscript.h"
#include "effectruns.h"
#include "qlcpalette.h"
#include "doc.h"

namespace
{
/* A script's settings, read once per file (and again when it changes): a
   tooltip must not evaluate the script every time it shows. */
struct Defs
{
    QDateTime modified;
    QList<EffectScript::ParamDef> params;
    bool oneShot = false;
    bool release = false;
    QString cycleParam;
};

const Defs &defsFor(const QString &path)
{
    static QHash<QString, Defs> cache;
    const QDateTime mod = QFileInfo(path).lastModified();
    auto it = cache.find(path);
    if (it != cache.end() && it->modified == mod)
        return *it;
    Defs d;
    d.modified = mod;
    EffectScript s;
    if (s.load(path))
    {
        d.params = s.paramDefs();
        d.oneShot = s.isOneShot();
        d.release = s.onFinish() == QLatin1String("release");
        if (!s.isOneShot())
            d.cycleParam = s.cycleParam();
    }
    return *cache.insert(path, d);
}

QString number(double v)
{
    if (std::fabs(v - std::round(v)) < 1e-6)
        return QString::number(qint64(std::round(v)));
    return QString::number(v, 'f', 2);
}

QString runsText(const EffectRuns::Settings &s)
{
    if (s.loops())
        return EffectOrigin::tr("Loop");
    if (s.runs == 1)
        return EffectOrigin::tr("Once");
    return EffectOrigin::tr("%1 times").arg(s.runs);
}
}

void EffectOrigin::scriptRunDefaults(const QString &path, bool *oneShot, bool *release)
{
    const Defs &d = defsFor(path);
    if (oneShot != nullptr)
        *oneShot = d.oneShot;
    if (release != nullptr)
        *release = d.release;
}

QString EffectOrigin::scriptCycleParam(const QString &path, EffectScript::ParamDef *def)
{
    const Defs &d = defsFor(path);
    if (d.cycleParam.isEmpty())
        return QString();
    for (const EffectScript::ParamDef &pd : d.params)
        if (pd.name == d.cycleParam && pd.enumValues.isEmpty())
        {
            if (def != nullptr)
                *def = pd;
            return d.cycleParam;
        }
    return QString();
}

bool EffectOrigin::isMinePath(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QString mine = QFileInfo(EffectPresetCache::userPresetsDirectory().absolutePath()).canonicalFilePath();
    const QString sys = QFileInfo(EffectPresetCache::systemPresetsDirectory().absolutePath()).canonicalFilePath();
    const QString dir = QFileInfo(QFileInfo(path).absolutePath()).canonicalFilePath();
    return !mine.isEmpty() && dir == mine && dir != sys;
}

QString EffectOrigin::baseName(Doc *doc, const QLCPalette *p)
{
    if (p == nullptr || p->type() != QLCPalette::Effect)
        return QString();
    if (!p->effectPreset().isEmpty())
        return p->effectPreset();
    if (!p->wledEffect().isEmpty())
        return tr("WLED %1").arg(p->wledEffect());
    if (p->scriptPath().isEmpty())
        return QString();
    if (doc != nullptr && doc->effectScriptRunner() != nullptr)
    {
        EffectScriptCache *sc = doc->effectScriptRunner()->cache();
        const EffectScriptCache::ScriptMeta m = sc->scriptMeta(EffectScriptCache::portableName(p->scriptPath()));
        if (!m.displayName.isEmpty())
            return m.displayName;
    }
    return EffectScriptCache::nameFromPath(p->scriptPath());
}

QStringList EffectOrigin::compare(Doc *doc, const QString &scriptPath, const QString &base,
                                  const QMap<QString, double> &original,
                                  const QMap<QString, double> &values,
                                  const QMap<QString, QString> &strings)
{
    Q_UNUSED(doc)
    QStringList out;
    if (scriptPath.isEmpty())
        return out;
    const Defs &d = defsFor(scriptPath);
    for (const EffectScript::ParamDef &pd : d.params)
    {
        const QString label = pd.description.isEmpty() ? pd.name : pd.description;
        if (pd.type == QLatin1String("path") || pd.type == QLatin1String("text")
            || pd.type == QLatin1String("image"))
        {
            const QString now = strings.value(pd.name);
            if (!now.isEmpty() && now != pd.defaultText)
                out << tr("%1: changed").arg(label);
            continue;
        }
        const double was = original.value(pd.name, pd.defaultValue);
        const double now = values.value(pd.name, was);
        if (std::fabs(now - was) < 1e-4)
            continue;
        auto show = [&pd](double v) {
            if (!pd.enumValues.isEmpty())
                return pd.enumValues.value(qBound(0, int(std::lround(v)), pd.enumValues.size() - 1));
            return number(v);
        };
        out << tr("%1: %2 (%3: %4)").arg(label, show(now), base, show(was));
    }
    // How it runs: only where it says something the effect itself doesn't.
    const EffectRuns::Settings mine = EffectRuns::resolve(values, d.oneShot, d.release);
    const EffectRuns::Settings orig = EffectRuns::resolve(original, d.oneShot, d.release);
    if (mine.runs != orig.runs)
        out << tr("Runs: %1 (%2: %3)").arg(runsText(mine), base, runsText(orig));
    if (!mine.loops() && mine.release != orig.release)
        out << tr("At the end: %1").arg(mine.release ? tr("Release") : tr("Hold"));
    if (!mine.loops() && mine.lengthMs != orig.lengthMs)
        out << tr("Length: %1").arg(mine.lengthMs > 0 ? tr("%1 s").arg(number(mine.lengthMs / 1000.0))
                                                       : tr("auto"));
    return out;
}

QStringList EffectOrigin::differences(Doc *doc, const QLCPalette *p)
{
    if (p == nullptr || p->type() != QLCPalette::Effect || doc == nullptr
        || doc->effectScriptRunner() == nullptr || p->scriptPath().isEmpty())
        return QStringList();
    QMap<QString, double> original;
    if (!p->effectPreset().isEmpty())
        original = doc->effectScriptRunner()->presetCache()->preset(p->effectPreset()).params;
    return compare(doc, p->scriptPath(), baseName(doc, p), original, p->effectParamValues(),
                   p->effectStringParams());
}

QStringList EffectOrigin::presetDifferences(Doc *doc, const QString &presetName)
{
    if (doc == nullptr || doc->effectScriptRunner() == nullptr)
        return QStringList();
    const EffectPresetCache::Preset pr = doc->effectScriptRunner()->presetCache()->preset(presetName);
    EffectScriptCache *sc = doc->effectScriptRunner()->cache();
    const QString path = sc->scriptPath(pr.script);
    const QString base = sc->scriptMeta(pr.script).displayName.isEmpty()
                             ? EffectScriptCache::nameFromPath(path) : sc->scriptMeta(pr.script).displayName;
    return compare(doc, path, base, QMap<QString, double>(), pr.params, QMap<QString, QString>());
}

QString EffectOrigin::basedOnLine(const QString &base, int changes)
{
    if (base.isEmpty())
        return QString();
    if (changes <= 0)
        return tr("based on %1").arg(base);
    return tr("based on %1 · %n change(s)", "", changes).arg(base);
}

QString EffectOrigin::differencesTip(const QString &base, const QStringList &diffs)
{
    if (base.isEmpty())
        return QString();
    if (diffs.isEmpty())
        return tr("Based on %1, with its settings as they are.").arg(base);
    return tr("Based on %1. Different from it:").arg(base) + QStringLiteral("\n• ")
           + diffs.join(QStringLiteral("\n• "));
}
