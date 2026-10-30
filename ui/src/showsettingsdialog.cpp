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
#include <QFileDialog>
#include <QDir>
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
#include "teachingsettings.h"
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
    TeachingSettings *teach = m_doc->teachingSettings();
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
    m_lockDesign = new QCheckBox(tr("Going back to Design asks for the PIN"), box);
    m_lockDesign->setObjectName(QStringLiteral("showSettingsLockDesign"));
    m_lockDesign->setToolTip(tr("With a PIN set: the show stays in Operate until the PIN is given, "
                                "and Live Edit is off. Untick it to use the PIN for Teaching mode only."));
    m_lockDesign->setChecked(op->lockDesignSetting());
    vl->addWidget(m_lockDesign);
    root->addWidget(box);

    QGroupBox *tbox = new QGroupBox(tr("Teaching"), this);
    QVBoxLayout *tl = new QVBoxLayout(tbox);
    m_teaching = new QCheckBox(tr("Teaching mode"), tbox);
    m_teaching->setObjectName(QStringLiteral("showSettingsTeaching"));
    m_teaching->setToolTip(tr("A show handed to students: the scramble practice is on, real network "
                              "output is held, and running the show says it is a teaching show."));
    m_teaching->setChecked(teach->enabled());
    tl->addWidget(m_teaching);
    m_allowOutput = new QCheckBox(tr("Let real output through (instructor)"), tbox);
    m_allowOutput->setObjectName(QStringLiteral("showSettingsAllowOutput"));
    m_allowOutput->setToolTip(tr("Teaching mode holds Art-Net, sACN and other network output so a "
                                 "student copy cannot reach the school network. Tick this to send it."));
    m_allowOutput->setChecked(teach->allowOutput());
    tl->addWidget(m_allowOutput);
    QFormLayout *lf = new QFormLayout();
    m_lesson = new QLineEdit(teach->lesson(), tbox);
    m_lesson->setObjectName(QStringLiteral("showSettingsLesson"));
    m_lesson->setPlaceholderText(tr("3.6"));
    lf->addRow(tr("Lesson:"), m_lesson);
    m_link = new QLineEdit(teach->link(), tbox);
    m_link->setObjectName(QStringLiteral("showSettingsLink"));
    m_link->setPlaceholderText(tr("Page of the lesson handout (a web address)"));
    lf->addRow(tr("Handout link:"), m_link);
    tl->addLayout(lf);
    m_handout = new QPushButton(tr("Make Student Handout…"), tbox);
    m_handout->setObjectName(QStringLiteral("showSettingsHandout"));
    m_handout->setToolTip(tr("Save a copy of this show for students, with Teaching mode on. "
                             "This show is left as it is."));
    connect(m_handout, &QPushButton::clicked, this, &ShowSettingsDialog::slotHandout);
    tl->addWidget(m_handout);
    root->addWidget(tbox);

    QGroupBox *pbox = new QGroupBox(tr("PIN"), this);
    QVBoxLayout *pl = new QVBoxLayout(pbox);
    QHBoxLayout *pinRow = new QHBoxLayout();
    m_pinText = new QLabel(pbox);
    m_pinText->setObjectName(QStringLiteral("showSettingsPinText"));
    m_pinText->setWordWrap(true);
    pinRow->addWidget(m_pinText, 1);
    m_setPin = new QPushButton(pbox);
    m_setPin->setObjectName(QStringLiteral("showSettingsSetPin"));
    connect(m_setPin, &QPushButton::clicked, this, &ShowSettingsDialog::slotSetPin);
    pinRow->addWidget(m_setPin);
    m_removePin = new QPushButton(tr("Remove PIN"), pbox);
    m_removePin->setObjectName(QStringLiteral("showSettingsRemovePin"));
    connect(m_removePin, &QPushButton::clicked, this, &ShowSettingsDialog::slotRemovePin);
    pinRow->addWidget(m_removePin);
    pl->addLayout(pinRow);

    QLabel *note = new QLabel(tr("One PIN for the show. It keeps curious hands off: going back to "
                                 "Design from Operate asks for it (if ticked), and so does turning "
                                 "Teaching mode off or letting real output through. It is not a lock "
                                 "against someone who can edit the show file. Keep a copy without "
                                 "the PIN."), pbox);
    note->setWordWrap(true);
    QFont small = note->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    note->setFont(small);
    pl->addWidget(note);
    root->addWidget(pbox);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &ShowSettingsDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &ShowSettingsDialog::reject);
    root->addWidget(btns);

    connect(m_teaching, &QCheckBox::toggled, this, &ShowSettingsDialog::updateEnabled);
    updatePinText();
    updateEnabled();
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
    m_pinText->setText(has ? tr("A PIN is set.") : tr("No PIN: anyone can change these settings."));
    m_setPin->setText(has ? tr("Change PIN…") : tr("Set PIN…"));
    m_removePin->setEnabled(has);
    if (m_lockDesign != nullptr)
        m_lockDesign->setEnabled(has);
}

void ShowSettingsDialog::updateEnabled()
{
    m_allowOutput->setEnabled(m_teaching->isChecked());
    m_lesson->setEnabled(m_teaching->isChecked());
    m_link->setEnabled(m_teaching->isChecked());
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
    TeachingSettings *teach = m_doc->teachingSettings();

    /* Turning Teaching off, or letting real output through, asks for the
       show's PIN (the stored one, not one just set in this dialog). */
    const bool off = teach->enabled() && !m_teaching->isChecked();
    const bool lets = m_teaching->isChecked() && !teach->allowOutput() && m_allowOutput->isChecked();
    if ((off || lets) && op->hasPin()
        && !askForPin(this, op->pinHash(),
                      off ? tr("Enter the PIN to turn Teaching mode off.")
                          : tr("Enter the PIN to let real output through.")))
        return;

    teach->setEnabled(m_teaching->isChecked());
    teach->setAllowOutput(m_teaching->isChecked() && m_allowOutput->isChecked());
    if (m_teaching->isChecked())
        teach->setLesson(m_lesson->text().trimmed(), m_link->text().trimmed());

    op->setOpenInOperate(m_openInOperate->isChecked());
    op->setLockDesign(m_lockDesign->isChecked());
    if (m_newPinHash != op->pinHash())
    {
        if (m_newPinHash.isEmpty())
            op->setPin(QString());
        else
            op->setPinHash(m_newPinHash);
    }
    QDialog::accept();
}

HandoutOptions &ShowSettingsDialog::testHandout()
{
    static HandoutOptions options;
    return options;
}

void ShowSettingsDialog::slotHandout()
{
    // The handout is made from the settings as they are now.
    accept();
    if (result() == QDialog::Accepted)
        m_handoutRequested = true;
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

bool ShowSettingsDialog::askHandoutOptions(Doc *doc, QWidget *parent, HandoutOptions *out)
{
    if (s_testMode)
    {
        *out = testHandout();
        return !out->path.isEmpty();
    }

    TeachingSettings *teach = doc->teachingSettings();
    QDialog d(parent);
    d.setObjectName(QStringLiteral("studentHandoutDialog"));
    d.setWindowTitle(tr("Make Student Handout"));
    d.setProperty("helpTopic", QStringLiteral("teaching.md#make-student-handout"));
    QVBoxLayout *vl = new QVBoxLayout(&d);
    QLabel *intro = new QLabel(tr("A copy of this show for students, with Teaching mode on. "
                                  "This show is left as it is."), &d);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    QString hash = doc->operateSettings()->pinHash();
    QHBoxLayout *fileRow = new QHBoxLayout();
    QLineEdit *path = new QLineEdit(&d);
    path->setObjectName(QStringLiteral("handoutPath"));
    const QString cur = doc->workspacePath();
    path->setText(QDir(cur.isEmpty() ? QDir::homePath() : cur).absoluteFilePath(tr("Handout.qlcc")));
    fileRow->addWidget(new QLabel(tr("Save as:"), &d));
    fileRow->addWidget(path, 1);
    QPushButton *browse = new QPushButton(tr("Browse…"), &d);
    QObject::connect(browse, &QPushButton::clicked, &d, [&]() {
        const QString f = QFileDialog::getSaveFileName(&d, tr("Make Student Handout"), path->text(),
                                                       tr("Show with its files (*.qlcc)"));
        if (!f.isEmpty())
            path->setText(f);
    });
    fileRow->addWidget(browse);
    vl->addLayout(fileRow);

    QHBoxLayout *pinRow = new QHBoxLayout();
    QLabel *pinText = new QLabel(&d);
    pinRow->addWidget(pinText, 1);
    QPushButton *setPin = new QPushButton(tr("Set PIN…"), &d);
    QPushButton *noPin = new QPushButton(tr("No PIN"), &d);
    auto showPin = [&]() {
        pinText->setText(hash.isEmpty() ? tr("The copy has no PIN: students can switch Teaching mode off.")
                                        : tr("The copy has a PIN: Teaching mode stays on without it."));
    };
    QObject::connect(setPin, &QPushButton::clicked, &d, [&]() {
        const QString pin = askNewPin(&d);
        if (!pin.isEmpty())
            hash = ShowPin::hash(pin);
        showPin();
    });
    QObject::connect(noPin, &QPushButton::clicked, &d, [&]() { hash.clear(); showPin(); });
    pinRow->addWidget(setPin);
    pinRow->addWidget(noPin);
    vl->addLayout(pinRow);
    showPin();

    QCheckBox *practice = new QCheckBox(tr("Leave out practice venues and their answer key"), &d);
    practice->setChecked(true);
    QCheckBox *names = new QCheckBox(tr("Blank the “checked by” names"), &d);
    names->setChecked(true);
    QCheckBox *hold = new QCheckBox(tr("Hold real network output in the copy"), &d);
    hold->setChecked(true);
    vl->addWidget(practice);
    vl->addWidget(names);
    vl->addWidget(hold);
    QFormLayout *lf = new QFormLayout();
    QLineEdit *lesson = new QLineEdit(teach->lesson(), &d);
    QLineEdit *link = new QLineEdit(teach->link(), &d);
    lf->addRow(tr("Lesson:"), lesson);
    lf->addRow(tr("Handout link:"), link);
    vl->addLayout(lf);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    QObject::connect(btns, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    vl->addWidget(btns);
    if (d.exec() != QDialog::Accepted || path->text().trimmed().isEmpty())
        return false;

    out->path = path->text().trimmed();
    out->pinHash = hash;
    out->dropPractice = practice->isChecked();
    out->dropNames = names->isChecked();
    out->holdOutput = hold->isChecked();
    out->lesson = lesson->text().trimmed();
    out->link = link->text().trimmed();
    return true;
}
