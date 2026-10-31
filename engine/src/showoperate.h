/*
  Q Light Controller Plus
  showoperate.h

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

#ifndef SHOWOPERATE_H
#define SHOWOPERATE_H

#include <QObject>
#include <QString>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCOperate QStringLiteral("Operate")

/**
 * A PIN, kept as a salted PBKDF2-HMAC-SHA256 hash:
 * "pbkdf2-sha256$<iterations>$<salt base64>$<hash base64>". QtCore only, so
 * the engine can check it (and Teaching mode can share it). A PIN is 4 to 8
 * digits; it keeps curious hands off, it is not a lock against someone who
 * edits the show file.
 */
namespace ShowPin
{
    /** 4 to 8 digits. */
    QLC_ENGINE_EXPORT bool isValid(const QString &pin);
    /** The stored form of @p pin, with a fresh salt. */
    QLC_ENGINE_EXPORT QString hash(const QString &pin);
    /** Does @p pin match @p stored (a hash())? False for a malformed one. */
    QLC_ENGINE_EXPORT bool verify(const QString &pin, const QString &stored);
}

/**
 * The show's Operate settings (File ▸ Show Settings): whether opening it
 * goes straight to Operate, and a PIN that going back to Design asks for.
 * Saved with the show as <Operate OpenIn="1" PinHash="..." LockDesign="0"
 * RemoteGo="1"/> (LockDesign only when off, RemoteGo only when on) under the
 * Engine, only when either is set, so a show without them saves as before.
 * QLC+ 4 skips the element.
 */
class QLC_ENGINE_EXPORT ShowOperate final : public QObject
{
    Q_OBJECT

public:
    explicit ShowOperate(Doc *doc);

    /** Opening the show goes to Operate (as -p does). */
    bool openInOperate() const { return m_openInOperate; }
    void setOpenInOperate(bool on);

    /** A PIN is set: leaving Operate asks for it. */
    bool hasPin() const { return !m_pinHash.isEmpty(); }
    /** The stored hash (ShowPin::hash()); empty for none. */
    QString pinHash() const { return m_pinHash; }
    /** Set (a valid PIN) or, with an empty one, remove the PIN. False, and
     *  nothing changed, for an invalid PIN. */
    bool setPin(const QString &pin);
    /** Set an already-hashed PIN (ShowPin::hash()), as Show Settings does
     *  on OK; empty removes it. */
    void setPinHash(const QString &hash);
    /** Going back to Design asks for the PIN: a PIN is set and "lock Design"
     *  is on (the default; a Teaching handout may use the PIN for Teaching
     *  only, so a student can still leave Operate). */
    bool locksDesign() const { return hasPin() && m_lockDesign; }
    bool lockDesignSetting() const { return m_lockDesign; }
    void setLockDesign(bool on);
    /** Does @p pin open the lock? True when there is no PIN. */
    bool checkPin(const QString &pin) const;

    /** The phone Run page may send GO (RouteWeb): off by default, per show
     *  (docs/design/RUN_COMMANDS.md). With a PIN set the page asks for it
     *  once per session. */
    bool remoteGo() const { return m_remoteGo; }
    void setRemoteGo(bool on);

    /** What a copy of the show is written with instead of the open show's
     *  own Operate settings (Make Student Handout). */
    struct SaveOverride
    {
        bool openInOperate = false;
        QString pinHash;
        bool lockDesign = false;
    };
    void setSaveOverride(const SaveOverride *saveOverride) { m_saveOverride = saveOverride; }

    /** Back to a new show's (nothing set). */
    void reset();

    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);

signals:
    void settingsChanged();

private:
    void changed();

private:
    Doc *m_doc;
    bool m_openInOperate = false;
    bool m_lockDesign = true;
    bool m_remoteGo = false;
    QString m_pinHash;
    const SaveOverride *m_saveOverride = nullptr;
};

/** @} */

#endif
