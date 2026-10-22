/*
  Q Light Controller
  eosfixturemapper.cpp

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

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QFile>
#include <QDir>

#include <algorithm>

#include "eosfixturemapper.h"
#include "eosasciiparser.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcconfig.h"
#include "qlcfile.h"

QString EosFixtureChoice::text() const
{
    if (isGeneric())
        return QObject::tr("Generic dimmer");
    return QStringLiteral("%1 %2 (%3)").arg(manufacturer, model, mode);
}

EosFixtureMapper::EosFixtureMapper(const QLCFixtureDefCache *cache)
    : m_cache(cache)
{
}

QString EosFixtureMapper::defaultMapPath()
{
    const QDir dir = QLCFile::userDirectory(QString(USERQLCPLUSDIR), QString(), QStringList() << "*.json");
    return dir.absoluteFilePath(QStringLiteral("eos-fixture-map.json"));
}

bool EosFixtureMapper::loadMap(const QString &path)
{
    QFile f(path);
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it)
    {
        const QJsonObject o = it.value().toObject();
        EosFixtureChoice c;
        c.source = EosFixtureChoice::Saved;
        c.manufacturer = o.value(QStringLiteral("manufacturer")).toString();
        c.model = o.value(QStringLiteral("model")).toString();
        c.mode = o.value(QStringLiteral("mode")).toString();
        if (!c.manufacturer.isEmpty() && !c.model.isEmpty())
            m_map.insert(it.key(), c);
    }
    return true;
}

bool EosFixtureMapper::saveMap(const QString &path) const
{
    QJsonObject root;
    QStringList keys = m_map.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString &k : keys)
    {
        const EosFixtureChoice &c = m_map[k];
        QJsonObject o;
        o.insert(QStringLiteral("manufacturer"), c.manufacturer);
        o.insert(QStringLiteral("model"), c.model);
        o.insert(QStringLiteral("mode"), c.mode);
        root.insert(k, o);
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

QString EosFixtureMapper::key(const QString &manufacturer, const QString &model)
{
    return manufacturer.trimmed() + QLatin1Char('|') + model.trimmed();
}

QString EosFixtureMapper::key(const EosPersonality &p)
{
    return key(p.manufacturer, p.model);
}

QStringList EosFixtureMapper::words(const QString &name)
{
    QString s = name.toLower();
    s.replace(QRegularExpression(QStringLiteral("[_\\-/(),+]")), QStringLiteral(" "));
    // "5ch" and "5 channel" read the same
    s.replace(QRegularExpression(QStringLiteral("(\\d+)\\s*(ch|chan|channel|channels)\\b")), QStringLiteral("\\1ch"));
    return s.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QString EosFixtureMapper::manufacturerName(const QString &eosManufacturer)
{
    QString m = eosManufacturer.trimmed();
    // ETC's own library folders: ETC_Fixtures, ETC_Arch, ETC_Conventional...
    if (m.startsWith(QLatin1String("ETC"), Qt::CaseInsensitive))
        return QStringLiteral("ETC");
    m.replace(QLatin1Char('_'), QLatin1Char(' '));
    return m;
}

int EosFixtureMapper::editDistance(const QString &a, const QString &b)
{
    const int n = int(a.length()), m = int(b.length());
    QVector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j)
        prev[j] = j;
    for (int i = 1; i <= n; ++i)
    {
        cur[0] = i;
        for (int j = 1; j <= m; ++j)
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1,
                                prev[j - 1] + (a.at(i - 1).toLower() == b.at(j - 1).toLower() ? 0 : 1) });
        std::swap(prev, cur);
    }
    return prev[m];
}

namespace
{

QLCFixtureMode *modeByName(QLCFixtureDef *def, const QString &name)
{
    if (def == nullptr)
        return nullptr;
    for (QLCFixtureMode *m : def->modes())
        if (m->name() == name)
            return m;
    return nullptr;
}

} // namespace

QList<EosFixtureChoice> EosFixtureMapper::candidates(const EosPersonality &p, int max) const
{
    QList<EosFixtureChoice> out;
    if (m_cache == nullptr)
        return out;

    const QString eosManuf = manufacturerName(p.manufacturer);
    const QStringList eosWords = words(p.model);
    const QStringList manufWords = words(eosManuf);

    // Manufacturers the library spells the same, or one inside the other
    // ("Chauvet" / "Chauvet Professional").
    QStringList makers;
    for (const QString &m : m_cache->manufacturers())
    {
        const QString a = m.toLower(), b = eosManuf.toLower();
        if (a == b || (!b.isEmpty() && (a.startsWith(b + QLatin1Char(' ')) || b.startsWith(a + QLatin1Char(' ')))))
            makers << m;
    }

    struct Cand { QString manuf, model; int score; };
    QList<Cand> models;
    for (const QString &maker : makers)
        for (const QString &model : m_cache->models(maker))
        {
            const QStringList mw = words(model);
            int shared = 0, extra = 0;
            for (const QString &w : mw)
            {
                if (eosWords.contains(w))
                    ++shared;
                else
                    ++extra;
            }
            if (shared == 0)
                continue;
            int score = shared * 10 - extra * 4 - editDistance(model.left(eosWords.join(' ').length()),
                                                                eosWords.join(' ')) / 3;
            if (maker.compare(eosManuf, Qt::CaseInsensitive) == 0)
                score += 5;
            models << Cand { maker, model, score };
        }
    std::sort(models.begin(), models.end(), [](const Cand &a, const Cand &b) { return a.score > b.score; });

    for (const Cand &c : models)
    {
        if (out.count() >= max)
            break;
        QLCFixtureDef *def = m_cache->fixtureDef(c.manuf, c.model);
        if (def == nullptr)
            continue;
        // Only a mode as wide as the Eos footprint keeps the addresses.
        QLCFixtureMode *best = nullptr;
        int bestScore = -1;
        for (QLCFixtureMode *mode : def->modes())
        {
            if (p.footprint > 0 && int(mode->channels().count()) != p.footprint)
                continue;
            int s = 0;
            for (const QString &w : words(mode->name()))
                if (eosWords.contains(w))
                    s += 3;
            if (s > bestScore)
            {
                bestScore = s;
                best = mode;
            }
        }
        if (best == nullptr)
            continue;
        EosFixtureChoice ch;
        ch.source = EosFixtureChoice::Suggested;
        ch.manufacturer = c.manuf;
        ch.model = c.model;
        ch.mode = best->name();
        ch.score = c.score + bestScore;
        out << ch;
    }
    std::stable_sort(out.begin(), out.end(), [](const EosFixtureChoice &a, const EosFixtureChoice &b) {
        return a.score > b.score;
    });
    Q_UNUSED(manufWords)
    return out;
}

EosFixtureChoice EosFixtureMapper::resolve(const EosPersonality &p) const
{
    const QString k = key(p);
    if (m_map.contains(k) && m_cache != nullptr)
    {
        const EosFixtureChoice &c = m_map[k];
        QLCFixtureDef *def = m_cache->fixtureDef(c.manufacturer, c.model);
        if (modeByName(def, c.mode) != nullptr)
            return c;
    }

    // Eos's own "Generic Dimmer" is the library's generic dimmer.
    if (p.footprint <= 1 && words(p.model).contains(QStringLiteral("dimmer")))
        return EosFixtureChoice();

    const QList<EosFixtureChoice> cands = candidates(p, 4);
    if (!cands.isEmpty())
    {
        EosFixtureChoice best = cands.first();
        // Exact: the library's model is the start of the Eos model and
        // every word Eos adds after it names the mode.
        const QStringList ew = words(p.model), mw = words(best.model), modew = words(best.mode);
        bool exact = best.manufacturer.compare(manufacturerName(p.manufacturer), Qt::CaseInsensitive) == 0
                     && ew.mid(0, mw.count()) == mw;
        for (const QString &w : ew.mid(mw.count()))
            exact = exact && modew.contains(w);
        if (exact)
            best.source = EosFixtureChoice::Exact;
        return best;
    }
    return EosFixtureChoice();
}

void EosFixtureMapper::setChoice(const QString &key, const EosFixtureChoice &choice)
{
    if (choice.isGeneric())
    {
        m_map.remove(key);
        return;
    }
    EosFixtureChoice c = choice;
    c.source = EosFixtureChoice::Saved;
    m_map.insert(key, c);
}
