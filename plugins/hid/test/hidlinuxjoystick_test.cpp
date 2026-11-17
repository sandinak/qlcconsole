/*
  Q Light Controller Plus
  hidlinuxjoystick_test.cpp

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
#include <QTemporaryDir>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <linux/joystick.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "hidlinuxjoystick_test.h"
#include "hidlinuxjoystick.h"

namespace
{
/* Static: a queued value may still be on its way when the test ends. */
QAtomicInt s_pressed(-1);

qint64 processCpuMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return qint64(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

bool writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(text + "\n") == text.size() + 1;
}

/** The pad goes: its fd answers at once with a hang-up and reads nothing. */
bool goneNow(HIDLinuxJoystick *pad)
{
    int dead[2];
    if (::pipe(dead) != 0)
        return false;
    ::close(dead[1]);
    const bool ok = ::dup2(dead[0], pad->handle()) == pad->handle();
    ::close(dead[0]);
    return ok;
}

/** A writer on the FIFO node, once the reader has it open again (a FIFO
    opens for writing only then); -1 if that takes over @a ms. */
int writerWithin(const QString &node, int ms)
{
    int writer = -1;
    QElapsedTimer waited;
    waited.start();
    while (writer < 0 && waited.elapsed() < ms)
    {
        writer = ::open(QFile::encodeName(node).constData(), O_WRONLY | O_NONBLOCK);
        if (writer < 0)
            QTest::qWait(20);
    }
    return writer;
}

bool press(int writer, short value)
{
    struct js_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = JS_EVENT_BUTTON;
    ev.number = 0;
    ev.value = value;
    return ::write(writer, &ev, sizeof(ev)) == ssize_t(sizeof(ev));
}
}

void HIDLinuxJoystick_Test::padValue(quint32 universe, quint32 line, quint32 channel, uchar value)
{
    Q_UNUSED(universe);
    Q_UNUSED(line);
    if (channel == 0)
        s_pressed.storeRelaxed(value);
}

void HIDLinuxJoystick_Test::aPadThatGoesIsWaitedForAndReadAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString node = dir.filePath(QStringLiteral("js0"));
    const QString away = dir.filePath(QStringLiteral("js0.gone"));
    QCOMPARE(::mkfifo(QFile::encodeName(node).constData(), 0600), 0);

    /* Its sysfs: a DualSense on Bluetooth. */
    const QString sys = dir.filePath(QStringLiteral("sys"));
    const QString dev = sys + QStringLiteral("/js0/device/");
    const QByteArray mac("a0:b1:c2:d3:e4:f5");
    QVERIFY(writeFile(dev + QStringLiteral("id/vendor"), "054c"));
    QVERIFY(writeFile(dev + QStringLiteral("id/product"), "0ce6"));
    QVERIFY(writeFile(dev + QStringLiteral("uniq"), mac));
    QVERIFY(writeFile(dev + QStringLiteral("phys"), "00:1a:7d:da:71:13"));
    HIDLinuxJoystick::setSysfsInputForTest(sys);

    QByteArray path = QFile::encodeName(node);
    wchar_t empty[] = L"";
    struct hid_device_info info;
    memset(&info, 0, sizeof(info));
    info.path = path.data();
    info.vendor_id = 0x054c;
    info.product_id = 0x0ce6;
    info.serial_number = empty;
    info.manufacturer_string = empty;
    info.product_string = empty;

    HIDLinuxJoystick *pad = new HIDLinuxJoystick(nullptr, 0, &info);
    s_pressed.storeRelaxed(-1);
    /* By name: the plugin's signals are its DLL's (dllsignals_test). */
    QVERIFY(connect(pad, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)),
                    this, SLOT(padValue(quint32,quint32,quint32,uchar)), Qt::QueuedConnection));
    QVERIFY(pad->openInput());
    QTRY_VERIFY(pad->isRunning());
    QVERIFY(pad->handle() >= 0);

    /* Gone, and the node not there to open again. */
    QCOMPARE(::rename(QFile::encodeName(node).constData(), QFile::encodeName(away).constData()), 0);
    QVERIFY(goneNow(pad));

    /* Waiting for it costs (next to) nothing: the old reader spun on
       poll() at a whole core. */
    const qint64 cpu0 = processCpuMs();
    QTest::qSleep(1000);
    const qint64 used = processCpuMs() - cpu0;
    QVERIFY2(used < 100, qPrintable(QStringLiteral("%1 ms of CPU in 1 s waiting for a gone pad").arg(used)));

    /* Back at the same node: read again, a button press arrives. */
    QCOMPARE(::rename(QFile::encodeName(away).constData(), QFile::encodeName(node).constData()), 0);
    int writer = writerWithin(node, 3000);
    QVERIFY2(writer >= 0, "the reader never opened the node again");
    QVERIFY(press(writer, 1));
    QTRY_COMPARE_WITH_TIMEOUT(s_pressed.loadRelaxed(), 255, 3000);
    ::close(writer);

    /* Gone again, and another DualSense takes its number (another
       Bluetooth address): not read on this pad's line. */
    QVERIFY(writeFile(dev + QStringLiteral("uniq"), "a0:b1:c2:d3:e4:99"));
    QVERIFY(goneNow(pad));
    /* A poll() the reader is in when the fd is swapped holds the old node
       open until it returns (KPollTimeout): past that, nothing reads it. */
    QTest::qWait(300);
    writer = writerWithin(node, 800);
    QVERIFY2(writer < 0, "another pad at its node was read as this one");
    /* Nor one whose address can't be read. */
    QVERIFY(QFile::remove(dev + QStringLiteral("uniq")));
    writer = writerWithin(node, 500);
    QVERIFY2(writer < 0, "a pad with no address to compare was read as this one");
    /* Its own address again: read again. */
    QVERIFY(writeFile(dev + QStringLiteral("uniq"), mac));
    writer = writerWithin(node, 3000);
    QVERIFY2(writer >= 0, "the pad never came back");
    QVERIFY(press(writer, 0));
    QTRY_COMPARE_WITH_TIMEOUT(s_pressed.loadRelaxed(), 0, 3000);
    ::close(writer);

    /* Taken apart while it reads: the reader is stopped first (its maps
       are this class's members). */
    QVERIFY(pad->isRunning());
    delete pad;
    HIDLinuxJoystick::setSysfsInputForTest(QStringLiteral("/sys/class/input"));
}

QTEST_MAIN(HIDLinuxJoystick_Test)
