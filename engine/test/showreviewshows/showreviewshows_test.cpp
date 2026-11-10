/*
  Q Light Controller Plus - Unit test
  showreviewshows_test.cpp

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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QGuiApplication>

#include "showreviewshows_test.h"
#include "showreviewcli.h"

namespace
{
struct Expect
{
    const char *show;       // file name in docs/course/shows, or "Sample.qxw"
    const char *checks;     // sorted, comma-separated open check ids (any severity)
};

/* Every show the release ships, with the Show Review checks open on it, any
   severity (no blockers: that is also asserted). A change to a check or a show
   that moves a row fails the gate; fix the cause, or move the row on purpose.

   Why each id is expected (all verified 2026-11-06, 0.4.1):
   - PAT-06 (note): the course rig patches no output on purpose; it only previews.
   - FIX-17, FIX-18 (notes): fixture definitions of the rig: two have gobo slots
     without pictures, the wall washer has no beam angle. About the definitions,
     not the lesson.
   - FILE-03 (this computer's copy of an embedded definition differs) is left out
     of the comparison: it depends on the user's own fixtures folder (the show's
     embedded "user" definitions match or not per computer), so it differs
     between the developers' Macs and a clean Linux box.
   - PWR-12 (warning), from 1.3: the audience-tower washes' cones clip the cyc
     (16-22%) in Verse; real spill. The Truss Cyc boxes carry the Cyc stage look,
     so their light on the cyc counts as meant.
   - No MOV-08: the Pose (AT-Spots at Soloist DS, from 1.3) and before
     the Fan (Truss-Spots at the Riser Line, from 2.5) carries a dark preset, so
     those Block cues don't swing their movers in lit. Fails here if a rebuilt
     show loses it (courseshow_test markTheMoversInTheShows puts it back).
   - CUE-08 (note), from 2.1: Verse releases layers with no base cue.
   - CUE-15 (note), from 2.7: sub-cues on Sub GO, no widget sends it yet.
   - TEACH-01 (note): 3.6's start is the teaching-mode lesson.
   - CUE-01, CUE-02, VC-03: 3.9's start is broken on purpose (lesson 3.9 mends them).
   - FIX-07 (note): the sample's generic 6-channel dimmers give no power figure. */
const Expect kExpected[] = {
    { "ShowChoir-Classroom.qlcc", "FIX-17,FIX-18,PAT-06" },
    { "lesson-1-1-finish.qlcc", "FIX-17,FIX-18,PAT-06" },
    { "lesson-1-1-start.qlcc", "FIX-17,FIX-18,PAT-06" },
    { "lesson-1-2-finish.qlcc", "FIX-17,FIX-18,PAT-06" },
    { "lesson-1-2-start.qlcc", "FIX-17,FIX-18,PAT-06" },
    { "lesson-1-3-finish.qlcc", "FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-1-3-start.qlcc", "FIX-17,FIX-18,PAT-06" },
    { "lesson-1-4-finish.qlcc", "FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-1-4-start.qlcc", "FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-1-5-start.qlcc", "FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-1-finish.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-1-start.qlcc", "FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-2-finish.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-2-start.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-3-finish.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-3-start.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-4-finish.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-4-start.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-5-finish.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-5-start.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-6-finish.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-6-start.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-7-finish.qlcc", "CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-2-7-start.qlcc", "CUE-08,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-3-5-finish.qlcc", "CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-3-5-start.qlcc", "CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-3-6-start.qlcc", "FIX-17,FIX-18,PAT-06,TEACH-01" },
    { "lesson-3-8-finish.qlcc", "CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-3-8-start.qlcc", "CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-3-9-finish.qlcc", "CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12" },
    { "lesson-3-9-start.qlcc", "CUE-01,CUE-02,CUE-08,CUE-15,FIX-17,FIX-18,PAT-06,PWR-12,VC-03" },
    { "Sample.qxw", "FIX-07,FIX-17,FIX-18,PAT-06" },
};

QString showPath(const QString &name)
{
    return name == QLatin1String("Sample.qxw") ? QString(SAMPLE_SHOW) : QString(COURSE_SHOWS_DIR) + "/" + name;
}

QString resourceDir()
{
    return QString(QLC_TEST_RESOURCE_ROOT);
}

/** The open checks of @a path, "A,B" sorted and unique. */
QString reviewed(const QString &path, QString *detail)
{
    QString text, errors;
    QTextStream out(&text), err(&errors);
    const int code = ShowReviewCli::run({ "--review", path, "--min", "note", "--format", "json" }, out, err, true,
                                        resourceDir());
    out.flush();
    if (code == int(ShowReviewCli::Unreadable))
        return QStringLiteral("UNREADABLE ") + errors;
    const QJsonObject json = QJsonDocument::fromJson(text.toUtf8()).object();
    QSet<QString> ids;
    for (const QJsonValue &v : json.value("findings").toArray())
    {
        const QJsonObject f = v.toObject();
        if (f.value("state").toString() != QLatin1String("open"))
            continue;
        if (f.value("check").toString() == QLatin1String("FILE-03"))
            continue;
        ids.insert(f.value("check").toString());
        if (detail)
            *detail += QStringLiteral("  %1 %2: %3\n").arg(f.value("check").toString(), f.value("severity").toString(),
                                                             f.value("message").toString());
    }
    QStringList list = QStringList(ids.begin(), ids.end());
    list.sort();
    return list.join(',');
}
}

void ShowReviewShows_Test::everyShippedShowReviewsAsExpected()
{
    // Loading a show talks a lot (hundreds of lines per show); only the
    // verdicts matter, and QtTest stops printing after its warning limit.
    const QtMessageHandler previous = qInstallMessageHandler(
        [](QtMsgType type, const QMessageLogContext &, const QString &msg) {
            if (type == QtInfoMsg)
                fprintf(stderr, "%s\n", qPrintable(msg));
        });
    struct Restore { QtMessageHandler h; ~Restore() { qInstallMessageHandler(h); } } restore{ previous };

    const bool dump = qEnvironmentVariableIsSet("QLC_REVIEW_DUMP");
    for (const Expect &e : kExpected)
    {
        QString detail;
        QElapsedTimer clock;
        clock.start();
        const QString got = reviewed(showPath(QString::fromLatin1(e.show)), &detail);
        if (dump)
            qInfo().noquote() << "REVIEW" << e.show << "=>" << got << clock.elapsed() << "ms\n" << detail;
        QVERIFY2(!got.startsWith(QLatin1String("UNREADABLE")), qPrintable(QString::fromLatin1(e.show) + " " + got));
        if (dump)
            continue;
        QVERIFY2(!detail.contains(QLatin1String(" blocker:")),
                 qPrintable(QString::fromLatin1(e.show) + " has a blocker\n" + detail));
        QVERIFY2(got == QString::fromLatin1(e.checks),
                 qPrintable(QStringLiteral("%1: expected [%2] got [%3]\n%4")
                                .arg(QString::fromLatin1(e.show), QString::fromLatin1(e.checks), got, detail)));
    }
}

void ShowReviewShows_Test::theTableCoversEveryShippedShow()
{
    QSet<QString> listed;
    for (const Expect &e : kExpected)
        listed.insert(QString::fromLatin1(e.show));
    const QStringList onDisk = QDir(COURSE_SHOWS_DIR).entryList({ "*.qlcc" }, QDir::Files);
    QVERIFY(onDisk.count() > 20);
    for (const QString &f : onDisk)
        QVERIFY2(listed.contains(f), qPrintable(f + " is shipped but not in kExpected"));
    QVERIFY(listed.contains(QStringLiteral("Sample.qxw")));
    QVERIFY(QFile::exists(SAMPLE_SHOW));
}

QTEST_MAIN(ShowReviewShows_Test)
