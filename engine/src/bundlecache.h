/*
  Q Light Controller Plus
  bundlecache.h

  Scans the Bundle directories for .json files and exposes CRUD operations.
  Search order: user dir first (user overrides shipped on name collision).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef BUNDLECACHE_H
#define BUNDLECACHE_H

#include "qlcbundle.h"

#include <QObject>
#include <QMap>
#include <QList>
#include <QDir>

class BundleCache : public QObject
{
    Q_OBJECT

public:
    explicit BundleCache(QObject *parent = nullptr);

    /** Scan all directories and rebuild the in-memory cache. */
    void rescan();

    /** All valid bundles discovered, sorted by name. */
    QList<QLCBundle> bundles() const;

    /** Bundle by name, or invalid if not found. */
    QLCBundle bundle(const QString &name) const;

    bool contains(const QString &name) const { return m_bundles.contains(name); }

    /**
     * Save @p b to the user bundles dir. Overwrites if same name already
     * exists there. Calls rescan() on success. Returns false on I/O error.
     */
    bool saveBundle(const QLCBundle &b);

    /**
     * Delete the user-dir file for @p name. Shipped bundles cannot be
     * deleted (they are not in the user dir). Returns false if not found
     * or if only a shipped copy exists.
     */
    bool deleteBundle(const QString &name);

    static QDir systemBundlesDirectory();
    static QDir userBundlesDirectory();

private:
    void scanDir(const QDir &dir);

    QMap<QString, QLCBundle> m_bundles;  //!< name → Bundle (first-found wins)
};

#endif // BUNDLECACHE_H
