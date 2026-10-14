/*
  Q Light Controller Plus
  showcontrollistener.cpp

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

#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QSettings>
#include <QTime>

#include "showcontrollistener.h"
#include "showcontrol.h"
#include "doc.h"

#define SETTINGS_OSC_ON     QStringLiteral("showcontrol/oscEnabled")
#define SETTINGS_OSC_PORT   QStringLiteral("showcontrol/oscPort")
#define SETTINGS_MSC_ON     QStringLiteral("showcontrol/mscEnabled")
#define SETTINGS_MSC_DEVICE QStringLiteral("showcontrol/mscDeviceId")

ShowControlListener::ShowControlListener(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &ShowControlListener::slotReadyRead);
    connect(m_doc->showControl(), &ShowControl::commandHandled,
            this, &ShowControlListener::slotCommandHandled);
    apply();
}

bool ShowControlListener::oscEnabled()
{
    // Off until asked for: anyone on the network could fire cues.
    return QSettings().value(SETTINGS_OSC_ON, false).toBool();
}

int ShowControlListener::oscPort()
{
    return qBound(1, QSettings().value(SETTINGS_OSC_PORT, DefaultOscPort).toInt(), 65535);
}

bool ShowControlListener::mscEnabled()
{
    // On: MSC only arrives on a MIDI input someone patched.
    return QSettings().value(SETTINGS_MSC_ON, true).toBool();
}

int ShowControlListener::mscDeviceId()
{
    return qBound(0, QSettings().value(SETTINGS_MSC_DEVICE, 1).toInt(), 0x6F);
}

void ShowControlListener::setSettings(bool oscOn, int port, bool mscOn, int deviceId)
{
    QSettings s;
    s.setValue(SETTINGS_OSC_ON, oscOn);
    s.setValue(SETTINGS_OSC_PORT, port);
    s.setValue(SETTINGS_MSC_ON, mscOn);
    s.setValue(SETTINGS_MSC_DEVICE, deviceId);
}

void ShowControlListener::apply()
{
    ShowControl *sc = m_doc->showControl();
    sc->setMscEnabled(mscEnabled());
    sc->setMscDeviceId(mscDeviceId());

    const int port = oscPort();
    if (!oscEnabled())
    {
        m_socket->close();
        setStatus(tr("OSC: off"));
        return;
    }
    if (m_socket->state() == QAbstractSocket::BoundState && m_socket->localPort() == port)
        return;
    m_socket->close();
    if (m_socket->bind(QHostAddress(QHostAddress::Any), quint16(port)))
        setStatus(tr("Listening for OSC on port %1").arg(port));
    else
        setStatus(tr("Can't listen for OSC on port %1: %2").arg(port).arg(m_socket->errorString()));
}

void ShowControlListener::slotReadyRead()
{
    while (m_socket->hasPendingDatagrams())
    {
        const QNetworkDatagram d = m_socket->receiveDatagram();
        if (d.isValid())
            m_doc->showControl()->handleOsc(d.data());
    }
}

void ShowControlListener::slotCommandHandled(const QString &what, bool done)
{
    m_last = QStringLiteral("%1  %2%3").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                           done ? QString() : tr("(not done) "), what);
    emit lastCommandChanged(m_last);
}

void ShowControlListener::setStatus(const QString &status)
{
    if (status == m_status)
        return;
    m_status = status;
    emit statusChanged(m_status);
}
