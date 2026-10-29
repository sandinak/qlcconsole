/*
  Q Light Controller Plus
  webaccesscontroller.h

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

#ifndef WEBACCESSCONTROLLER_H
#define WEBACCESSCONTROLLER_H

#include <QObject>
#include <QHostAddress>
#include <QString>

class VirtualConsole;
class SimpleDesk;
class WebAccess;
class Doc;

/**
 * Runs web access for the show that is open: started when a show that
 * asks for it opens (Doc::webAccess(), Connections ▸ Show control & sync ▸
 * Web access), restarted when it asks for another port or audience, and
 * stopped by a show that doesn't ask for it.
 *
 * - The command line wins: started with -w / -wa (setCommandLine()), the
 *   server runs as told there for the whole session and the show's setting
 *   waits (state CommandLine).
 * - A show loaded through the web page itself leaves web access as it is,
 *   so whoever loaded it isn't cut off mid-show.
 * - A port that can't be had (in use, or an address not this computer's)
 *   is a status (Failed), never a crash; the show opens anyway.
 * - Nothing is built while it's off: no server, no socket.
 */
class WebAccessController final : public QObject
{
    Q_OBJECT

public:
    WebAccessController(Doc *doc, VirtualConsole *vc, SimpleDesk *sd, QObject *parent = nullptr);
    ~WebAccessController();

    /** Started from the command line (-w, -wp, -wa, -a, -wb): runs now, as
     *  told, whatever the show says. Port 0: the default (9999). */
    void setCommandLine(int port, bool auth, const QString &passwdFile,
                        const QHostAddress &bindAddress);

    /** The server, or nullptr while web access is off. */
    WebAccess *server() const { return m_server; }

    /** Bring the server in line with the show's setting now (normally done
     *  for you, a moment after a show loads or the setting changes). */
    void apply();

signals:
    /** A running server's requests, relayed for the App (as with -w). */
    void toggleDocMode();
    void loadProject(QString xmlData);
    void storeAutostartProject(QString filename);

private:
    void scheduleApply();
    void stop();
    void start(int port, bool auth, const QString &passwdFile, const QHostAddress &bind);

private:
    Doc *m_doc;
    VirtualConsole *m_vc;
    SimpleDesk *m_sd;
    WebAccess *m_server = nullptr;

    /** What the running server was started with. */
    int m_port = 0;
    bool m_auth = false;
    QString m_passwdFile;
    QHostAddress m_bind;

    bool m_commandLine = false;
    bool m_pending = false;
    /** The next apply follows a show loaded through the web page. */
    bool m_loadedFromWeb = false;
};

#endif
