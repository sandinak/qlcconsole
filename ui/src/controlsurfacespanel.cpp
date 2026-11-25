/*
  Q Light Controller Plus - qlcconsole
  controlsurfacespanel.cpp

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

#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QShowEvent>

#include "controlsurfacespanel.h"
#include "controlowners.h"
#include "virtualconsole.h"
#include "vcwidget.h"
#include "vcframe.h"
#include "vcproperties.h"
#include "qlcinputprofile.h"
#include "qlcinputsource.h"
#include "inputoutputmap.h"
#include "showwebaccess.h"
#include "speedmasters.h"
#include "showfeatures.h"
#include "encoderbank.h"
#include "controlmap.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include "universe.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "pmjoverlay.h"
#include "doc.h"

namespace
{
struct Row
{
    const char *key;
    const char *object;
};
const Row kRows[] = {
    { "map",     "surfaceMap" },
    { "estop",   "surfaceEmergencyStop" },
    { "encoder", "surfaceEncoderBank" },
    { "vc",      "surfaceVirtualConsole" },
    { "masters", "surfaceMasters" },
    { "gm",      "surfaceGrandMaster" },
    { "pmj",     "surfacePmj" },
    { "web",     "surfaceCompanion" },
    { "pads",    "surfacePads" },
    { "devices", "surfaceDevices" },
    { "collisions", "surfaceCollisions" },
    { "places",  "surfacePlaces" },
};
}

ControlSurfacesPanel::ControlSurfacesPanel(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("controlSurfacesPanel"));
    setProperty("helpTopic", QStringLiteral("control-surfaces.md"));

    struct Text { QString name; QString what; QString button; QString tip; };
    const QMap<QString, Text> text = {
        { QStringLiteral("map"),
          { tr("Triggers (Control Map)"),
            tr("One button or pad fires one thing: a scene, an effect, a cue list, a cue or a hit."),
            tr("Set Up…"),
            tr("Open the Control Map table. To add a trigger, right-click a scene, effect, cue list, "
               "cue or hit and choose Learn Trigger…, then press the control.") } },
        { QStringLiteral("estop"),
          { tr("Emergency stop"),
            tr("One button that is ALL OFF, from Design or Operate, at any moment."),
            tr("Set Up…"),
            tr("Open Emergency Stop… under the Control Map table: press the button, release it, press it again.") } },
        { QStringLiteral("encoder"),
          { tr("Encoder bank"),
            tr("The endless encoders of a MIDI controller turn the Workbench selection in Design."),
            tr("Set Up…"),
            tr("Open the Encoder bank section under the Control Map table.") } },
        { QStringLiteral("vc"),
          { tr("Virtual Console widgets"),
            tr("A button, slider or cue list on the Virtual Console that a control drives (a fader that sets a level)."),
            tr("Set Up…"),
            tr("Open the Virtual Console. Right-click a widget, choose Properties, then External Input.") } },
        { QStringLiteral("masters"),
          { tr("Speed masters"),
            tr("A fader or button for a speed master: tap, rate or pause."),
            tr("Set Up…"),
            tr("Open Speed Masters: each master has an Inputs… button.") } },
        { QStringLiteral("gm"),
          { tr("Grand Master"),
            tr("One fader for the level of the whole show."),
            tr("Set Up…"),
            tr("Open the Virtual Console's Settings on the Grand Master page: press Choose or "
               "Auto Detect, then move the fader.") } },
        { QStringLiteral("pmj"),
          { tr("PMJ Black 1"),
            tr("Its faders, encoders and buttons work by themselves once it is patched with the PMJ Black 1 profile."),
            tr("Set Up…"),
            tr("Go to the patch: patch the PMJ as an input with the profile PMJ Black 1, and its "
               "output as the feedback line so its lamps light.") } },
        { QStringLiteral("web"),
          { tr("Companion and Stream Deck"),
            tr("A Stream Deck through Bitfocus Companion, over this show's web access."),
            tr("Set Up…"),
            tr("Open Web access: turn it on, then add the qlcconsole module in Companion.") } },
        { QStringLiteral("pads"),
          { tr("Game pads"),
            tr("Pad Run and the focus tools from a game controller."),
            tr("Set Up…"),
            tr("Go to the patch: patch the pad to a universe's input, then right-click it for its "
               "test and jobs.") } },
        { QStringLiteral("devices"),
          { tr("Other connected devices"),
            tr("A MIDI or USB controller that is plugged in but no universe's input has. Nothing "
               "hears it until it is patched."),
            tr("Set Up…"),
            tr("Go to the patch: patch the device to a universe's input, with its profile.") } },
        { QStringLiteral("collisions"),
          { tr("Who has it now"),
            tr("A control that two of the above listen to does two jobs at once. One job per control: "
               "each line says where to fix it."),
            QString(), QString() } },
        { QStringLiteral("places"),
          { tr("Triggers by place"),
            tr("A control can fire one thing in a cue and another everywhere else: the narrowest "
               "live place has it. Each control with a trigger at a place, and which one has it now."),
            QString(), QString() } },
    };

    QGridLayout *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);
    m_featureOff = new QLabel(tr("Control surfaces are off for this show: triggers and the encoder "
                                 "bank are put away. Turn them on in View ▸ Workspace ▸ Tabs & "
                                 "Features…. The emergency stop is always here."), this);
    m_featureOff->setObjectName(QStringLiteral("surfacesFeatureOff"));
    m_featureOff->setWordWrap(true);
    m_featureOff->setStyleSheet(QStringLiteral("color: gray;"));
    grid->addWidget(m_featureOff, 0, 0, 1, 3);

    int r = 1;
    for (const Row &row : kRows)
    {
        const QString key = QString::fromLatin1(row.key);
        const Text &t = text.value(key);
        QLabel *what = new QLabel(QStringLiteral("<b>%1</b><br>%2").arg(t.name.toHtmlEscaped(), t.what.toHtmlEscaped()), this);
        what->setObjectName(QString::fromLatin1(row.object) + QStringLiteral("What"));
        what->setWordWrap(true);
        what->setTextFormat(Qt::RichText);
        QLabel *status = new QLabel(this);
        status->setObjectName(QString::fromLatin1(row.object) + QStringLiteral("Status"));
        status->setWordWrap(true);
        grid->addWidget(what, r, 0);
        m_status.insert(key, status);
        m_what.insert(key, what);
        // Who has it now is read only: each line says where its fix is.
        if (t.button.isEmpty())
        {
            grid->addWidget(status, r, 1, 1, 2);
            ++r;
            continue;
        }
        QPushButton *btn = new QPushButton(t.button, this);
        btn->setObjectName(QString::fromLatin1(row.object) + QStringLiteral("SetUp"));
        btn->setToolTip(t.tip);
        btn->setAccessibleName(tr("Set up %1").arg(t.name));
        connect(btn, &QPushButton::clicked, this, [this, key]() { emit setUpRequested(key); });
        grid->addWidget(status, r, 1);
        grid->addWidget(btn, r, 2, Qt::AlignTop);
        m_buttons.insert(key, btn);
        ++r;
    }
    grid->setRowStretch(r, 1);

    ControlMap *cm = m_doc->controlMap();
    connect(cm, &ControlMap::bindingsChanged, this, &ControlSurfacesPanel::refresh);
    connect(cm, &ControlMap::stopStateChanged, this, &ControlSurfacesPanel::refresh);
    /* Who has a control now follows the GO, the mode and the selection
       (Q296): only that row, and only while it is shown (a light pass comes
       with every GO and function start; showEvent() reads everything). */
    connect(cm, &ControlMap::lampsChanged, this, [this]() {
        if (isVisible())
            refreshPlaces();
    });
    connect(ShowFeatures::instance(), &ShowFeatures::changed, this, &ControlSurfacesPanel::refresh);
    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::inputPatchChanged, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::profileChanged, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::universeAdded, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::universeRemoved, this, &ControlSurfacesPanel::refresh);
    // A device plugged in or pulled out: what is connected and set up changed.
    connect(iom, &InputOutputMap::pluginConfigurationChanged, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::inputDeviceDisconnected, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::offlinePatchesChanged, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::inputSlotChanged, this, &ControlSurfacesPanel::refresh);
    /* The PMJ line says whether a feedback line is patched, and a device
       used as an output or feedback is not listed as one waiting to be set
       up: follow each universe's output and feedback patches. */
    auto watch = [this](Universe *u) {
        if (u == nullptr)
            return;
        connect(u, &Universe::hasFeedbackChanged, this, &ControlSurfacesPanel::refresh,
                Qt::UniqueConnection);
        connect(u, &Universe::outputPatchChanged, this, &ControlSurfacesPanel::refresh,
                Qt::UniqueConnection);
        connect(u, &Universe::outputPatchesCountChanged, this, &ControlSurfacesPanel::refresh,
                Qt::UniqueConnection);
    };
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        watch(iom->universe(u));
    connect(iom, &InputOutputMap::universeAdded, this, [iom, watch](quint32 id) { watch(iom->universe(id)); });
    connect(m_doc->speedMasters(), &SpeedMasters::changed, this, &ControlSurfacesPanel::refresh);
    connect(cm->encoderBank(), &EncoderBank::deviceChanged, this, &ControlSurfacesPanel::refresh);
    connect(m_doc->webAccess(), &ShowWebAccess::settingsChanged, this, &ControlSurfacesPanel::refresh);
    connect(m_doc->webAccess(), &ShowWebAccess::statusChanged, this, &ControlSurfacesPanel::refresh);
    refresh();
}

QStringList ControlSurfacesPanel::keys()
{
    QStringList k;
    for (const Row &row : kRows)
        k << QString::fromLatin1(row.key);
    return k;
}

QString ControlSurfacesPanel::status(const QString &key) const
{
    return m_status.contains(key) ? m_status.value(key)->text() : QString();
}

QString ControlSurfacesPanel::summary() const
{
    QString text = triggersText() + QStringLiteral(" · ") + stopText();
    const int n = ControlOwners::collisions(m_doc).count();
    if (n > 0)
        text += QStringLiteral(" · ") + (n == 1 ? tr("1 control with two jobs")
                                                : tr("%1 controls with two jobs").arg(n));
    return text;
}

QString ControlSurfacesPanel::placesText() const
{
    ControlMap *cm = m_doc->controlMap();
    // Each control with a trigger at a place, or with more than one trigger.
    QList<QPair<quint32, quint32>> controls;
    for (const ControlMap::Binding &b : cm->bindings())
    {
        if (b.target.type == ControlMap::EmergencyStopTarget)
            continue;
        const QPair<quint32, quint32> c(b.universe, b.channel);
        if (controls.contains(c))
            continue;
        const QList<ControlMap::Binding> on = cm->bindingsOn(b.universe, b.channel);
        bool placed = on.count() > 1;
        for (const ControlMap::Binding &o : on)
            placed |= o.place.scope != ControlMap::Everywhere;
        if (placed)
            controls << c;
    }
    QStringList lines;
    for (const QPair<quint32, quint32> &c : std::as_const(controls))
    {
        const QString line = tr("%1: %2.").arg(cm->controlName(c.first, c.second),
                                               cm->placesText(cm->bindingsOn(c.first, c.second)));
        const ControlMap::Binding *now = cm->bindingOn(c.first, c.second);
        lines << (now != nullptr ? tr("%1 Now: %2 (%3).").arg(line, cm->targetName(now->target),
                                                              cm->placeName(now->place))
                                 : tr("%1 Now: nothing (no place of it is live).").arg(line));
    }
    // A trigger whose place was deleted (widened) or is from a newer version: said.
    lines << cm->placeNotes();
    return lines.join(QStringLiteral("\n"));
}

void ControlSurfacesPanel::refreshPlaces()
{
    // Triggers by place: there only while a trigger has a place (or a control has two).
    const QString places = ShowFeatures::isOn(ShowFeatures::ControlSurfaces) ? placesText() : QString();
    m_status["places"]->setText(places);
    m_status["places"]->setVisible(!places.isEmpty());
    m_what["places"]->setVisible(!places.isEmpty());
}

QString ControlSurfacesPanel::collisionsText() const
{
    const QList<ControlOwners::Collision> all = ControlOwners::collisions(m_doc);
    if (all.isEmpty())
        return tr("Every control has one job");
    QStringList lines;
    for (const ControlOwners::Collision &c : all)
        lines << ControlOwners::collisionText(m_doc, c);
    return lines.join(QStringLiteral("\n"));
}

void ControlSurfacesPanel::showEvent(QShowEvent *event)
{
    // A Virtual Console input says nothing when it changes: read it again.
    refresh();
    QWidget::showEvent(event);
}

QString ControlSurfacesPanel::triggersText() const
{
    ControlMap *cm = m_doc->controlMap();
    const int n = cm->count() - (cm->binding(ControlMap::emergencyStop()) != nullptr ? 1 : 0);
    if (n <= 0)
        return tr("No triggers learned");
    return n == 1 ? tr("1 trigger learned") : tr("%1 triggers learned").arg(n);
}

QString ControlSurfacesPanel::stopText() const
{
    ControlMap *cm = m_doc->controlMap();
    const ControlMap::Binding *stop = cm->binding(ControlMap::emergencyStop());
    if (stop == nullptr)
        return tr("Emergency stop: not learned");
    const QString problem = cm->stopProblem();
    const QString on = cm->controlName(stop->universe, stop->channel);
    return problem.isEmpty() ? tr("Emergency stop: %1").arg(on)
                             : tr("Emergency stop: %1 %2, learn it again on a button").arg(on, problem);
}

QString ControlSurfacesPanel::encoderText() const
{
    return m_doc->controlMap()->encoderBank()->isEnabled() ? tr("Encoder bank: on")
                                                           : tr("Encoder bank: not set up");
}

QString ControlSurfacesPanel::vcText() const
{
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc == nullptr || vc->contents() == nullptr)
        return tr("No widget has an input");
    int n = 0;
    const QList<VCWidget *> widgets = vc->contents()->findChildren<VCWidget *>();
    for (VCWidget *w : widgets)
    {
        const QSharedPointer<QLCInputSource> src = w->inputSource(0);
        if (!src.isNull() && src->isValid())
            ++n;
    }
    if (n == 0)
        return tr("No widget has an input");
    return n == 1 ? tr("1 widget has an input") : tr("%1 widgets have an input").arg(n);
}

QString ControlSurfacesPanel::mastersText() const
{
    SpeedMasters *sm = m_doc->speedMasters();
    int n = 0;
    for (int i = 0; i < SpeedMasters::MaxCount; ++i)
        for (int c = 0; c < SpeedMasters::ControlCount; ++c)
            n += sm->input(i, SpeedMasters::Control(c), nullptr, nullptr) ? 1 : 0;
    return n == 0 ? tr("Speed masters: no input") : tr("Speed masters: %1 inputs").arg(n);
}

QString ControlSurfacesPanel::grandMasterText() const
{
    VirtualConsole *vc = VirtualConsole::instance();
    const bool gm = vc != nullptr
            && vc->properties().grandMasterInputUniverse() != InputOutputMap::invalidUniverse();
    return gm ? tr("Grand Master: input set") : tr("Grand Master: no input");
}

QString ControlSurfacesPanel::pmjText() const
{
    /* The overlay hears only a universe whose input has exactly the PMJ
       Black 1 profile (PMJOverlay::isPmjUniverse()): another profile with
       "PMJ" in its name is not the board, and is not said to be. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || ip->profileName().compare(PMJOverlay::deviceId, Qt::CaseInsensitive) != 0)
            continue;
        return iom->feedbackPatch(u) != nullptr
                ? tr("Patched on universe %1, with its feedback line").arg(u + 1)
                : tr("Patched on universe %1, no feedback line: its lamps stay dark until its "
                     "output is patched as feedback").arg(u + 1);
    }
    return tr("Not patched (no input has the PMJ Black 1 profile)");
}

QString ControlSurfacesPanel::webText() const
{
    const ShowWebAccess *web = m_doc->webAccess();
    switch (web->state())
    {
        case ShowWebAccess::Running:
        case ShowWebAccess::CommandLine:
            return tr("Web access on (port %1)").arg(web->port());
        case ShowWebAccess::Failed:
            return tr("Web access could not start (port %1)").arg(web->port());
        default:
            return web->isEnabled() ? tr("Web access starting (port %1)").arg(web->port())
                                    : tr("Companion: web access off");
    }
}

namespace
{
/** Whether line @p line of @p plugin is a game controller. */
bool isPad(QLCIOPlugin *plugin, quint32 line)
{
    return !plugin->inputDeviceKind(line).isEmpty()
        || !InputOutputMap::channelRoles(plugin, line).isEmpty();
}

/** Whether input line @p name of @p plugin is the input side of a DMX
    interface (an Enttec DMX USB Pro, a Peperoni, a uDMX, an FX5 on HID):
    an output dongle, not a control surface. */
bool isDmxInterface(QLCIOPlugin *plugin, const QString &name)
{
    const QString p = plugin->name();
    if (p == QLatin1String("DMX USB") || p == QLatin1String("Peperoni") || p == QLatin1String("uDMX"))
        return true;
    // HID: a joystick or pad is input only; a line that is also an output is a DMX interface.
    return p == QLatin1String("HID") && plugin->outputs().contains(name);
}

/** A software MIDI port, not a controller: the ALSA "Midi Through" loopback
    (every Linux box and the Pi), macOS IAC Driver buses, PipeWire and JACK
    bridges, and the RtMidi / loopMIDI virtual ports on Windows. One list. */
bool isVirtualPort(const QString &name)
{
    static const QStringList kVirtual = {
        QStringLiteral("Midi Through"), QStringLiteral("IAC Driver"),
        QStringLiteral("PipeWire"), QStringLiteral("JACK"),
        QStringLiteral("RtMidi"), QStringLiteral("loopMIDI"),
    };
    for (const QString &v : kVirtual)
        if (name.contains(v, Qt::CaseInsensitive))
            return true;
    return false;
}

/** Whether a line named @p name of @p plugin is patched as some universe's
    output or feedback: a device in use (a MIDI box that only gets feedback). */
bool usedAsOutput(InputOutputMap *iom, QLCIOPlugin *plugin, const QString &name)
{
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        for (int i = 0; i < iom->outputPatchesCount(u); ++i)
        {
            const OutputPatch *op = iom->outputPatch(u, i);
            if (op != nullptr && op->plugin() == plugin && op->outputName() == name)
                return true;
        }
        const OutputPatch *fb = iom->feedbackPatch(u);
        if (fb != nullptr && fb->plugin() == plugin && fb->outputName() == name)
            return true;
    }
    return false;
}
}

QList<QPair<QString, bool>> ControlSurfacesPanel::unpatchedDevices() const
{
    /* Every hardware input line plugged in now that no universe's input
       has (InputOutputMap::hasFreeInputDevice(): by device, so one of two
       pads of one name is told apart), once per free device. */
    QList<QPair<QString, bool>> out;
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (m_doc->ioPluginCache() == nullptr)
        return out;
    for (QLCIOPlugin *plugin : m_doc->ioPluginCache()->plugins())
    {
        if (plugin == nullptr || !plugin->linesAreHardware()
            || (plugin->capabilities() & QLCIOPlugin::Input) == 0)
            continue;
        const QStringList lines = plugin->inputs();
        QHash<QString, int> seen;
        for (int l = 0; l < lines.size(); ++l)
        {
            const QString &name = lines.at(l);
            if (name.isEmpty() || isVirtualPort(name))
                continue;
            // A DMX interface's input, or a device used as an output or
            // feedback line, is not a control surface waiting to be set up.
            const bool pad = isPad(plugin, quint32(l));
            if (!pad && (isDmxInterface(plugin, name) || usedAsOutput(iom, plugin, name)))
                continue;
            // The n-th line of a name is free when n of that name are.
            int taken = 0;
            for (quint32 u = 0; u < iom->universesCount(); ++u)
            {
                InputPatch *ip = iom->inputPatch(u);
                if (ip != nullptr && ip->plugin() == plugin && ip->inputName() == name)
                    ++taken;
            }
            const int nth = seen.value(name) + 1;
            seen.insert(name, nth);
            if (nth > taken && iom->hasFreeInputDevice(plugin, name))
                out << qMakePair(name, pad);
        }
    }
    return out;
}

QString ControlSurfacesPanel::padsText() const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    int n = 0;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->plugin() != nullptr && isPad(ip->plugin(), ip->input()))
            ++n;
    }
    QStringList lines;
    if (n > 0)
        lines << (n == 1 ? tr("1 pad connected and set up") : tr("%1 pads connected and set up").arg(n));
    // One plugged in that no universe has: connected, and said so.
    for (const QPair<QString, bool> &d : unpatchedDevices())
        if (d.second)
            lines << tr("%1: connected, not set up").arg(d.first);
    if (lines.isEmpty())
        return tr("No pad connected");
    return lines.join(QStringLiteral("\n"));
}

QString ControlSurfacesPanel::devicesText() const
{
    QStringList lines;
    for (const QPair<QString, bool> &d : unpatchedDevices())
        if (!d.second)
            lines << tr("%1: not set up").arg(d.first);
    if (lines.isEmpty())
        return tr("Every connected device is set up");
    return lines.join(QStringLiteral("\n"));
}

void ControlSurfacesPanel::refresh()
{
    const bool on = ShowFeatures::isOn(ShowFeatures::ControlSurfaces);
    m_featureOff->setVisible(!on);
    const QString off = tr("Off for this show");
    m_status["map"]->setText(on ? triggersText() : off);
    m_status["estop"]->setText(stopText());
    m_status["encoder"]->setText(on ? encoderText() : off);
    m_status["vc"]->setText(vcText());
    m_status["masters"]->setText(mastersText());
    m_status["gm"]->setText(grandMasterText());
    m_status["pmj"]->setText(pmjText());
    m_status["web"]->setText(webText());
    m_status["pads"]->setText(padsText());
    m_status["devices"]->setText(devicesText());
    // Other connected devices is there only while one is waiting to be set up.
    bool waiting = false;
    for (const QPair<QString, bool> &d : unpatchedDevices())
        waiting = waiting || !d.second;
    m_status["devices"]->setVisible(waiting);
    m_what["devices"]->setVisible(waiting);
    m_buttons["devices"]->setVisible(waiting);
    m_status["collisions"]->setText(collisionsText());
    // Who has it now is there only while a control has two jobs.
    const bool clash = !ControlOwners::collisions(m_doc).isEmpty();
    m_status["collisions"]->setVisible(clash);
    m_what["collisions"]->setVisible(clash);
    refreshPlaces();
    // Triggers and the encoder bank are put away; the stop and the rest stay.
    m_buttons["map"]->setEnabled(on);
    m_buttons["encoder"]->setEnabled(on);
    const QString now = summary();
    if (now != m_lastSummary)
    {
        m_lastSummary = now;
        emit summaryChanged();
    }
}
