/*
  Q Light Controller Plus - qlcconsole
  configimportdialog.h

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

#ifndef CONFIGIMPORTDIALOG_H
#define CONFIGIMPORTDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QList>

#include "configimport.h"

class QDialogButtonBox;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTextBrowser;
class QCheckBox;
class QComboBox;
class QSpinBox;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * File ▸ Import ▸ Universes and I/O Setup (docs/design/AI_ASSIST.md section
 * 5b): pick what to take from another show file, map it, see the problems,
 * warnings and the changes it would make, and Import. It is a form for the
 * `import_config` operation's arguments: arguments() is what the registry
 * runs (as one undo step). Works with AI Assist off; AI Assist proposes the
 * same operation. Nothing is changed until Import.
 */
class ConfigImportDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigImportDialog(QWidget *parent, Doc *doc, const QString &file);

    /** The operation's arguments as the form has them. */
    QJsonObject arguments() const;
    /** The last preview (problems stop the Import button). */
    const ConfigImport::Plan &plan() const { return m_plan; }

    /** Test seams: the form's parts. */
    QPlainTextEdit *preview() const { return m_preview; }
    QPushButton *importButton() const { return m_import; }

private slots:
    void refresh();

private:
    static QMap<QString, QString> parseLines(const QString &text);

    Doc *m_doc;
    QString m_file;
    ConfigImport::Source m_source;
    ConfigImport::Plan m_plan;
    QList<QCheckBox *> m_sections;
    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_addresses = nullptr;
    QPlainTextEdit *m_plugins = nullptr;
    QComboBox *m_collision = nullptr;
    QComboBox *m_missing = nullptr;
    QPlainTextEdit *m_preview = nullptr;
    QPushButton *m_import = nullptr;
};

/** @} */

#endif
