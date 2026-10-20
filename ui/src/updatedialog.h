/*
  Q Light Controller Plus
  updatedialog.h

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

#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>

#include "qlcui_export.h"

class UpdateManager;
class QTextBrowser;
class QProgressBar;
class QPushButton;
class QLabel;
class QTimer;

/** @addtogroup ui UI
 * @{
 */

/**
 * Software Update: what was found, its release notes (the release's own
 * text, as Markdown), and Install / Restart Now / Later / Skip This
 * Version. Not modal -- the show keeps running behind it; Restart Now is
 * only enabled while nothing is on stage.
 */
class QLC_UI_EXPORT UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(UpdateManager *manager, QWidget *parent = nullptr);
    ~UpdateDialog();

private slots:
    void refresh();

private:
    UpdateManager *m_manager;
    QLabel *m_heading;
    QLabel *m_detail;
    QTextBrowser *m_notes;
    QProgressBar *m_progress;
    QLabel *m_hint;
    QPushButton *m_install;
    QPushButton *m_restart;
    QPushButton *m_open;
    QPushButton *m_skip;
    QPushButton *m_later;
    QTimer *m_safeTimer;
    QString m_notesShown;
};

/** @} */

#endif
