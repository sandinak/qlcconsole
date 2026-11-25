/*
  Q Light Controller Plus - qlcconsole
  desksetuppanel.cpp

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

#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

#include "showsettingsdialog.h"
#include "desksetuppanel.h"
#include "showoperate.h"
#include "doc.h"

namespace
{
std::function<bool(QWidget *, const QString &)> s_pinAsker;
std::function<QString(QWidget *, bool)> s_fileAsker;

const char *kObject[DeskRecord::RoleCount] = { "deskStop", "deskGrandMaster", "deskGo", "deskBack" };
}

void DeskSetupPanel::setPinAsker(const std::function<bool(QWidget *, const QString &)> &asker)
{
    s_pinAsker = asker;
}

void DeskSetupPanel::setFileAsker(const std::function<QString(QWidget *, bool)> &asker)
{
    s_fileAsker = asker;
}

DeskSetupPanel::DeskSetupPanel(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_desk(doc->deskRecord())
{
    setObjectName(QStringLiteral("deskSetupPanel"));
    setProperty("helpTopic", QStringLiteral("control-surfaces.md#this-computers-desk"));

    QGridLayout *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);

    QLabel *head = new QLabel(QStringLiteral("<b>%1</b><br>%2")
                                  .arg(tr("This computer's desk").toHtmlEscaped(),
                                       tr("STOP, the Grand Master, GO and Back for every show opened on "
                                          "this computer. Kept on this computer, never in the show; the "
                                          "show's own stop still works.").toHtmlEscaped()),
                              this);
    head->setObjectName(QStringLiteral("deskHead"));
    head->setWordWrap(true);
    head->setTextFormat(Qt::RichText);
    grid->addWidget(head, 0, 0, 1, 4);

    struct Text { QString name; QString what; };
    const Text text[DeskRecord::RoleCount] = {
        { tr("STOP"), tr("ALL OFF. Learn: press the button, release it, press it again.") },
        { tr("Grand Master"), tr("The level of the whole show. Learn: move the fader.") },
        { tr("GO"), tr("GO on the Run tab's list, one press. Learn: press the button.") },
        { tr("Back"), tr("Back on the Run tab's list. Learn: press the button.") },
    };
    int r = 1;
    for (DeskRecord::Role role : DeskRecord::roles())
    {
        const QString base = QString::fromLatin1(kObject[role]);
        QLabel *what = new QLabel(QStringLiteral("<b>%1</b><br>%2").arg(text[role].name.toHtmlEscaped(),
                                                                       text[role].what.toHtmlEscaped()), this);
        what->setObjectName(base + QStringLiteral("What"));
        what->setWordWrap(true);
        what->setTextFormat(Qt::RichText);
        m_status[role] = new QLabel(this);
        m_status[role]->setObjectName(base + QStringLiteral("Status"));
        m_status[role]->setWordWrap(true);
        m_on[role] = new QCheckBox(tr("On"), this);
        m_on[role]->setObjectName(base + QStringLiteral("On"));
        m_on[role]->setToolTip(tr("Turn %1 off on this computer only: the control is then free for the show.")
                                   .arg(text[role].name));
        m_on[role]->setAccessibleName(tr("%1 on this computer").arg(text[role].name));
        m_learn[role] = new QPushButton(tr("Learn…"), this);
        m_learn[role]->setObjectName(base + QStringLiteral("Learn"));
        m_learn[role]->setAccessibleName(tr("Learn %1").arg(text[role].name));
        grid->addWidget(what, r, 0);
        grid->addWidget(m_status[role], r, 1);
        grid->addWidget(m_on[role], r, 2, Qt::AlignTop);
        grid->addWidget(m_learn[role], r, 3, Qt::AlignTop);
        connect(m_learn[role], &QPushButton::clicked, this, [this, role]() { startLearn(role); });
        connect(m_on[role], &QCheckBox::toggled, this, [this, role](bool on) {
            if (m_desk->binding(role).on == on)
                return;
            if (!unlocked(tr("Enter the show's PIN to change this computer's desk.")))
            {
                refresh();
                return;
            }
            m_desk->setOn(role, on);
        });
        ++r;
    }

    m_learnLine = new QLabel(this);
    m_learnLine->setObjectName(QStringLiteral("deskLearnLine"));
    m_learnLine->setWordWrap(true);
    m_learnLine->hide();
    grid->addWidget(m_learnLine, r++, 0, 1, 4);

    m_warning = new QLabel(this);
    m_warning->setObjectName(QStringLiteral("deskWarning"));
    m_warning->setWordWrap(true);
    m_warning->setStyleSheet(QStringLiteral("color: #a06000;"));
    m_warning->hide();
    grid->addWidget(m_warning, r++, 0, 1, 4);

    QHBoxLayout *files = new QHBoxLayout();
    files->addStretch(1);
    m_export = new QPushButton(tr("Export…"), this);
    m_export->setObjectName(QStringLiteral("deskExport"));
    m_export->setToolTip(tr("Save this computer's desk to a file, to set up another computer the same way."));
    m_import = new QPushButton(tr("Import…"), this);
    m_import->setObjectName(QStringLiteral("deskImport"));
    m_import->setToolTip(tr("Set this computer's desk from a file exported on another computer."));
    files->addWidget(m_export);
    files->addWidget(m_import);
    grid->addLayout(files, r++, 0, 1, 4);
    connect(m_export, &QPushButton::clicked, this, &DeskSetupPanel::slotExport);
    connect(m_import, &QPushButton::clicked, this, &DeskSetupPanel::slotImport);

    connect(m_desk, &DeskRecord::changed, this, &DeskSetupPanel::refresh);
    connect(m_desk, &DeskRecord::learned, this, [this](DeskRecord::Role role) {
        m_learnLine->setText(tr("%1 learned: %2.").arg(DeskRecord::roleName(role), m_desk->stateText(role)));
        m_learnLine->show();
        refresh();
    });
    connect(m_desk, &DeskRecord::learnRefused, this, [this](const QString &why) {
        m_learnLine->setText(why);
        m_learnLine->show();
    });
    connect(m_desk, &DeskRecord::learnCancelled, this, [this](const QString &why) {
        m_learnLine->setText(why);
        m_learnLine->setVisible(!why.isEmpty());
        refresh();
    });
    connect(m_desk, &DeskRecord::learnConfirming, this, [this]() {
        m_learnLine->setText(tr("Release it and press it again."));
        m_learnLine->show();
    });
    connect(m_desk, &DeskRecord::grandMasterWaitingChanged, this, &DeskSetupPanel::refresh);
    refresh();
}

DeskSetupPanel::~DeskSetupPanel()
{
    if (m_desk != nullptr)
        m_desk->cancelLearn();
}

void DeskSetupPanel::hideEvent(QHideEvent *event)
{
    /* A learn left open on a panel no one sees would take the next control
       touched (another tab, the Run tab, Operate): it ends with the panel. */
    if (m_desk != nullptr && m_desk->isLearning())
        m_desk->cancelLearn();
    QWidget::hideEvent(event);
}

bool DeskSetupPanel::unlocked(const QString &why)
{
    if (!m_desk->isLocked())
        return true;
    const bool ok = s_pinAsker ? s_pinAsker(this, why)
                               : ShowSettingsDialog::askForPin(this, m_doc->operateSettings()->pinHash(), why);
    if (ok)
        m_desk->setUnlocked(true);
    return ok;
}

void DeskSetupPanel::startLearn(DeskRecord::Role role)
{
    if (m_desk->isLearning() && m_desk->learningRole() == role)
    {
        m_desk->cancelLearn();
        m_learnLine->hide();
        refresh();
        return;
    }
    if (!unlocked(tr("Enter the show's PIN to change this computer's desk.")))
        return;
    if (!m_desk->learn(role))
        return;
    m_learnLine->setText(DeskRecord::takesFader(role)
                             ? tr("Move the fader for %1. Press Cancel to stop.").arg(DeskRecord::roleName(role))
                             : role == DeskRecord::Stop
                                   ? tr("Press the button for STOP, release it, and press it again.")
                                   : tr("Press the button for %1.").arg(DeskRecord::roleName(role)));
    m_learnLine->show();
    refresh();
}

void DeskSetupPanel::refresh()
{
    for (DeskRecord::Role role : DeskRecord::roles())
    {
        QString status = m_desk->stateText(role);
        if (role == DeskRecord::GrandMaster && m_desk->grandMasterWaiting())
            status += QStringLiteral("\n") + m_desk->grandMasterHint();
        if (role == DeskRecord::Go && m_desk->isLive(DeskRecord::Go))
            status += QStringLiteral("\n") + m_desk->goTargetText();
        m_status[role]->setText(status);
        const DeskRecord::Binding b = m_desk->binding(role);
        m_on[role]->blockSignals(true);
        m_on[role]->setChecked(b.isSet() && b.on);
        m_on[role]->setEnabled(b.isSet());
        m_on[role]->blockSignals(false);
        const bool learning = m_desk->isLearning() && m_desk->learningRole() == role;
        m_learn[role]->setText(learning ? tr("Cancel") : tr("Learn…"));
    }
    const QString dup = m_desk->duplicateWarning();
    m_warning->setText(dup);
    m_warning->setVisible(!dup.isEmpty());
    m_export->setEnabled(m_desk->anySet());
}

void DeskSetupPanel::slotExport()
{
    const QString path = s_fileAsker ? s_fileAsker(this, true)
                                     : QFileDialog::getSaveFileName(this, tr("Export This Computer's Desk"),
                                                                    QString(), tr("Desk files (*.qlcdesk)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_desk->exportTo(path, &error) && !s_fileAsker)
        QMessageBox::warning(this, tr("Export This Computer's Desk"), error);
}

void DeskSetupPanel::slotImport()
{
    if (!unlocked(tr("Enter the show's PIN to change this computer's desk.")))
        return;
    const QString path = s_fileAsker ? s_fileAsker(this, false)
                                     : QFileDialog::getOpenFileName(this, tr("Import This Computer's Desk"),
                                                                    QString(), tr("Desk files (*.qlcdesk)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_desk->importFrom(path, &error) && !s_fileAsker)
        QMessageBox::warning(this, tr("Import This Computer's Desk"), error);
    refresh();
}
