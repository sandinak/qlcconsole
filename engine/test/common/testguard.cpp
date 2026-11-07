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
 *    the signal first, as always. This one adds the faulting address and
 *    the stack of the thread that took the signal: QTest's own stack dump
 *    needs a debugger to attach, which a CI runner may not allow, and the
 *    0.4.1 SIGBUS on a GitHub macOS runner (a timer-thread signal into a
 *    destroyed object) left nothing but "signal 10" and an address to go
 *    on. QLC_TEST_CRASH_REPORTS=1 keeps the
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
#include <execinfo.h>
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
void say(const char *text, size_t len)
{
    ssize_t ignored = write(STDERR_FILENO, text, len);
    (void)ignored;
}

/** "<label>0x<hex>", without printf, which isn't async-signal-safe. */
void sayHex(const char *label, unsigned long value)
{
    char buf[64];
    size_t n = 0;
    while (label[n] != '\0' && n < 40)
    {
        buf[n] = label[n];
        ++n;
    }
    buf[n++] = '0';
    buf[n++] = 'x';
    for (int shift = int(sizeof(value) * 8) - 4; shift >= 0; shift -= 4)
        buf[n++] = "0123456789abcdef"[(value >> shift) & 0xf];
    say(buf, n);
}

void exitWithoutCrashReport(int sig, siginfo_t *info, void *)
{
    // write(2) and _exit(2), and backtrace(3), warmed up in noCrashDialogs()
    // so it binds nothing lazily here: on macOS it walks frame pointers and
    // takes no lock. backtrace_symbols_fd(3) reads the symbols with
    // dladdr(); a crash inside dyld itself could stall it there -- a hang
    // to the gate's timeout instead of an exit, in a program that crashed
    // anyway.
    char num[] = "testguard: fatal signal 00";
    num[sizeof(num) - 3] = char('0' + (sig / 10) % 10);
    num[sizeof(num) - 2] = char('0' + sig % 10);
    say(num, sizeof(num) - 1);
    sayHex(" at address ", reinterpret_cast<unsigned long>(info != nullptr ? info->si_addr : nullptr));
    static const char stack[] = ", this thread's stack:\n";
    say(stack, sizeof(stack) - 1);
    void *frames[64];
    const int depth = backtrace(frames, 64);
    backtrace_symbols_fd(frames, depth, STDERR_FILENO);
    static const char msg[] =
        "testguard: exiting without a crash report "
        "(QLC_TEST_CRASH_REPORTS=1 keeps them)\n";
    say(msg, sizeof(msg) - 1);
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

    // backtrace() once now, so the handler's first call binds nothing.
    void *warm[4];
    (void)backtrace(warm, 4);

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = exitWithoutCrashReport;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
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
