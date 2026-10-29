/*
  Q Light Controller Plus
  bugreportdialog.h

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

#ifndef BUGREPORTDIALOG_H
#define BUGREPORTDIALOG_H

#include <functional>
#include <QDialog>
#include <QStringList>
#include <QUrl>
#include <QMap>

#include "qlcui_export.h"

class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Help > Troubleshooting > Report a Bug: the pieces of a GitHub issue, and
 * the scrubbing that keeps a show's private details out of it. Nothing here
 * talks to the network: the issue page opens in the browser, prefilled
 * through the issue form's query parameters
 * (.github/ISSUE_TEMPLATE/bug_report.yml), and the person submits it there.
 */
namespace BugReport
{
    /** Where the issue form lives. */
    QLC_UI_EXPORT QString newIssueBase();
    /** The issue form's file name (the "template" query parameter). */
    QLC_UI_EXPORT QString templateName();
    /** Longest URL the dialog opens; browsers and GitHub cut beyond ~8k. */
    constexpr int MaxUrlLength = 7500;

    /** The issue's fields, keyed by the form's field ids. */
    struct QLC_UI_EXPORT Fields
    {
        QString title;
        QString whatHappened;   // what-happened
        QString expected;       // expected
        QString steps;          // steps
        QString details;        // details
        QString version;        // version
    };

    /** @p text with the home folder written as ~, IPv4/IPv6 addresses,
     *  "password=..."-style values and every string in @p secrets (the
     *  show's path and name, fixture names, the user name) taken out. */
    QLC_UI_EXPORT QString scrub(const QString &text, const QStringList &secrets);

    /** The new-issue URL for @p fields. If it would be longer than
     *  @p maxLength, the details (then the steps, what happened and what was
     *  expected) are cut, ending with a note that the full report is on the
     *  clipboard, and @p trimmed is set. */
    QLC_UI_EXPORT QUrl issueUrl(const Fields &fields, bool *trimmed = nullptr,
                                int maxLength = MaxUrlLength);

    /** The whole report as Markdown, as the issue would show it. */
    QLC_UI_EXPORT QString reportText(const Fields &fields);
}

/** What the dialog reads about the running app; App fills it in. */
struct QLC_UI_EXPORT BugReportContext
{
    Doc *doc = nullptr;
    /** The workspace view's name ("Classic QLC+", "qlcconsole", "Everything"). */
    QString viewName;
    /** The open show's path: never shown, only scrubbed out. */
    QString showPath;
    /** Help > Troubleshooting > Copy Layout Report's text, built on demand. */
    std::function<QString()> layoutReport;
    /** The last lines of the app's log, if it keeps one. */
    QStringList logLines;
};

class QLC_UI_EXPORT BugReportDialog : public QDialog
{
    Q_OBJECT

public:
    BugReportDialog(const BugReportContext &context, QWidget *parent = nullptr);

    /** The parts of "Details that will be included", in order. */
    static QStringList partKeys();

    /** Every string the report must not carry for this show. */
    QStringList secrets() const;

    /** The text of part @p key (already scrubbed), or empty if the app has
     *  nothing for it. */
    QString partText(const QString &key) const;

    /** The fields as they stand now (details: the editable box). */
    BugReport::Fields fields() const;

    /** What opens a URL. The default is QDesktopServices::openUrl; tests put
     *  a recorder here so no browser ever starts. Returns the old one. */
    using UrlOpener = std::function<bool(const QUrl &)>;
    static UrlOpener setUrlOpener(const UrlOpener &opener);

    /** The last lines of the log file the app keeps with --log, if any. */
    static QStringList recentLogLines(int count = 40);

public slots:
    /** Open the prefilled issue in the browser; a cut report goes to the
     *  clipboard whole. True if the opener accepted the URL. */
    bool openInGitHub();
    /** The whole report to the clipboard. */
    void copyReport();
    /** Rebuild the details box from the ticked parts. */
    void rebuildDetails();

private:
    void updateButtons();

private:
    BugReportContext m_context;
    QMap<QString, QString> m_parts;          // key -> scrubbed text
    QMap<QString, QCheckBox *> m_checks;     // key -> its tick box
    QLineEdit *m_title;
    QPlainTextEdit *m_what;
    QPlainTextEdit *m_expected;
    QPlainTextEdit *m_steps;
    QPlainTextEdit *m_details;
    QLabel *m_note;
    QPushButton *m_openBtn;
    QPushButton *m_copyBtn;
};

/** @} */

#endif
