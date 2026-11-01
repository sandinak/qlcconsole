/*
  Q Light Controller Plus
  clinodisplay_test.cpp

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
#include <QProcess>
#include <QFileInfo>

#include "clinodisplay_test.h"

// --version and --help answer before QApplication exists, so they work with
// no display: ssh, a headless Pi. The child gets a Qt platform that cannot
// open (the real binary, never a window).
static QString appBinary()
{
    const QString base = QDir::current().absoluteFilePath("../../../main/qlcconsole");
    return QFileInfo::exists(base + ".exe") ? base + ".exe" : base;
}

static QProcess *run(const QStringList &args, QByteArray &out, int &code)
{
    QProcess *p = new QProcess;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORM", "no-such-platform-plugin");
    env.remove("DISPLAY");
    p->setProcessEnvironment(env);
    p->start(appBinary(), args);
    if (p->waitForFinished(20000) == false)
        p->kill(), p->waitForFinished();
    out = p->readAllStandardOutput();
    code = p->exitStatus() == QProcess::NormalExit ? p->exitCode() : -1;
    return p;
}

void CliNoDisplay_Test::version()
{
    QVERIFY2(QFileInfo::exists(appBinary()), qPrintable(appBinary()));
    for (const QString &flag : {QString("--version"), QString("-v")})
    {
        QByteArray out;
        int code = 0;
        QScopedPointer<QProcess> p(run({flag}, out, code));
        QCOMPARE(code, 0);
        QVERIFY2(out.contains("Apache 2.0"), out.constData());
        QVERIFY(out.contains("Based on QLC+"));
    }
}

void CliNoDisplay_Test::help()
{
    for (const QString &flag : {QString("--help"), QString("-h")})
    {
        QByteArray out;
        int code = 0;
        QScopedPointer<QProcess> p(run({flag}, out, code));
        QCOMPARE(code, 0);
        QVERIFY2(out.contains("Usage:"), out.constData());
        QVERIFY(out.contains("--version"));
    }
}

QTEST_GUILESS_MAIN(CliNoDisplay_Test)
