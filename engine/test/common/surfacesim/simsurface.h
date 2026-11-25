/*
  Q Light Controller Plus
  simsurface.h

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

#ifndef SIMSURFACE_H
#define SIMSURFACE_H

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QtTest>

/**
 * A simulated control surface (docs/design/SURFACE_SIMULATOR.md): a device
 * that is not there, entering the app where the real one does -- a MIDI
 * board as the bytes it sends, through the MIDI plugin's own decoder, onto
 * a QLCIOPlugin's valueChanged(); a game pad as the HID reports it sends,
 * through the HID plugin's own layout tables, onto its virtual pad -- and
 * keeping everything the app sends back (lamps, MIDI out, light, rumble).
 *
 * What a person does is said by control NAME, in the device's own words
 * ("Go", "Master", "r2", "cross"); a control the device does not have fails
 * the test there (lastError()), never silently does nothing.
 *
 * Header only: the engine's and the UI's tests share it
 * (engine/test/common/surfacesim/surfacesim.cmake).
 */
class SimSurface
{
public:
    virtual ~SimSurface() = default;

    /** What the device is ("PMJ Black 1", "DualSense"). */
    virtual QString deviceName() const = 0;

    /** A control's value as the device would send it, in the device's own
     *  range (MIDI 0..127; a pad's axis 0..255, a button 0 or 1). False:
     *  no such control (lastError() says which). */
    virtual bool set(const QString &control, int value) = 0;
    /** The device's top value for @p control (127 for MIDI, 1 for a pad
     *  button, 255 for a pad axis). */
    virtual int top(const QString &control) const = 0;
    /** Bytes exactly as the device sends them: MIDI messages, or one HID
     *  input report. */
    virtual bool raw(const QByteArray &bytes) = 0;

    /** The cable out: the system stops listing the device. */
    virtual bool unplug() = 0;
    /** The cable back in. @p dump: what the device sends first -- its
     *  state, control by control (many boards do on connect). */
    virtual bool replug(const QList<QPair<QString, int>> &dump = {}) = 0;

    /** Everything the app has queued and the input patches hold, taken
     *  (the input flush a MasterTimer tick runs; posted events). */
    virtual void settle() = 0;

    /* ---- what a person does, built on the above ---- */

    bool press(const QString &control) { return set(control, top(control)); }
    bool release(const QString &control) { return set(control, 0); }
    /** Press, hold @p holdMs, release. */
    bool tap(const QString &control, int holdMs = 40)
    {
        if (!press(control))
            return false;
        wait(holdMs);
        return release(control);
    }
    /** Several held at once, in this order, then let go in reverse. */
    bool chord(const QStringList &controls, int holdMs = 40)
    {
        for (const QString &c : controls)
            if (!press(c))
                return false;
        wait(holdMs);
        for (int i = int(controls.size()) - 1; i >= 0; --i)
            if (!release(controls.at(i)))
                return false;
        return true;
    }
    /** A fader or stick moved from @p from to @p to over @p ms, in steps
     *  of about 10 ms (a hand's move, as the device reports it). */
    bool ramp(const QString &control, int from, int to, int ms = 100)
    {
        const int steps = qMax(1, ms / 10);
        for (int i = 0; i <= steps; ++i)
        {
            if (!set(control, from + (to - from) * i / steps))
                return false;
            if (i < steps)
                wait(ms / steps);
        }
        return true;
    }
    /** A fader that sits at @p at but whose pot is noisy: @p count reads
     *  alternating @p at and @p at + 1 (fader jitter). */
    bool jitter(const QString &control, int at, int count = 10)
    {
        for (int i = 0; i < count; ++i)
            if (!set(control, at + (i % 2)))
                return false;
        return true;
    }

    /** Real time passes (a hold, a gap), the app running meanwhile;
     *  timeScale() shortens it (0: none). */
    void wait(int ms)
    {
        const int scaled = int(ms * m_timeScale);
        settle();
        if (scaled > 0)
            QTest::qWait(scaled);
        settle();
    }
    void setTimeScale(double scale) { m_timeScale = scale; }
    double timeScale() const { return m_timeScale; }

    /** Why the last call failed ("no control 'Gp' on PMJ Black 1"). */
    QString lastError() const { return m_error; }

protected:
    bool fail(const QString &why)
    {
        m_error = why;
        qWarning().noquote() << "[SimSurface]" << deviceName() << why;
        return false;
    }

    QString m_error;
    double m_timeScale = 1.0;
};

/** settle() as every simulated device does it: posted events, three times
 *  (a slot that posts again lands too). */
inline void simProcessEvents()
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

#endif
