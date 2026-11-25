/*
  Q Light Controller Plus
  simpad.h

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

#ifndef SIMPAD_H
#define SIMPAD_H

#include <QMap>
#include <QMetaObject>
#include <QVariantList>
#include <cstring>

#include "simsurface.h"
#include "fakehidbackend.h"
#include "gamepadlayout.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"
#include "doc.h"

/**
 * A game pad that is not there, as the HID reports it sends.
 *
 * Listed by the system (FakeHIDBackend: vendor, product, name, path),
 * opened by the real HID plugin into its virtual pad, patched as a person
 * patches it. A press is a whole input report, byte for byte as the pad
 * sends it over USB:
 *  - DualSense: report 0x01 (64 bytes): sticks, L2/R2, the hat and the
 *    face buttons in byte 8, L1 R1 L2 R2 Create Options L3 R3 in byte 9,
 *    PS / touchpad click / mute in byte 10 (as hid-playstation reads it);
 *  - DualShock 4: report 0x01: sticks, buttons in 5-7, L2/R2 in 8-9;
 *  - Xbox (USB through Apple's driver): the GIP report 0x20 (and 0x07,
 *    the Xbox button).
 * Each report goes through the plugin's own reading: the GIP report
 * through GamepadLayout::parseGipReport(); a PlayStation report as its HID
 * elements (usage page, usage, logical range -- the pad's report
 * descriptor), each through GamepadLayout::mapElement() /
 * scaleToByte() / hatToDpad(), as HIDOSXJoystick::processValue() reads
 * them. What changed leaves as that reader's emitChanges() sends it --
 * axes on channels 0-5, buttons after them -- through the virtual pad,
 * the signal a real pad's reads leave through.
 *
 * Out: what the app sends the pad (light, rumble, triggers) is kept by the
 * virtual pad; takeFeedback() reads it.
 *
 * Controls by the layout's role names ("a", "lb", "rt", "lx", "dpad_up"),
 * or the pad's own ("cross", "l1", "r2", "options").
 */
class SimPad final : public SimSurface
{
public:
    enum Model { DualSense, DualShock4, XboxUsb };

    SimPad(Doc *doc, QLCIOPlugin *hid, FakeHIDBackend *fake, Model model,
           const QString &path = QString(), quint32 universe = 0)
        : m_doc(doc), m_hid(hid), m_fake(fake), m_model(model), m_universe(universe)
    {
        m_path = path.isEmpty() ? QStringLiteral("USB_%1_%2_14200000")
                                      .arg(vendorId(), 4, 16, QLatin1Char('0'))
                                      .arg(productId(), 4, 16, QLatin1Char('0'))
                                : path;
        GamepadLayout::centre(&m_state);
        m_sent = m_state;
    }

    quint16 vendorId() const
    {
        return m_model == XboxUsb ? GamepadLayout::kMicrosoftVendorId : GamepadLayout::kSonyVendorId;
    }
    quint16 productId() const
    {
        switch (m_model)
        {
            case DualSense:  return 0x0CE6;
            case DualShock4: return 0x09CC;
            case XboxUsb:    return 0x0B12;
        }
        return 0;
    }
    QString deviceName() const override
    {
        switch (m_model)
        {
            case DualSense:  return QStringLiteral("DualSense Wireless Controller");
            case DualShock4: return QStringLiteral("Wireless Controller");
            case XboxUsb:    return QStringLiteral("Xbox Wireless Controller");
        }
        return QString();
    }
    QString path() const { return m_path; }
    quint32 universe() const { return m_universe; }
    /** The plugin line it is on (-1: not there). */
    int line() const
    {
        int l = -1;
        QMetaObject::invokeMethod(m_hid, "inputForPath", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, l), Q_ARG(QString, m_path));
        return l;
    }

    /** Listed, opened by the plugin, patched on universe(). The plugin
     *  must already have @p fake as its test backend (setTestBackend()). */
    bool plug()
    {
        m_fake->list(m_path, vendorId(), productId(), deviceName(), 0, ++m_instance);
        QMetaObject::invokeMethod(m_hid, "rescanDevices", Qt::DirectConnection);
        if (!waitListed(true))
            return fail(QStringLiteral("the plugin did not open it"));
        InputOutputMap *iom = m_doc->inputOutputMap();
        while (iom->universesCount() <= m_universe)
            if (!iom->addUniverse())
                return fail(QStringLiteral("no universe %1").arg(m_universe + 1));
        if (!iom->setInputPatch(m_universe, m_hid->name(), deviceName(), quint32(line())))
            return fail(QStringLiteral("its input did not patch"));
        GamepadLayout::centre(&m_state);
        m_sent = m_state;
        m_sentValid = false;
        settle();
        return true;
    }

    /* ---- in ---- */

    /** @p control at @p value: a button 0 / 1 (or any non-zero), an axis
     *  0..255 (sticks centred at 128, 0 = left / up; triggers 0 released).
     *  The whole report the pad would send with it is built and fed. */
    bool set(const QString &control, int value) override
    {
        GamepadLayout::Mapping m = mappingOf(control);
        if (m.kind == GamepadLayout::Unmapped)
            return fail(QStringLiteral("no control '%1'").arg(control));
        if (m.kind == GamepadLayout::AxisElement)
            m_held.axes[m.index] = uchar(qBound(0, value, 255));
        else
            m_held.buttons[m.index] = value != 0;
        return raw(report());
    }
    int top(const QString &control) const override
    {
        return mappingOf(control).kind == GamepadLayout::AxisElement ? 255 : 1;
    }

    /** One input report, as the pad sends it. */
    bool raw(const QByteArray &bytes) override
    {
        if (line() < 0)
            return fail(QStringLiteral("a report while unplugged"));
        GamepadLayout::PadState next = m_state;
        if (m_model == XboxUsb)
        {
            if (!GamepadLayout::parseGipReport(reinterpret_cast<const uchar *>(bytes.constData()),
                                               int(bytes.size()), &next))
                return fail(QStringLiteral("parseGipReport() refused the report"));
        }
        else if (!readPlayStation(bytes, &next))
            return fail(QStringLiteral("not a %1 input report").arg(deviceName()));
        m_state = next;
        emitChanges();
        settle();
        return true;
    }

    /** The report the pad sends for what is held now (set()). */
    QByteArray report() const
    {
        const GamepadLayout::PadState &s = m_held;
        auto b = [&s](int button) { return s.buttons[button] ? 1 : 0; };
        if (m_model == XboxUsb)
        {
            QByteArray r(18, char(0));
            r[0] = char(0x20);
            r[3] = char(14);
            r[4] = char((b(GamepadLayout::Menu) << 2) | (b(GamepadLayout::View) << 3)
                        | (b(GamepadLayout::A) << 4) | (b(GamepadLayout::B) << 5)
                        | (b(GamepadLayout::X) << 6) | (b(GamepadLayout::Y) << 7));
            r[5] = char(b(GamepadLayout::DpadUp) | (b(GamepadLayout::DpadDown) << 1)
                        | (b(GamepadLayout::DpadLeft) << 2) | (b(GamepadLayout::DpadRight) << 3)
                        | (b(GamepadLayout::LB) << 4) | (b(GamepadLayout::RB) << 5)
                        | (b(GamepadLayout::LS) << 6) | (b(GamepadLayout::RS) << 7));
            auto put16 = [&r](int at, int v) { r[at] = char(v & 0xFF); r[at + 1] = char((v >> 8) & 0xFF); };
            put16(6, s.axes[GamepadLayout::LeftTrigger] * 1023 / 255);
            put16(8, s.axes[GamepadLayout::RightTrigger] * 1023 / 255);
            auto stick = [](int byte) { return (byte - 128) * 256; };
            put16(10, stick(s.axes[GamepadLayout::LeftX]));
            put16(12, stick(255 - s.axes[GamepadLayout::LeftY]));
            put16(14, stick(s.axes[GamepadLayout::RightX]));
            put16(16, stick(255 - s.axes[GamepadLayout::RightY]));
            return r;
        }
        const int hat = hatValue(b(GamepadLayout::DpadUp), b(GamepadLayout::DpadDown),
                                 b(GamepadLayout::DpadLeft), b(GamepadLayout::DpadRight));
        const uchar face = uchar((b(GamepadLayout::X) << 4) | (b(GamepadLayout::A) << 5)
                                 | (b(GamepadLayout::B) << 6) | (b(GamepadLayout::Y) << 7) | hat);
        const uchar l2 = s.axes[GamepadLayout::LeftTrigger], r2 = s.axes[GamepadLayout::RightTrigger];
        const uchar shoulders = uchar(b(GamepadLayout::LB) | (b(GamepadLayout::RB) << 1)
                                      | ((l2 > 0) << 2) | ((r2 > 0) << 3)
                                      | (b(GamepadLayout::View) << 4) | (b(GamepadLayout::Menu) << 5)
                                      | (b(GamepadLayout::LS) << 6) | (b(GamepadLayout::RS) << 7));
        const uchar system = uchar(b(GamepadLayout::Guide) | (b(GamepadLayout::Share) << 1));
        QByteArray r(64, char(0));
        r[0] = char(0x01);
        r[1] = char(s.axes[GamepadLayout::LeftX]);
        r[2] = char(s.axes[GamepadLayout::LeftY]);
        r[3] = char(s.axes[GamepadLayout::RightX]);
        r[4] = char(s.axes[GamepadLayout::RightY]);
        if (m_model == DualSense)
        {
            r[5] = char(l2);
            r[6] = char(r2);
            r[7] = char(m_counter);
            r[8] = char(face);
            r[9] = char(shoulders);
            r[10] = char(system);
        }
        else
        {
            r[5] = char(face);
            r[6] = char(shoulders);
            r[7] = char(system | (m_counter << 2));
            r[8] = char(l2);
            r[9] = char(r2);
        }
        return r;
    }

    /* ---- out ---- */

    /** What the app sent the pad (sendFeedBack()'s params) since the last
     *  take: {"gamepad": "light", "rgb": "#ff0000"}, "rumble", "triggers". */
    QVariantList takeFeedback()
    {
        settle();
        QVariantList out;
        const int l = line();
        if (l >= 0)
            QMetaObject::invokeMethod(m_hid, "takeVirtualFeedback", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariantList, out), Q_ARG(quint32, quint32(l)));
        m_feedback << out;
        return out;
    }
    /** Every feedback taken so far. */
    QVariantList feedback() const { return m_feedback; }
    /** The colour of the last light the app sent ("" for the pad's own;
     *  invalid if none was sent). */
    QString lastLight()
    {
        takeFeedback();
        for (int i = int(m_feedback.size()) - 1; i >= 0; --i)
        {
            const QVariantMap p = m_feedback.at(i).toMap();
            if (p.value(QStringLiteral("gamepad")).toString() == QLatin1String("light"))
                return p.value(QStringLiteral("rgb")).toString();
        }
        return QStringLiteral("(none)");
    }

    /* ---- the channels, checked against the plugin ---- */

    /** The plugin channel @p control arrives on. */
    int channelOf(const QString &control) const
    {
        const GamepadLayout::Mapping m = mappingOf(control);
        if (m.kind == GamepadLayout::AxisElement)
            return m.index;
        if (m.kind == GamepadLayout::ButtonElement)
            return GamepadLayout::AxisCount + m.index;
        return -1;
    }
    /** Every control of the layout whose channel the plugin gives another
     *  role than the layout does ("rt on 5: plugin says 'lt'"): the
     *  simulator and the real plugin disagree, so this pad's tests would
     *  prove nothing. Empty: they agree. */
    QStringList roleMismatches() const
    {
        QStringList out;
        const int l = line();
        if (l < 0)
            return { QStringLiteral("not plugged") };
        for (int a = 0; a < GamepadLayout::AxisCount; ++a)
            if (m_hid->inputChannelRole(quint32(l), quint32(a)) != GamepadLayout::axisRole(a))
                out << QStringLiteral("axis %1 (%2): plugin says '%3'").arg(a).arg(GamepadLayout::axisRole(a),
                                                                               m_hid->inputChannelRole(quint32(l), quint32(a)));
        for (int b = 0; b < GamepadLayout::ButtonCount; ++b)
        {
            const QString want = GamepadLayout::buttonRole(b);
            if (want.isEmpty())
                continue;   // Share: no gamepad role (a PlayStation pad's touchpad click)
            const QString got = m_hid->inputChannelRole(quint32(l), quint32(GamepadLayout::AxisCount + b));
            if (want != got)
                out << QStringLiteral("button %1 (%2): plugin says '%3'").arg(b).arg(want, got);
        }
        return out;
    }

    /* ---- the cable ---- */

    bool unplug() override
    {
        m_fake->unlist(m_path);
        QMetaObject::invokeMethod(m_hid, "rescanDevices", Qt::DirectConnection);
        if (!waitListed(false))
            return fail(QStringLiteral("still there after the unplug"));
        settle();
        return true;
    }
    bool replug(const QList<QPair<QString, int>> &dump = {}) override
    {
        GamepadLayout::centre(&m_state);
        GamepadLayout::centre(&m_held);
        m_sent = m_state;
        m_sentValid = false;
        m_fake->list(m_path, vendorId(), productId(), deviceName(), 0, ++m_instance);
        QMetaObject::invokeMethod(m_hid, "rescanDevices", Qt::DirectConnection);
        if (!waitListed(true))
            return fail(QStringLiteral("not back after the replug"));
        settle();
        for (const auto &d : dump)
            if (!set(d.first, d.second))
                return false;
        return true;
    }

    void settle() override
    {
        simFlushInputs(m_doc->inputOutputMap());
    }

    /** A stick that does not centre: it rests @p offset off centre
     *  (+-1..127) and wanders by one, @p reads times (stick drift). */
    bool drift(const QString &stick, int offset, int reads = 20)
    {
        for (int i = 0; i < reads; ++i)
            if (!set(stick, 128 + offset + (i % 2)))
                return false;
        return true;
    }

private:
    GamepadLayout::Mapping mappingOf(const QString &control) const
    {
        static const QMap<QString, QString> own = {
            { "cross", "a" }, { "circle", "b" }, { "square", "x" }, { "triangle", "y" },
            { "l1", "lb" }, { "r1", "rb" }, { "l2", "lt" }, { "r2", "rt" },
            { "create", "back" }, { "share", "back" }, { "view", "back" },
            { "options", "start" }, { "menu", "start" },
            { "l3", "ls" }, { "r3", "rs" }, { "ps", "guide" }, { "xbox", "guide" },
            { "touchpad", "share" },
        };
        const QString c = control.toLower();
        const QString role = own.value(c, c);
        GamepadLayout::Mapping m;
        for (int a = 0; a < GamepadLayout::AxisCount; ++a)
            if (GamepadLayout::axisRole(a) == role)
            {
                m.kind = GamepadLayout::AxisElement;
                m.index = a;
                return m;
            }
        for (int b = 0; b < GamepadLayout::ButtonCount; ++b)
            if (GamepadLayout::buttonRole(b) == role || (role == QLatin1String("share") && b == GamepadLayout::Share))
            {
                m.kind = GamepadLayout::ButtonElement;
                m.index = b;
                return m;
            }
        return m;
    }

    static int hatValue(int up, int down, int left, int right)
    {
        // 0 = N, clockwise; 8 = released (the null state).
        if (up && right) return 1;
        if (down && right) return 3;
        if (down && left) return 5;
        if (up && left) return 7;
        if (up) return 0;
        if (right) return 2;
        if (down) return 4;
        if (left) return 6;
        return 8;
    }

    /** The report's HID elements, each read as the plugin reads it. */
    bool readPlayStation(const QByteArray &bytes, GamepadLayout::PadState *state)
    {
        const bool ds = m_model == DualSense;
        if (bytes.size() < (ds ? 11 : 10) || uchar(bytes.at(0)) != 0x01)
            return false;
        auto at = [&bytes](int i) { return int(uchar(bytes.at(i))); };
        const int face = ds ? at(8) : at(5);
        const int shoulders = ds ? at(9) : at(6);
        const int system = ds ? at(10) : at(7);
        struct Element { quint32 page, usage; int raw, min, max; };
        QList<Element> els = {
            { 0x01, 0x30, at(1), 0, 255 },          // X: left stick
            { 0x01, 0x31, at(2), 0, 255 },          // Y
            { 0x01, 0x32, at(3), 0, 255 },          // Z: right stick X
            { 0x01, 0x35, at(4), 0, 255 },          // Rz: right stick Y
            { 0x01, 0x33, ds ? at(5) : at(8), 0, 255 },     // Rx: L2
            { 0x01, 0x34, ds ? at(6) : at(9), 0, 255 },     // Ry: R2
            { 0x01, 0x39, face & 0x0F, 0, 7 },      // the hat (8: released)
        };
        // Buttons 1-14: Square Cross Circle Triangle, L1 R1 L2 R2 Create Options L3 R3, PS, touchpad.
        const int bits = ((face >> 4) & 0x0F) | (shoulders << 4) | ((system & 0x03) << 12);
        for (int u = 1; u <= 14; ++u)
            els << Element{ 0x09, quint32(u), (bits >> (u - 1)) & 1, 0, 1 };

        const GamepadLayout::Family family = GamepadLayout::PlayStation;
        for (const Element &e : std::as_const(els))
        {
            const GamepadLayout::Mapping m = GamepadLayout::mapElement(family, e.page, e.usage);
            switch (m.kind)
            {
                case GamepadLayout::AxisElement:
                    state->axes[m.index] = GamepadLayout::scaleToByte(e.raw, e.min, e.max);
                    break;
                case GamepadLayout::ButtonElement:
                    state->buttons[m.index] = e.raw != 0;
                    break;
                case GamepadLayout::HatElement:
                {
                    bool dpad[4];
                    GamepadLayout::hatToDpad(e.raw, e.min, e.max, dpad);
                    state->buttons[GamepadLayout::DpadUp] = dpad[0];
                    state->buttons[GamepadLayout::DpadDown] = dpad[1];
                    state->buttons[GamepadLayout::DpadLeft] = dpad[2];
                    state->buttons[GamepadLayout::DpadRight] = dpad[3];
                    break;
                }
                case GamepadLayout::Unmapped:
                    break;
            }
        }
        return true;
    }

    /** HIDOSXJoystick::emitChanges(): what changed, axes then buttons. */
    void emitChanges()
    {
        const int l = line();
        auto inject = [this, l](int channel, int value) {
            bool ok = false;
            QMetaObject::invokeMethod(m_hid, "injectVirtualInput", Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, ok), Q_ARG(quint32, quint32(l)),
                                      Q_ARG(quint32, quint32(channel)), Q_ARG(uint, uint(value)));
            if (!ok)
                fail(QStringLiteral("the virtual pad took no input on channel %1").arg(channel));
        };
        for (int a = 0; a < GamepadLayout::AxisCount; ++a)
            if (!m_sentValid || m_sent.axes[a] != m_state.axes[a])
                inject(a, m_state.axes[a]);
        for (int b = 0; b < GamepadLayout::ButtonCount; ++b)
            if (!m_sentValid || m_sent.buttons[b] != m_state.buttons[b])
                inject(GamepadLayout::AxisCount + b, m_state.buttons[b] ? 255 : 0);
        m_sent = m_state;
        m_sentValid = true;
        m_counter++;
    }

    bool waitListed(bool present)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000)
        {
            int opening = 0;
            bool scanning = false;
            QMetaObject::invokeMethod(m_hid, "openingCount", Qt::DirectConnection, Q_RETURN_ARG(int, opening));
            QMetaObject::invokeMethod(m_hid, "isScanning", Qt::DirectConnection, Q_RETURN_ARG(bool, scanning));
            if ((line() >= 0) == present && opening == 0 && !scanning)
                return true;
            QTest::qWait(5);
        }
        return false;
    }

    Doc *m_doc;
    QLCIOPlugin *m_hid;
    FakeHIDBackend *m_fake;
    Model m_model;
    quint32 m_universe;
    QString m_path;
    quint64 m_instance = 0;
    GamepadLayout::PadState m_state;   // as the plugin read the last report
    GamepadLayout::PadState m_sent;    // as last sent on
    GamepadLayout::PadState m_held = initialHeld();    // what set() holds
    bool m_sentValid = false;
    uchar m_counter = 0;
    QVariantList m_feedback;

    static GamepadLayout::PadState initialHeld()
    {
        GamepadLayout::PadState s;
        GamepadLayout::centre(&s);
        return s;
    }
};

#endif
