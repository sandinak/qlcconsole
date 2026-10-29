/*
  Q Light Controller Plus
  webaccesscontroller.cpp

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

#include <QTimer>
#include <QDebug>

#include "webaccesscontroller.h"
#include "showwebaccess.h"
#include "webaccess.h"
#include "doc.h"

WebAccessController::WebAccessController(Doc *doc, VirtualConsole *vc, SimpleDesk *sd, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_vc(vc)
    , m_sd(sd)
{
    Q_ASSERT(m_doc != nullptr);
    connect(m_doc->webAccess(), &ShowWebAccess::settingsChanged, this, &WebAccessController::scheduleApply);
    connect(m_doc, &Doc::loaded, this, &WebAccessController::scheduleApply);
    connect(m_doc, &Doc::cleared, this, &WebAccessController::scheduleApply);
    // The show that is open already (app.startup() loads it before us).
    scheduleApply();
}

WebAccessController::~WebAccessController()
{
    delete m_server;
    m_server = nullptr;
}

void WebAccessController::setCommandLine(int port, bool auth, const QString &passwdFile,
                                         const QHostAddress &bindAddress)
{
    m_commandLine = true;
    stop();
    start(port, auth, passwdFile, bindAddress);
}

void WebAccessController::scheduleApply()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() {
        m_pending = false;
        apply();
    });
}

void WebAccessController::apply()
{
    ShowWebAccess *show = m_doc->webAccess();

    if (m_commandLine)
    {
        if (m_server != nullptr)
            show->setStatus(ShowWebAccess::CommandLine, m_server->url(),
                            tr("Started from the command line (-w); this show's setting is used "
                               "when qlcconsole starts without it."));
        return;
    }

    // Mid-load: the show's own setting comes with Doc::loaded().
    if (m_doc->loadStatus() == Doc::Loading)
        return;

    if (m_loadedFromWeb)
    {
        /* A show loaded through the web page: whoever loaded it keeps the
           page they are on, whatever the new show says. */
        m_loadedFromWeb = false;
        if (m_server != nullptr)
        {
            show->setStatus(ShowWebAccess::Running, m_server->url(),
                            tr("Kept running: this show was loaded through the web page."));
            return;
        }
    }

    if (!show->isEnabled())
    {
        stop();
        show->setStatus(ShowWebAccess::Off, QString());
        return;
    }

    const QHostAddress bind = show->network() ? QHostAddress(QHostAddress::AnyIPv4)
                                              : QHostAddress(QHostAddress::LocalHost);
    if (m_server != nullptr && m_port == show->port() && m_auth == show->login()
        && m_bind == bind && m_passwdFile.isEmpty())
    {
        show->setStatus(ShowWebAccess::Running, m_server->url());
        return;
    }

    stop();
    start(show->port(), show->login(), QString(), bind);
}

void WebAccessController::stop()
{
    if (m_server == nullptr)
        return;
    // Never from inside the server's own slot: apply() runs from a timer.
    delete m_server;
    m_server = nullptr;
    m_port = 0;
}

void WebAccessController::start(int port, bool auth, const QString &passwdFile, const QHostAddress &bind)
{
    ShowWebAccess *show = m_doc->webAccess();
    WebAccess *web = new WebAccess(m_doc, m_vc, m_sd, port, auth, passwdFile, bind);
    if (web->listenPort() == 0)
    {
        delete web;
        const int p = port ? port : ShowWebAccess::DefaultPort;
        show->setStatus(ShowWebAccess::Failed, QString(),
                        bind.isNull() || bind.isLoopback() || bind == QHostAddress(QHostAddress::AnyIPv4)
                        ? tr("Port %1 is in use by another program (another qlcconsole or QLC+?). "
                             "Pick another port.").arg(p)
                        : tr("Could not listen on %1 port %2: the address isn't this computer's, "
                             "or the port is in use.").arg(bind.toString()).arg(p));
        return;
    }

    m_server = web;
    m_port = port;
    m_auth = auth;
    m_passwdFile = passwdFile;
    m_bind = bind;

    connect(web, &WebAccess::toggleDocMode, this, &WebAccessController::toggleDocMode);
    connect(web, &WebAccess::loadProject, this, [this](const QString &xml) {
        m_loadedFromWeb = true;
        emit loadProject(xml);
    });
    connect(web, &WebAccess::storeAutostartProject, this, &WebAccessController::storeAutostartProject);

    show->setStatus(m_commandLine ? ShowWebAccess::CommandLine : ShowWebAccess::Running, web->url(),
                    m_commandLine ? tr("Started from the command line (-w); this show's setting is "
                                       "used when qlcconsole starts without it.")
                                  : QString());
}
