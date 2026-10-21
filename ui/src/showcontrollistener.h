/*
  Q Light Controller Plus
  showcontrollistener.h

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

#ifndef SHOWCONTROLLISTENER_H
#define SHOWCONTROLLISTENER_H

#include <QObject>
#include <QString>

class QUdpSocket;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Show control's way in from the network and the settings: listens for OSC
 * on a UDP port of its own and hands each datagram to the show's
 * ShowControl; applies the MSC settings to it; keeps the last command that
 * arrived, so a QLab hookup can be checked (SHOW_CONTROL_DESIGN.md).
 * The engine doesn't link Qt Network, so this lives in the UI.
 */
class ShowControlListener final : public QObject
{
    Q_OBJECT

public:
    explicit ShowControlListener(Doc *doc, QObject *parent = nullptr);

    /** The settings (QSettings, "showcontrol/..."). */
    static bool oscEnabled();
    static int oscPort();
    static bool mscEnabled();
    static int mscDeviceId();
    static void setSettings(bool oscOn, int port, bool mscOn, int deviceId);
    /** The one input MSC is answered on, by plugin and input name as the
     *  plugin reports it ("MIDI", "IAC Driver Bus 1"); empty, all of them.
     *  Machine-wide, like the rest: the port belongs to the computer. */
    static QString mscInputPlugin();
    static QString mscInputName();

    static const int DefaultOscPort = 53100;

    /** Reads the settings and listens (or stops) to match. */
    void apply();

    /** One setting changed, saved and put into effect at once (Connections'
     *  Show control & sync page edits through these). */
    void setOscEnabled(bool on);
    void setOscPort(int port);
    void setMscEnabled(bool on);
    void setMscDeviceId(int id);
    /** Empty @a input: every MIDI input. */
    void setMscInput(const QString &plugin, const QString &input);

    enum OscState { OscOff, OscListening, OscError };
    OscState oscState() const { return m_oscState; }

    /** "Listening for OSC on port 53100", "Off", or why it can't. */
    QString status() const { return m_status; }
    /** The last command that arrived, and what came of it; empty: none yet. */
    QString lastCommand() const { return m_last; }
    /** The same, for what came by OSC only (with a datagram that wasn't a
     *  cue command, its address), and by MSC only. */
    QString lastOsc() const { return m_lastOsc; }
    QString lastMsc() const { return m_lastMsc; }

signals:
    void statusChanged(const QString &status);
    void lastCommandChanged(const QString &what);
    void lastOscChanged(const QString &what);
    void lastMscChanged(const QString &what);
    /** A setting changed through the setters above, or apply(). */
    void settingsChanged();

private slots:
    void slotReadyRead();
    void slotCommandHandled(const QString &what, bool done);
    void slotCommandFrom(int source, quint32 universe, const QString &what, bool done);

private:
    void bindOsc();
    void setStatus(OscState state, const QString &status);
    static QString stamp(const QString &what, bool done);

private:
    Doc *m_doc;
    QUdpSocket *m_socket;
    OscState m_oscState;
    QString m_status;
    QString m_last;
    QString m_lastOsc;
    QString m_lastMsc;
};

/** @} */

#endif
