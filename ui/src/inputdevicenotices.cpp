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
#include <QSet>
#include <QMessageBox>
#include <QPushButton>

#include "inputdevicenotices.h"
#include "controlowners.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "inputpatch.h"
#include "showstatus.h"
#include "universe.h"
#include "controlmap.h"
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
        connect(iom, &InputOutputMap::inputSlotChanged, this, &InputDeviceNotices::schedule);
    }
    /* The stop's first value after a replug taken for the device's state
       dump: said where the replug is said, never only in a debug log. */
    ControlMap *cm = m_doc != nullptr ? m_doc->controlMap() : nullptr;
    if (cm != nullptr)
        connect(cm, &ControlMap::stateDumpIgnored, this, &InputDeviceNotices::slotStateDumpIgnored);
    schedule();
}

QList<InputDeviceNotices::Device> InputDeviceNotices::devices() const
{
    QList<Device> list;
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return list;

    /* A spare unplugged and plugged back goes to the first universe in
       order that waits for its name (retryOfflinePatches()): only that one
       is told it can take it over. */
    QSet<QString> advised;
    foreach (Universe *uni, iom->universes())
    {
        if (uni == nullptr)
            continue;
        QStringList slotNames;
        foreach (const Universe::InputSlotDevice &sd, uni->inputSlot())
            slotNames << (sd.anyGamepad ? tr("any game controller") : sd.uid);

        if (InputPatch *ip = uni->inputPatch())
        {
            if (ip->plugin() == nullptr || ip->plugin()->linesAreHardware() == false)
                continue;
            Device d;
            d.universe = uni->id();
            d.plugin = ip->pluginName();
            d.name = ip->inputUID().isEmpty() ? ip->inputName() : ip->inputUID();
            d.connected = true;
            d.slotCount = slotNames.count();
            d.slotPosition = uni->inputSlotLive() + 1;
            d.slotDevices = slotNames;
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
            d.slotCount = slotNames.count();
            d.slotDevices = slotNames;
            d.sameNameHere = plugin->inputs().contains(rec.uid);
            // One that no universe has: only that one may take over (round-8 review).
            d.spareHere = d.sameNameHere && iom->hasFreeInputDevice(plugin, rec.uid);
            if (d.spareHere && uni->hasInputSlot() == false)
            {
                const QString key = rec.plugin + QLatin1Char('\n') + rec.uid;
                d.spareHere = advised.contains(key) == false;
                advised.insert(key);
            }
            list << d;
        }
    }
    return list;
}

void InputDeviceNotices::reset()
{
    m_said.clear();
    m_offered.clear();
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

QString InputDeviceNotices::goneText(const Device &d, bool disconnected)
{
    /* Another device of that exact name connected: the universe is not
       given it by itself (it may not be the same pad; board Q253). One no
       universe has may be used here -- said how; one another universe has
       is that universe's, and nothing is said of it (round-8 review). */
    if (d.spareHere)
        return disconnected
            ? tr("%1 '%2' disconnected — universe %3 waits for its own device. Another '%2' is connected: "
                 "to use it here, unplug it and plug it back in, or patch it in Inputs/Outputs.")
                  .arg(kindName(d.plugin), d.name).arg(d.universe + 1)
            : tr("%1 '%2' not connected — universe %3 waits for its own device. Another '%2' is connected: "
                 "to use it here, unplug it and plug it back in, or patch it in Inputs/Outputs.")
                  .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
    if (d.sameNameHere)
        return disconnected
            ? tr("%1 '%2' disconnected — universe %3 waits for its own device.")
                  .arg(kindName(d.plugin), d.name).arg(d.universe + 1)
            : tr("%1 '%2' not connected — universe %3 waits for its own device.")
                  .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
    return disconnected
        ? tr("%1 '%2' disconnected — universe %3").arg(kindName(d.plugin), d.name).arg(d.universe + 1)
        : tr("%1 '%2' not connected — universe %3").arg(kindName(d.plugin), d.name).arg(d.universe + 1);
}

QString InputDeviceNotices::jobsText(Doc *doc, quint32 universe)
{
    return ControlOwners::jobsOn(doc, universe).join(QStringLiteral(", "));
}

QString InputDeviceNotices::stateDumpText(const QString &what, quint32 universe)
{
    return tr("Ignored %1's first value after replug (state dump) — universe %2").arg(what).arg(universe + 1);
}

void InputDeviceNotices::slotStateDumpIgnored(quint32 universe, quint32 channel, const QString &what)
{
    Q_UNUSED(channel);
    emit notice(stateDumpText(what, universe), false);
}

void InputDeviceNotices::noteWake(qint64 sleptMs)
{
    // Nothing patched to a device: nothing to find.
    if (devices().isEmpty())
        return;
    const qint64 s = qMax<qint64>(1, (sleptMs + 500) / 1000);
    const QString slept = s < 120 ? tr("%1 s").arg(s) : tr("%1 min").arg((s + 30) / 60);
    emit notice(tr("Slept %1, finding devices…").arg(slept), false);
    m_resayMissing = true;
    // What comes back, or doesn't, is said once it has settled.
    schedule();
}

QString InputDeviceNotices::repatchText(Doc *doc, quint32 universe, const QString &newDevice)
{
    InputOutputMap *iom = doc != nullptr ? doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return QString();
    QString was;
    if (InputPatch *ip = iom->inputPatch(universe))
        was = ip->inputName();
    else if (Universe *uni = iom->universe(universe))
    {
        foreach (const Universe::OfflinePatch &rec, uni->offlinePatches())
        {
            if (rec.tag == Universe::InputPatchTag && rec.uid.isEmpty() == false)
                was = rec.uid;
        }
    }
    if (was == newDevice)
        return QString();
    const QString jobs = jobsText(doc, universe);
    if (jobs.isEmpty())
        return QString();
    if (newDevice.isEmpty())
        return tr("Universe %1's controls stop working: %2.").arg(universe + 1).arg(jobs);
    if (was.isEmpty())
        return tr("Universe %1's controls move to '%2': %3.").arg(universe + 1).arg(newDevice, jobs);
    return tr("Universe %1's controls move from '%2' to '%3': %4. '%2' will do none of them.")
        .arg(universe + 1).arg(was, newDevice, jobs);
}

bool InputDeviceNotices::confirmRepatch(QWidget *parent, Doc *doc, quint32 universe, const QString &newDevice)
{
    const QString text = repatchText(doc, universe, newDevice);
    if (text.isEmpty())
        return true;
    QMessageBox box(QMessageBox::Warning, tr("Change Input"), text, QMessageBox::Cancel, parent);
    box.setInformativeText(tr("Each control keeps its job on the universe; the new device's "
                              "controls with the same numbers do them."));
    QPushButton *go = box.addButton(newDevice.isEmpty() ? tr("Unpatch") : tr("Patch"),
                                    QMessageBox::AcceptRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.setObjectName(QStringLiteral("repatchWarning"));
    box.exec();
    return box.clickedButton() == go;
}

void InputDeviceNotices::schedule()
{
    // Each change starts the wait again: a flap is one change, not two.
    m_settle->start();
}

QString InputDeviceNotices::withJobs(const QString &text, quint32 universe) const
{
    const QString jobs = jobsText(m_doc, universe);
    if (jobs.isEmpty())
        return text;
    // Named, so the operator knows what is gone before reaching for it (phase R).
    QString head = text;
    if (head.endsWith(QLatin1Char('.')))
        head.chop(1);
    return tr("%1. Not working until it is back: %2.").arg(head, jobs);
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
        const bool seen = was != m_said.constEnd();
        const bool known = seen && was->name == d.name;
        if (d.connected == false && d.slotCount > 1)
        {
            // A controller slot with none of its devices here.
            if (seen && was->connected == false && m_resayMissing == false)
                continue;
            said << (seen ? tr("%1 '%2' disconnected — universe %3: none of its %4 controllers is connected")
                                .arg(kindName(d.plugin), was->name).arg(d.universe + 1).arg(d.slotCount)
                          : tr("Universe %1: none of its %2 controllers is connected (%3)")
                                .arg(d.universe + 1).arg(d.slotCount)
                                .arg(d.slotDevices.join(QStringLiteral(", "))));
            warning = true;
        }
        else if (d.connected == false && known == false)
        {
            // A show opened with it switched off, or asleep.
            said << withJobs(goneText(d, false), d.universe);
            warning = true;
        }
        else if (d.connected == false && was->connected)
        {
            said << withJobs(goneText(d, true), d.universe);
            warning = true;
        }
        else if (d.connected == false && m_resayMissing)
        {
            // Still missing after a wake: said again, in amber.
            said << withJobs(goneText(d, false), d.universe);
            warning = true;
        }
        else if (d.connected && d.slotCount > 1 && seen && (known == false || was->connected == false))
        {
            // A controller slot: which of its devices drives it now.
            said << tr("%1 '%2' in use — universe %3 (%4 of %5 in its list)")
                        .arg(kindName(d.plugin), d.name).arg(d.universe + 1)
                        .arg(d.slotPosition).arg(d.slotCount);
        }
        else if (d.connected && known && was->connected == false)
        {
            said << tr("%1 '%2' connected — universe %3")
                        .arg(kindName(d.plugin), d.name).arg(d.universe + 1);
        }
    }
    m_said = next;
    m_resayMissing = false;

    updateStatus(now);
    if (said.isEmpty() == false)
        emit notice(said.join(QStringLiteral(" · ")), warning);

    /* A pad here that nothing has, while a universe waits for one that is
       not: offer it (one click, and remembered with the show). */
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return;
    QSet<QString> standing;
    QString made;
    foreach (const InputOutputMap::InputSlotOffer &o, iom->inputSlotOffers())
    {
        const QString key = QStringLiteral("%1\n%2\n%3").arg(o.universe).arg(o.plugin, o.device);
        standing.insert(key);
        if (m_offered.contains(key) || made.isEmpty() == false)
            continue;
        made = key;
        QString waitingFor;
        foreach (const Device &d, now)
            if (d.universe == o.universe)
                waitingFor = d.slotCount > 1 ? d.slotDevices.join(QStringLiteral(", ")) : d.name;
        emit offer(tr("%1 '%2' is here; universe %3 waits for '%4', which is not.")
                       .arg(kindName(o.plugin), o.device).arg(o.universe + 1).arg(waitingFor),
                   tr("Use '%1' for universe %2 (and remember it)").arg(o.device).arg(o.universe + 1),
                   o.universe, o.plugin, o.device);
        // One at a time (the notice line holds one); the next on a later change.
    }
    m_offered.intersect(standing);
    if (made.isEmpty() == false)
        m_offered.insert(made);
}

bool InputDeviceNotices::adopt(quint32 universe, const QString &plugin, const QString &device)
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || iom->addInputSlotDevice(universe, plugin, device) == false)
        return false;
    m_doc->setModified();
    return true;
}

void InputDeviceNotices::updateStatus(const QList<Device> &now) const
{
    QStringList items;
    QString first;
    foreach (const Device &d, now)
    {
        if (d.connected)
            continue;
        // The footer's plain line: waiting for its own device, when one of that name is here.
        const QString line = d.sameNameHere
            ? tr("%1 '%2' not connected — universe %3 waits for its own device")
                  .arg(kindName(d.plugin), d.name).arg(d.universe + 1)
            : tr("%1 '%2' not connected — universe %3").arg(kindName(d.plugin), d.name).arg(d.universe + 1);
        if (first.isEmpty())
            first = line;
        items << withJobs(line, d.universe);
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

/*****************************************************************************
 * Stick rest
 *****************************************************************************/

void InputDeviceNotices::watchSticks(QLCIOPlugin *plugin)
{
    if (plugin == nullptr)
        return;
    /* By name: QLCIOPlugin is built into every plugin too, so on Windows a
       member-pointer connection to its signals never fires. */
    connect(plugin, SIGNAL(inputStickDrift(quint32,int,double)),
            this, SLOT(slotStickDrift(quint32,int,double)), Qt::UniqueConnection);
    connect(plugin, SIGNAL(inputSticksCentred(quint32,int)),
            this, SLOT(slotSticksCentred(quint32,int)), Qt::UniqueConnection);
}

QString InputDeviceNotices::controllerName(QLCIOPlugin *plugin, quint32 input)
{
    const QStringList inputs = plugin != nullptr ? plugin->inputs() : QStringList();
    QString name = int(input) < inputs.size() ? inputs.at(int(input)).trimmed() : QString();
    const int paren = name.lastIndexOf(QLatin1String(" ("));
    if (paren > 0 && name.endsWith(QLatin1Char(')')))
        name.truncate(paren);
    return name.isEmpty() ? tr("The controller") : name;
}

bool InputDeviceNotices::centreSticks(QLCIOPlugin *plugin, quint32 input)
{
    if (plugin == nullptr || !plugin->centreInputSticks(input))
    {
        emit notice(tr("%1's sticks can't be centred: its input plugin doesn't correct them.")
                    .arg(controllerName(plugin, input)), false);
        return false;
    }
    emit notice(tr("Centring %1's sticks: let go of them for a moment.")
                .arg(controllerName(plugin, input)), false);
    return true;
}

void InputDeviceNotices::slotStickDrift(quint32 input, int channel, double offset)
{
    QLCIOPlugin *plugin = qobject_cast<QLCIOPlugin *>(sender());
    if (plugin == nullptr)
        return;
    const QString name = controllerName(plugin, input);
    const QString key = plugin->name() + QLatin1Char('\n') + name;
    if (m_driftSaid.contains(key))
        return;
    m_driftSaid.insert(key);
    QString axis = plugin->inputChannelName(input, quint32(channel));
    if (axis.isEmpty())
        axis = tr("axis %1").arg(channel + 1);
    const QString value = QString::asprintf("%+.2f", offset);
    emit stickOffer(tr("%1's stick isn't resting in the middle (%2 %3): what it drives can creep. "
                       "Let go of the sticks and centre them.").arg(name, axis, value),
                    tr("Centre Sticks"), plugin, input);
}

void InputDeviceNotices::slotSticksCentred(quint32 input, int held)
{
    QLCIOPlugin *plugin = qobject_cast<QLCIOPlugin *>(sender());
    if (plugin == nullptr)
        return;
    const QString name = controllerName(plugin, input);
    if (held > 0)
        emit notice(tr("%1: %n stick(s) were held off the middle and left as they were. "
                       "Let go of the sticks and centre them again.", "", held).arg(name), false);
    else
        emit notice(tr("%1's sticks are centred.").arg(name), false);
}
