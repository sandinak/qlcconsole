/*
  Q Light Controller Plus
  controllerjobs.cpp

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

#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

#include "controllerjobs.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"
#include "inputpatch.h"
#include "doc.h"

namespace
{
const QLatin1String kMac("mac:");
const QLatin1String kPort("port:");
const QLatin1String kName("name:");

/* A settings key holds no '/' or '\' (QSettings' separators): a Linux
   port ("usb-0000:00:14.0-2/input0") or a Windows path would split it. */
QString toKey(const QString &identity)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(identity, ":-._ "));
}

QString fromKey(const QString &key)
{
    return QUrl::fromPercentEncoding(key.toLatin1());
}
}

ControllerJobs::ControllerJobs(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    load();
}

ControllerJobs::~ControllerJobs()
{
}

QList<ControllerJobs::Job> ControllerJobs::jobs()
{
    return { RunCues, FollowSpot, Focus };
}

QString ControllerJobs::jobName(Job job)
{
    switch (job)
    {
        case RunCues:       return tr("Run cues");
        case FollowSpot:    return tr("Follow spot");
        case Focus:         return tr("Focus");
        default:            break;
    }
    return QString();
}

QString ControllerJobs::jobKey(Job job)
{
    switch (job)
    {
        case RunCues:       return QStringLiteral("run");
        case FollowSpot:    return QStringLiteral("followspot");
        case Focus:         return QStringLiteral("focus");
        default:            break;
    }
    return QString();
}

QString ControllerJobs::settingsGroup()
{
    return QStringLiteral("controllerJobs");
}

/*****************************************************************************
 * Who a controller is
 *****************************************************************************/

QString ControllerJobs::macIn(const QString &name)
{
    /* Six hex pairs, one separator throughout, not inside a longer run of
       hex (a serial number is not an address). */
    static const QRegularExpression re(QStringLiteral(
        "(?<![0-9A-Fa-f:-])([0-9A-Fa-f]{2})([:-])([0-9A-Fa-f]{2})\\2([0-9A-Fa-f]{2})\\2"
        "([0-9A-Fa-f]{2})\\2([0-9A-Fa-f]{2})\\2([0-9A-Fa-f]{2})(?![0-9A-Fa-f:-])"));
    const QRegularExpressionMatch m = re.match(name);
    if (!m.hasMatch())
        return QString();
    QStringList parts;
    for (int i : { 1, 3, 4, 5, 6, 7 })
        parts << m.captured(i).toLower();
    return parts.join(QLatin1Char(':'));
}

QString ControllerJobs::identityFor(const QString &plugin, const QString &name,
                                    const QString &location)
{
    const QString mac = macIn(name);
    if (!mac.isEmpty())
        return kMac + mac;
    if (!location.trimmed().isEmpty())
        return kPort + plugin + QLatin1Char(':') + location.trimmed();
    return kName + plugin + QLatin1Char(':') + name.trimmed();
}

QString ControllerJobs::identityOf(QLCIOPlugin *plugin, quint32 input)
{
    if (plugin == nullptr)
        return QString();
    const QStringList inputs = plugin->inputs();
    if (input >= quint32(inputs.size()))
        return QString();
    return identityFor(plugin->name(), inputs.at(int(input)), plugin->inputDeviceLocation(input));
}

QString ControllerJobs::identityOfUniverse(quint32 universe) const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || universe >= iom->universesCount())
        return QString();
    InputPatch *ip = iom->inputPatch(universe);
    if (ip == nullptr || ip->plugin() == nullptr)
        return QString();
    /* The device on the line now (a controller slot may have another pad
       there than the one it was patched with). */
    const QString name = ip->inputName();
    return identityFor(ip->plugin()->name(), name, ip->plugin()->inputDeviceLocation(ip->input()));
}

QString ControllerJobs::describe(const QString &identity)
{
    if (identity.startsWith(kMac))
        return tr("Bluetooth %1").arg(identity.mid(kMac.size()).toUpper());
    if (identity.startsWith(kPort))
    {
        // port:<plugin>:<location>; the plugin's name holds no colon.
        const QString rest = identity.mid(kPort.size());
        const QString location = rest.mid(rest.indexOf(QLatin1Char(':')) + 1);
        if (location.startsWith(QLatin1String("USB "), Qt::CaseInsensitive))
            return tr("USB port %1").arg(location.mid(4));
        return tr("port %1").arg(location);
    }
    if (identity.isEmpty())
        return QString();
    return tr("by name only (two of this model can't be told apart)");
}

bool ControllerJobs::isNameOnly(const QString &identity)
{
    return identity.startsWith(kName);
}

QString ControllerJobs::modelName(const QString &name)
{
    QString n = name.trimmed();
    const int paren = n.lastIndexOf(QLatin1String(" ("));
    if (paren > 0 && n.endsWith(QLatin1Char(')')))
        n.truncate(paren);
    return n.trimmed();
}

QString ControllerJobs::shortName(const QString &name, const QString &identity)
{
    const QString model = modelName(name);
    if (identity.startsWith(kMac))
        return QStringLiteral("%1 (…%2)").arg(model, identity.right(5).toUpper());
    if (identity.startsWith(kPort))
        return QStringLiteral("%1 (%2)").arg(model, describe(identity));
    return model;
}

QString ControllerJobs::nameOfUniverse(quint32 universe) const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || universe >= iom->universesCount())
        return QString();
    InputPatch *ip = iom->inputPatch(universe);
    if (ip == nullptr || ip->plugin() == nullptr)
        return QString();
    return shortName(ip->inputName(), identityOf(ip->plugin(), ip->input()));
}

/*****************************************************************************
 * The jobs
 *****************************************************************************/

void ControllerJobs::load()
{
    m_jobs.clear();
    QSettings s;
    s.beginGroup(settingsGroup());
    for (const QString &key : s.childKeys())
    {
        const QStringList on = s.value(key).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
        Jobs j = NoJobs;
        for (Job job : jobs())
            if (on.contains(jobKey(job)))
                j |= job;
        if (j != Jobs(AllJobs))
            m_jobs.insert(fromKey(key), j);
    }
    s.endGroup();
}

ControllerJobs::Jobs ControllerJobs::jobsOf(const QString &identity) const
{
    return m_jobs.value(identity, Jobs(AllJobs));
}

void ControllerJobs::setJobs(const QString &identity, Jobs j)
{
    if (identity.isEmpty())
        return;
    j &= Jobs(AllJobs);
    if (jobsOf(identity) == j)
        return;
    QSettings s;
    s.beginGroup(settingsGroup());
    if (j == Jobs(AllJobs))
    {
        m_jobs.remove(identity);
        s.remove(toKey(identity));
    }
    else
    {
        m_jobs.insert(identity, j);
        QStringList on;
        for (Job job : jobs())
            if (j.testFlag(job))
                on << jobKey(job);
        // "" is no job at all, kept as such (not the default).
        s.setValue(toKey(identity), on.join(QLatin1Char(',')));
    }
    s.endGroup();
    emit jobsChanged(identity);
}

void ControllerJobs::setJob(const QString &identity, Job job, bool on)
{
    Jobs j = jobsOf(identity);
    j.setFlag(job, on);
    setJobs(identity, j);
}

bool ControllerJobs::has(quint32 universe, Job job) const
{
    if (m_jobs.isEmpty())
        return true;        // nothing saved: every pad does everything
    const QString identity = identityOfUniverse(universe);
    if (identity.isEmpty())
        return true;
    return jobsOf(identity).testFlag(job);
}

void ControllerJobs::resetAll()
{
    const QStringList was = saved();
    QSettings s;
    s.remove(settingsGroup());
    m_jobs.clear();
    for (const QString &identity : was)
        emit jobsChanged(identity);
}

QStringList ControllerJobs::saved() const
{
    QStringList out = m_jobs.keys();
    out.sort();
    return out;
}
