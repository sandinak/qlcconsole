/*
  Q Light Controller Plus - qlcconsole
  inputdevicenotices.h

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

#ifndef INPUTDEVICENOTICES_H
#define INPUTDEVICENOTICES_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include <QStringList>
#include "qlcui_export.h"

class QLCIOPlugin;
class QWidget;
class QTimer;
class Doc;

/**
 * Says when a device a universe's input is patched to -- a game controller,
 * a MIDI surface -- comes or goes.
 *
 * The engine keeps such a patch offline while its device is away and puts
 * it back when the device reappears (InputOutputMap::followInputDevices()).
 * This turns that into words: a notice ("Controller 'X' disconnected —
 * universe 2", amber; "... connected", plain) and a footer readiness entry
 * ("input.devices", ShowStatus) for as long as one is missing.
 *
 * A Bluetooth pad going to sleep and waking again, or reconnecting, can go
 * and come back within a second or two: nothing is said until the state has
 * held for settleTime(), and a device that came back by then is not
 * mentioned at all.
 */
class QLC_UI_EXPORT InputDeviceNotices : public QObject
{
    Q_OBJECT

public:
    explicit InputDeviceNotices(Doc *doc, QObject *parent = nullptr);

    /** One device input patch, as it stands. */
    struct Device
    {
        quint32 universe = 0;
        QString plugin;
        QString name;
        bool connected = false;
        /** A controller slot (InputOutputMap::setInputSlot()): which of how
         *  many devices this is (1-based; 0 when none is connected), and
         *  every device it may use. Empty / 0 for a single-device patch. */
        int slotPosition = 0;
        int slotCount = 0;
        QStringList slotDevices;
        /** Not connected, while another device of that exact name is: the
         *  universe waits for its own one (a pad already connected never
         *  takes it over by itself). @c spareHere: one of them is no
         *  universe's (it may take over, unplugged and plugged back in) and
         *  this is the first universe in order waiting for that name (the
         *  one a replugged spare goes to). */
        bool sameNameHere = false;
        bool spareHere = false;
    };
    /** What is said for @p d gone: @p disconnected (it was here), else
     *  not connected (the show opened without it); in a whole sentence. */
    static QString goneText(const Device &d, bool disconnected);

    /** Every input patched to a device (a plugin whose lines are hardware):
     *  the open ones, and the ones kept offline whose plugin is here. */
    QList<Device> devices() const;

    /** A show was opened or cleared: forget what was said. The next notice
     *  names what is missing, not what is there. */
    void reset();

    /** How long a change has to hold before it is said (ms). */
    int settleTime() const;
    void setSettleTime(int ms);

    /** Say what changed now instead of after the settle time. */
    void settleNow();

    /** "Controller", "MIDI device" or "Input device", for @p plugin. */
    static QString kindName(const QString &plugin);

    /** What the show's controls on @p universe do, in words (phase R):
     *  "Emergency Stop, Grand Master, GO of Main" (ControlOwners::jobsOn());
     *  empty when nothing listens there. */
    static QString jobsText(Doc *doc, quint32 universe);

    /** The Mac woke after sleeping @p sleptMs (ControllerKeepAlive::woke):
     *  "Slept 40 s, finding devices…" once, when a device input is patched.
     *  The devices that come back (or not) are said as usual after it. */
    void noteWake(qint64 sleptMs);

    /** What moves when @p universe's input is repatched to @p newDevice
     *  (empty: unpatched), in a sentence; empty when nothing would (no
     *  control listens there, or it is the same device). */
    static QString repatchText(Doc *doc, quint32 universe, const QString &newDevice);
    /** Ask before a repatch that moves controls (repatchText()): true to go
     *  on. Nothing asked (true) when nothing moves. */
    static bool confirmRepatch(QWidget *parent, Doc *doc, quint32 universe, const QString &newDevice);

signals:
    /** Show @p text; @p warning (a device went): amber, and it stays. */
    void notice(const QString &text, bool warning);
    /** A game controller is here that nothing has, and universe @p universe
     *  waits for one that is not: @p text says so, @p button offers to use
     *  it (InputOutputMap::addInputSlotDevice(), adopt()). Once per pad
     *  and universe while it stays. */
    void offer(const QString &text, const QString &button, quint32 universe,
               const QString &plugin, const QString &device);

    /** A controller's stick rests off the middle (a worn stick): @p text
     *  says so, @p button offers Centre Sticks (centreSticks()). Once per
     *  controller a session. */
    void stickOffer(const QString &text, const QString &button, QLCIOPlugin *plugin, quint32 input);

public:
    /** "Use '<pad>' for universe N (and remember it)": add it to the
     *  universe's controller slot and use it now. */
    bool adopt(quint32 universe, const QString &plugin, const QString &device);

    /** Hear @p plugin's stick drift and Centre Sticks results (by name:
     *  see App's plugin loading). */
    void watchSticks(QLCIOPlugin *plugin);
    /** Centre Sticks on @p plugin's @p input; says so (notice()). */
    bool centreSticks(QLCIOPlugin *plugin, quint32 input);

    /** "DUALSHOCK 4", for @p plugin's @p input: its name without the
     *  address in brackets. */
    static QString controllerName(QLCIOPlugin *plugin, quint32 input);

private slots:
    void slotStickDrift(quint32 input, int channel, double offset);
    void slotSticksCentred(quint32 input, int held);

private:
    void schedule();
    void updateStatus(const QList<Device> &now) const;
    /** @p text, then what stops working with universe @p universe's device. */
    QString withJobs(const QString &text, quint32 universe) const;

    Doc *m_doc;
    QTimer *m_settle;
    /** What was last said, by universe. */
    QMap<quint32, Device> m_said;
    /** Offers made ("universe\nplugin\ndevice") while they stand. */
    QSet<QString> m_offered;
    /** Controllers whose stick drift was said ("plugin\ndevice"). */
    QSet<QString> m_driftSaid;
    /** A wake's "finding devices…" line replaced the notice: the next
     *  settle says again what is still missing, so the standing amber line
     *  (and what stops working with it) is never lost to the wake line. */
    bool m_resayMissing = false;
};

#endif
