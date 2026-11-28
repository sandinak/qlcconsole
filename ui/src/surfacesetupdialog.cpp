/*
  Q Light Controller Plus - qlcconsole
  surfacesetupdialog.cpp

  Copyright (C) Branson Matheson

  Align, Undo and Identify follow qlcplus-midi-profiler's `opendeck align`,
  `restore` and `identify` (Apache-2.0, by Branson Matheson).

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

#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTimer>

#include "surfacesetupdialog.h"
#include "apputil.h"
#include "inputoutputmap.h"
#include "doc.h"

SurfaceSetupDialog::SurfaceSetupDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_setup(doc->surfaceSetup())
{
    setObjectName(QStringLiteral("surfaceSetupDialog"));
    setWindowTitle(tr("Controller Setup"));
    setProperty("helpTopic", QStringLiteral("control-surfaces.md"));

    QVBoxLayout *lay = new QVBoxLayout(this);
    m_intro = new QLabel(tr("What a controller needs set on the device itself so its profile "
                            "works: its mode, and its lamps under their own buttons. Checking "
                            "only reads the device; nothing is written until you press a button "
                            "here, and a backup comes first."), this);
    m_intro->setWordWrap(true);
    lay->addWidget(m_intro);

    QHBoxLayout *dev = new QHBoxLayout();
    m_device = new QComboBox(this);
    m_device->setObjectName(QStringLiteral("surfaceSetupDevice"));
    m_device->setAccessibleName(tr("Controller"));
    m_deviceLabel = new QLabel(this);
    m_deviceLabel->setObjectName(QStringLiteral("surfaceSetupDeviceName"));
    QFont bold = m_deviceLabel->font();
    bold.setBold(true);
    m_deviceLabel->setFont(bold);
    dev->addWidget(m_deviceLabel);
    dev->addWidget(m_device, 1);
    lay->addLayout(dev);
    for (quint32 u : m_setup->universes())
        m_device->addItem(m_setup->title(u), u);
    connect(m_device, &QComboBox::currentIndexChanged, this, [this]() {
        m_error->clear();
        checkNow();
    });

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("surfaceSetupStatus"));
    m_status->setWordWrap(true);
    lay->addWidget(m_status);
    m_backup = new QLabel(this);
    m_backup->setObjectName(QStringLiteral("surfaceSetupBackup"));
    m_backup->setWordWrap(true);
    m_backup->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backup->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_backup);
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("surfaceSetupError"));
    m_error->setWordWrap(true);
    lay->addWidget(m_error);
    m_patchFeedback = new QPushButton(tr("Patch Feedback"), this);
    m_patchFeedback->setObjectName(QStringLiteral("surfaceSetupPatchFeedback"));
    m_patchFeedback->setToolTip(tr("Send this board's lamps and faders back to its own output."));
    m_patchFeedback->hide();
    lay->addWidget(m_patchFeedback, 0, Qt::AlignLeft);
    connect(m_patchFeedback, &QPushButton::clicked, this, &SurfaceSetupDialog::patchFeedback);

    m_buttons = new QWidget(this);
    QHBoxLayout *row = new QHBoxLayout(m_buttons);
    row->setContentsMargins(0, 0, 0, 0);
    auto button = [this, row](const QString &text, const char *name, const QString &tip) {
        QPushButton *b = new QPushButton(text, m_buttons);
        b->setObjectName(QString::fromLatin1(name));
        b->setToolTip(tip);
        b->setAutoDefault(false);
        row->addWidget(b);
        return b;
    };
    m_check = button(tr("Check Again"), "surfaceSetupCheck",
                     tr("Read the device's settings again and compare them with its profile. Reads only."));
    m_align = button(tr("Align Lamps"), "surfaceSetupAlign",
                     tr("Set each lamp to listen on its own button's note, so the lamp the app lights "
                        "is the one under that button. The device's whole setup is backed up first."));
    m_undo = button(tr("Undo Align"), "surfaceSetupUndo",
                    tr("Put back the backup the last Align took."));
    m_identify = button(tr("Identify Lamps…"), "surfaceSetupIdentify",
                        tr("Flash each lamp in turn; press the button under it. For a board with no "
                           "pairing shipped. The device is put back as it was at the end."));
    m_send = button(tr("Send Again"), "surfaceSetupSend",
                    tr("Send its mode again now. It is sent by itself when it connects, after a "
                       "replug and after the computer wakes."));
    m_mode = button(tr("Check Mode"), "surfaceSetupMode",
                    tr("Checks the mode its pads are in: press any pad when asked. Nothing is written."));
    row->addStretch(1);
    lay->addWidget(m_buttons);
    connect(m_check, &QPushButton::clicked, this, &SurfaceSetupDialog::checkNow);
    connect(m_align, &QPushButton::clicked, this, &SurfaceSetupDialog::alignLamps);
    connect(m_undo, &QPushButton::clicked, this, &SurfaceSetupDialog::undoAlign);
    connect(m_identify, &QPushButton::clicked, this, &SurfaceSetupDialog::startIdentify);
    connect(m_send, &QPushButton::clicked, this, &SurfaceSetupDialog::sendAgain);
    connect(m_mode, &QPushButton::clicked, this, &SurfaceSetupDialog::checkMode);
    // A mode check's press arrives in the engine: what it found is shown here.
    connect(m_setup, &SurfaceSetup::changed, this, [this](quint32 u) {
        if (u == universe() && !m_identifying)
            updateState();
    });

    m_identifyBox = new QWidget(this);
    m_identifyBox->setObjectName(QStringLiteral("surfaceSetupIdentifyBox"));
    QHBoxLayout *ib = new QHBoxLayout(m_identifyBox);
    ib->setContentsMargins(0, 0, 0, 0);
    m_identifyText = new QLabel(m_identifyBox);
    m_identifyText->setObjectName(QStringLiteral("surfaceSetupIdentifyText"));
    m_identifyText->setWordWrap(true);
    m_skip = new QPushButton(tr("Skip"), m_identifyBox);
    m_skip->setObjectName(QStringLiteral("surfaceSetupSkip"));
    m_skip->setToolTip(tr("Nothing is flashing: go on to the next lamp."));
    m_finish = new QPushButton(tr("Finish"), m_identifyBox);
    m_finish->setObjectName(QStringLiteral("surfaceSetupFinish"));
    m_finish->setToolTip(tr("Stop here: the device is put back and the lamps found so far are kept."));
    ib->addWidget(m_identifyText, 1);
    ib->addWidget(m_skip, 0, Qt::AlignTop);
    ib->addWidget(m_finish, 0, Qt::AlignTop);
    m_identifyBox->hide();
    lay->addWidget(m_identifyBox);
    connect(m_skip, &QPushButton::clicked, this, &SurfaceSetupDialog::skipLamp);
    connect(m_finish, &QPushButton::clicked, this, &SurfaceSetupDialog::finishIdentify);

    lay->addStretch(1);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);

    m_flash = new QTimer(this);
    m_flash->setInterval(220);
    connect(m_flash, &QTimer::timeout, this, [this]() {
        m_flashOn = !m_flashOn;
        m_setup->flashLamp(universe(), m_lamp, m_flashOn);
    });
    // A press during Identify pairs the flashing lamp with that button.
    connect(m_doc->inputOutputMap(), &InputOutputMap::inputValueChanged, this,
            [this](quint32 u, quint32 channel, uchar value) {
        if (!m_identifying || u != universe() || value == 0)
            return;
        SurfaceSetup::LampPair p;
        if (!SurfaceSetup::pairFromChannel(channel, m_lamp, &p))
            return;
        for (const SurfaceSetup::LampPair &f : std::as_const(m_found))
            if (f.note == p.note && f.channel == p.channel)
                return;     // that button has its lamp already
        m_found << p;
        nextLamp();
    });

    resize(640, 320);
    updateState();
    // Opening checks: a read only.
    QTimer::singleShot(0, this, &SurfaceSetupDialog::checkNow);
}

SurfaceSetupDialog::~SurfaceSetupDialog()
{
    // Closed while waiting for a pad: the board's presses work again.
    if (m_device->count() > 0)
        m_setup->cancelModeCheck(universe());
    // Closed in the middle of Identify: the board is put back.
    if (m_identifying)
    {
        m_flash->stop();
        m_setup->flashLamp(universe(), m_lamp, false);
        m_setup->endIdentify(universe(), m_found);
    }
}

void SurfaceSetupDialog::open(Doc *doc, QWidget *parent)
{
    SurfaceSetupDialog dlg(doc, parent);
    dlg.exec();
}

void SurfaceSetupDialog::open(Doc *doc, QWidget *parent, quint32 universe)
{
    SurfaceSetupDialog dlg(doc, parent);
    dlg.selectUniverse(universe);
    dlg.exec();
}

bool SurfaceSetupDialog::selectUniverse(quint32 universe)
{
    const int i = m_device->findData(universe);
    if (i < 0)
        return false;
    m_device->setCurrentIndex(i);
    return true;
}

quint32 SurfaceSetupDialog::universe() const
{
    return m_device->count() == 0 ? UINT_MAX : m_device->currentData().toUInt();
}

QString SurfaceSetupDialog::statusText() const
{
    return m_status->text();
}

void SurfaceSetupDialog::showError(const QString &error)
{
    m_error->setText(error);
    m_error->setStyleSheet(error.isEmpty() ? QString()
                                           : QStringLiteral("QLabel { color:#000000; background:#f5a623; "
                                                            "padding:2px 6px; border-radius:3px; }"));
}

void SurfaceSetupDialog::updateState()
{
    const quint32 u = universe();
    const SurfaceSetup::Spec *spec = u == UINT_MAX ? nullptr : m_setup->specOf(u);
    {
        InputOutputMap *fbm = m_doc->inputOutputMap();
        m_patchFeedback->setVisible(u != UINT_MAX && fbm->inputPatch(u) != nullptr
                                    && fbm->inputPatch(u)->plugin() != nullptr
                                    && fbm->feedbackPatch(u) == nullptr);
    }
    m_device->setVisible(m_device->count() > 1);
    m_deviceLabel->setVisible(m_device->count() <= 1);
    m_deviceLabel->setText(m_device->count() == 1 ? m_device->itemText(0) : QString());
    if (spec == nullptr)
    {
        m_status->setText(tr("No controller this app sets up is patched. Patch a PMJ Black 1, an "
                             "Akai APC40 mkII or a Novation Launchpad Mini MK3 as an input with its "
                             "profile, and its output as the feedback line, then come back."));
        m_buttons->hide();
        m_backup->clear();
        return;
    }
    const SurfaceSetup::Check c = m_setup->lastCheck(u);
    m_status->setText(c.text);
    m_buttons->setVisible(!m_identifying);
    m_check->setVisible(spec->openDeck);
    m_align->setVisible(spec->openDeck);
    m_undo->setVisible(spec->openDeck);
    m_identify->setVisible(spec->openDeck);
    m_send->setVisible(!spec->init.isEmpty());
    m_mode->setVisible(spec->modeChannel != 0);
    m_mode->setEnabled(c.state != SurfaceSetup::Check::WaitingForPress);
    m_align->setEnabled(c.state == SurfaceSetup::Check::Misplaced);
    m_identify->setEnabled(c.state != SurfaceSetup::Check::NoAnswer && c.state != SurfaceSetup::Check::NotChecked);
    const QString backup = m_setup->lastBackup(u);
    m_undo->setEnabled(!backup.isEmpty() && c.state != SurfaceSetup::Check::NoAnswer);
    m_backup->setText(backup.isEmpty() ? QString()
                                       : tr("Backup before the last Align: %1").arg(backup));
    m_align->setDefault(m_align->isEnabled());
}

bool SurfaceSetupDialog::confirmWrite(bool *confirmed)
{
    *confirmed = false;
    const QString refusal = m_setup->writeRefusal(false);
    if (refusal.isEmpty())
        return true;
    if (m_autoConfirm >= 0)
        *confirmed = m_autoConfirm == 1;
    else
        *confirmed = QMessageBox::question(this, tr("Write to the Controller?"), refusal,
                                           QMessageBox::Yes | QMessageBox::Cancel,
                                           QMessageBox::Cancel) == QMessageBox::Yes;
    if (!*confirmed)
        showError(tr("Nothing was written: the show is in Operate."));
    return *confirmed;
}

void SurfaceSetupDialog::patchFeedback()
{
    const quint32 u = universe();
    if (u == UINT_MAX)
        return;
    const QString dev = m_doc->inputOutputMap()->pairFeedback(u);
    if (dev.isEmpty())
        showError(tr("No single output matches this board. In Connections, right-click its "
                     "output line and choose Patch a Universe Here (feedback)."));
    else
    {
        showError(QString());
        m_doc->setModified();
    }
    updateState();
}

void SurfaceSetupDialog::checkNow()
{
    const quint32 u = universe();
    if (u == UINT_MAX || m_identifying)
    {
        updateState();
        return;
    }
    QGuiApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    m_setup->check(u);
    QGuiApplication::restoreOverrideCursor();
    updateState();
}

void SurfaceSetupDialog::alignLamps()
{
    bool confirmed = false;
    if (!confirmWrite(&confirmed))
        return;
    QString error;
    QGuiApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    const bool ok = m_setup->align(universe(), confirmed, &error);
    QGuiApplication::restoreOverrideCursor();
    showError(ok ? QString() : error);
    updateState();
}

void SurfaceSetupDialog::undoAlign()
{
    bool confirmed = false;
    if (!confirmWrite(&confirmed))
        return;
    QString error;
    QGuiApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    const bool ok = m_setup->undoAlign(universe(), confirmed, &error);
    QGuiApplication::restoreOverrideCursor();
    showError(ok ? QString() : error);
    updateState();
}

void SurfaceSetupDialog::sendAgain()
{
    const bool ok = m_setup->sendInit(universe());
    showError(ok ? QString() : tr("It could not be sent: patch its output as the feedback line."));
    updateState();
    if (ok)
        m_status->setText(tr("Sent. %1").arg(m_setup->lastCheck(universe()).text));
}

void SurfaceSetupDialog::checkMode()
{
    showError(QString());
    m_setup->beginModeCheck(universe());
    updateState();
}

void SurfaceSetupDialog::startIdentify()
{
    bool confirmed = false;
    if (!confirmWrite(&confirmed))
        return;
    QString error;
    QGuiApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    const bool ok = m_setup->beginIdentify(universe(), confirmed, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!ok)
    {
        showError(error);
        return;
    }
    showError(QString());
    m_identifying = true;
    m_found.clear();
    m_lamp = -1;
    m_device->setEnabled(false);
    m_buttons->hide();
    m_identifyBox->show();
    nextLamp();
}

void SurfaceSetupDialog::nextLamp()
{
    m_flash->stop();
    if (m_lamp >= 0)
        m_setup->flashLamp(universe(), m_lamp, false);
    ++m_lamp;
    const int n = m_setup->lampCount(universe());
    if (m_lamp >= n)
    {
        finishIdentify();
        return;
    }
    m_identifyText->setText(tr("Lamp %1 of %2 is flashing: press the button under it. Skip when "
                               "nothing flashes. %3 paired so far.")
                                .arg(m_lamp + 1).arg(n).arg(m_found.size()));
    m_flashOn = true;
    m_setup->flashLamp(universe(), m_lamp, true);
    m_flash->start();
}

void SurfaceSetupDialog::skipLamp()
{
    if (m_identifying)
        nextLamp();
}

void SurfaceSetupDialog::finishIdentify()
{
    if (!m_identifying)
        return;
    m_flash->stop();
    m_setup->flashLamp(universe(), m_lamp, false);
    m_identifying = false;
    QString error;
    QGuiApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    const bool ok = m_setup->endIdentify(universe(), m_found, &error);
    QGuiApplication::restoreOverrideCursor();
    m_identifyBox->hide();
    m_device->setEnabled(true);
    showError(ok ? QString() : tr("The device could not be put back (%1).").arg(error));
    updateState();
}
