/*
  Q Light Controller Plus - qlcconsole
  surfaceautopatch.cpp

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

#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include "surfaceautopatch.h"
#include "surfaceboardlog.h"
#include "surfacesetup.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcinputprofile.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "patchundo.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

namespace
{
QString keyOf(const QString &device)
{
    return QString(device.trimmed()).replace(QLatin1Char('/'), QLatin1Char('_'))
                                    .replace(QLatin1Char('\\'), QLatin1Char('_'));
}
const QString kLastUniverse = QStringLiteral("surfaces/lastUniverse/");
const QString kDeclined = QStringLiteral("surfaces/declined/");
const QString kKeepPatch = QStringLiteral("surfaces/keepPatch/");
/* A port a network MIDI session has (macOS "Network Session 1", a custom
   "Network RTP Session 1", rtpMIDI's): never a board's own feedback. */
bool isNetworkSession(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("\\b(session|rtp)\\b|rtpmidi"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch();
}
}

SurfaceAutoPatch::SurfaceAutoPatch(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::pluginConfigurationChanged, this,
            [this](const QString &name, bool) { slotPluginChanged(name); });
    // Patched by hand: the device may be set up by itself again later.
    connect(iom, &InputOutputMap::inputPatchChanged, this, [this](quint32 universe) {
        InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
        if (ip != nullptr && ip->plugin() != nullptr && !m_patched.contains(universe))
            setDeclined(ip->inputName(), false);
    });
    connect(m_doc, &Doc::loaded, this, [this]() {
        snapshot();
        if (m_enabled)
            QTimer::singleShot(m_delayMs, this, &SurfaceAutoPatch::scanNow);
    });
    snapshot();
}

void SurfaceAutoPatch::snapshot()
{
    m_present.clear();
    if (m_doc->ioPluginCache() == nullptr)
        return;
    for (QLCIOPlugin *p : m_doc->ioPluginCache()->plugins())
        if (p != nullptr && p->linesAreHardware() && (p->capabilities() & QLCIOPlugin::Input))
        {
            const QStringList lines = p->inputs();
            m_present.insert(p->name(), QSet<QString>(lines.begin(), lines.end()));
        }
}

void SurfaceAutoPatch::setEnabled(bool on, bool scan)
{
    m_enabled = on;
    snapshot();
    if (on && scan)
        scanNow();
}

bool SurfaceAutoPatch::isDeclined(const QString &device)
{
    return QSettings().value(kDeclined + keyOf(device), false).toBool();
}

void SurfaceAutoPatch::setDeclined(const QString &device, bool declined)
{
    QSettings s;
    if (declined)
        s.setValue(kDeclined + keyOf(device), true);
    else
        s.remove(kDeclined + keyOf(device));
}

void SurfaceAutoPatch::slotPluginChanged(const QString &pluginName)
{
    QLCIOPlugin *plugin = m_doc->ioPluginCache() != nullptr ? m_doc->ioPluginCache()->plugin(pluginName) : nullptr;
    if (plugin == nullptr || !plugin->linesAreHardware() || !(plugin->capabilities() & QLCIOPlugin::Input))
        return;
    const QStringList lines = plugin->inputs();
    const QSet<QString> now(lines.begin(), lines.end());
    const QSet<QString> before = m_present.value(pluginName);
    m_present.insert(pluginName, now);
    if (now != before)
        emit devicesChanged();
    if (!m_enabled)
        return;
    for (const QString &device : now)
        if (!before.contains(device))
        {
            /* After a moment: the show's own patch (followInputDevices) or a
               person's goes first; then it is still free, or nothing is done. */
            QPointer<QLCIOPlugin> guard(plugin);
            QTimer::singleShot(m_delayMs, this, [this, guard, device]() {
                if (guard != nullptr)
                    consider(guard, device);
            });
        }
}

void SurfaceAutoPatch::scanNow()
{
    if (m_doc->ioPluginCache() == nullptr)
        return;
    for (QLCIOPlugin *p : m_doc->ioPluginCache()->plugins())
        if (p != nullptr && p->linesAreHardware() && (p->capabilities() & QLCIOPlugin::Input))
            for (const QString &device : p->inputs())
                consider(p, device);
}

bool SurfaceAutoPatch::known(QLCIOPlugin *plugin, const QString &device) const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->plugin() == plugin && ip->inputName() == device)
            return true;
        if (Universe *uni = iom->universe(u))
            for (const Universe::OfflinePatch &op : uni->offlinePatches())
                if (op.tag == Universe::InputPatchTag && op.plugin == plugin->name() && op.uid == device)
                    return true;
    }
    return false;
}

void SurfaceAutoPatch::consider(QLCIOPlugin *plugin, const QString &device)
{
    if (!m_enabled || device.trimmed().isEmpty() || !plugin->inputs().contains(device))
        return;
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (known(plugin, device))
    {
        // Already patched: its patch put right where it is wrong.
        for (quint32 u = 0; u < iom->universesCount(); ++u)
        {
            InputPatch *ip = iom->inputPatch(u);
            if (ip != nullptr && ip->plugin() == plugin && ip->inputName() == device)
                repair(u);
        }
        return;
    }
    if (isDeclined(device) || InputOutputMap::isNotSurface(device))
        return;
    const QString profile = iom->profileForDevice(device);
    if (profile.isEmpty())
    {
        const QString maybe = iom->profileToAskFor(device);
        if (!maybe.isEmpty() && !m_asked.contains(device))
        {
            m_asked.insert(device);
            emit askToUse(plugin->name(), device, maybe);
        }
        return;
    }
    setUp(plugin, device, profile);
}

quint32 SurfaceAutoPatch::freeUniverse(const QString &device)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    auto isFree = [iom](quint32 u, bool noOutputs) {
        Universe *uni = iom->universe(u);
        if (uni == nullptr)
            return false;
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->plugin() != nullptr)
            return false;
        if (iom->feedbackPatch(u) != nullptr && iom->feedbackPatch(u)->isPatched())
            return false;
        for (const Universe::OfflinePatch &op : uni->offlinePatches())
            if (op.tag != Universe::OutputPatchTag || noOutputs)
                return false;
        if (noOutputs && uni->outputPatchesCount() > 0)
            return false;
        return true;
    };
    bool ok = false;
    const quint32 last = QSettings().value(kLastUniverse + keyOf(device)).toUInt(&ok);
    if (ok && last < iom->universesCount() && isFree(last, false))
        return last;
    // An empty universe first (no DMX going out of it), else one with no input.
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        if (isFree(u, true))
            return u;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        if (isFree(u, false))
            return u;
    if (!iom->addUniverse())
        return UINT_MAX;
    return iom->universesCount() - 1;
}

quint32 SurfaceAutoPatch::setUp(QLCIOPlugin *plugin, const QString &device, const QString &profileIn, QString *why)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    const QString profile = profileIn.isEmpty() ? iom->profileForDevice(device) : profileIn;
    const int line = plugin != nullptr ? int(plugin->inputs().indexOf(device)) : -1;
    if (line < 0 || profile.isEmpty())
    {
        if (why != nullptr)
            *why = line < 0 ? tr("%1 is not connected").arg(device) : tr("no profile for %1").arg(device);
        return UINT_MAX;
    }
    const quint32 u = freeUniverse(device);
    if (u == UINT_MAX)
    {
        if (why != nullptr)
            *why = tr("no free universe");
        return UINT_MAX;
    }
    m_patched.insert(u, device);
    if (!iom->setInputPatch(u, plugin->name(), device, quint32(line), profile))
    {
        m_patched.remove(u);
        if (why != nullptr)
            *why = tr("%1 could not be opened").arg(device);
        return UINT_MAX;
    }
    const QString fb = iom->pairFeedback(u);
    QSettings().setValue(kLastUniverse + keyOf(device), u);
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
    {
        ss->boardLog()->note(u, tr("Recognised %1 as %2: patched on universe %3, feedback %4")
                                    .arg(device, profile).arg(u + 1)
                                    .arg(fb.isEmpty() ? tr("none (no output of that name)") : fb));
        // Its init now the feedback line is there too (it went once on the patch).
        ss->sendInit(u, tr("after the auto-patch"));
    }
    emit autoPatched(u, profile, device);
    emit devicesChanged();
    return u;
}

bool SurfaceAutoPatch::undo(quint32 universe)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom->inputPatch(universe);
    const QString device = m_patched.value(universe, ip != nullptr ? ip->inputName() : QString());
    if (device.isEmpty())
        return false;
    m_patched.remove(universe);
    OutputPatch *fb = iom->feedbackPatch(universe);
    if (fb != nullptr && fb->isPatched())
        iom->setOutputPatch(universe, KOutputNone, QString(), 0, true);
    iom->setInputPatch(universe, KInputNone, QString(), QLCIOPlugin::invalidLine());
    setDeclined(device, true);
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
        ss->boardLog()->note(universe, tr("Auto-patch of %1 undone: not set up by itself again").arg(device));
    emit devicesChanged();
    return true;
}

quint32 SurfaceAutoPatch::useAs(QLCIOPlugin *plugin, const QString &device, const QString &profile)
{
    InputOutputMap::setConfirmedProfile(device, profile);
    InputOutputMap::setNotSurface(device, false);
    setDeclined(device, false);
    InputOutputMap *iom = m_doc->inputOutputMap();
    // Already patched (by hand, with no profile): given the profile there.
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->plugin() == plugin && ip->inputName() == device)
        {
            setProfile(u, profile);
            return u;
        }
    }
    return setUp(plugin, device, profile);
}

QString SurfaceAutoPatch::repair(quint32 universe)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom->inputPatch(universe);
    QLCIOPlugin *plugin = ip != nullptr ? ip->plugin() : nullptr;
    if (plugin == nullptr || !plugin->linesAreHardware())
        return QString();
    const QString device = ip->inputName();
    if (device.trimmed().isEmpty() || !plugin->inputs().contains(device) || InputOutputMap::isNotSurface(device))
        return QString();
    const QString want = iom->profileForDevice(device);
    if (want.isEmpty())
        return QString();          // not a board this app knows
    if (QSettings().value(kKeepPatch + keyOf(device), false).toBool())
        return QString();          // its repair was undone: left as it is

    // The profile: none, not installed, or another board's.
    bool fixProfile = false;
    const QString cur = ip->profileName();
    if (cur != want)
    {
        QLCInputProfile *cp = cur.isEmpty() ? nullptr : iom->profile(cur);
        fixProfile = cp == nullptr || (!cp->deviceMatches().isEmpty() && cp->deviceMatchScore(device) == 0);
    }

    // The feedback: none, another board, a network session, or a port gone.
    bool fixFeedback = false;
    QString was;
    const InputOutputMap::FeedbackLine own = iom->feedbackLineFor(universe);
    if (!own.plugin.isEmpty())
    {
        OutputPatch *fb = iom->feedbackPatch(universe);
        if (fb == nullptr || !fb->isPatched() || fb->plugin() == nullptr)
            fixFeedback = true;
        else if (!(fb->pluginName() == own.plugin && fb->output() == own.line))
        {
            was = fb->outputName();
            const QStringList outs = fb->plugin()->outputs();
            const bool gone = int(fb->output()) >= outs.size() || outs.at(int(fb->output())) != was;
            const bool board = !was.isEmpty() && was != device && !iom->profileForDevice(was).isEmpty();
            fixFeedback = gone || board || isNetworkSession(was);
        }
    }
    if (!fixProfile && !fixFeedback)
        return QString();

    QStringList what;
    if (fixFeedback)
        what << (was.isEmpty() ? tr("feedback set to its own port") : tr("feedback repaired to its own port"));
    if (fixProfile)
        what << tr("profile set to %1").arg(want);
    const QString text = what.join(tr(", "));
    const QString summary = tr("repair %1 on universe %2").arg(want).arg(universe + 1);
    if (PatchUndo *pu = iom->patchUndo())
    {
        pu->capture(QList<quint32>() << universe, summary);
        m_repairUndo.insert(universe, summary);
    }
    if (fixProfile)
        iom->setInputProfile(universe, want);
    if (fixFeedback)
        iom->setOutputPatch(universe, own.plugin, QString(), own.line, true, 0);
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
    {
        ss->boardLog()->note(universe, tr("Repaired %1: %2%3").arg(want, text,
                                 was.isEmpty() ? QString() : tr(" (it was %1)").arg(was)));
        ss->sendInit(universe, tr("after the repair"));
    }
    emit repaired(universe, want, text);
    emit devicesChanged();
    return text;
}

bool SurfaceAutoPatch::undoRepair(quint32 universe)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    PatchUndo *pu = iom->patchUndo();
    const QString summary = m_repairUndo.take(universe);
    if (pu == nullptr || summary.isEmpty() || !pu->canUndo() || pu->summary() != summary)
        return false;
    if (!pu->undo())
        return false;
    InputPatch *ip = iom->inputPatch(universe);
    const QString device = ip != nullptr ? ip->inputName() : QString();
    if (!device.isEmpty())
        QSettings().setValue(kKeepPatch + keyOf(device), true);
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
        ss->boardLog()->note(universe, tr("Repair undone: this patch is left as it is on this computer"));
    emit devicesChanged();
    return true;
}

bool SurfaceAutoPatch::setFeedback(quint32 universe, const QString &output)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom->inputPatch(universe);
    if (output.isEmpty())
    {
        iom->setOutputPatch(universe, KOutputNone, QString(), 0, true);
        if (SurfaceSetup *ss = m_doc->surfaceSetup())
            ss->boardLog()->note(universe, tr("Feedback: none"));
        return true;
    }
    if (ip == nullptr || ip->plugin() == nullptr)
        return false;
    const int line = int(ip->plugin()->outputs().indexOf(output));
    if (line < 0)
        return false;
    const bool ok = iom->setOutputPatch(universe, ip->plugin()->name(), output, quint32(line), true);
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
    {
        ss->boardLog()->note(universe, tr("Feedback: %1").arg(output));
        ss->sendInit(universe, tr("after its feedback line was set"));
    }
    return ok;
}

bool SurfaceAutoPatch::setProfile(quint32 universe, const QString &profile)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom->inputPatch(universe);
    if (ip == nullptr || ip->plugin() == nullptr)
        return false;
    if (ip->profileName() == profile)
    {
        if (SurfaceSetup *ss = m_doc->surfaceSetup())
            ss->sendInit(universe, tr("profile chosen again"));
        return true;
    }
    // The profile change sends the init (SurfaceSetup follows profileChanged).
    iom->setInputProfile(universe, profile);
    if (iom->feedbackPatch(universe) == nullptr || !iom->feedbackPatch(universe)->isPatched())
        iom->pairFeedback(universe);
    return true;
}
