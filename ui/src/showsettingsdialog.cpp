/*
  Q Light Controller Plus
  showsettingsdialog.cpp

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

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

#include "showsettingsdialog.h"
#include "showoperate.h"
#include "doc.h"

ShowSettingsDialog::ShowSettingsDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("showSettingsDialog"));
    setWindowTitle(tr("Show Settings"));
    setProperty("helpTopic", QStringLiteral("getting-started.md#show-settings"));

    ShowOperate *op = m_doc->operateSettings();
    m_newPinHash = op->pinHash();

    QVBoxLayout *root = new QVBoxLayout(this);

    QGroupBox *box = new QGroupBox(tr("Operate"), this);
    QVBoxLayout *vl = new QVBoxLayout(box);
    m_openInOperate = new QCheckBox(tr("Open this show in Operate"), box);
    m_openInOperate->setObjectName(QStringLiteral("showSettingsOpenInOperate"));
    m_openInOperate->setToolTip(tr("Opening the show goes straight to Operate, as the -p "
                                   "command-line option does, so the show is ready to run."));
    m_openInOperate->setChecked(op->openInOperate());
    vl->addWidget(m_openInOperate);

    QHBoxLayout *pinRow = new QHBoxLayout();
    m_pinText = new QLabel(box);
    m_pinText->setObjectName(QStringLiteral("showSettingsPinText"));
    m_pinText->setWordWrap(true);
    pinRow->addWidget(m_pinText, 1);
    m_setPin = new QPushButton(box);
    m_setPin->setObjectName(QStringLiteral("showSettingsSetPin"));
    connect(m_setPin, &QPushButton::clicked, this, &ShowSettingsDialog::slotSetPin);
    pinRow->addWidget(m_setPin);
    m_removePin = new QPushButton(tr("Remove PIN"), box);
    m_removePin->setObjectName(QStringLiteral("showSettingsRemovePin"));
    connect(m_removePin, &QPushButton::clicked, this, &ShowSettingsDialog::slotRemovePin);
    pinRow->addWidget(m_removePin);
    vl->addLayout(pinRow);

    QLabel *note = new QLabel(tr("A PIN keeps curious hands off a running show: going back to "
                                 "Design asks for it, and Live Edit is off. It is not a lock "
                                 "against someone who can edit the show file. Keep a copy "
                                 "without the PIN."), box);
    note->setWordWrap(true);
    QFont small = note->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    note->setFont(small);
    vl->addWidget(note);
    root->addWidget(box);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &ShowSettingsDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &ShowSettingsDialog::reject);
    root->addWidget(btns);

    updatePinText();
}

static bool s_testMode = false;

void ShowSettingsDialog::setTestMode(bool on)
{
    s_testMode = on;
    if (!on)
        testAnswers().clear();
}

QStringList &ShowSettingsDialog::testAnswers()
{
    static QStringList answers;
    return answers;
}

void ShowSettingsDialog::updatePinText()
{
    const bool has = !m_newPinHash.isEmpty();
    m_pinText->setText(has ? tr("A PIN is set: going back to Design asks for it.")
                           : tr("No PIN: anyone can go back to Design."));
    m_setPin->setText(has ? tr("Change PIN…") : tr("Set PIN…"));
    m_removePin->setEnabled(has);
}

void ShowSettingsDialog::slotSetPin()
{
    /* Changing the show's own PIN asks for it first; one set in this
       dialog and not yet saved doesn't. */
    const QString current = m_doc->operateSettings()->pinHash();
    if (!current.isEmpty() && m_newPinHash == current
        && !askForPin(this, current, tr("Enter the current PIN to change it.")))
        return;
    const QString pin = askNewPin(this);
    if (pin.isEmpty())
        return;
    m_newPinHash = ShowPin::hash(pin);
    updatePinText();
}

void ShowSettingsDialog::slotRemovePin()
{
    const QString current = m_doc->operateSettings()->pinHash();
    if (!current.isEmpty() && m_newPinHash == current
        && !askForPin(this, current, tr("Enter the current PIN to remove it.")))
        return;
    m_newPinHash.clear();
    updatePinText();
}

void ShowSettingsDialog::accept()
{
    ShowOperate *op = m_doc->operateSettings();
    op->setOpenInOperate(m_openInOperate->isChecked());
    if (m_newPinHash != op->pinHash())
    {
        if (m_newPinHash.isEmpty())
            op->setPin(QString());
        else
            op->setPinHash(m_newPinHash);
    }
    QDialog::accept();
}

/* One modal question with a hidden line edit: the text, or empty on Cancel. */
static QString askHidden(QWidget *parent, const QString &title, const QString &label)
{
    if (s_testMode)
    {
        QStringList &answers = ShowSettingsDialog::testAnswers();
        return answers.isEmpty() ? QString() : answers.takeFirst();
    }

    QDialog d(parent);
    d.setWindowTitle(title);
    QVBoxLayout *vl = new QVBoxLayout(&d);
    QLabel *l = new QLabel(label, &d);
    l->setWordWrap(true);
    vl->addWidget(l);
    QLineEdit *edit = new QLineEdit(&d);
    edit->setObjectName(QStringLiteral("pinEdit"));
    edit->setEchoMode(QLineEdit::Password);
    edit->setMaxLength(8);
    edit->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhSensitiveData);
    vl->addWidget(edit);
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    QObject::connect(btns, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    vl->addWidget(btns);
    if (d.exec() != QDialog::Accepted)
        return QString();
    return edit->text();
}

QString ShowSettingsDialog::askNewPin(QWidget *parent)
{
    QString why;
    for (;;)
    {
        const QString pin = askHidden(parent, tr("Set PIN"),
                                      why + tr("A PIN of 4 to 8 digits:"));
        if (pin.isEmpty())
            return QString();
        if (!ShowPin::isValid(pin))
        {
            why = tr("That isn't 4 to 8 digits. ");
            continue;
        }
        const QString again = askHidden(parent, tr("Set PIN"), tr("The same PIN again:"));
        if (again.isEmpty())
            return QString();
        if (again != pin)
        {
            why = tr("The two didn't match. ");
            continue;
        }
        return pin;
    }
}

bool ShowSettingsDialog::askForPin(QWidget *parent, const QString &hash, const QString &why)
{
    const QString pin = askHidden(parent, tr("PIN"), why);
    if (pin.isEmpty())
        return false;
    if (ShowPin::verify(pin, hash))
        return true;
    if (!s_testMode)
        QMessageBox::warning(parent, tr("PIN"), tr("That isn't the PIN."));
    return false;
}
