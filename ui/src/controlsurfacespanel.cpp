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

#include "controlsurfacespanel.h"
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
#include "qlcioplugin.h"
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
    { "pmj",     "surfacePmj" },
    { "web",     "surfaceCompanion" },
    { "pads",    "surfacePads" },
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
          { tr("Speed masters and Grand Master"),
            tr("A fader or button for a master."),
            tr("Set Up…"),
            tr("Open Speed Masters: each master has an Inputs… button. The Grand Master's input is in "
               "Virtual Console ▸ Properties ▸ Grand Master.") } },
        { QStringLiteral("pmj"),
          { tr("PMJ Black 1"),
            tr("Its faders, encoders and buttons work by themselves once it is patched with the PMJ Black 1 profile."),
            tr("Set Up…"),
            tr("Go to the patch: patch the PMJ as an input with the profile PMJ Black 1 and its feedback line.") } },
        { QStringLiteral("web"),
          { tr("Companion and Stream Deck"),
            tr("A Stream Deck through Bitfocus Companion, over this show's web access."),
            tr("Set Up…"),
            tr("Open Web access: turn it on, then add the qlcconsole module in Companion.") } },
        { QStringLiteral("pads"),
          { tr("Game pads"),
            tr("Pad Run and the focus tools from a game controller."),
            tr("Set Up…"),
            tr("Go to the patch: right-click the pad for its test and jobs.") } },
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
        QPushButton *btn = new QPushButton(t.button, this);
        btn->setObjectName(QString::fromLatin1(row.object) + QStringLiteral("SetUp"));
        btn->setToolTip(t.tip);
        btn->setAccessibleName(tr("Set up %1").arg(t.name));
        connect(btn, &QPushButton::clicked, this, [this, key]() { emit setUpRequested(key); });
        grid->addWidget(what, r, 0);
        grid->addWidget(status, r, 1);
        grid->addWidget(btn, r, 2, Qt::AlignTop);
        m_status.insert(key, status);
        m_buttons.insert(key, btn);
        ++r;
    }
    grid->setRowStretch(r, 1);

    ControlMap *cm = m_doc->controlMap();
    connect(cm, &ControlMap::bindingsChanged, this, &ControlSurfacesPanel::refresh);
    connect(cm, &ControlMap::stopStateChanged, this, &ControlSurfacesPanel::refresh);
    connect(ShowFeatures::instance(), &ShowFeatures::changed, this, &ControlSurfacesPanel::refresh);
    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::inputPatchChanged, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::universeAdded, this, &ControlSurfacesPanel::refresh);
    connect(iom, &InputOutputMap::universeRemoved, this, &ControlSurfacesPanel::refresh);
    connect(m_doc->speedMasters(), &SpeedMasters::changed, this, &ControlSurfacesPanel::refresh);
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
    return triggersText() + QStringLiteral(" · ") + stopText();
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
    QString text = n == 0 ? tr("Speed masters: no input") : tr("Speed masters: %1 inputs").arg(n);
    VirtualConsole *vc = VirtualConsole::instance();
    const bool gm = vc != nullptr
            && vc->properties().grandMasterInputUniverse() != InputOutputMap::invalidUniverse();
    return text + QStringLiteral("\n") + (gm ? tr("Grand Master: input set") : tr("Grand Master: no input"));
}

QString ControlSurfacesPanel::pmjText() const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->profile() != nullptr
            && ip->profile()->name().contains(QLatin1String("PMJ")))
            return tr("Patched on universe %1").arg(u + 1);
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

QString ControlSurfacesPanel::padsText() const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    int n = 0;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->plugin() != nullptr
            && (!ip->plugin()->inputDeviceKind(ip->input()).isEmpty()
                || !InputOutputMap::channelRoles(ip->plugin(), ip->input()).isEmpty()))
            ++n;
    }
    if (n == 0)
        return tr("No pad connected");
    return n == 1 ? tr("1 pad connected") : tr("%1 pads connected").arg(n);
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
    m_status["pmj"]->setText(pmjText());
    m_status["web"]->setText(webText());
    m_status["pads"]->setText(padsText());
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
