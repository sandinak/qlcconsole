/*
  Q Light Controller Plus
  showoperate.cpp

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

#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <QStringList>

#include "showoperate.h"
#include "doc.h"

#define KXMLOperateOpenIn  QStringLiteral("OpenIn")
#define KXMLOperatePinHash QStringLiteral("PinHash")
#define KXMLOperateLockDesign QStringLiteral("LockDesign")

/*********************************************************************
 * ShowPin
 *********************************************************************/

namespace
{
const char *kPinScheme = "pbkdf2-sha256";
/* A 4-8 digit PIN has at most 10^8 values: the iterations slow a guess
   from the file a little, they can't make a short PIN strong. Kept low
   enough that checking one is instant. */
const int kPinIterations = 20000;

/* PBKDF2-HMAC-SHA256, one 32-byte block (RFC 8018). */
QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, password);
    mac.addData(salt);
    mac.addData(QByteArray::fromHex("00000001"));
    QByteArray u = mac.result();
    QByteArray t = u;
    for (int i = 1; i < iterations; ++i)
    {
        mac.reset();
        mac.addData(u);
        u = mac.result();
        for (int b = 0; b < t.size(); ++b)
            t[b] = char(t.at(b) ^ u.at(b));
    }
    return t;
}
}

bool ShowPin::isValid(const QString &pin)
{
    static const QRegularExpression re(QStringLiteral("^[0-9]{4,8}$"));
    return re.match(pin).hasMatch();
}

QString ShowPin::hash(const QString &pin)
{
    QByteArray salt(16, '\0');
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = char(QRandomGenerator::system()->bounded(256));
    const QByteArray h = pbkdf2(pin.toUtf8(), salt, kPinIterations);
    return QStringLiteral("%1$%2$%3$%4").arg(QLatin1String(kPinScheme))
                                         .arg(kPinIterations)
                                         .arg(QString::fromLatin1(salt.toBase64()),
                                              QString::fromLatin1(h.toBase64()));
}

bool ShowPin::verify(const QString &pin, const QString &stored)
{
    const QStringList parts = stored.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts.at(0) != QLatin1String(kPinScheme))
        return false;
    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations < 1 || iterations > 10000000)
        return false;
    const QByteArray salt = QByteArray::fromBase64(parts.at(2).toLatin1());
    const QByteArray want = QByteArray::fromBase64(parts.at(3).toLatin1());
    if (salt.isEmpty() || want.size() != 32)
        return false;
    const QByteArray got = pbkdf2(pin.toUtf8(), salt, iterations);
    // Compared in full, whatever differs first.
    char diff = 0;
    for (int i = 0; i < got.size(); ++i)
        diff |= char(got.at(i) ^ want.at(i));
    return diff == 0;
}

/*********************************************************************
 * ShowOperate
 *********************************************************************/

ShowOperate::ShowOperate(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

void ShowOperate::changed()
{
    if (m_doc != nullptr)
        m_doc->setModified();
    emit settingsChanged();
}

void ShowOperate::setOpenInOperate(bool on)
{
    if (on == m_openInOperate)
        return;
    m_openInOperate = on;
    changed();
}

bool ShowOperate::setPin(const QString &pin)
{
    if (pin.isEmpty())
    {
        if (m_pinHash.isEmpty())
            return true;
        m_pinHash.clear();
        changed();
        return true;
    }
    if (!ShowPin::isValid(pin))
        return false;
    m_pinHash = ShowPin::hash(pin);
    changed();
    return true;
}

void ShowOperate::setPinHash(const QString &hash)
{
    if (hash == m_pinHash)
        return;
    m_pinHash = hash;
    changed();
}

void ShowOperate::setLockDesign(bool on)
{
    if (on == m_lockDesign)
        return;
    m_lockDesign = on;
    changed();
}

bool ShowOperate::checkPin(const QString &pin) const
{
    if (m_pinHash.isEmpty())
        return true;
    return ShowPin::verify(pin, m_pinHash);
}

void ShowOperate::reset()
{
    if (!m_openInOperate && m_pinHash.isEmpty() && m_lockDesign)
        return;
    m_openInOperate = false;
    m_lockDesign = true;
    m_pinHash.clear();
    emit settingsChanged();
}

void ShowOperate::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);
    const bool openIn = m_saveOverride != nullptr ? m_saveOverride->openInOperate : m_openInOperate;
    const QString pinHash = m_saveOverride != nullptr ? m_saveOverride->pinHash : m_pinHash;
    const bool lockDesign = m_saveOverride != nullptr ? m_saveOverride->lockDesign : m_lockDesign;
    // Nothing set: nothing written, so the file is what it was before.
    if (!openIn && pinHash.isEmpty())
        return;

    doc->writeStartElement(KXMLQLCOperate);
    if (openIn)
        doc->writeAttribute(KXMLOperateOpenIn, QStringLiteral("1"));
    if (!pinHash.isEmpty())
        doc->writeAttribute(KXMLOperatePinHash, pinHash);
    if (!lockDesign && !pinHash.isEmpty())
        doc->writeAttribute(KXMLOperateLockDesign, QStringLiteral("0"));
    doc->writeEndElement();
}

void ShowOperate::loadXML(QXmlStreamReader &root)
{
    const QXmlStreamAttributes attrs = root.attributes();
    m_openInOperate = attrs.value(KXMLOperateOpenIn) == QLatin1String("1");
    m_pinHash = attrs.value(KXMLOperatePinHash).toString();
    m_lockDesign = attrs.value(KXMLOperateLockDesign) != QLatin1String("0");
    root.skipCurrentElement();

    // Loading doesn't modify the show.
    emit settingsChanged();
}
