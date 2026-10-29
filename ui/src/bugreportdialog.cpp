/*
  Q Light Controller Plus
  bugreportdialog.cpp

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

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QClipboard>
#include <QGroupBox>
#include <QLineEdit>
#include <QFileInfo>
#include <QSysInfo>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QSet>

#include "bugreportdialog.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcconfig.h"
#include "function.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

/*****************************************************************************
 * BugReport: URL, report text, scrubbing
 *****************************************************************************/

namespace
{
    const QString kCutNote = QStringLiteral(
        "\n\n[cut short: the full details are on your clipboard -- paste them into the issue]");
    const QString kTitlePrefix = QStringLiteral("[Bug] ");
    const int kMaxTitle = 200;

    QString encodedUrl(const BugReport::Fields &f)
    {
        QString title = f.title.trimmed();
        if (!title.startsWith(kTitlePrefix))
            title.prepend(kTitlePrefix);
        title = title.left(kMaxTitle);

        // Encoded by hand: QUrlQuery leaves '+' alone, which GitHub reads as
        // a space ("C++" came back "C  ").
        const QList<QPair<QString, QString>> items = {
            { QStringLiteral("template"), BugReport::templateName() },
            { QStringLiteral("title"), title },
            { QStringLiteral("what-happened"), f.whatHappened },
            { QStringLiteral("expected"), f.expected },
            { QStringLiteral("steps"), f.steps },
            { QStringLiteral("version"), f.version },
            { QStringLiteral("details"), f.details } };
        QStringList parts;
        for (const auto &it : items)
        {
            if (it.second.isEmpty())
                continue;
            parts << it.first + QLatin1Char('=')
                     + QString::fromLatin1(QUrl::toPercentEncoding(it.second));
        }
        return BugReport::newIssueBase() + QLatin1Char('?') + parts.join(QLatin1Char('&'));
    }

    QString wordBounded(const QString &literal)
    {
        return QStringLiteral("(?<![\\w])") + QRegularExpression::escape(literal)
               + QStringLiteral("(?![\\w])");
    }
}

QString BugReport::newIssueBase()
{
    return QStringLiteral("https://github.com/sandinak/qlcconsole/issues/new");
}

QString BugReport::templateName()
{
    return QStringLiteral("bug_report.yml");
}

QString BugReport::scrub(const QString &text, const QStringList &secrets)
{
    QString out = text;

    // Longest first: the show's path before its file name, a fixture
    // "Spot 10" before "Spot 1".
    QStringList paths, names;
    for (const QString &s : secrets)
    {
        if (s.contains(QLatin1Char('/')) || s.contains(QLatin1Char('\\')))
            paths << s;
        else if (s.size() >= 2)   // one letter would eat every word holding it
            names << s;
    }
    auto longestFirst = [](const QString &a, const QString &b) { return a.size() > b.size(); };
    std::sort(paths.begin(), paths.end(), longestFirst);
    std::sort(names.begin(), names.end(), longestFirst);

    // Paths go wherever they are, before the home folder turns into ~.
    for (const QString &s : paths)
        out.replace(s, QStringLiteral("[removed]"), Qt::CaseInsensitive);

    // The home folder as ~, both spellings.
    const QString home = QDir::homePath();
    if (home.size() > 1)
    {
        out.replace(QDir::toNativeSeparators(home), QStringLiteral("~"), Qt::CaseInsensitive);
        out.replace(home, QStringLiteral("~"), Qt::CaseInsensitive);
    }
    // Anyone else's home folder: the user name goes.
    static const QRegularExpression otherHome(
        QStringLiteral("((?:/Users/|/home/|[A-Za-z]:\\\\Users\\\\|[A-Za-z]:/Users/))[^/\\\\\\s]+"));
    out.replace(otherHome, QStringLiteral("\\1[user]"));

    // Names only as whole words, so a fixture called "Ed" leaves "Edited" alone.
    for (const QString &s : names)
        out.replace(QRegularExpression(wordBounded(s), QRegularExpression::CaseInsensitiveOption),
                    QStringLiteral("[removed]"));

    // "password=...", "psk: ..." and the like.
    static const QRegularExpression secretValue(
        QStringLiteral("\\b(password|passwd|pwd|passphrase|psk|secret|token)\\b(\\s*[:=]\\s*)\\S+"),
        QRegularExpression::CaseInsensitiveOption);
    out.replace(secretValue, QStringLiteral("\\1\\2[removed]"));

    // IPv4, then IPv6 (and MAC) addresses: anything hex with "::" or five
    // colons. "12:30:45" and "App::init" stay.
    static const QRegularExpression ipv4(
        QStringLiteral("(?<![\\d.])(?:\\d{1,3}\\.){3}\\d{1,3}(?![\\d.])"));
    out.replace(ipv4, QStringLiteral("[ip]"));
    static const QRegularExpression ipv6(
        QStringLiteral("(?<![\\w:])(?=[0-9A-Fa-f:]*::|(?:[0-9A-Fa-f]{1,4}[:-]){5})"
                       "[0-9A-Fa-f:-]{2,}[0-9A-Fa-f:](?:%\\w+)?(?![\\w:])"));
    out.replace(ipv6, QStringLiteral("[ip]"));
    return out;
}

QUrl BugReport::issueUrl(const Fields &fields, bool *trimmed, int maxLength)
{
    Fields f = fields;
    bool cut = false;

    // Cut the details first, then the steps, then what happened and what was
    // expected: each to the longest start that still fits, with the note.
    QString *order[] = { &f.details, &f.steps, &f.whatHappened, &f.expected };
    for (QString *field : order)
    {
        if (encodedUrl(f).size() <= maxLength)
            break;
        const QString full = *field;
        int lo = 0, hi = full.size();
        *field = kCutNote.trimmed();
        if (encodedUrl(f).size() > maxLength)
        {
            cut = true;
            continue;   // even the note alone doesn't fit: cut the next one too
        }
        while (lo < hi)
        {
            const int mid = (lo + hi + 1) / 2;
            *field = full.left(mid) + kCutNote;
            if (encodedUrl(f).size() <= maxLength)
                lo = mid;
            else
                hi = mid - 1;
        }
        *field = lo > 0 ? full.left(lo) + kCutNote : kCutNote.trimmed();
        cut = true;
    }

    if (trimmed != nullptr)
        *trimmed = cut;
    return QUrl::fromEncoded(encodedUrl(f).toLatin1(), QUrl::StrictMode);
}

QString BugReport::reportText(const Fields &f)
{
    QString title = f.title.trimmed();
    if (!title.startsWith(kTitlePrefix))
        title.prepend(kTitlePrefix);

    QStringList out;
    out << title << QString();
    auto section = [&out](const QString &heading, const QString &body) {
        out << QStringLiteral("### ") + heading << QString()
            << (body.trimmed().isEmpty() ? QStringLiteral("_No response_") : body.trimmed())
            << QString();
    };
    section(QStringLiteral("What happened?"), f.whatHappened);
    section(QStringLiteral("What did you expect?"), f.expected);
    section(QStringLiteral("Steps to reproduce"), f.steps);
    section(QStringLiteral("Version"), f.version);
    out << QStringLiteral("### Details") << QString()
        << QStringLiteral("```text") << f.details.trimmed() << QStringLiteral("```");
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/*****************************************************************************
 * BugReportDialog
 *****************************************************************************/

namespace
{
    BugReportDialog::UrlOpener &urlOpener()
    {
        static BugReportDialog::UrlOpener opener = [](const QUrl &url) {
            return QDesktopServices::openUrl(url);
        };
        return opener;
    }

    const QString kVersion = QStringLiteral("version");
    const QString kOs = QStringLiteral("os");
    const QString kQt = QStringLiteral("qt");
    const QString kView = QStringLiteral("view");
    const QString kCounts = QStringLiteral("counts");
    const QString kPlugins = QStringLiteral("plugins");
    const QString kLayout = QStringLiteral("layout");
    const QString kLog = QStringLiteral("log");

    QString baseVersion()
    {
        const QString build = QStringLiteral(APPVERSION);
        const int plus = build.indexOf(QLatin1Char('+'));
        return plus > 0 ? build.left(plus) : build;
    }

    QString versionLine()
    {
        return QStringLiteral("%1 (build %2, %3)")
            .arg(QStringLiteral(QLCCONSOLE_VERSION), QStringLiteral(APPVERSION),
                 QStringLiteral(QLCCONSOLE_GITREV));
    }
}

BugReportDialog::UrlOpener BugReportDialog::setUrlOpener(const UrlOpener &opener)
{
    UrlOpener old = urlOpener();
    urlOpener() = opener;
    return old;
}

QStringList BugReportDialog::recentLogLines(int count)
{
    // Only qlcconsole --log keeps a log (main.cpp, ~/qlcconsole.log).
    QFile file(QDir::homePath() + QStringLiteral("/qlcconsole.log"));
    if (!file.open(QIODevice::ReadOnly))
        return QStringList();
    const qint64 tail = 64 * 1024;
    if (file.size() > tail)
        file.seek(file.size() - tail);
    QStringList lines = QString::fromLocal8Bit(file.readAll())
                            .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (file.size() > tail && !lines.isEmpty())
        lines.removeFirst();   // likely cut mid-line
    return lines.mid(qMax(0, lines.size() - count));
}

QStringList BugReportDialog::partKeys()
{
    return { kVersion, kOs, kQt, kView, kCounts, kPlugins, kLayout, kLog };
}

QStringList BugReportDialog::secrets() const
{
    QStringList out;
    if (!m_context.showPath.isEmpty())
    {
        const QFileInfo fi(m_context.showPath);
        out << m_context.showPath << QDir::toNativeSeparators(m_context.showPath)
            << fi.fileName();
        // Its folder too, unless that is the home folder itself (which
        // becomes ~) or the root.
        const QString folder = fi.absolutePath();
        if (QDir(folder) != QDir::home() && !QDir(folder).isRoot())
            out << folder << QDir::toNativeSeparators(folder);
        if (fi.completeBaseName().size() >= 3)
            out << fi.completeBaseName();
    }
    // The user's name, wherever it shows up.
    const QString user = QDir(QDir::homePath()).dirName();
    if (user.size() >= 2)
        out << user;
    for (const char *var : { "USER", "USERNAME", "LOGNAME" })
    {
        const QString v = qEnvironmentVariable(var);
        if (v.size() >= 2)
            out << v;
    }
    if (m_context.doc != nullptr)
    {
        for (Fixture *fxi : m_context.doc->fixtures())
            if (fxi->name().size() >= 2)
                out << fxi->name();
        for (FixtureGroup *grp : m_context.doc->fixtureGroups())
            if (grp->name().size() >= 2)
                out << grp->name();
    }
    out.removeDuplicates();
    return out;
}

QString BugReportDialog::partText(const QString &key) const
{
    return m_parts.value(key);
}

BugReportDialog::BugReportDialog(const BugReportContext &context, QWidget *parent)
    : QDialog(parent)
    , m_context(context)
{
    setWindowTitle(tr("Report a Bug"));
    setObjectName(QStringLiteral("bugReportDialog"));
    setProperty("helpTopic", QStringLiteral("troubleshooting.md#reporting-a-bug"));

    /* What the app knows, each part scrubbed once here. */
    const QStringList hidden = secrets();
    QStringList raw;
    m_parts[kVersion] = QStringLiteral("qlcconsole %1, based on QLC+ %2")
                            .arg(versionLine(), baseVersion());
    m_parts[kOs] = QStringLiteral("OS: %1 (%2; built for %3)")
                       .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(),
                            QSysInfo::buildCpuArchitecture());
    m_parts[kQt] = QStringLiteral("Qt %1 (built with %2), platform %3")
                       .arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR),
                            QGuiApplication::platformName());
    if (!m_context.viewName.isEmpty())
        m_parts[kView] = QStringLiteral("View: %1").arg(m_context.viewName);

    if (Doc *doc = m_context.doc)
    {
        QMap<QString, int> byType;
        for (Function *f : doc->functions())
            byType[Function::typeToString(f->type())]++;
        QStringList types;
        for (auto it = byType.constBegin(); it != byType.constEnd(); ++it)
            types << QStringLiteral("%1 %2").arg(it.key()).arg(it.value());
        const InputOutputMap *ioMap = doc->inputOutputMap();
        m_parts[kCounts] = QStringLiteral("Show: %1 fixtures, %2 fixture groups, %3 universes, %4 functions%5")
                               .arg(doc->fixtures().size())
                               .arg(doc->fixtureGroups().size())
                               .arg(ioMap != nullptr ? int(ioMap->universesCount()) : 0)
                               .arg(doc->functions().size())
                               .arg(types.isEmpty() ? QString()
                                                    : QStringLiteral(" (%1)").arg(types.join(QStringLiteral(", "))));

        // Plugin names only: never a line, a device or an address.
        QSet<QString> plugins;
        if (ioMap != nullptr)
        {
            for (Universe *u : ioMap->universes())
            {
                if (u == nullptr)
                    continue;
                if (InputPatch *ip = u->inputPatch())
                    plugins.insert(ip->pluginName());
                for (int i = 0; i < u->outputPatchesCount(); i++)
                    if (OutputPatch *op = u->outputPatch(i))
                        plugins.insert(op->pluginName());
                if (OutputPatch *fb = u->feedbackPatch())
                    plugins.insert(fb->pluginName());
            }
        }
        plugins.remove(QString());
        plugins.remove(QStringLiteral("None"));
        QStringList names(plugins.begin(), plugins.end());
        names.sort(Qt::CaseInsensitive);
        m_parts[kPlugins] = QStringLiteral("I/O plugins in use: %1")
                                .arg(names.isEmpty() ? QStringLiteral("none")
                                                     : names.join(QStringLiteral(", ")));
    }
    if (m_context.layoutReport)
        m_parts[kLayout] = m_context.layoutReport().trimmed();
    if (!m_context.logLines.isEmpty())
        m_parts[kLog] = QStringLiteral("Recent log lines:\n") + m_context.logLines.join(QLatin1Char('\n'));

    for (auto it = m_parts.begin(); it != m_parts.end(); ++it)
        it.value() = BugReport::scrub(it.value(), hidden);

    /* The form. */
    QVBoxLayout *lay = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    m_title = new QLineEdit(this);
    m_title->setObjectName(QStringLiteral("bugTitle"));
    m_title->setPlaceholderText(tr("A short summary, e.g. \"Chaser skips a step after Undo\""));
    form->addRow(tr("Title"), m_title);
    lay->addLayout(form);

    auto box = [this, lay](const QString &label, const QString &name, const QString &hint, int lines) {
        QLabel *l = new QLabel(label, this);
        lay->addWidget(l);
        QPlainTextEdit *e = new QPlainTextEdit(this);
        e->setObjectName(name);
        e->setPlaceholderText(hint);
        e->setTabChangesFocus(true);
        e->setMinimumHeight(e->fontMetrics().lineSpacing() * lines + 12);
        l->setBuddy(e);
        lay->addWidget(e);
        return e;
    };
    m_what = box(tr("What happened?"), QStringLiteral("bugWhat"),
                 tr("What you saw, and what you were doing when it happened."), 4);
    m_expected = box(tr("What did you expect?"), QStringLiteral("bugExpected"),
                     tr("What should have happened instead."), 2);
    m_steps = box(tr("Steps to reproduce (optional)"), QStringLiteral("bugSteps"),
                  tr("1. Open a show with ...\n2. Click ...\n3. See ..."), 3);

    QGroupBox *group = new QGroupBox(tr("Details that will be included"), this);
    QVBoxLayout *glay = new QVBoxLayout(group);
    QGridLayout *grid = new QGridLayout;
    const QList<QPair<QString, QString>> labels = {
        { kVersion, tr("App version and build") },
        { kOs, tr("Operating system and processor") },
        { kQt, tr("Qt version") },
        { kView, tr("Current view") },
        { kCounts, tr("Show size (counts only)") },
        { kPlugins, tr("I/O plugins in use (names only)") },
        { kLayout, tr("Layout report (window and screen sizes)") },
        { kLog, tr("Recent log lines (may mention your show: read them first)") } };
    int row = 0, col = 0;
    for (const auto &p : labels)
    {
        QCheckBox *cb = new QCheckBox(p.second, group);
        cb->setObjectName(QStringLiteral("bugPart_") + p.first);
        const bool has = m_parts.contains(p.first) && !m_parts.value(p.first).isEmpty();
        // The layout report and the log are long and the log can name
        // things: off until asked for.
        cb->setChecked(has && p.first != kLayout && p.first != kLog);
        cb->setEnabled(has);
        if (!has && p.first == kLog)
            cb->setToolTip(tr("qlcconsole keeps a log only when started with --log."));
        connect(cb, &QCheckBox::toggled, this, &BugReportDialog::rebuildDetails);
        m_checks[p.first] = cb;
        grid->addWidget(cb, row, col);
        if (++col == 2)
        {
            col = 0;
            row++;
        }
    }
    glay->addLayout(grid);
    m_details = new QPlainTextEdit(group);
    m_details->setObjectName(QStringLiteral("bugDetails"));
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_details->setMinimumHeight(m_details->fontMetrics().lineSpacing() * 6 + 12);
    m_details->setToolTip(tr("This is exactly what goes in the issue. Edit it as you like; "
                             "ticking a box rebuilds it."));
    glay->addWidget(m_details);
    QLabel *privacy = new QLabel(tr("Never included: the show's name or path, fixture names, "
                                    "your user name or home folder, IP addresses, passwords."), group);
    privacy->setWordWrap(true);
    privacy->setEnabled(false);
    glay->addWidget(privacy);
    lay->addWidget(group, 1);

    QLabel *how = new QLabel(tr("Nothing is sent by qlcconsole: your browser opens the GitHub issue "
                                "and you submit it there. Filing needs a GitHub account; without one, "
                                "Copy Report and send it another way."), this);
    how->setWordWrap(true);
    lay->addWidget(how);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("bugNote"));
    m_note->setWordWrap(true);
    m_note->setVisible(false);
    lay->addWidget(m_note);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_openBtn = buttons->addButton(tr("Open in GitHub"), QDialogButtonBox::AcceptRole);
    m_openBtn->setObjectName(QStringLiteral("bugOpen"));
    m_openBtn->setDefault(true);
    m_copyBtn = buttons->addButton(tr("Copy Report"), QDialogButtonBox::ActionRole);
    m_copyBtn->setObjectName(QStringLiteral("bugCopy"));
    // Open doesn't close the dialog: if the browser didn't come up, the
    // words are still here to copy.
    disconnect(buttons, &QDialogButtonBox::accepted, this, nullptr);
    connect(m_openBtn, &QPushButton::clicked, this, &BugReportDialog::openInGitHub);
    connect(m_copyBtn, &QPushButton::clicked, this, &BugReportDialog::copyReport);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttons);

    connect(m_title, &QLineEdit::textChanged, this, &BugReportDialog::updateButtons);
    connect(m_what, &QPlainTextEdit::textChanged, this, &BugReportDialog::updateButtons);

    rebuildDetails();
    updateButtons();
    resize(640, 760);
}

void BugReportDialog::rebuildDetails()
{
    QStringList out;
    for (const QString &key : partKeys())
    {
        QCheckBox *cb = m_checks.value(key);
        if (cb != nullptr && cb->isChecked() && !m_parts.value(key).isEmpty())
        {
            if ((key == kLayout || key == kLog) && !out.isEmpty())
                out << QString();
            out << m_parts.value(key);
        }
    }
    m_details->setPlainText(out.join(QLatin1Char('\n')));
}

void BugReportDialog::updateButtons()
{
    m_openBtn->setEnabled(!m_title->text().trimmed().isEmpty()
                          && !m_what->toPlainText().trimmed().isEmpty());
}

BugReport::Fields BugReportDialog::fields() const
{
    // What the person typed is theirs, fixture names and all; only the show's
    // path, the home folder and addresses come out of it.
    QStringList pathSecrets;
    if (!m_context.showPath.isEmpty())
    {
        pathSecrets << m_context.showPath << QDir::toNativeSeparators(m_context.showPath)
                    << QFileInfo(m_context.showPath).fileName();
    }
    BugReport::Fields f;
    f.title = BugReport::scrub(m_title->text().trimmed(), pathSecrets);
    f.whatHappened = BugReport::scrub(m_what->toPlainText().trimmed(), pathSecrets);
    f.expected = BugReport::scrub(m_expected->toPlainText().trimmed(), pathSecrets);
    f.steps = BugReport::scrub(m_steps->toPlainText().trimmed(), pathSecrets);
    // The details box can be edited, so it is scrubbed again in full.
    f.details = BugReport::scrub(m_details->toPlainText().trimmed(), secrets());
    QCheckBox *v = m_checks.value(kVersion);
    if (v != nullptr && v->isChecked())
        f.version = versionLine();
    return f;
}

bool BugReportDialog::openInGitHub()
{
    const BugReport::Fields f = fields();
    bool trimmed = false;
    const QUrl url = BugReport::issueUrl(f, &trimmed);
    if (trimmed)
    {
        QGuiApplication::clipboard()->setText(BugReport::reportText(f));
        m_note->setText(tr("The report was too long for a link, so the issue opens with it cut short: "
                           "the full details are on your clipboard -- paste them into the issue."));
        m_note->setVisible(true);
    }
    else
    {
        m_note->setVisible(false);
    }
    return urlOpener()(url);
}

void BugReportDialog::copyReport()
{
    QGuiApplication::clipboard()->setText(BugReport::reportText(fields()));
    m_note->setText(tr("The report is on your clipboard."));
    m_note->setVisible(true);
}
