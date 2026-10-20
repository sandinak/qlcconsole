/*
  Q Light Controller Plus
  updatedialog.cpp

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
#include <QProgressBar>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

#include "updateinstaller.h"
#include "updatemanager.h"
#include "updatedialog.h"
#include "qlcconfig.h"

UpdateDialog::UpdateDialog(UpdateManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setObjectName(QStringLiteral("updateDialog"));
    setProperty("helpTopic", QStringLiteral("updates.md"));
    setWindowTitle(tr("Software Update"));
    setModal(false);
    resize(620, 520);

    QVBoxLayout *lay = new QVBoxLayout(this);
    m_heading = new QLabel(this);
    m_heading->setObjectName(QStringLiteral("updateHeading"));
    QFont f = m_heading->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.25);
    m_heading->setFont(f);
    m_heading->setWordWrap(true);
    lay->addWidget(m_heading);

    m_detail = new QLabel(this);
    m_detail->setObjectName(QStringLiteral("updateDetail"));
    m_detail->setWordWrap(true);
    m_detail->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_detail->setOpenExternalLinks(true);
    lay->addWidget(m_detail);

    m_notes = new QTextBrowser(this);
    m_notes->setObjectName(QStringLiteral("updateNotes"));
    m_notes->setOpenExternalLinks(true);
    lay->addWidget(m_notes, 1);

    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("updateProgress"));
    m_progress->setRange(0, 100);
    lay->addWidget(m_progress);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("updateHint"));
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_hint);

    QDialogButtonBox *box = new QDialogButtonBox(this);
    m_skip = box->addButton(tr("Skip This Version"), QDialogButtonBox::DestructiveRole);
    m_skip->setObjectName(QStringLiteral("updateSkip"));
    m_later = box->addButton(tr("Later"), QDialogButtonBox::RejectRole);
    m_later->setObjectName(QStringLiteral("updateLater"));
    m_open = box->addButton(tr("Open Download"), QDialogButtonBox::ActionRole);
    m_open->setObjectName(QStringLiteral("updateOpen"));
    m_restart = box->addButton(tr("Restart and Install"), QDialogButtonBox::ActionRole);
    m_restart->setObjectName(QStringLiteral("updateRestart"));
    m_install = box->addButton(tr("Install"), QDialogButtonBox::AcceptRole);
    m_install->setObjectName(QStringLiteral("updateInstall"));
    lay->addWidget(box);

    connect(m_later, &QPushButton::clicked, this, &QDialog::close);
    connect(m_skip, &QPushButton::clicked, this, [this]() {
        m_manager->skipVersion();
        close();
    });
    connect(m_install, &QPushButton::clicked, this, [this]() {
        if (UpdateInstaller::method() == UpdateInstaller::NoInstall)
            m_manager->openReleasePage();
        else
            m_manager->install();
    });
    connect(m_open, &QPushButton::clicked, this, [this]() { m_manager->openDownload(); });
    connect(m_restart, &QPushButton::clicked, this, [this]() {
        if (m_manager->restartNow())
            close();
    });

    connect(m_manager, &UpdateManager::stateChanged, this, &UpdateDialog::refresh);
    connect(m_manager->checker(), &UpdateChecker::statusChanged, this, &UpdateDialog::refresh);

    // Whether the show is quiet enough to restart changes on its own (a
    // cue goes, Operate mode): look again while the dialog is open.
    m_safeTimer = new QTimer(this);
    m_safeTimer->setInterval(1000);
    connect(m_safeTimer, &QTimer::timeout, this, &UpdateDialog::refresh);
    m_safeTimer->start();

    refresh();
}

UpdateDialog::~UpdateDialog()
{
}

void UpdateDialog::refresh()
{
    const UpdateManager::State state = m_manager->state();
    const UpdateChecker *checker = m_manager->checker();
    const UpdateRelease rel = m_manager->release();
    QString why;
    const UpdateInstaller::Method how = UpdateInstaller::method(&why);
    QString busy;
    const bool safe = m_manager->canInstallNow(&busy);

    m_install->setVisible(false);
    m_restart->setVisible(false);
    m_open->setVisible(false);
    m_skip->setVisible(false);
    m_progress->setVisible(false);
    m_notes->setVisible(rel.isValid());
    m_later->setText(tr("Later"));

    if (rel.isValid() && m_notesShown != rel.tag)
    {
        m_notesShown = rel.tag;
        m_notes->setMarkdown(rel.body.isEmpty() ? tr("*No release notes.*") : rel.body);
    }

    const QString current = QStringLiteral(QLCCONSOLE_VERSION);
    QString hint;
    switch (state)
    {
        case UpdateManager::NoUpdate:
        {
            m_later->setText(tr("Close"));
            switch (checker->status())
            {
                case UpdateChecker::Checking:
                    m_heading->setText(tr("Checking for updates…"));
                    m_detail->clear();
                    break;
                case UpdateChecker::UpToDate:
                    m_heading->setText(tr("qlcconsole is up to date"));
                    m_detail->setText(tr("This is %1, the newest on the %2 channel.")
                                          .arg(current, UpdateChecker::channel() == UpdateChecker::Stable
                                                        ? tr("Stable") : tr("Pre-release")));
                    break;
                case UpdateChecker::Idle:
                    m_heading->setText(tr("Software Update"));
                    m_detail->setText(tr("This is qlcconsole %1.").arg(current));
                    break;
                default:
                    m_heading->setText(tr("Could not check for updates"));
                    m_detail->setText(checker->errorText());
                    break;
            }
            break;
        }
        case UpdateManager::Available:
        case UpdateManager::Failed:
            m_heading->setText(state == UpdateManager::Failed
                               ? tr("qlcconsole %1 was not installed").arg(rel.version)
                               : tr("qlcconsole %1 is available").arg(rel.version));
            m_detail->setText(state == UpdateManager::Failed
                              ? m_manager->message()
                              : tr("You have %1. <a href=\"%2\">%3</a> on GitHub.")
                                    .arg(current, rel.pageUrl.toString().toHtmlEscaped(),
                                         rel.name.toHtmlEscaped()));
            m_skip->setVisible(true);
            m_install->setVisible(true);
            if (how == UpdateInstaller::NoInstall)
            {
                m_install->setText(tr("Open Release Page"));
                hint = tr("Built from source (Debian, Ubuntu, Raspberry Pi): update the checkout "
                          "(git pull) and rebuild. On a PC, the release page has an AppImage.");
            }
            else
            {
                m_install->setText(state == UpdateManager::Failed ? tr("Try Again") : tr("Install"));
                hint = how == UpdateInstaller::InPlace
                    ? tr("Install downloads and checks the update now; it is put in place when "
                         "you quit qlcconsole, or restart from here -- never during a show.")
                    : tr("%1 Install downloads and checks it; you then install it yourself.").arg(why);
            }
            break;
        case UpdateManager::Downloading:
        case UpdateManager::Preparing:
            m_heading->setText(state == UpdateManager::Downloading
                               ? tr("Downloading qlcconsole %1").arg(rel.version)
                               : tr("Checking qlcconsole %1").arg(rel.version));
            m_detail->setText(m_manager->message());
            m_progress->setVisible(true);
            if (state == UpdateManager::Preparing)
                m_progress->setRange(0, 0);
            else
            {
                m_progress->setRange(0, 100);
                m_progress->setValue(m_manager->progressPercent());
            }
            hint = tr("The show keeps running; nothing changes until you quit.");
            break;
        case UpdateManager::Ready:
            m_heading->setText(tr("qlcconsole %1 is ready to install").arg(rel.version));
            m_detail->setText(m_manager->message());
            m_restart->setVisible(true);
            m_restart->setEnabled(safe);
            m_later->setText(tr("When I Quit"));
            hint = safe ? tr("Restart and Install saves nothing for you: you are asked about "
                             "unsaved changes first, then qlcconsole reopens on this show.")
                        : tr("Not now: %1 It installs when you quit.").arg(busy);
            break;
        case UpdateManager::Manual:
            m_heading->setText(tr("qlcconsole %1 is downloaded").arg(rel.version));
            m_detail->setText(m_manager->message());
            m_open->setVisible(true);
            m_skip->setVisible(true);
            break;
    }
    m_hint->setText(hint);
    m_hint->setVisible(hint.isEmpty() == false);
}
