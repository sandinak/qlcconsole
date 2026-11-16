/*
  Q Light Controller Plus
  hidtrace.h

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

#ifndef HIDTRACE_H
#define HIDTRACE_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QStringList>
#include <QThread>
#include <QWaitCondition>
#include <QFile>
#include <QString>

class HIDDevice;

/**
 * A record of what the HID plugin did in one session, for QLC_HID_RECORD=
 * <dir>: pads-<date-time>.jsonl there, one JSON event per line, each with
 * "t" (monotonic ms since the session began) and "ev":
 *
 *   arrived      path, vid, pid (hex), name, serial, location,
 *                identity (what ControllerJobs gives it), axes, buttons
 *   gone         path, name
 *   input        path, ch (channel index), value (as delivered to the engine)
 *   open_failed  path, name, vid, pid, why ("stuck" | "nothing" | "given_up")
 *
 * Plugin-level events only, never raw HID reports. The plugin holds one of
 * these only when the variable is set: unset, the cost is one null check
 * per event. Lines are queued and written by a thread of its own at the
 * lowest priority, so the reader and the GUI never wait on the disk; a
 * queue that outgrows kMaxQueued drops lines (counted in a final "dropped"
 * event) rather than grow.
 *
 * plugins/hid/test/padtraceplayer.h plays a trace back through the plugin.
 */
class HIDTrace final : public QThread
{
    Q_OBJECT

public:
    /** A recorder writing into @p dir (made), or null if it cannot. */
    static HIDTrace *create(const QString &dir);
    /** Writes what is queued, then ends. */
    ~HIDTrace() override;

    QString fileName() const { return m_file.fileName(); }

    void arrived(const HIDDevice *dev, const QString &serial);
    void gone(const HIDDevice *dev);
    void input(const QString &path, quint32 channel, uchar value);
    void openFailed(const QString &path, const QString &name, unsigned short vid,
                    unsigned short pid, const QString &why);

    /** What ControllerJobs::identityFor() gives a device of the HID plugin:
     *  mirrored here, as the plugin does not link the engine (the engine's
     *  padtraces test holds the two equal). */
    static QString identityFor(const QString &name, const QString &location);

    static constexpr int kMaxQueued = 200000;

protected:
    void run() override;

private:
    HIDTrace() = default;
    void push(QByteArray line);
    qint64 now() const { return m_clock.elapsed(); }

    QFile m_file;
    QElapsedTimer m_clock;
    QMutex m_lock;
    QWaitCondition m_wake;
    QList<QByteArray> m_queue;
    bool m_quit = false;
    int m_dropped = 0;
};

#endif
