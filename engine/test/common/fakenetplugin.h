/*
  Q Light Controller Plus - Test Unit
  fakenetplugin.h

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

#ifndef FAKENETPLUGIN_H
#define FAKENETPLUGIN_H

#include <QMap>
#include <QString>
#include <QVariant>

#include "qlcioplugin.h"

// Registering means appending to IOPluginCache's private list, which only
// works if this header is the first to pull ioplugincache.h in.
#ifdef IOPLUGINCACHE_H
#error "include fakenetplugin.h before any engine header"
#endif
#define private public
#include "ioplugincache.h"
#undef private
#include "doc.h"

/** A network output plugin under whatever name a test needs ("ArtNet",
 *  "E1.31"), with two interface lines, that remembers the parameters set on it.
 *  Not loaded from disk: register() puts it in a Doc's plugin cache, so the
 *  patch logic that keys on the plugin NAME can be exercised without hardware
 *  or a network. */
class FakeNetPlugin : public QLCIOPlugin
{
public:
    explicit FakeNetPlugin(const QString &name) : m_name(name) {}

    void init() override {}
    QString name() const override { return m_name; }
    int capabilities() const override { return Output; }
    QString pluginInfo() const override { return QStringLiteral("fake"); }

    QStringList outputs() override { return QStringList() << QStringLiteral("eth0") << QStringLiteral("eth1"); }
    bool openOutput(quint32, quint32) override { return true; }
    void closeOutput(quint32, quint32) override {}

    void setParameter(quint32 universe, quint32, Capability, QString name, QVariant value) override
    { m_params[universe][name] = value; }
    void unSetParameter(quint32 universe, quint32, Capability, QString name) override
    { m_params[universe].remove(name); }
    QMap<QString, QVariant> getParameters(quint32 universe, quint32, Capability) const override
    { return m_params.value(universe); }

    /** Add this plugin to a Doc's plugin cache under its name. */
    void registerIn(Doc *doc) { doc->ioPluginCache()->m_plugins.append(this); }

    QMap<quint32, QMap<QString, QVariant> > m_params;

private:
    QString m_name;
};

#endif
