/*
  Q Light Controller Plus - qlcconsole
  desksetuppanel.h

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

#ifndef DESKSETUPPANEL_H
#define DESKSETUPPANEL_H

#include <QWidget>
#include <functional>

#include "deskrecord.h"
#include "qlcui_export.h"

class QPushButton;
class QCheckBox;
class QLabel;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * Connections ▸ Control surfaces ▸ This computer's desk: the four static
 * roles (STOP, Grand Master, GO, Back) of DeskRecord, one row each -- what
 * it is on now, an On tick (Q317) and Learn… -- with Export… and Import…
 * of the record (a small file to carry to another computer). Every change
 * asks for the show's PIN when it has one (Q319). No settings of its own.
 */
class QLC_UI_EXPORT DeskSetupPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit DeskSetupPanel(Doc *doc, QWidget *parent = nullptr);
    ~DeskSetupPanel() override;

    QPushButton *learnButton(DeskRecord::Role role) const { return m_learn[role]; }
    QCheckBox *onBox(DeskRecord::Role role) const { return m_on[role]; }
    QLabel *statusLabel(DeskRecord::Role role) const { return m_status[role]; }
    QLabel *learnLine() const { return m_learnLine; }
    QPushButton *exportButton() const { return m_export; }
    QPushButton *importButton() const { return m_import; }

    /** Asks for the show's PIN (a test's answer instead of the dialog). */
    static void setPinAsker(const std::function<bool(QWidget *, const QString &why)> &asker);
    /** The file Export… / Import… use (a test's, instead of the file dialog). */
    static void setFileAsker(const std::function<QString(QWidget *, bool save)> &asker);

public slots:
    void refresh();

protected:
    /** Hidden (another tab, another page, the window): a learn ends. */
    void hideEvent(QHideEvent *event) override;

private:
    /** The PIN asked when the show has one; true when the change may go on. */
    bool unlocked(const QString &why);
    void startLearn(DeskRecord::Role role);
    void slotExport();
    void slotImport();

private:
    Doc *m_doc;
    DeskRecord *m_desk;
    QLabel *m_status[DeskRecord::RoleCount] = {};
    QCheckBox *m_on[DeskRecord::RoleCount] = {};
    QPushButton *m_learn[DeskRecord::RoleCount] = {};
    QLabel *m_learnLine = nullptr;
    QLabel *m_warning = nullptr;
    QPushButton *m_export = nullptr;
    QPushButton *m_import = nullptr;
};

/** @} */

#endif
