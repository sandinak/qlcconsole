/*
  Q Light Controller Plus
  bundlecache.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "bundlecache.h"
#include "qlcfile.h"

#include <QDebug>
#include <algorithm>

#define BUNDLEDIR     "Resources/Bundles"
#define USERBUNDLEDIR "Library/Application Support/QLC+/Bundles"

BundleCache::BundleCache(QObject *parent)
    : QObject(parent)
{
}

void BundleCache::rescan()
{
    m_bundles.clear();
    scanDir(userBundlesDirectory());
    scanDir(systemBundlesDirectory());
}

QList<QLCBundle> BundleCache::bundles() const
{
    QList<QLCBundle> list = m_bundles.values();
    std::sort(list.begin(), list.end(),
              [](const QLCBundle &a, const QLCBundle &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    return list;
}

QLCBundle BundleCache::bundle(const QString &name) const
{
    return m_bundles.value(name);
}

bool BundleCache::saveBundle(const QLCBundle &b)
{
    if (!b.isValid())
        return false;

    QDir dir = userBundlesDirectory();
    if (!dir.exists())
        dir.mkpath(".");

    const QString filename = QLCBundle::sanitiseFilename(b.name) + ".json";
    if (!b.saveToFile(dir.absoluteFilePath(filename)))
        return false;

    rescan();
    return true;
}

bool BundleCache::deleteBundle(const QString &name)
{
    QDir dir = userBundlesDirectory();
    const QString filename = QLCBundle::sanitiseFilename(name) + ".json";
    const QString path = dir.absoluteFilePath(filename);
    if (!QFile::exists(path))
    {
        qWarning() << "[BundleCache] no user-dir file for" << name
                   << "(shipped bundles cannot be deleted)";
        return false;
    }
    if (!QFile::remove(path))
    {
        qWarning() << "[BundleCache] failed to remove" << path;
        return false;
    }
    rescan();
    return true;
}

QDir BundleCache::systemBundlesDirectory()
{
    return QLCFile::systemDirectory(QString(BUNDLEDIR), QString(".json"));
}

QDir BundleCache::userBundlesDirectory()
{
    return QLCFile::userDirectory(QString(USERBUNDLEDIR),
                                  QString(BUNDLEDIR),
                                  QStringList() << "*.json");
}

void BundleCache::scanDir(const QDir &dir)
{
    if (!dir.exists() || !dir.isReadable())
        return;

    const QStringList files = dir.entryList(QStringList() << "*.json",
                                            QDir::Files | QDir::Readable);
    for (const QString &file : files)
    {
        const QLCBundle b = QLCBundle::fromFile(dir.absoluteFilePath(file));
        if (b.isValid() && !m_bundles.contains(b.name))
        {
            m_bundles.insert(b.name, b);
            qDebug() << "[BundleCache] found:" << b.name << "at" << b.path;
        }
    }
}
