/*
  Q Light Controller Plus
  showcontrolsyncpage.cpp

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

#include <QNetworkInterface>
#include <QScrollArea>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>

#include "showcontrolsyncpage.h"
#include "showcontrollistener.h"
#include "timecodesource.h"
#include "inputoutputmap.h"
#include "showcontrol.h"
#include "inputpatch.h"
#include "doc.h"

namespace
{
/** Hour:minute:second:frame at @a fps. */
QString timecodeText(quint32 ms, int fps)
{
    if (fps <= 0)
        fps = 30;
    const quint32 sec = ms / 1000;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(sec / 3600, 2, 10, QChar('0'))
        .arg((sec % 3600) / 60, 2, 10, QChar('0'))
        .arg(sec % 60, 2, 10, QChar('0'))
        .arg(int((ms % 1000) * quint32(fps) / 1000), 2, 10, QChar('0'));
}

/** A universe's name, or "Universe N" when it has none. */
QString universeName(InputOutputMap *iom, quint32 index)
{
    const QString name = iom->getUniverseNameByIndex(index);
    return name.isEmpty() ? ShowControlSyncPage::tr("Universe %1").arg(index + 1) : name;
}

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    QLabel *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("color: gray;"));
    return l;
}

const char *kGreen = "color: #27ae60;";
const char *kAmber = "color: #e67e22;";
const char *kRed   = "color: #c0392b;";
const char *kGrey  = "color: gray;";
}

ShowControlSyncPage::ShowControlSyncPage(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_updating(false)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("connectionsShowControl"));
    setProperty("helpTopic", QStringLiteral("show-control.md"));

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    QWidget *content = new QWidget(scroll);
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->addWidget(noteLabel(tr("Everything outside that controls or clocks the show: cue "
                                "commands from QLab (or anything that sends OSC or MIDI Show "
                                "Control) and MIDI timecode. Changes take effect at once."),
                             content));
    lay->addWidget(buildOscGroup());
    lay->addWidget(buildMscGroup());
    lay->addWidget(buildMtcGroup());
    lay->addWidget(buildQLabOutGroup());
    lay->addStretch();
    scroll->setWidget(content);

    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::universeAdded, this, &ShowControlSyncPage::refreshMidiInputs);
    connect(iom, &InputOutputMap::universeRemoved, this, &ShowControlSyncPage::refreshMidiInputs);
    connect(iom, &InputOutputMap::pluginConfigurationChanged,
            this, &ShowControlSyncPage::refreshMidiInputs);

    TimecodeSource *tc = m_doc->timecodeSource();
    connect(tc, &TimecodeSource::timeChanged, this, &ShowControlSyncPage::slotTimeChanged);
    connect(tc, &TimecodeSource::runningChanged, this, &ShowControlSyncPage::refreshTimecode);
    connect(tc, &TimecodeSource::sourceUniverseChanged,
            this, &ShowControlSyncPage::refreshMidiInputs);

    setListener(nullptr);
    refreshMidiInputs();
}

QGroupBox *ShowControlSyncPage::buildOscGroup()
{
    QGroupBox *box = new QGroupBox(tr("OSC in"), this);
    box->setObjectName(QStringLiteral("scOscGroup"));
    QFormLayout *form = new QFormLayout(box);

    QHBoxLayout *row = new QHBoxLayout();
    m_oscCheck = new QCheckBox(tr("Listen for OSC cue commands on port"), box);
    m_oscCheck->setObjectName(QStringLiteral("scOscEnabled"));
    m_oscCheck->setToolTip(tr("QLab's Network cue (or anything that sends OSC) can GO a cue: "
                              "/go, /cue/12/start, /subgo, /stop... Anyone on the network can, so it's off "
                              "until you turn it on."));
    m_oscPortSpin = new QSpinBox(box);
    m_oscPortSpin->setObjectName(QStringLiteral("scOscPort"));
    m_oscPortSpin->setRange(1, 65535);
    // Bind once the number is finished, not on every digit typed.
    m_oscPortSpin->setKeyboardTracking(false);
    m_oscPortSpin->setToolTip(tr("Not 53000 or 53001 if QLab runs on this computer: it uses those."));
    row->addWidget(m_oscCheck);
    row->addWidget(m_oscPortSpin);
    row->addStretch();
    form->addRow(row);

    QStringList addresses;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
            addresses << a.toString();
    QLabel *where = noteLabel(addresses.isEmpty()
        ? tr("Send to this computer as localhost (127.0.0.1).")
        : tr("Send to this computer at %1 (or 127.0.0.1 on this computer).")
              .arg(addresses.join(tr(" or "))), box);
    where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(where);

    m_oscStatus = new QLabel(box);
    m_oscStatus->setObjectName(QStringLiteral("scOscStatus"));
    m_oscStatus->setWordWrap(true);
    form->addRow(tr("Status:"), m_oscStatus);

    m_oscLast = new QLabel(box);
    m_oscLast->setObjectName(QStringLiteral("scOscLast"));
    m_oscLast->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_oscLast->setWordWrap(true);
    form->addRow(tr("Last received:"), m_oscLast);

    connect(m_oscCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating && m_listener)
            m_listener->setOscEnabled(on);
    });
    connect(m_oscPortSpin, &QSpinBox::valueChanged, this, [this](int port) {
        if (!m_updating && m_listener)
            m_listener->setOscPort(port);
    });
    return box;
}

QGroupBox *ShowControlSyncPage::buildMscGroup()
{
    QGroupBox *box = new QGroupBox(tr("MIDI Show Control in"), this);
    box->setObjectName(QStringLiteral("scMscGroup"));
    QFormLayout *form = new QFormLayout(box);

    QHBoxLayout *row = new QHBoxLayout();
    m_mscCheck = new QCheckBox(tr("Answer MIDI Show Control as device"), box);
    m_mscCheck->setObjectName(QStringLiteral("scMscEnabled"));
    m_mscCheck->setToolTip(tr("MSC GO, STOP, RESUME, LOAD, ALL_OFF, RESET and GO_OFF arriving on "
                              "a patched MIDI input, for lighting, moving lights or all types. "
                              "The all-call (127) is always answered."));
    m_mscDeviceSpin = new QSpinBox(box);
    m_mscDeviceSpin->setObjectName(QStringLiteral("scMscDevice"));
    m_mscDeviceSpin->setRange(0, 0x6F);
    m_mscDeviceSpin->setKeyboardTracking(false);
    m_mscDeviceSpin->setToolTip(tr("The device number QLab's MIDI cue sends to."));
    row->addWidget(m_mscCheck);
    row->addWidget(m_mscDeviceSpin);
    row->addStretch();
    form->addRow(row);

    m_mscInputCombo = new QComboBox(box);
    m_mscInputCombo->setObjectName(QStringLiteral("scMscInput"));
    m_mscInputCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_mscInputCombo->setToolTip(tr("Answer MSC from every MIDI input, or only from the port QLab "
                                   "sends on. The choice is this computer's: it names the port, "
                                   "not a universe, so it holds for every show."));
    connect(m_mscInputCombo, &QComboBox::activated,
            this, &ShowControlSyncPage::slotMscInputActivated);
    form->addRow(tr("Listen on:"), m_mscInputCombo);

    QHBoxLayout *inputs = new QHBoxLayout();
    m_mscInputs = new QLabel(box);
    m_mscInputs->setObjectName(QStringLiteral("scMscInputs"));
    m_mscInputs->setWordWrap(true);
    m_mscInputs->setToolTip(tr("Where MSC can come in: the universes with that MIDI input "
                               "patched (the same inputs MIDI timecode can come in on)."));
    m_patchMidiButton = new QPushButton(tr("Patch a MIDI input…"), box);
    m_patchMidiButton->setObjectName(QStringLiteral("scPatchMidi"));
    connect(m_patchMidiButton, &QPushButton::clicked,
            this, &ShowControlSyncPage::patchMidiInputRequested);
    inputs->addWidget(m_mscInputs, 1);
    inputs->addWidget(m_patchMidiButton, 0, Qt::AlignTop);
    form->addRow(QString(), inputs);

    m_mscLast = new QLabel(box);
    m_mscLast->setObjectName(QStringLiteral("scMscLast"));
    m_mscLast->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_mscLast->setWordWrap(true);
    form->addRow(tr("Last received:"), m_mscLast);

    connect(m_mscCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating && m_listener)
            m_listener->setMscEnabled(on);
    });
    connect(m_mscDeviceSpin, &QSpinBox::valueChanged, this, [this](int id) {
        if (!m_updating && m_listener)
            m_listener->setMscDeviceId(id);
    });
    return box;
}

QGroupBox *ShowControlSyncPage::buildMtcGroup()
{
    QGroupBox *box = new QGroupBox(tr("MIDI timecode"), this);
    box->setObjectName(QStringLiteral("scMtcGroup"));
    QFormLayout *form = new QFormLayout(box);

    m_mtcSourceCombo = new QComboBox(box);
    m_mtcSourceCombo->setObjectName(QStringLiteral("scMtcSource"));
    m_mtcSourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_mtcSourceCombo->setToolTip(tr("Which universe's MIDI input the timecode is taken from. "
                                    "The same choice as the footer's MTC chip."));
    connect(m_mtcSourceCombo, &QComboBox::activated,
            this, &ShowControlSyncPage::slotMtcSourceActivated);
    form->addRow(tr("Source:"), m_mtcSourceCombo);

    m_mtcStatus = new QLabel(box);
    m_mtcStatus->setObjectName(QStringLiteral("scMtcStatus"));
    form->addRow(tr("Timecode:"), m_mtcStatus);

    m_mtcHealth = new QLabel(box);
    m_mtcHealth->setObjectName(QStringLiteral("scMtcHealth"));
    m_mtcHealth->setWordWrap(true);
    m_mtcHealth->setToolTip(tr("Measured on the incoming code: how fast it advances against "
                               "real time, how evenly it arrives (jitter), and how often."));
    form->addRow(tr("Sync health:"), m_mtcHealth);

    form->addRow(noteLabel(tr("A show follows it when Follow MTC is on in Shows; its offset "
                              "is set there too."), box));
    return box;
}

QGroupBox *ShowControlSyncPage::buildQLabOutGroup()
{
    QGroupBox *box = new QGroupBox(tr("QLab out"), this);
    box->setObjectName(QStringLiteral("scQLabOutGroup"));
    QVBoxLayout *lay = new QVBoxLayout(box);
    lay->addWidget(noteLabel(tr("Sending to QLab (firing its cues from here) isn't built yet. "
                                "When it is, it will be set up on this page."), box));
    return box;
}

void ShowControlSyncPage::setListener(ShowControlListener *listener)
{
    if (m_listener)
        disconnect(m_listener, nullptr, this, nullptr);
    m_listener = listener;

    const bool have = (listener != nullptr);
    for (QWidget *w : { static_cast<QWidget *>(m_oscCheck), static_cast<QWidget *>(m_oscPortSpin),
                        static_cast<QWidget *>(m_mscCheck), static_cast<QWidget *>(m_mscDeviceSpin),
                        static_cast<QWidget *>(m_mscInputCombo) })
        w->setEnabled(have);

    if (have)
    {
        connect(listener, &ShowControlListener::settingsChanged,
                this, &ShowControlSyncPage::slotListenerSettingsChanged);
        connect(listener, &ShowControlListener::statusChanged,
                this, &ShowControlSyncPage::slotOscStatusChanged);
        connect(listener, &ShowControlListener::lastOscChanged,
                this, &ShowControlSyncPage::slotLastOscChanged);
        connect(listener, &ShowControlListener::lastMscChanged,
                this, &ShowControlSyncPage::slotLastMscChanged);
    }
    slotListenerSettingsChanged();
    slotLastOscChanged(have ? listener->lastOsc() : QString());
    slotLastMscChanged(have ? listener->lastMsc() : QString());
}

void ShowControlSyncPage::slotListenerSettingsChanged()
{
    // The settings themselves, as saved -- the same whether or not the
    // listener exists yet.
    m_updating = true;
    m_oscCheck->setChecked(ShowControlListener::oscEnabled());
    m_oscPortSpin->setValue(ShowControlListener::oscPort());
    m_mscCheck->setChecked(ShowControlListener::mscEnabled());
    m_mscDeviceSpin->setValue(ShowControlListener::mscDeviceId());
    m_updating = false;
    m_mscDeviceSpin->setEnabled(m_listener && m_mscCheck->isChecked());
    m_mscInputCombo->setEnabled(m_listener && m_mscCheck->isChecked());
    slotOscStatusChanged();
    refreshMidiInputs();
}

void ShowControlSyncPage::slotOscStatusChanged()
{
    if (!m_listener)
    {
        m_oscStatus->setText(tr("Not started"));
        m_oscStatus->setStyleSheet(kGrey);
        return;
    }
    m_oscStatus->setText(m_listener->status());
    switch (m_listener->oscState())
    {
        case ShowControlListener::OscListening: m_oscStatus->setStyleSheet(kGreen); break;
        case ShowControlListener::OscError:     m_oscStatus->setStyleSheet(kRed); break;
        default:                                m_oscStatus->setStyleSheet(kGrey); break;
    }
    m_oscStatus->setProperty("oscState", int(m_listener->oscState()));
}

void ShowControlSyncPage::slotLastOscChanged(const QString &what)
{
    m_oscLast->setText(what.isEmpty() ? tr("Nothing received yet.") : what);
}

void ShowControlSyncPage::slotLastMscChanged(const QString &what)
{
    m_mscLast->setText(what.isEmpty() ? tr("Nothing received yet.") : what);
}

void ShowControlSyncPage::refreshMidiInputs()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    TimecodeSource *tc = m_doc->timecodeSource();
    const qint32 cur = tc->sourceUniverse();

    QStringList midi;
    QStringList patchedMidiInputs;
    m_mtcSourceCombo->clear();
    m_mtcSourceCombo->addItem(tr("Any universe (auto-detect)"), -1);

    /* The same universes the footer chip's menu offers: those with a MIDI
       input (the only kind that carries timecode), plus the one picked and
       the one code last came in on, so neither drops out of sight. */
    for (quint32 i = 0; i < iom->universesCount(); i++)
    {
        const quint32 uni = iom->getUniverseID(i);
        InputPatch *ip = iom->inputPatch(uni);
        const bool isMidi = (ip != nullptr && ip->pluginName() == QLatin1String("MIDI"));
        const bool selected = (qint32(uni) == cur);
        const bool sawCode = (tc->lastUniverse() == qint32(uni));
        QString name = universeName(iom, i);
        if (isMidi)
        {
            name += QStringLiteral("  —  %1").arg(ip->inputName());
            midi << name;
            patchedMidiInputs << ip->inputName();
        }
        else if (!selected && !sawCode)
        {
            continue;
        }
        else
        {
            name += tr("  —  no MIDI input");
        }
        m_mtcSourceCombo->addItem(name, qint32(uni));
    }
    m_mtcSourceCombo->setCurrentIndex(qMax(0, m_mtcSourceCombo->findData(cur)));

    /* MSC: every MIDI input, or the one chosen -- by name, from this
       computer's MIDI ports, the patched ones and the saved choice. */
    const QString wantPlugin = ShowControlListener::mscInputPlugin();
    const QString want = ShowControlListener::mscInputName();
    QStringList names = iom->pluginInputs(QStringLiteral("MIDI"));
    for (const QString &n : patchedMidiInputs)
        if (!names.contains(n))
            names << n;
    m_mscInputCombo->clear();
    m_mscInputCombo->addItem(tr("All MIDI inputs"), QStringList());
    for (const QString &n : names)
        m_mscInputCombo->addItem(n, QStringList{ QStringLiteral("MIDI"), n });
    if (!want.isEmpty())
    {
        int at = m_mscInputCombo->findData(QStringList{ wantPlugin, want });
        if (at < 0)
        {
            m_mscInputCombo->addItem(tr("%1 (not on this computer now)").arg(want),
                                     QStringList{ wantPlugin, want });
            at = m_mscInputCombo->count() - 1;
        }
        m_mscInputCombo->setCurrentIndex(at);
    }
    else
    {
        m_mscInputCombo->setCurrentIndex(0);
    }

    QStringList where;
    for (quint32 uni : m_doc->showControl()->mscUniverses())
        for (quint32 i = 0; i < iom->universesCount(); i++)
            if (iom->getUniverseID(i) == uni)
                where << universeName(iom, i);

    m_mscInputs->setProperty("mscInputPatched", want.isEmpty() || !where.isEmpty());
    if (want.isEmpty() && midi.isEmpty())
    {
        m_mscInputs->setText(tr("No MIDI input is patched, so neither MSC nor timecode can arrive."));
        m_mscInputs->setStyleSheet(kAmber);
    }
    else if (want.isEmpty())
    {
        m_mscInputs->setText(tr("Every patched MIDI input: %1").arg(midi.join(QStringLiteral("; "))));
        m_mscInputs->setStyleSheet(QString());
    }
    else if (where.isEmpty())
    {
        // Said plainly, and no quiet fall-back to every input.
        m_mscInputs->setText(tr("%1 isn't patched in this show, so MSC isn't answered. Patch it "
                                "as the input of a universe, or listen on all MIDI inputs.")
                             .arg(want));
        m_mscInputs->setStyleSheet(kRed);
    }
    else
    {
        m_mscInputs->setText(tr("Only %1, patched to %2.").arg(want, where.join(QStringLiteral(", "))));
        m_mscInputs->setStyleSheet(QString());
    }
    refreshTimecode();
}

void ShowControlSyncPage::slotMtcSourceActivated(int index)
{
    TimecodeSource *tc = m_doc->timecodeSource();
    const qint32 uni = m_mtcSourceCombo->itemData(index).toInt();
    if (uni == tc->sourceUniverse())
        return;
    tc->setSourceUniverse(uni);
    m_doc->setModified();       // saved with the show
}

void ShowControlSyncPage::slotMscInputActivated(int index)
{
    if (!m_listener)
        return;
    const QStringList id = m_mscInputCombo->itemData(index).toStringList();
    if (id.size() == 2)
        m_listener->setMscInput(id.at(0), id.at(1));
    else
        m_listener->setMscInput(QString(), QString());
}

void ShowControlSyncPage::slotTimeChanged()
{
    // Code arrives up to ~120 times a second; the readout doesn't need that.
    if (m_mtcThrottle.isValid() && m_mtcThrottle.elapsed() < 200)
        return;
    m_mtcThrottle.start();
    // A universe heard for the first time joins the source list.
    if (m_mtcSourceCombo->findData(m_doc->timecodeSource()->lastUniverse()) < 0)
        refreshMidiInputs();
    else
        refreshTimecode();
}

void ShowControlSyncPage::refreshTimecode()
{
    TimecodeSource *tc = m_doc->timecodeSource();
    const TimecodeSource::Health health = tc->health();
    m_mtcHealth->setProperty("health", int(health));

    if (health == TimecodeSource::NoSource)
    {
        m_mtcStatus->setText(tc->sourceUniverse() >= 0 ? tr("Waiting for the source to roll…")
                                                       : tr("None received yet"));
        m_mtcStatus->setStyleSheet(kGrey);
        m_mtcHealth->setText(tr("—"));
        m_mtcHealth->setStyleSheet(kGrey);
        return;
    }

    QString from;
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 i = 0; i < iom->universesCount(); i++)
        if (qint32(iom->getUniverseID(i)) == tc->lastUniverse())
            from = universeName(iom, i);

    const QString code = timecodeText(tc->positionMs(), tc->fps());
    if (tc->isRunning())
    {
        m_mtcStatus->setText(tr("● %1 at %2 fps, from %3").arg(code).arg(tc->fps()).arg(from));
        m_mtcStatus->setStyleSheet(kGreen);
    }
    else
    {
        m_mtcStatus->setText(tr("❚❚ %1 (holding), from %2").arg(code, from));
        m_mtcStatus->setStyleSheet(kAmber);
    }

    QString verdict;
    const char *style = kGrey;
    switch (health)
    {
        case TimecodeSource::Healthy:  verdict = tr("✓ healthy"); style = kGreen; break;
        case TimecodeSource::Usable:   verdict = tr("~ usable"); style = kAmber; break;
        case TimecodeSource::Unstable: verdict = tr("⚠ unstable"); style = kRed; break;
        default:                       verdict = tr("held (no fresh code)"); style = kAmber; break;
    }
    if (tc->isRunning())
        verdict += tr(" — %1× real time · jitter %2 ms · ~%3 ms/update")
                       .arg(tc->rateEstimate(), 0, 'f', 2)
                       .arg(qRound(tc->jitterMs()))
                       .arg(qRound(tc->avgIntervalMs()));
    m_mtcHealth->setText(verdict);
    m_mtcHealth->setStyleSheet(style);
}

void ShowControlSyncPage::showEvent(QShowEvent *ev)
{
    // Inputs are patched on the other Connections pages; catch up on them.
    refreshMidiInputs();
    QWidget::showEvent(ev);
}
