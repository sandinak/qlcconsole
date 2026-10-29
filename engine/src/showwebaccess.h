/*
  Q Light Controller Plus
  showwebaccess.h

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

#ifndef SHOWWEBACCESS_H
#define SHOWWEBACCESS_H

#include <QObject>
#include <QString>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCWebAccess QStringLiteral("WebAccess")

/**
 * The show's web access: whether opening it starts the web server (the
 * Virtual Console, Simple Desk and Configuration pages in a browser), on
 * which port, for whom (this computer only, or the network) and whether
 * users log in. Saved with the show as <WebAccess>, only when it differs
 * from a new show's (off, port 9999, this computer, no logins), so a show
 * that never had it saves as before. QLC+ 4 skips the element.
 *
 * The server itself is in the webaccess library (WebAccessController),
 * which follows settingsChanged() and reports back through setStatus():
 * the engine neither links Qt Network nor knows the server.
 */
class QLC_ENGINE_EXPORT ShowWebAccess final : public QObject
{
    Q_OBJECT

public:
    explicit ShowWebAccess(Doc *doc);

    static const int DefaultPort = 9999;

    bool isEnabled() const { return m_enabled; }
    int port() const { return m_port; }
    /** True: listen on every network (tablets); false: this computer only. */
    bool network() const { return m_network; }
    /** True: users log in (accounts in web_passwd, as with -wa). */
    bool login() const { return m_login; }

    /** Each marks the show modified and emits settingsChanged() when it
     *  changes something. A port outside 1-65535 is ignored. */
    void setEnabled(bool on);
    void setPort(int port);
    void setNetwork(bool on);
    void setLogin(bool on);

    /** Back to a new show's settings (off). */
    void reset();

    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);

    /*********************************************************************
     * What the server is doing (runtime, never saved)
     *********************************************************************/
public:
    enum State
    {
        Off,            //!< not running
        Running,        //!< listening, at url()
        Failed,         //!< asked for, but could not listen (message())
        CommandLine     //!< started from the command line (-w): the show's setting waits
    };

    State state() const { return m_state; }
    /** Where it listens ("http://127.0.0.1:9999/"); empty when not running. */
    QString url() const { return m_url; }
    /** Why it isn't running, or a note about how it runs; may be empty. */
    QString message() const { return m_message; }

    /** Set by whoever runs the server; emits statusChanged() on a change. */
    void setStatus(State state, const QString &url, const QString &message = QString());

signals:
    void settingsChanged();
    void statusChanged();

private:
    void changed();

private:
    Doc *m_doc;
    bool m_enabled = false;
    int m_port = DefaultPort;
    bool m_network = false;
    bool m_login = false;

    State m_state = Off;
    QString m_url;
    QString m_message;
};

/** @} */

#endif
