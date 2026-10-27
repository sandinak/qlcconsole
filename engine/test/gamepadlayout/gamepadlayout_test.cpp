/*
  Q Light Controller Plus - Unit test
  gamepadlayout_test.cpp

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

#include "gamepadlayout_test.h"
#include "gamepadlayout.h"
#include "hidprofile.h"

using namespace GamepadLayout;

Q_DECLARE_METATYPE(GamepadLayout::Family)
Q_DECLARE_METATYPE(GamepadLayout::Transport)

namespace
{
const quint32 GD = 0x01, SIM = 0x02, BTN = 0x09, CONSUMER = 0x0C;

/** The gamepad role element (@p page, @p usage) feeds on @p family: a
 *  stick or trigger's ("lx"...), a button's ("a"...), "hat", or "" for
 *  nothing. */
QString roleOf(Family family, quint32 page, quint32 usage)
{
    const Mapping m = mapElement(family, page, usage);
    switch (m.kind)
    {
        case AxisElement:   return axisRole(m.index);
        case ButtonElement: return m.index == Share ? QStringLiteral("share") : buttonRole(m.index);
        case HatElement:    return QStringLiteral("hat");
        case Unmapped:      break;
    }
    return QString();
}

/** An 18-byte GIP input report (ID 0x20 at [0]). */
QByteArray gip(uchar b4, uchar b5, int lt, int rt, int lx, int ly, int rx, int ry)
{
    QByteArray r(18, '\0');
    r[0] = char(0x20);
    r[4] = char(b4);
    r[5] = char(b5);
    auto put = [&r](int at, int v) { r[at] = char(v & 0xFF); r[at + 1] = char((v >> 8) & 0xFF); };
    put(6, lt); put(8, rt); put(10, lx); put(12, ly); put(14, rx); put(16, ry);
    return r;
}

bool parse(const QByteArray &r, PadState *s)
{
    return parseGipReport(reinterpret_cast<const uchar *>(r.constData()), r.size(), s);
}
}

void GamepadLayout_Test::transports()
{
    QCOMPARE(transportFromPath("USB_045e_0b12_14100000"), Usb);
    QCOMPARE(transportFromPath("Bluetooth_045e_02fd_5a0b1c2d"), Bluetooth);
    QCOMPARE(transportFromPath("Bluetooth Low Energy_045e_0b13_1"), BluetoothLE);
    QCOMPARE(transportFromPath("SPU_0000_0000_0"), UnknownTransport);
    QCOMPARE(transportFromPath("0b13_1"), UnknownTransport);
    QCOMPARE(transportFromPath(QString()), UnknownTransport);
    QCOMPARE(transportFromString("Bluetooth Low Energy"), BluetoothLE);
    QCOMPARE(transportFromString("usb"), Usb);

    // Each wireless firmware enumerates under its own product ID.
    QCOMPARE(transportOfProduct(0x045E, 0x0B13), BluetoothLE);
    QCOMPARE(transportOfProduct(0x045E, 0x02FD), Bluetooth);
    QCOMPARE(transportOfProduct(0x045E, 0x0B12), Usb);
    QCOMPARE(transportOfProduct(0x054C, 0x09CC), UnknownTransport);
    QCOMPARE(transportName(BluetoothLE), QString("Bluetooth LE"));
}

void GamepadLayout_Test::families_data()
{
    QTest::addColumn<int>("vid");
    QTest::addColumn<int>("pid");
    QTest::addColumn<GamepadLayout::Transport>("transport");
    QTest::addColumn<bool>("standard");
    QTest::addColumn<int>("maxButton");
    QTest::addColumn<bool>("simulation");
    QTest::addColumn<GamepadLayout::Family>("family");
    QTest::addColumn<QString>("model");

    // USB: Apple's driver shows only the vendor element carrying GIP.
    QTest::newRow("Series X|S USB") << 0x045E << 0x0B12 << Usb << false << 0 << false
                                    << XboxGip << "Xbox Series X|S Controller";
    QTest::newRow("One USB") << 0x045E << 0x02D1 << Usb << false << 0 << false
                             << XboxGip << "Xbox One Controller";
    QTest::newRow("One 2015 USB") << 0x045E << 0x02DD << Usb << false << 0 << false
                                  << XboxGip << "Xbox One Controller (2015)";
    QTest::newRow("One S USB") << 0x045E << 0x02EA << Usb << false << 0 << false
                               << XboxGip << "Xbox One S Controller";
    QTest::newRow("Elite USB") << 0x045E << 0x02E3 << Usb << false << 0 << false
                               << XboxGip << "Xbox One Elite Controller";
    QTest::newRow("Elite 2 USB") << 0x045E << 0x0B00 << Usb << false << 0 << false
                                 << XboxGip << "Xbox Elite Series 2 Controller";
    // Bluetooth, by firmware.
    QTest::newRow("One S BT first firmware") << 0x045E << 0x02E0 << Bluetooth << true << 10 << false
                                             << XboxBtClassic << "Xbox One S Controller (first Bluetooth firmware)";
    QTest::newRow("One S BT") << 0x045E << 0x02FD << Bluetooth << true << 15 << true
                              << XboxBtModern << "Xbox One S Controller";
    QTest::newRow("One S BLE") << 0x045E << 0x0B20 << BluetoothLE << true << 15 << true
                               << XboxBtModern << "Xbox One S Controller";
    QTest::newRow("Series X|S BLE") << 0x045E << 0x0B13 << BluetoothLE << true << 15 << true
                                    << XboxBtModern << "Xbox Series X|S Controller";
    QTest::newRow("Elite 2 BT") << 0x045E << 0x0B05 << Bluetooth << true << 15 << true
                                << XboxBtModern << "Xbox Elite Series 2 Controller";
    QTest::newRow("Elite 2 BLE") << 0x045E << 0x0B22 << BluetoothLE << true << 15 << true
                                 << XboxBtModern << "Xbox Elite Series 2 Controller";
    // The table wins over what the elements suggest...
    QTest::newRow("02E0 with many buttons") << 0x045E << 0x02E0 << Bluetooth << true << 16 << false
                                            << XboxBtClassic << "Xbox One S Controller (first Bluetooth firmware)";
    // ...and an ID it does not know is read from the elements.
    QTest::newRow("unknown, gaps") << 0x045E << 0x0BFF << Bluetooth << true << 15 << false
                                   << XboxBtModern << "";
    QTest::newRow("unknown, Brake") << 0x045E << 0x0BFF << BluetoothLE << true << 10 << true
                                    << XboxBtModern << "";
    QTest::newRow("unknown, in a row") << 0x045E << 0x0BFF << Bluetooth << true << 10 << false
                                       << XboxBtClassic << "";
    QTest::newRow("unknown, no elements") << 0x045E << 0x0BFF << Usb << false << 0 << false
                                          << XboxGip << "";
    // Not ours to re-order.
    QTest::newRow("Xbox 360") << 0x045E << 0x028E << Usb << true << 15 << false
                              << NotXbox << "Xbox 360 Controller";
    // Board Q139: a DualShock 4 or DualSense goes into the same layout.
    QTest::newRow("DualShock 4") << 0x054C << 0x09CC << Bluetooth << true << 14 << false
                                 << PlayStation << "DualShock 4 (second model)";
    QTest::newRow("DualSense") << 0x054C << 0x0CE6 << Usb << true << 15 << false
                               << PlayStation << "DualSense";
    // A Sony pad it does not know (a PS3 SIXAXIS) keeps its own order.
    QTest::newRow("SIXAXIS") << 0x054C << 0x0268 << Usb << true << 19 << false
                             << NotXbox << "";
}

void GamepadLayout_Test::families()
{
    QFETCH(int, vid);
    QFETCH(int, pid);
    QFETCH(GamepadLayout::Transport, transport);
    QFETCH(bool, standard);
    QFETCH(int, maxButton);
    QFETCH(bool, simulation);
    QFETCH(GamepadLayout::Family, family);
    QFETCH(QString, model);

    QCOMPARE(GamepadLayout::family(quint16(vid), quint16(pid), transport, standard, maxButton,
                                   simulation), family);
    QCOMPARE(modelName(quint16(vid), quint16(pid)), model);
}

void GamepadLayout_Test::classicElements()
{
    const Family f = XboxBtClassic;
    const char *buttons[] = { "a", "b", "x", "y", "lb", "rb", "back", "start", "ls", "rs" };
    for (int u = 1; u <= 10; ++u)
        QCOMPARE(roleOf(f, BTN, quint32(u)), QString(buttons[u - 1]));
    QCOMPARE(roleOf(f, BTN, 11), QString());
    QCOMPARE(roleOf(f, GD, 0x30), QString("lx"));
    QCOMPARE(roleOf(f, GD, 0x31), QString("ly"));
    QCOMPARE(roleOf(f, GD, 0x32), QString("lt"));
    QCOMPARE(roleOf(f, GD, 0x33), QString("rx"));
    QCOMPARE(roleOf(f, GD, 0x34), QString("ry"));
    QCOMPARE(roleOf(f, GD, 0x35), QString("rt"));
    QCOMPARE(roleOf(f, GD, 0x39), QString("hat"));
    QCOMPARE(roleOf(f, GD, 0x85), QString("guide"));
    QCOMPARE(roleOf(f, CONSUMER, 0x223), QString("guide"));
}

void GamepadLayout_Test::modernElements()
{
    const Family f = XboxBtModern;
    const QMap<int, QString> buttons = {
        { 1, "a" }, { 2, "b" }, { 3, "" }, { 4, "x" }, { 5, "y" }, { 6, "" }, { 7, "lb" },
        { 8, "rb" }, { 9, "" }, { 10, "" }, { 11, "back" }, { 12, "start" }, { 13, "guide" },
        { 14, "ls" }, { 15, "rs" }, { 16, "" } };
    for (auto it = buttons.constBegin(); it != buttons.constEnd(); ++it)
        QVERIFY2(roleOf(f, BTN, quint32(it.key())) == it.value(),
                 qPrintable(QString("Button %1 -> \"%2\", wanted \"%3\"").arg(it.key())
                            .arg(roleOf(f, BTN, quint32(it.key())), it.value())));
    QCOMPARE(roleOf(f, GD, 0x30), QString("lx"));
    QCOMPARE(roleOf(f, GD, 0x31), QString("ly"));
    QCOMPARE(roleOf(f, GD, 0x32), QString("rx"));       // the right stick, not a trigger
    QCOMPARE(roleOf(f, GD, 0x35), QString("ry"));
    QCOMPARE(roleOf(f, GD, 0x33), QString());
    QCOMPARE(roleOf(f, GD, 0x34), QString());
    QCOMPARE(roleOf(f, SIM, 0xC5), QString("lt"));      // Brake
    QCOMPARE(roleOf(f, SIM, 0xC4), QString("rt"));      // Accelerator
    QCOMPARE(roleOf(f, GD, 0x39), QString("hat"));
    QCOMPARE(roleOf(f, CONSUMER, 0x224), QString("back"));   // View, later firmware
    QCOMPARE(roleOf(f, CONSUMER, 0x223), QString("guide"));
    QCOMPARE(roleOf(f, CONSUMER, 0xB2), QString("share"));
    QCOMPARE(roleOf(f, 0xFF00, 0x20), QString());

    // GIP and other devices are not read by element at all.
    QCOMPARE(mapElement(XboxGip, BTN, 1).kind, Unmapped);
    QCOMPARE(mapElement(NotXbox, GD, 0x30).kind, Unmapped);
}

void GamepadLayout_Test::everyControlOnce()
{
    QList<quint32> pages = { GD, SIM, BTN, CONSUMER };
    for (Family f : { XboxBtClassic, XboxBtModern })
    {
        QMap<QString, int> seen;
        for (quint32 page : pages)
            for (quint32 usage = 0; usage < 0x300; ++usage)
            {
                const QString r = roleOf(f, page, usage);
                if (!r.isEmpty())
                    seen[r]++;
            }
        for (const char *r : { "a", "b", "x", "y", "lb", "rb", "back", "start", "ls", "rs",
                               "lx", "ly", "rx", "ry", "lt", "rt", "hat" })
            QVERIFY2(seen.value(r) >= 1, qPrintable(QString("%1: nothing gives %2").arg(int(f)).arg(r)));
        for (auto it = seen.constBegin(); it != seen.constEnd(); ++it)
        {
            /* View may come as a button or a Consumer key, and the Xbox
               button as a button, System Main Menu or AC Home. */
            const int allowed = it.key() == "guide" ? 3 : it.key() == "back" ? 2 : 1;
            QVERIFY2(it.value() <= allowed, qPrintable(QString("%1: %2 elements give %3")
                                                       .arg(int(f)).arg(it.value()).arg(it.key())));
        }
    }
}

void GamepadLayout_Test::hatSwitch()
{
    bool d[4];
    auto dpad = [&d]() { return QString("%1%2%3%4").arg(d[0] ? "U" : "").arg(d[1] ? "D" : "")
                                                   .arg(d[2] ? "L" : "").arg(d[3] ? "R" : ""); };
    // 0..7 from up, clockwise; 8 (the null state) centred.
    hatToDpad(0, 0, 7, d); QCOMPARE(dpad(), QString("U"));
    hatToDpad(1, 0, 7, d); QCOMPARE(dpad(), QString("UR"));
    hatToDpad(2, 0, 7, d); QCOMPARE(dpad(), QString("R"));
    hatToDpad(3, 0, 7, d); QCOMPARE(dpad(), QString("DR"));
    hatToDpad(4, 0, 7, d); QCOMPARE(dpad(), QString("D"));
    hatToDpad(5, 0, 7, d); QCOMPARE(dpad(), QString("DL"));
    hatToDpad(6, 0, 7, d); QCOMPARE(dpad(), QString("L"));
    hatToDpad(7, 0, 7, d); QCOMPARE(dpad(), QString("UL"));
    hatToDpad(8, 0, 7, d); QCOMPARE(dpad(), QString());
    // The Xbox's Bluetooth hat: 1..8, 0 centred.
    hatToDpad(1, 1, 8, d); QCOMPARE(dpad(), QString("U"));
    hatToDpad(5, 1, 8, d); QCOMPARE(dpad(), QString("D"));
    hatToDpad(0, 1, 8, d); QCOMPARE(dpad(), QString());
}

void GamepadLayout_Test::gipReport()
{
    PadState s;
    centre(&s);

    // Nothing pressed, sticks centred.
    QVERIFY(parse(gip(0, 0, 0, 0, 0, 0, 0, 0), &s));
    for (int b = 0; b < ButtonCount; ++b)
        QVERIFY(!s.buttons[b]);
    QVERIFY(qAbs(int(s.axes[LeftX]) - 128) <= 1);
    QVERIFY(qAbs(int(s.axes[LeftY]) - 128) <= 1);
    QCOMPARE(int(s.axes[LeftTrigger]), 0);

    // Byte 4: Menu, View, A, B, X, Y from bit 2.
    const struct { uchar b4, b5; Button button; } bits[] = {
        { 0x04, 0, Menu }, { 0x08, 0, View }, { 0x10, 0, A }, { 0x20, 0, B },
        { 0x40, 0, X }, { 0x80, 0, Y },
        { 0, 0x01, DpadUp }, { 0, 0x02, DpadDown }, { 0, 0x04, DpadLeft }, { 0, 0x08, DpadRight },
        { 0, 0x10, LB }, { 0, 0x20, RB }, { 0, 0x40, LS }, { 0, 0x80, RS } };
    for (const auto &bit : bits)
    {
        QVERIFY(parse(gip(bit.b4, bit.b5, 0, 0, 0, 0, 0, 0), &s));
        for (int b = 0; b < ButtonCount; ++b)
            QVERIFY2(s.buttons[b] == (b == bit.button),
                     qPrintable(QString("bits %1/%2: %3 is %4").arg(bit.b4, 0, 16).arg(bit.b5, 0, 16)
                                .arg(buttonName(b)).arg(s.buttons[b] ? "down" : "up")));
    }

    // Sticks: right is high, UP is low (as HID) though GIP has up positive.
    QVERIFY(parse(gip(0, 0, 1023, 512, 32767, 32767, -32768, -32768), &s));
    QCOMPARE(int(s.axes[LeftX]), 255);
    QCOMPARE(int(s.axes[LeftY]), 0);
    QCOMPARE(int(s.axes[RightX]), 0);
    QCOMPARE(int(s.axes[RightY]), 255);
    QCOMPARE(int(s.axes[LeftTrigger]), 255);
    QVERIFY(qAbs(int(s.axes[RightTrigger]) - 127) <= 1);

    // The Xbox button has a report of its own; the rest of the state stays.
    QByteArray guide(5, '\0');
    guide[0] = char(0x07);
    guide[4] = char(0x01);
    QVERIFY(parse(guide, &s));
    QVERIFY(s.buttons[Guide]);
    QCOMPARE(int(s.axes[LeftX]), 255);
    guide[4] = 0;
    QVERIFY(parse(guide, &s));
    QVERIFY(!s.buttons[Guide]);

    // Not a state report, or cut short: untouched.
    const PadState before = s;
    QByteArray other = gip(0x10, 0, 0, 0, 0, 0, 0, 0);
    other[0] = char(0x03);
    QVERIFY(!parse(other, &s));
    QVERIFY(!parse(gip(0x10, 0, 0, 0, 0, 0, 0, 0).left(17), &s));
    QVERIFY(!s.buttons[A]);
    QCOMPARE(int(s.axes[LeftX]), int(before.axes[LeftX]));
}

void GamepadLayout_Test::playStationElements()
{
    // Cross is A, Circle B, Square X, Triangle Y; Create is View, Options Menu.
    QCOMPARE(roleOf(PlayStation, BTN, 1), QString("x"));
    QCOMPARE(roleOf(PlayStation, BTN, 2), QString("a"));
    QCOMPARE(roleOf(PlayStation, BTN, 3), QString("b"));
    QCOMPARE(roleOf(PlayStation, BTN, 4), QString("y"));
    QCOMPARE(roleOf(PlayStation, BTN, 5), QString("lb"));
    QCOMPARE(roleOf(PlayStation, BTN, 6), QString("rb"));
    QVERIFY(roleOf(PlayStation, BTN, 7).isEmpty());       // L2's click: the trigger is the axis
    QVERIFY(roleOf(PlayStation, BTN, 8).isEmpty());
    QCOMPARE(roleOf(PlayStation, BTN, 9), QString("back"));
    QCOMPARE(roleOf(PlayStation, BTN, 10), QString("start"));
    QCOMPARE(roleOf(PlayStation, BTN, 11), QString("ls"));
    QCOMPARE(roleOf(PlayStation, BTN, 12), QString("rs"));
    QCOMPARE(roleOf(PlayStation, BTN, 13), QString("guide"));
    QCOMPARE(mapElement(PlayStation, BTN, 14).index, int(Share));
    QCOMPARE(roleOf(PlayStation, GD, 0x30), QString("lx"));
    QCOMPARE(roleOf(PlayStation, GD, 0x31), QString("ly"));
    QCOMPARE(roleOf(PlayStation, GD, 0x32), QString("rx"));
    QCOMPARE(roleOf(PlayStation, GD, 0x35), QString("ry"));
    QCOMPARE(roleOf(PlayStation, GD, 0x33), QString("lt"));
    QCOMPARE(roleOf(PlayStation, GD, 0x34), QString("rt"));
    QCOMPARE(mapElement(PlayStation, GD, 0x39).kind, HatElement);
    QVERIFY(isPlayStation(kSonyVendorId, 0x05C4));
    QVERIFY(!isPlayStation(kSonyVendorId, 0x0268));
    QVERIFY(!isPlayStation(kMicrosoftVendorId, 0x05C4));
    QCOMPARE(familyName(PlayStation), QString("PlayStation HID layout"));
}

void GamepadLayout_Test::matchesThePlayStationProfile()
{
    const QString path = QString(QLC_TEST_RESOURCE_ROOT) + "hidprofiles/playstation.qxhid";
    HIDProfile *p = HIDProfile::load(path);
    QVERIFY2(p != nullptr, qPrintable(path));
    // The same channels, roles and all, under PlayStation names.
    for (int a = 0; a < AxisCount; ++a)
        QCOMPARE(p->axisRole(a), axisRole(a));
    for (int b = 0; b < ButtonCount; ++b)
    {
        QCOMPARE(p->buttonRole(b), buttonRole(b));
        QVERIFY(!p->buttonName(b).isEmpty());
    }
    QCOMPARE(p->buttonName(A), QString("Cross"));
    for (quint16 pid : { 0x05C4, 0x09CC, 0x0BA0, 0x0CE6, 0x0DF2 })
    {
        QVERIFY(p->matchesDevice(kSonyVendorId, pid));
        QVERIFY(isPlayStation(kSonyVendorId, pid));
    }
    QVERIFY(!p->matchesDevice(kSonyVendorId, 0x0268));
    delete p;
}

void GamepadLayout_Test::matchesTheXboxProfile()
{
    const QString path = QString(QLC_TEST_RESOURCE_ROOT) + "hidprofiles/xbox.qxhid";
    HIDProfile *p = HIDProfile::load(path);
    QVERIFY2(p != nullptr, qPrintable(path));

    // What the reader sends on each channel is what the profile calls it.
    for (int a = 0; a < AxisCount; ++a)
    {
        QCOMPARE(p->axisRole(a), axisRole(a));
        QCOMPARE(p->axisName(a), axisName(a));
    }
    for (int b = 0; b < ButtonCount; ++b)
    {
        QCOMPARE(p->buttonRole(b), buttonRole(b));
        QVERIFY2(!p->buttonName(b).isEmpty(), qPrintable(buttonName(b)));
    }

    // Every model, wired or wireless, gets the profile.
    for (quint16 pid : { 0x02D1, 0x02DD, 0x02E3, 0x02EA, 0x0B00, 0x0B0A, 0x0B12, 0x02E0, 0x02FD,
                         0x0B05, 0x0B0C, 0x0B13, 0x0B20, 0x0B21, 0x0B22 })
        QVERIFY2(p->matchesDevice(kMicrosoftVendorId, pid),
                 qPrintable(QString("%1 is not in the profile").arg(pid, 4, 16, QChar('0'))));
    delete p;
}

void GamepadLayout_Test::noShippedProfileStepsAList()
{
    for (const char *name : { "xbox.qxhid", "playstation.qxhid" })
    {
        const QString path = QString(QLC_TEST_RESOURCE_ROOT) + "hidprofiles/" + name;
        HIDProfile *p = HIDProfile::load(path);
        QVERIFY2(p != nullptr, qPrintable(path));
        for (int b = 0; b < ButtonCount; ++b)
            QVERIFY2(!p->buttonAction(b).startsWith(QLatin1String("chaser_step")),
                     qPrintable(QString("%1: %2 is %3").arg(name, p->buttonName(b), p->buttonAction(b))));
        QVERIFY(p->buttonAction(A).isEmpty());
        QVERIFY(p->buttonAction(B).isEmpty());
        delete p;
    }
}

QTEST_GUILESS_MAIN(GamepadLayout_Test)
