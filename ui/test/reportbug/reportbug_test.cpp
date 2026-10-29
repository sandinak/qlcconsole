/*
  Q Light Controller Plus
  reportbug_test.cpp

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

#include <QtTest>
#include <QPlainTextEdit>
#include <QApplication>
#include <QPushButton>
#include <QClipboard>
#include <QCheckBox>
#include <QLineEdit>
#include <QMenuBar>
#include <QUrlQuery>
#include <QLabel>
#include <QMenu>
#include <QDir>

#include "bugreportdialog.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private

#include "reportbug_test.h"

namespace
{
    QString showPath()
    {
        return QDir::homePath() + QStringLiteral("/Shows/Smith Wedding 2026.qxw");
    }

    template <class T> T *child(QWidget *w, const char *name)
    {
        T *c = w->findChild<T *>(QString::fromLatin1(name));
        if (c == nullptr)
            qWarning("no %s", name);
        return c;
    }
}

void ReportBug_Test::initTestCase()
{
    // The home folder is whatever QDir::homePath() says (on Unix the test
    // runner's own HOME; on Windows the profile, which HOME doesn't move).
    qputenv("USER", "showowner");
    QVERIFY(QDir::homePath().size() > 1);
    // Never a real browser: every URL is only recorded.
    BugReportDialog::setUrlOpener([this](const QUrl &url) {
        m_opened << url;
        return true;
    });
}

void ReportBug_Test::init()
{
    m_opened.clear();
    m_doc = new Doc(this);
    Fixture *f = new Fixture(m_doc);
    f->setName(QStringLiteral("Stage Left Spot"));
    f->setChannels(4);
    QVERIFY(m_doc->addFixture(f));
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName(QStringLiteral("Choir Wash"));
    QVERIFY(m_doc->addFixtureGroup(grp));
}

void ReportBug_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void ReportBug_Test::theUrlCarriesTheFormAndEncodedFields()
{
    BugReport::Fields f;
    f.title = QStringLiteral("C++ & co = 100% #1");
    f.whatHappened = QStringLiteral("line one\nline two + more");
    f.expected = QStringLiteral("nothing?");
    f.details = QStringLiteral("Qt 6.9.3");
    f.version = QStringLiteral("0.3.1 (build 4.14.4+qlcconsole.9000)");

    bool trimmed = true;
    const QUrl url = BugReport::issueUrl(f, &trimmed);
    QVERIFY(!trimmed);
    const QString enc = QString::fromLatin1(url.toEncoded());
    QVERIFY2(enc.startsWith(QStringLiteral(
                 "https://github.com/sandinak/qlcconsole/issues/new?template=bug_report.yml&title=")),
             qPrintable(enc));
    // '+' must not travel bare: GitHub would read it as a space.
    QVERIFY2(enc.contains(QStringLiteral("title=%5BBug%5D%20C%2B%2B%20%26%20co%20%3D%20100%25%20%231")),
             qPrintable(enc));
    QVERIFY(enc.contains(QStringLiteral("what-happened=line%20one%0Aline%20two%20%2B%20more")));
    QVERIFY(enc.contains(QStringLiteral("&version=0.3.1%20%28build%204.14.4%2Bqlcconsole.9000%29")));
    QVERIFY(!enc.contains(QStringLiteral("steps=")));   // empty fields are left out

    const QUrlQuery q(url);
    QCOMPARE(q.queryItemValue(QStringLiteral("template"), QUrl::FullyDecoded), QStringLiteral("bug_report.yml"));
    QCOMPARE(q.queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded), QStringLiteral("[Bug] C++ & co = 100% #1"));
    QCOMPARE(q.queryItemValue(QStringLiteral("what-happened"), QUrl::FullyDecoded), f.whatHappened);
    QCOMPARE(q.queryItemValue(QStringLiteral("expected"), QUrl::FullyDecoded), f.expected);
    QCOMPARE(q.queryItemValue(QStringLiteral("details"), QUrl::FullyDecoded), f.details);

    // A title already prefixed isn't prefixed twice.
    f.title = QStringLiteral("[Bug] Already");
    QCOMPARE(QUrlQuery(BugReport::issueUrl(f)).queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded),
             QStringLiteral("[Bug] Already"));
}

void ReportBug_Test::scrubbingTakesOutWhatIsPrivate()
{
    const QString home = QDir::homePath();
    const QString show = showPath();
    const QStringList secrets = { show, QStringLiteral("Smith Wedding 2026.qxw"),
                                  QStringLiteral("Smith Wedding 2026"),
                                  QStringLiteral("Stage Left Spot"), QStringLiteral("Ed"),
                                  QStringLiteral("showowner") };
    const QString in = QStringLiteral(
        "Loaded %1 at 12:30:45\n"
        "fixtures in %2/Fixtures/my.qxf\n"
        "Smith Wedding 2026 window, Stage Left Spot at 50%\n"
        "Edited by showowner, Ed\n"
        "node 192.168.1.17 and 2.0.0.1, v6 fe80::1c2a:3bff:fe4d:5e6f%en0, mac aa:bb:cc:dd:ee:ff\n"
        "other /Users/someone/x and C:\\Users\\someone\\y\n"
        "web password=hunter2, psk: abc123\n"
        "qlcconsole 0.3.1 (build 4.14.4+qlcconsole.9000), Qt 6.9.3, App::init")
        .arg(show, home);
    const QString out = BugReport::scrub(in, secrets);

    for (const QString &gone : { show, home, QStringLiteral("Smith"), QStringLiteral("Stage Left Spot"),
                                 QStringLiteral("showowner"), QStringLiteral("192.168"),
                                 QStringLiteral("2.0.0.1"), QStringLiteral("fe80"),
                                 QStringLiteral("aa:bb"), QStringLiteral("someone"),
                                 QStringLiteral("hunter2"), QStringLiteral("abc123") })
        QVERIFY2(!out.contains(gone, Qt::CaseInsensitive), qPrintable(gone + QStringLiteral(" in:\n") + out));

    QVERIFY2(out.contains(QStringLiteral("fixtures in ~/Fixtures/my.qxf")), qPrintable(out));
    QVERIFY(out.contains(QStringLiteral("Loaded [removed] at 12:30:45")));
    QVERIFY(out.contains(QStringLiteral("Edited by [removed], [removed]")));   // "Ed" only as a word
    QVERIFY(out.contains(QStringLiteral("node [ip] and [ip]")));
    QVERIFY(out.contains(QStringLiteral("/Users/[user]/x")));
    QVERIFY(out.contains(QStringLiteral("password=[removed]")));
    QVERIFY(out.contains(QStringLiteral("qlcconsole 0.3.1 (build 4.14.4+qlcconsole.9000), Qt 6.9.3, App::init")));
}

void ReportBug_Test::untickedPartsAreAbsent()
{
    const QString show = showPath();
    BugReportContext ctx;
    ctx.doc = m_doc;
    ctx.viewName = QStringLiteral("Everything");
    ctx.showPath = show;
    ctx.layoutReport = [show]() {
        return QStringLiteral("LAYOUT Main window \"qlcconsole - Smith Wedding 2026\" from %1, Stage Left Spot")
            .arg(show);
    };
    ctx.logLines = { QStringLiteral("LOG Art-Net node 10.0.0.42 answered"),
                     QStringLiteral("LOG web password=hunter2") };
    BugReportDialog d(ctx);

    for (const QString &key : BugReportDialog::partKeys())
        QVERIFY2(child<QCheckBox>(&d, qPrintable(QStringLiteral("bugPart_") + key)) != nullptr, qPrintable(key));
    QCheckBox *layout = child<QCheckBox>(&d, "bugPart_layout");
    QCheckBox *log = child<QCheckBox>(&d, "bugPart_log");
    QCheckBox *version = child<QCheckBox>(&d, "bugPart_version");
    QCheckBox *counts = child<QCheckBox>(&d, "bugPart_counts");
    QVERIFY(!layout->isChecked() && layout->isEnabled());
    QVERIFY(!log->isChecked() && log->isEnabled());
    QVERIFY(version->isChecked() && counts->isChecked());

    QString details = d.fields().details;
    QVERIFY2(details.contains(QStringLiteral("based on QLC+ ")), qPrintable(details));
    QVERIFY(details.contains(QStringLiteral("View: Everything")));
    QVERIFY(details.contains(QStringLiteral("Show: 1 fixtures, 1 fixture groups")));
    QVERIFY(details.contains(QStringLiteral("I/O plugins in use: none")));
    QVERIFY(!details.contains(QStringLiteral("LAYOUT")));
    QVERIFY(!details.contains(QStringLiteral("LOG")));

    // Ticked, their text is in -- scrubbed.
    layout->setChecked(true);
    log->setChecked(true);
    details = d.fields().details;
    QVERIFY(details.contains(QStringLiteral("LAYOUT")));
    QVERIFY(details.contains(QStringLiteral("LOG Art-Net node [ip] answered")));
    for (const QString &gone : { QStringLiteral("Smith"), QStringLiteral("Stage Left"),
                                 QStringLiteral("10.0.0.42"), QStringLiteral("hunter2"), QDir::homePath() })
        QVERIFY2(!details.contains(gone), qPrintable(gone + QStringLiteral(" in:\n") + details));

    // Unticked again, gone again; the version leaves the details and the
    // form's version field.
    layout->setChecked(false);
    version->setChecked(false);
    counts->setChecked(false);
    const BugReport::Fields f = d.fields();
    QVERIFY(!f.details.contains(QStringLiteral("LAYOUT")));
    QVERIFY(!f.details.contains(QStringLiteral("based on QLC+")));
    QVERIFY(!f.details.contains(QStringLiteral("fixtures")));
    QVERIFY(f.version.isEmpty());

    child<QLineEdit>(&d, "bugTitle")->setText(QStringLiteral("Crash"));
    child<QPlainTextEdit>(&d, "bugWhat")->setPlainText(QStringLiteral("It crashed opening %1").arg(show));
    QVERIFY(d.openInGitHub());
    QCOMPARE(m_opened.size(), 1);
    const QUrlQuery q(m_opened.first());
    QVERIFY(!q.hasQueryItem(QStringLiteral("version")));
    const QString what = q.queryItemValue(QStringLiteral("what-happened"), QUrl::FullyDecoded);
    QCOMPARE(what, QStringLiteral("It crashed opening [removed]"));
    const QString sent = q.queryItemValue(QStringLiteral("details"), QUrl::FullyDecoded);
    QVERIFY(!sent.contains(QStringLiteral("LAYOUT")) && !sent.contains(QStringLiteral("fixtures")));
    QVERIFY(sent.contains(QStringLiteral("LOG")));

    // An edit in the details box is kept, and scrubbed too.
    child<QPlainTextEdit>(&d, "bugDetails")->setPlainText(QStringLiteral("my note, Stage Left Spot at 10.1.1.1"));
    QCOMPARE(d.fields().details, QStringLiteral("my note, [removed] at [ip]"));

    // An offscreen picture of the dialog, when asked for (not part of the check).
    const QString shot = qEnvironmentVariable("QLC_BUGREPORT_SHOT");
    if (!shot.isEmpty())
    {
        version->setChecked(true);
        counts->setChecked(true);
        child<QCheckBox>(&d, "bugPart_log")->setChecked(false);
        child<QPlainTextEdit>(&d, "bugWhat")->setPlainText(
            QStringLiteral("After Undo the chaser skips step 3 when it loops."));
        child<QPlainTextEdit>(&d, "bugExpected")->setPlainText(QStringLiteral("Every step plays."));
        child<QLineEdit>(&d, "bugTitle")->setText(QStringLiteral("Chaser skips a step after Undo"));
        d.show();
        QCoreApplication::processEvents();
        QVERIFY(d.grab().save(shot));
    }
}

void ReportBug_Test::anOverLongReportIsCutAndCopied()
{
    BugReportContext ctx;
    ctx.doc = m_doc;
    ctx.viewName = QStringLiteral("qlcconsole");
    BugReportDialog d(ctx);
    QLabel *note = child<QLabel>(&d, "bugNote");

    QString longText;
    for (int i = 0; i < 2000; i++)
        longText += QStringLiteral("step %1 & more+; ").arg(i);
    child<QLineEdit>(&d, "bugTitle")->setText(QStringLiteral("Long"));
    child<QPlainTextEdit>(&d, "bugWhat")->setPlainText(QStringLiteral("It went wrong."));
    child<QPlainTextEdit>(&d, "bugSteps")->setPlainText(longText);
    child<QPlainTextEdit>(&d, "bugDetails")->setPlainText(longText);
    QGuiApplication::clipboard()->clear();

    QVERIFY(d.openInGitHub());
    QCOMPARE(m_opened.size(), 1);
    const QByteArray enc = m_opened.first().toEncoded();
    QVERIFY2(enc.size() <= BugReport::MaxUrlLength, QByteArray::number(enc.size()).constData());
    QVERIFY(enc.size() > BugReport::MaxUrlLength - 200);   // cut to fit, not emptied
    const QUrlQuery q(m_opened.first());
    QVERIFY(q.queryItemValue(QStringLiteral("details"), QUrl::FullyDecoded).contains(QStringLiteral("on your clipboard")));
    QCOMPARE(q.queryItemValue(QStringLiteral("what-happened"), QUrl::FullyDecoded), QStringLiteral("It went wrong."));
    QVERIFY(note->isVisibleTo(&d));
    QVERIFY(note->text().contains(QStringLiteral("the full details are on your clipboard")));

    const QString clip = QGuiApplication::clipboard()->text();
    QCOMPARE(clip, BugReport::reportText(d.fields()));
    QVERIFY(clip.contains(QStringLiteral("step 1999 & more+;")));
    QVERIFY(clip.contains(QStringLiteral("### Steps to reproduce")));

    // A short one isn't cut and leaves the clipboard alone.
    QGuiApplication::clipboard()->setText(QStringLiteral("untouched"));
    child<QPlainTextEdit>(&d, "bugSteps")->setPlainText(QStringLiteral("1. Open"));
    child<QPlainTextEdit>(&d, "bugDetails")->setPlainText(QStringLiteral("Qt 6"));
    QVERIFY(d.openInGitHub());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("untouched"));
    QVERIFY(!note->isVisibleTo(&d));

    // Huge free text everywhere still fits.
    BugReport::Fields big;
    big.title = longText;
    big.whatHappened = big.expected = big.steps = big.details = longText;
    bool trimmed = false;
    QVERIFY(BugReport::issueUrl(big, &trimmed).toEncoded().size() <= BugReport::MaxUrlLength);
    QVERIFY(trimmed);
}

void ReportBug_Test::copyReportOpensNothing()
{
    BugReportContext ctx;
    ctx.doc = m_doc;
    BugReportDialog d(ctx);
    QPushButton *open = child<QPushButton>(&d, "bugOpen");
    QVERIFY(!open->isEnabled());   // nothing to file yet
    child<QLineEdit>(&d, "bugTitle")->setText(QStringLiteral("Title"));
    QVERIFY(!open->isEnabled());
    child<QPlainTextEdit>(&d, "bugWhat")->setPlainText(QStringLiteral("What"));
    QVERIFY(open->isEnabled());

    QGuiApplication::clipboard()->clear();
    child<QPushButton>(&d, "bugCopy")->click();
    QVERIFY(m_opened.isEmpty());
    const QString clip = QGuiApplication::clipboard()->text();
    QVERIFY2(clip.startsWith(QStringLiteral("[Bug] Title\n\n### What happened?\n\nWhat\n")), qPrintable(clip));
    QVERIFY(clip.contains(QStringLiteral("### Details\n\n```text\n")));
    QVERIFY(clip.contains(QStringLiteral("### Steps to reproduce\n\n_No response_")));
}

void ReportBug_Test::theMenuItemIsInEveryView()
{
    App app;
    app.init();
    QMenu *help = app.menuBar()->findChild<QMenu *>(QStringLiteral("helpMenu"));
    QVERIFY(help != nullptr);
    QMenu *trouble = help->findChild<QMenu *>(QStringLiteral("troubleshootingMenu"));
    QVERIFY(trouble != nullptr);

    for (App::WorkspaceView v : { App::ClassicView, App::ConsoleView, App::EverythingView })
    {
        app.setWorkspaceView(v, false);
        QCoreApplication::processEvents();
        QVERIFY(help->menuAction()->isVisible());
        QAction *bug = nullptr;
        for (QAction *a : trouble->actions())
            if (a->objectName() == QStringLiteral("reportBug"))
                bug = a;
        QVERIFY2(bug != nullptr, qPrintable(App::workspaceViewName(v)));
        QVERIFY(bug->isVisible() && bug->isEnabled());
        QVERIFY(bug->text().startsWith(QStringLiteral("Report a Bug")));

        bug->trigger();
        BugReportDialog *d = app.findChild<BugReportDialog *>();
        QVERIFY(d != nullptr);
        QCOMPARE(d->partText(QStringLiteral("view")),
                 QStringLiteral("View: %1").arg(App::workspaceViewName(v)));
        QVERIFY(m_opened.isEmpty());   // opening the dialog opens no browser
        delete d;
    }
}

QTEST_MAIN(ReportBug_Test)
