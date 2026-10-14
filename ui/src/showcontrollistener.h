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

    static const int DefaultOscPort = 53100;

    /** Reads the settings and listens (or stops) to match. */
    void apply();

    /** "Listening for OSC on port 53100", "Off", or why it can't. */
    QString status() const { return m_status; }
    /** The last command that arrived, and what came of it; empty: none yet. */
    QString lastCommand() const { return m_last; }

signals:
    void statusChanged(const QString &status);
    void lastCommandChanged(const QString &what);

private slots:
    void slotReadyRead();
    void slotCommandHandled(const QString &what, bool done);

private:
    void setStatus(const QString &status);

private:
    Doc *m_doc;
    QUdpSocket *m_socket;
    QString m_status;
    QString m_last;
};

/** @} */

#endif
