/*
  Q Light Controller Plus - qlcconsole
  hostilecase.h

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

/* Hostile input cases that may crash or hang the program on a build that
 * still has the bug. Each such test function runs ITSELF again in a child
 * process (this test program, that one function), so a crash, an assert or a
 * hang fails that one function and the rest of the program still runs.
 *
 *   void X_Test::aBadPacketIsDropped()
 *   {
 *       HOSTILE_CASE(5000);          // parent: runs the child, checks it
 *       ...the hostile part...       // child: only this runs
 *   }
 *
 * The child installs its own handlers for the fatal signals and leaves with
 * std::_Exit(), so a crash leaves no crash report (.ips) or core file behind.
 * QLC_HOSTILE_CHILD=1 marks the child; run a case by hand with it set to
 * see the crash in a debugger. */

#ifndef HOSTILECASE_H
#define HOSTILECASE_H

#include <QCoreApplication>
#include <QProcess>
#include <QtTest>
#include <csignal>
#include <cstdlib>

namespace HostileCase
{
    inline bool inChild()
    {
        return qEnvironmentVariableIntValue("QLC_HOSTILE_CHILD") == 1;
    }

    inline void fatalSignal(int sig)
    {
        std::_Exit(100 + sig);
    }

    /* In the child: a fatal signal ends it quietly with 100 + the signal. */
    inline void armChild()
    {
        std::signal(SIGSEGV, fatalSignal);
        std::signal(SIGABRT, fatalSignal);
        std::signal(SIGILL, fatalSignal);
        std::signal(SIGFPE, fatalSignal);
#if defined(SIGBUS)
        std::signal(SIGBUS, fatalSignal);
#endif
    }

    /* In the parent: run @p function of this program in a child and say how
       it ended. Returns an empty string when it passed. */
    inline QString runChild(const char *function, int timeoutMs)
    {
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QLC_HOSTILE_CHILD"), QStringLiteral("1"));
        if (!env.contains(QStringLiteral("QT_QPA_PLATFORM")))
            env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        p.setProcessEnvironment(env);
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(QCoreApplication::applicationFilePath(),
                { QString::fromLatin1(function), QStringLiteral("-nocrashhandler") });
        if (!p.waitForStarted(10000))
            return QStringLiteral("the child did not start");
        if (!p.waitForFinished(timeoutMs))
        {
            p.kill();
            p.waitForFinished(5000);
            return QStringLiteral("HANG: still running after %1 ms").arg(timeoutMs);
        }
        const QByteArray out = p.readAll();
        const int code = p.exitCode();
        if (p.exitStatus() == QProcess::CrashExit)
            return QStringLiteral("CRASH (signal)\n") + QString::fromLocal8Bit(out.right(3000));
        if (code >= 100 && code < 140)
            return QStringLiteral("CRASH: signal %1\n").arg(code - 100)
                   + QString::fromLocal8Bit(out.right(3000));
        if (code != 0)
            return QStringLiteral("FAIL (exit %1)\n").arg(code) + QString::fromLocal8Bit(out.right(3000));
        return QString();
    }
}

/* At the top of a test function: the parent runs the function in a child and
   checks it; the child carries on into the function's body. */
#define HOSTILE_CASE(timeoutMs)                                                        \
    do {                                                                               \
        if (!HostileCase::inChild())                                                   \
        {                                                                              \
            const QString why = HostileCase::runChild(QTest::currentTestFunction(),    \
                                                      (timeoutMs));                    \
            QVERIFY2(why.isEmpty(), qPrintable(why));                                  \
            return;                                                                    \
        }                                                                              \
        HostileCase::armChild();                                                       \
    } while (0)

#endif
