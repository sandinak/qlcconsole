/*
  Q Light Controller Plus
  testguard.cpp

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

/* Linked into EVERY test program (the top-level CMakeLists.txt attaches it
 * to each executable under a test/ directory or linking Qt Test), however
 * that program is started: by unittest.sh, by its test.sh, by hand, by a
 * script, from CI.
 *
 * 1. Never on the real screen. Before main() -- so before any QApplication
 *    or QGuiApplication reads it -- QT_QPA_PLATFORM is set to offscreen.
 *    Every test.sh sets it too; this is the second wall, for a program run
 *    any other way. QLC_TEST_REAL_SCREEN=1 lifts both, to watch a test on
 *    purpose (QT_QPA_PLATFORM then picks the platform as usual).
 *
 * 2. No "quit unexpectedly" dialog on macOS. A crashing test program (the UI
 *    tests are .app bundles) made ReportCrash put that dialog on the screen.
 *    A fatal signal here ends the program with _exit(128 + signal) instead,
 *    which a shell reports exactly as it does a death by that signal, and
 *    ReportCrash -- which acts only on a process the kernel kills -- never
 *    hears of it: no dialog, and no .ips report either. QTest installs its
 *    own handler later, prints the failing function and a backtrace, and
 *    then chains to this one, so the log keeps everything. A debugger sees
 *    the signal first, as always. QLC_TEST_CRASH_REPORTS=1 keeps the
 *    system's crash reports (and its dialog) for a run that wants them.
 *    Nothing system-wide is changed.
 *
 * Plain C library calls only: some test programs link nothing but QtCore,
 * and this runs before anything is set up.
 *
 * platforms/linux/check-test-guard.sh fails the gate when a test program
 * lacks this (it looks for QLC_TEST_OFFSCREEN_GUARD in the binary). */

#include <cstdlib>
#include <cstring>

#if defined(__APPLE__)
#include <csignal>
#include <unistd.h>
#endif

namespace
{

bool isSet(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && std::strcmp(value, "1") == 0;
}

void setEnv(const char *name, const char *value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

#if defined(__APPLE__)
void exitWithoutCrashReport(int sig)
{
    // Async-signal-safe only: write(2) and _exit(2).
    static const char msg[] =
        "testguard: fatal signal, exiting without a crash report "
        "(QLC_TEST_CRASH_REPORTS=1 keeps them)\n";
    ssize_t ignored = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)ignored;
    _exit(128 + sig);
}

void noCrashDialogs()
{
    // Its own stack, so a stack overflow's SIGSEGV can still be handled.
    static char altStack[64 * 1024];
    stack_t ss;
    std::memset(&ss, 0, sizeof(ss));
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    sigaltstack(&ss, nullptr);

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = exitWithoutCrashReport;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_ONSTACK;
    const int fatal[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP, SIGSYS };
    for (int sig : fatal)
        sigaction(sig, &sa, nullptr);
}
#endif

struct TestGuard
{
    TestGuard()
    {
        // Says to the program, and to the gate's check, that this ran.
        setEnv("QLC_TEST_OFFSCREEN_GUARD", "1");
        if (!isSet("QLC_TEST_REAL_SCREEN"))
            setEnv("QT_QPA_PLATFORM", "offscreen");
#if defined(__APPLE__)
        if (!isSet("QLC_TEST_CRASH_REPORTS"))
            noCrashDialogs();
#endif
    }
};

const TestGuard testGuard;

} // namespace
