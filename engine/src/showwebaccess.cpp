/*
  Q Light Controller Plus
  showwebaccess.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "showwebaccess.h"
#include "doc.h"

#define KXMLWebAccessEnabled QStringLiteral("Enabled")
#define KXMLWebAccessPort    QStringLiteral("Port")
#define KXMLWebAccessNetwork QStringLiteral("Network")
#define KXMLWebAccessLogin   QStringLiteral("Login")

ShowWebAccess::ShowWebAccess(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

void ShowWebAccess::changed()
{
    if (m_doc != nullptr)
        m_doc->setModified();
    emit settingsChanged();
}

void ShowWebAccess::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    changed();
}

void ShowWebAccess::setPort(int port)
{
    if (port < 1 || port > 65535 || port == m_port)
        return;
    m_port = port;
    changed();
}

void ShowWebAccess::setNetwork(bool on)
{
    if (on == m_network)
        return;
    m_network = on;
    changed();
}

void ShowWebAccess::setLogin(bool on)
{
    if (on == m_login)
        return;
    m_login = on;
    changed();
}

void ShowWebAccess::reset()
{
    if (!m_enabled && m_port == DefaultPort && !m_network && !m_login)
        return;
    m_enabled = false;
    m_port = DefaultPort;
    m_network = false;
    m_login = false;
    emit settingsChanged();
}

void ShowWebAccess::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);
    // A new show's settings: nothing, so the file is what it was before.
    if (!m_enabled && m_port == DefaultPort && !m_network && !m_login)
        return;

    doc->writeStartElement(KXMLQLCWebAccess);
    doc->writeAttribute(KXMLWebAccessEnabled, m_enabled ? QStringLiteral("1") : QStringLiteral("0"));
    doc->writeAttribute(KXMLWebAccessPort, QString::number(m_port));
    if (m_network)
        doc->writeAttribute(KXMLWebAccessNetwork, QStringLiteral("1"));
    if (m_login)
        doc->writeAttribute(KXMLWebAccessLogin, QStringLiteral("1"));
    doc->writeEndElement();
}

void ShowWebAccess::loadXML(QXmlStreamReader &root)
{
    const QXmlStreamAttributes attrs = root.attributes();
    bool ok = false;
    const int port = attrs.value(KXMLWebAccessPort).toInt(&ok);

    m_enabled = attrs.value(KXMLWebAccessEnabled) == QLatin1String("1");
    m_port = (ok && port >= 1 && port <= 65535) ? port : DefaultPort;
    m_network = attrs.value(KXMLWebAccessNetwork) == QLatin1String("1");
    m_login = attrs.value(KXMLWebAccessLogin) == QLatin1String("1");
    root.skipCurrentElement();

    // Loading doesn't modify the show.
    emit settingsChanged();
}

void ShowWebAccess::setStatus(State state, const QString &url, const QString &message)
{
    if (state == m_state && url == m_url && message == m_message)
        return;
    m_state = state;
    m_url = url;
    m_message = message;
    emit statusChanged();
}
