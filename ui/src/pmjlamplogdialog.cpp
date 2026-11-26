/*
  Q Light Controller Plus - qlcconsole
  pmjlamplogdialog.cpp

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

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include "pmjlamplogdialog.h"
#include "pmjoverlay.h"

PmjLampLogDialog::PmjLampLogDialog(PMJOverlay *overlay, QWidget *parent)
    : QDialog(parent)
    , m_overlay(overlay)
{
    setObjectName(QStringLiteral("pmjLampLog"));
    setWindowTitle(tr("PMJ Lamp Log"));
    QVBoxLayout *lay = new QVBoxLayout(this);
    QLabel *what = new QLabel(tr("The last lamps sent to the PMJ Black 1, newest last: the control, its note "
                                 "and MIDI channel, the velocity and what the board shows for it, and the "
                                 "MIDI bytes. A lamp that is sent but does not light is the board's: its "
                                 "LEDs may not listen on its buttons' notes (see Help)."), this);
    what->setWordWrap(true);
    lay->addWidget(what);
    m_log = new QPlainTextEdit(this);
    m_log->setObjectName(QStringLiteral("pmjLampLogText"));
    m_log->setReadOnly(true);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lay->addWidget(m_log, 1);

    QHBoxLayout *test = new QHBoxLayout();
    test->addWidget(new QLabel(tr("Button:"), this));
    m_control = new QComboBox(this);
    m_control->setObjectName(QStringLiteral("pmjLampTestControl"));
    for (const QString &name : { QStringLiteral("Go"), QStringLiteral("Back"), QStringLiteral("Effects"),
                                 QStringLiteral("1-Load"), QStringLiteral("Pre Page"), QStringLiteral("Left") })
    {
        for (quint32 ch = 32768; ch < 33024; ++ch)
            if (PMJOverlay::controlName(ch) == name)
            {
                m_control->addItem(name, ch);
                break;
            }
    }
    test->addWidget(m_control);
    m_test = new QPushButton(tr("Lamp Test"), this);
    m_test->setObjectName(QStringLiteral("pmjLampTest"));
    m_test->setToolTip(tr("Light this button's lamp at every level and blink the board has, one step a "
                          "second and a half. Say what each step looks like."));
    test->addWidget(m_test);
    test->addStretch(1);
    lay->addLayout(test);
    m_step = new QLabel(this);
    m_step->setObjectName(QStringLiteral("pmjLampTestStep"));
    m_step->setWordWrap(true);
    lay->addWidget(m_step);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *copy = bb->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    connect(copy, &QPushButton::clicked, this, [this]() { QApplication::clipboard()->setText(logText()); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);

    if (m_overlay != nullptr)
    {
        connect(m_overlay, &PMJOverlay::lampLogged, this, &PmjLampLogDialog::refresh);
        connect(m_overlay, &PMJOverlay::lampTestStepped, this, [this](const QString &step) {
            m_step->setText(step);
            m_test->setText(step.isEmpty() ? tr("Lamp Test") : tr("Stop Test"));
        });
        connect(m_test, &QPushButton::clicked, this, [this]() {
            if (m_overlay == nullptr)
                return;
            if (m_overlay->lampTestRunning())
                m_overlay->stopLampTest();
            else
                m_overlay->startLampTest(m_control->currentData().toUInt());
        });
    }
    else
    {
        m_test->setEnabled(false);
        m_step->setText(tr("No PMJ Black 1 is patched."));
    }
    resize(760, 480);
    refresh();
}

PmjLampLogDialog::~PmjLampLogDialog()
{
    if (m_overlay != nullptr)
        m_overlay->stopLampTest();
}

QString PmjLampLogDialog::logText() const
{
    return m_log->toPlainText();
}

void PmjLampLogDialog::refresh()
{
    const QStringList lines = m_overlay == nullptr ? QStringList() : m_overlay->lampLog();
    m_log->setPlainText(lines.isEmpty() ? tr("No lamp sent yet.") : lines.join(QLatin1Char('\n')));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}
