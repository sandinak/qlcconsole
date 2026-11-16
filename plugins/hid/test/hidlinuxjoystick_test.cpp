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
}

void HIDLinuxJoystick_Test::aPadThatGoesIsWaitedForAndReadAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString node = dir.filePath(QStringLiteral("js0"));
    const QString away = dir.filePath(QStringLiteral("js0.gone"));
    QCOMPARE(::mkfifo(QFile::encodeName(node).constData(), 0600), 0);

    QByteArray path = QFile::encodeName(node);
    wchar_t empty[] = L"";
    struct hid_device_info info;
    memset(&info, 0, sizeof(info));
    info.path = path.data();
    info.serial_number = empty;
    info.manufacturer_string = empty;
    info.product_string = empty;

    HIDLinuxJoystick *pad = new HIDLinuxJoystick(nullptr, 0, &info);
    s_pressed.storeRelaxed(-1);
    connect(pad, &HIDDevice::valueChanged, this,
            [](quint32, quint32, quint32 channel, uchar value) {
                if (channel == 0)
                    s_pressed.storeRelaxed(value);
            }, Qt::QueuedConnection);
    QVERIFY(pad->openInput());
    QTRY_VERIFY(pad->isRunning());
    QVERIFY(pad->handle() >= 0);

    /* The pad goes: its fd answers at once with a hang-up and reads
       nothing, and the node is not there to open again. */
    QCOMPARE(::rename(QFile::encodeName(node).constData(), QFile::encodeName(away).constData()), 0);
    int dead[2];
    QCOMPARE(::pipe(dead), 0);
    ::close(dead[1]);
    QCOMPARE(::dup2(dead[0], pad->handle()), pad->handle());
    ::close(dead[0]);

    /* Waiting for it costs (next to) nothing: the old reader spun on
       poll() at a whole core. */
    const qint64 cpu0 = processCpuMs();
    QTest::qSleep(1000);
    const qint64 used = processCpuMs() - cpu0;
    QVERIFY2(used < 100, qPrintable(QStringLiteral("%1 ms of CPU in 1 s waiting for a gone pad").arg(used)));

    /* Back at the same node: read again, a button press arrives. */
    QCOMPARE(::rename(QFile::encodeName(away).constData(), QFile::encodeName(node).constData()), 0);
    /* A FIFO opens for writing only once the reader has it again. */
    int writer = -1;
    QElapsedTimer waited;
    waited.start();
    while (writer < 0 && waited.elapsed() < 3000)
    {
        writer = ::open(QFile::encodeName(node).constData(), O_WRONLY | O_NONBLOCK);
        if (writer < 0)
            QTest::qWait(20);
    }
    QVERIFY2(writer >= 0, "the reader never opened the node again");
    struct js_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = JS_EVENT_BUTTON;
    ev.number = 0;
    ev.value = 1;
    QCOMPARE(int(::write(writer, &ev, sizeof(ev))), int(sizeof(ev)));
    QTRY_COMPARE_WITH_TIMEOUT(s_pressed.loadRelaxed(), 255, 3000);
    ::close(writer);

    /* Taken apart while it reads: the reader is stopped first (its maps
       are this class's members). */
    QVERIFY(pad->isRunning());
    delete pad;
}

QTEST_MAIN(HIDLinuxJoystick_Test)
