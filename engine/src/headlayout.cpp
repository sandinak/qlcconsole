/*
  Q Light Controller Plus
  headlayout.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QHash>
#include <QMutex>

#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "headlayout.h"
#include "fixture.h"
#include "qlcfile.h"

#define USERHEADLAYOUTDIR "Library/Application Support/qlcconsole/HeadLayouts"

namespace
{
    QString &overridePath()
    {
        static QString path;
        return path;
    }

    /* What load() has read, by key -- invalid entries too, so a model with
       no map costs one look at the disk, not one per frame of the rig view. */
    QHash<QString, HeadLayout> &cache()
    {
        static QHash<QString, HeadLayout> c;
        return c;
    }

    /* The rig view draws fixtures on several threads at once, and each may
       ask for its layout. */
    QMutex &cacheLock()
    {
        static QMutex m;
        return m;
    }

    /* A file name for a key: the key with anything a file system might
       object to replaced. */
    QString fileNameFor(const QString &key)
    {
        QString n = key;
        for (QChar &c : n)
            if (!(c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_')
                  || c == QLatin1Char(' ') || c == QLatin1Char('.')))
                c = QLatin1Char('_');
        return n + QStringLiteral(".json");
    }
}

/*****************************************************************************
 * HeadLayout
 *****************************************************************************/

QString HeadLayout::keyFor(const Fixture *fx)
{
    if (fx == nullptr || fx->fixtureDef() == nullptr || fx->fixtureMode() == nullptr)
        return QString();
    const QLCFixtureDef *def = fx->fixtureDef();
    if (def->manufacturer() == KXMLFixtureGeneric && def->model() == KXMLFixtureGeneric)
        return QString();
    return def->manufacturer() + QLatin1Char('|') + def->model()
           + QLatin1Char('|') + fx->fixtureMode()->name();
}

QSize HeadLayout::declaredGrid(const QLCFixtureMode *mode)
{
    if (mode == nullptr)
        return QSize();
    const QSize sz = mode->physical().layoutSize();
    const int n = mode->heads().size();
    if (sz.width() <= 0 || sz.height() <= 0 || sz == QSize(1, 1) || sz.width() * sz.height() != n)
        return QSize();
    return sz;
}

HeadLayout HeadLayout::headOrder(const Fixture *fx)
{
    HeadLayout l;
    l.key = keyFor(fx);
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    const int n = mode != nullptr ? mode->heads().size() : 0;
    if (l.key.isEmpty() || n == 0)
        return HeadLayout();
    QSize sz = declaredGrid(mode);
    if (!sz.isValid())
        sz = QSize(n, 1);              // a strip: one row, in head order
    l.size = sz;
    for (int h = 0; h < n; ++h)
        l.cells << QPoint(h % sz.width(), h / sz.width());
    return l;
}

HeadLayout HeadLayout::fromRowPattern(const Fixture *fx, const QStringList &rowKinds)
{
    HeadLayout base = headOrder(fx);
    if (!base.isValid() || rowKinds.isEmpty())
        return HeadLayout();
    const QLCFixtureMode *mode = fx->fixtureMode();
    const int cols = base.size.width();
    const int n = mode->heads().size();

    // Every row of a kind is filled by that kind's heads, in head order.
    QHash<QString, QList<int> > headsOf;
    for (int h = 0; h < n; ++h)
        headsOf[HeadKinds::kindOf(mode, h)] << h;
    QHash<QString, int> rowsOf;
    for (const QString &k : rowKinds)
        rowsOf[k]++;
    for (auto it = headsOf.constBegin(); it != headsOf.constEnd(); ++it)
        if (rowsOf.value(it.key()) * cols != it.value().size())
            return HeadLayout();          // the rows cannot hold exactly these heads

    HeadLayout l;
    l.key = base.key;
    l.size = QSize(cols, rowKinds.size());
    l.cells.fill(QPoint(-1, -1), n);
    QHash<QString, int> used;
    for (int r = 0; r < rowKinds.size(); ++r)
    {
        const QList<int> &pool = headsOf[rowKinds.at(r)];
        for (int c = 0; c < cols; ++c)
            l.cells[pool.at(used[rowKinds.at(r)]++)] = QPoint(c, r);
    }
    return l;
}

QStringList HeadLayout::rowKinds(const Fixture *fx) const
{
    QStringList out;
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return out;
    QVector<QString> kind(size.height());
    QVector<bool> mixed(size.height(), false);
    for (int h = 0; h < cells.size() && h < mode->heads().size(); ++h)
    {
        const int r = cells.at(h).y();
        if (r < 0 || r >= size.height())
            continue;
        const QString k = HeadKinds::kindOf(mode, h);
        if (kind[r].isEmpty())
            kind[r] = k;
        else if (kind[r] != k)
            mixed[r] = true;
    }
    for (int r = 0; r < size.height(); ++r)
        out << (mixed[r] ? QString() : kind[r]);
    return out;
}

/*****************************************************************************
 * HeadKinds
 *****************************************************************************/

QString HeadKinds::kindOf(const QLCFixtureMode *mode, int head)
{
    if (mode == nullptr || head < 0 || head >= mode->heads().size())
        return QString();
    bool r = false, g = false, b = false, w = false, a = false, uv = false,
         c = false, m = false, y = false, lime = false, indigo = false, plain = false;
    foreach (quint32 ci, mode->heads().at(head).channels())
    {
        const QLCChannel *ch = mode->channel(ci);
        if (ch == nullptr || ch->group() != QLCChannel::Intensity)
            continue;
        switch (ch->colour())
        {
        case QLCChannel::Red:     r = true; break;
        case QLCChannel::Green:   g = true; break;
        case QLCChannel::Blue:    b = true; break;
        case QLCChannel::White:   w = true; break;
        case QLCChannel::Amber:   a = true; break;
        case QLCChannel::UV:      uv = true; break;
        case QLCChannel::Cyan:    c = true; break;
        case QLCChannel::Magenta: m = true; break;
        case QLCChannel::Yellow:  y = true; break;
        case QLCChannel::Lime:    lime = true; break;
        case QLCChannel::Indigo:  indigo = true; break;
        default:                  plain = true; break;   // a dimmer
        }
    }
    QString k;
    if (r) k += QLatin1Char('R');
    if (g) k += QLatin1Char('G');
    if (b) k += QLatin1Char('B');
    if (a) k += QLatin1Char('A');
    if (w) k += QLatin1Char('W');
    if (c) k += QLatin1Char('C');
    if (m) k += QLatin1Char('M');
    if (y) k += QLatin1Char('Y');
    if (lime) k += QStringLiteral("L");
    if (indigo) k += QStringLiteral("In");   // not "I": that is a plain dimmer head
    if (uv) k += k.isEmpty() ? QStringLiteral("UV") : QStringLiteral("+UV");
    if (k.isEmpty() && plain)
        k = QStringLiteral("I");
    return k;
}

QStringList HeadKinds::kindsOf(const Fixture *fx)
{
    QStringList out;
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return out;
    for (int h = 0; h < mode->heads().size(); ++h)
    {
        const QString k = kindOf(mode, h);
        if (!k.isEmpty() && !out.contains(k))
            out << k;
    }
    return out;
}

bool HeadKinds::isMixed(const Fixture *fx)
{
    return kindsOf(fx).size() > 1;
}

/*****************************************************************************
 * HeadLayoutLibrary
 *****************************************************************************/

void HeadLayoutLibrary::setUserDirectoryOverride(const QString &path)
{
    QMutexLocker lock(&cacheLock());
    overridePath() = path;
    cache().clear();
}

QDir HeadLayoutLibrary::userDirectory()
{
    if (!overridePath().isEmpty())
    {
        QDir dir(overridePath());
        if (!dir.exists())
            dir.mkpath(".");
        dir.setFilter(QDir::Files);
        dir.setNameFilters(QStringList() << "*.json");
        return dir;
    }
    return QLCFile::userDirectory(QString(USERHEADLAYOUTDIR), QString(), QStringList() << "*.json");
}

HeadLayout HeadLayoutLibrary::load(const QString &key)
{
    if (key.isEmpty())
        return HeadLayout();
    QMutexLocker lock(&cacheLock());
    if (cache().contains(key))
        return cache().value(key);
    cache().insert(key, HeadLayout());
    QFile f(userDirectory().absoluteFilePath(fileNameFor(key)));
    if (!f.open(QIODevice::ReadOnly))
        return HeadLayout();
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    HeadLayout l;
    l.key = o.value(QStringLiteral("key")).toString();
    if (l.key != key)
        return HeadLayout();
    l.size = QSize(o.value(QStringLiteral("cols")).toInt(), o.value(QStringLiteral("rows")).toInt());
    const QJsonArray cells = o.value(QStringLiteral("cells")).toArray();
    for (const QJsonValue &v : cells)
    {
        const QJsonArray p = v.toArray();
        l.cells << QPoint(p.at(0).toInt(-1), p.at(1).toInt(-1));
    }
    if (!l.isValid())
        return HeadLayout();
    cache().insert(key, l);
    return l;
}

bool HeadLayoutLibrary::save(const HeadLayout &layout)
{
    if (!layout.isValid())
        return false;
    QJsonArray cells;
    foreach (const QPoint &p, layout.cells)
        cells.append(QJsonArray({ p.x(), p.y() }));
    QJsonObject o;
    o.insert(QStringLiteral("key"), layout.key);
    o.insert(QStringLiteral("cols"), layout.size.width());
    o.insert(QStringLiteral("rows"), layout.size.height());
    o.insert(QStringLiteral("cells"), cells);
    QDir dir = userDirectory();
    if (!dir.exists())
        dir.mkpath(".");
    QFile f(dir.absoluteFilePath(fileNameFor(layout.key)));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    QMutexLocker lock(&cacheLock());
    cache().insert(layout.key, layout);
    return true;
}
