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
#include "inputoutputmap.h"
#include "showcontrol.h"
#include "doc.h"

#define SETTINGS_OSC_ON     QStringLiteral("showcontrol/oscEnabled")
#define SETTINGS_OSC_PORT   QStringLiteral("showcontrol/oscPort")
#define SETTINGS_MSC_ON     QStringLiteral("showcontrol/mscEnabled")
#define SETTINGS_MSC_DEVICE QStringLiteral("showcontrol/mscDeviceId")
#define SETTINGS_MSC_INPUT_PLUGIN QStringLiteral("showcontrol/mscInputPlugin")
#define SETTINGS_MSC_INPUT  QStringLiteral("showcontrol/mscInput")

ShowControlListener::ShowControlListener(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_socket(new QUdpSocket(this))
    , m_oscState(OscOff)
{
    connect(m_socket, &QUdpSocket::readyRead, this, &ShowControlListener::slotReadyRead);
    connect(m_doc->showControl(), &ShowControl::commandHandled,
            this, &ShowControlListener::slotCommandHandled);
    connect(m_doc->showControl(), &ShowControl::commandFrom, this,
            [this](ShowControl::Source source, quint32 universe, const QString &what, bool done) {
                slotCommandFrom(int(source), universe, what, done);
            });
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

QString ShowControlListener::mscInputPlugin()
{
    return QSettings().value(SETTINGS_MSC_INPUT_PLUGIN).toString();
}

QString ShowControlListener::mscInputName()
{
    return QSettings().value(SETTINGS_MSC_INPUT).toString();
}

void ShowControlListener::setSettings(bool oscOn, int port, bool mscOn, int deviceId)
{
    QSettings s;
    s.setValue(SETTINGS_OSC_ON, oscOn);
    s.setValue(SETTINGS_OSC_PORT, port);
    s.setValue(SETTINGS_MSC_ON, mscOn);
    s.setValue(SETTINGS_MSC_DEVICE, deviceId);
}

void ShowControlListener::setOscEnabled(bool on)
{
    QSettings().setValue(SETTINGS_OSC_ON, on);
    apply();
}

void ShowControlListener::setOscPort(int port)
{
    QSettings().setValue(SETTINGS_OSC_PORT, qBound(1, port, 65535));
    apply();
}

void ShowControlListener::setMscEnabled(bool on)
{
    QSettings().setValue(SETTINGS_MSC_ON, on);
    apply();
}

void ShowControlListener::setMscDeviceId(int id)
{
    QSettings().setValue(SETTINGS_MSC_DEVICE, qBound(0, id, 0x6F));
    apply();
}

void ShowControlListener::setMscInput(const QString &plugin, const QString &input)
{
    QSettings s;
    if (input.isEmpty())
    {
        s.remove(SETTINGS_MSC_INPUT_PLUGIN);
        s.remove(SETTINGS_MSC_INPUT);
    }
    else
    {
        s.setValue(SETTINGS_MSC_INPUT_PLUGIN, plugin);
        s.setValue(SETTINGS_MSC_INPUT, input);
    }
    apply();
}

void ShowControlListener::apply()
{
    ShowControl *sc = m_doc->showControl();
    sc->setMscEnabled(mscEnabled());
    sc->setMscDeviceId(mscDeviceId());
    sc->setMscInput(mscInputPlugin(), mscInputName());

    bindOsc();
    emit settingsChanged();
}

void ShowControlListener::bindOsc()
{
    const int port = oscPort();
    if (!oscEnabled())
    {
        m_socket->close();
        setStatus(OscOff, tr("OSC: off"));
        return;
    }
    if (m_socket->state() == QAbstractSocket::BoundState && m_socket->localPort() == port)
        return;
    m_socket->close();
    if (m_socket->bind(QHostAddress(QHostAddress::Any), quint16(port)))
        setStatus(OscListening, tr("Listening for OSC on port %1").arg(port));
    else
        setStatus(OscError, tr("Can't listen for OSC on port %1: %2")
                                .arg(port).arg(m_socket->errorString()));
}

void ShowControlListener::slotReadyRead()
{
    while (m_socket->hasPendingDatagrams())
    {
        const QNetworkDatagram d = m_socket->receiveDatagram();
        if (!d.isValid())
            continue;
        const QByteArray data = d.data();
        if (ShowControl::parseOsc(data).isEmpty())
        {
            // Not a cue command: say what it was, so a mistyped address in
            // QLab shows up here rather than as nothing at all.
            const int nul = data.indexOf('\0');
            const QString address = QString::fromUtf8(data.left(nul < 0 ? qMin(int(data.size()), 64) : nul));
            m_lastOsc = stamp(tr("%1 — not a cue command").arg(address.isEmpty() ? tr("(empty)") : address),
                              false);
            emit lastOscChanged(m_lastOsc);
            continue;
        }
        m_doc->showControl()->handleOsc(data);
    }
}

QString ShowControlListener::stamp(const QString &what, bool done)
{
    return QStringLiteral("%1  %2%3").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                         done ? QString() : tr("(not done) "), what);
}

void ShowControlListener::slotCommandHandled(const QString &what, bool done)
{
    m_last = stamp(what, done);
    emit lastCommandChanged(m_last);
}

void ShowControlListener::slotCommandFrom(int source, quint32 universe, const QString &what, bool done)
{
    if (source == ShowControl::Osc)
    {
        m_lastOsc = stamp(what, done);
        emit lastOscChanged(m_lastOsc);
    }
    else if (source == ShowControl::Msc)
    {
        QString where;
        InputOutputMap *iom = m_doc->inputOutputMap();
        for (quint32 i = 0; iom != nullptr && i < iom->universesCount(); i++)
        {
            if (iom->getUniverseID(i) != universe)
                continue;
            where = iom->getUniverseNameByIndex(i);
            if (where.isEmpty())
                where = tr("Universe %1").arg(i + 1);
            break;
        }
        m_lastMsc = stamp(where.isEmpty() ? what : tr("%1 (on %2)").arg(what, where), done);
        emit lastMscChanged(m_lastMsc);
    }
}

void ShowControlListener::setStatus(OscState state, const QString &status)
{
    m_oscState = state;
    if (status == m_status)
        return;
    m_status = status;
    emit statusChanged(m_status);
}
