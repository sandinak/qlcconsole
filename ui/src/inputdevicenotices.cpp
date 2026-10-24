/*
  Q Light Controller Plus - qlcconsole
  inputdevicenotices.cpp

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

#include <QTimer>

#include "inputdevicenotices.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "inputpatch.h"
#include "showstatus.h"
#include "universe.h"
#include "doc.h"

#define STATUS_KEY QStringLiteral("input.devices")

InputDeviceNotices::InputDeviceNotices(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_settle(new QTimer(this))
{
    m_settle->setSingleShot(true);
    m_settle->setInterval(2500);
    connect(m_settle, &QTimer::timeout, this, &InputDeviceNotices::settleNow);

    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom != nullptr)
    {
        connect(iom, &InputOutputMap::inputDeviceDisconnected, this, &InputDeviceNotices::schedule);
        connect(iom, &InputOutputMap::inputDeviceConnected, this, &InputDeviceNotices::schedule);
        connect(iom, &InputOutputMap::offlinePatchesChanged, this, &InputDeviceNotices::schedule);
        connect(iom, &InputOutputMap::pluginConfigurationChanged, this, &InputDeviceNotices::schedule);
    }
    schedule();
}

QList<InputDeviceNotices::Device> InputDeviceNotices::devices() const
{
    QList<Device> list;
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return list;

    foreach (Universe *uni, iom->universes())
    {
        if (uni == nullptr)
            continue;
        if (InputPatch *ip = uni->inputPatch())
        {
            if (ip->plugin() == nullptr || ip->plugin()->linesAreHardware() == false)
                continue;
            Device d;
            d.universe = uni->id();
            d.plugin = ip->pluginName();
            d.name = ip->inputUID().isEmpty() ? ip->inputName() : ip->inputUID();
            d.connected = true;
            list << d;
            continue;
        }
        foreach (const Universe::OfflinePatch &rec, uni->offlinePatches())
        {
            if (rec.tag != Universe::InputPatchTag || rec.uid.isEmpty())
                continue;
            /* Its plugin not installed here is another matter (the
               readiness entry for kept patches says so). */
            QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(rec.plugin);
            if (plugin == nullptr || plugin->linesAreHardware() == false)
                continue;
            Device d;
            d.universe = uni->id();
            d.plugin = rec.plugin;
            d.name = rec.uid;
            d.connected = false;
            list << d;
        }
    }
    return list;
}

void InputDeviceNotices::reset()
{
    m_said.clear();
    schedule();
}

int InputDeviceNotices::settleTime() const
{
    return m_settle->interval();
}

void InputDeviceNotices::setSettleTime(int ms)
{
    m_settle->setInterval(ms);
}

QString InputDeviceNotices::kindName(const QString &plugin)
{
    if (plugin == QLatin1String("HID"))
        return tr("Controller");
    if (plugin == QLatin1String("MIDI"))
        return tr("MIDI device");
    return tr("Input device");
}

void InputDeviceNotices::schedule()
{
    // Each change starts the wait again: a flap is one change, not two.
    m_settle->start();
}

void InputDeviceNotices::settleNow()
{
    m_settle->stop();

    const QList<Device> now = devices();
    QStringList said;
    bool warning = false;
    QMap<quint32, Device> next;

    foreach (const Device &d, now)
    {
        next.insert(d.universe, d);
        const QMap<quint32, Device>::const_iterator was = m_said.constFind(d.universe);
        const bool known = was != m_said.constEnd() && was->name == d.name;
        if (d.connected == false && known == false)
        {
            // A show opened with it switched off, or asleep.
            said << tr("%1 '%2' not connected — universe %3")
                        .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
            warning = true;
        }
        else if (d.connected == false && was->connected)
        {
            said << tr("%1 '%2' disconnected — universe %3")
                        .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
            warning = true;
        }
        else if (d.connected && known && was->connected == false)
        {
            said << tr("%1 '%2' connected — universe %3")
                        .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
        }
    }
    m_said = next;

    updateStatus(now);
    if (said.isEmpty() == false)
        emit notice(said.join(QStringLiteral(" · ")), warning);
}

void InputDeviceNotices::updateStatus(const QList<Device> &now) const
{
    QStringList items;
    QString first;
    foreach (const Device &d, now)
    {
        if (d.connected)
            continue;
        const QString line = tr("%1 '%2' not connected — universe %3")
                                 .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
        if (first.isEmpty())
            first = line;
        items << line;
    }

    if (items.isEmpty())
    {
        ShowStatus::instance()->clearStatus(STATUS_KEY);
        return;
    }
    ShowStatus::instance()->setStatus(STATUS_KEY, ShowStatus::Warning,
        items.count() == 1 ? first : tr("%1 input devices not connected").arg(items.count()),
        tr("The patch is kept, with its profile and controller roles. It reconnects "
           "by itself when the device is switched on or wakes -- nothing to re-patch."),
        items);
}
