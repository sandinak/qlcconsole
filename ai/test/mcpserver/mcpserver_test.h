/*
  Q Light Controller Plus - qlcconsole
  mcpserver_test.h

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

#ifndef MCPSERVER_TEST_H
#define MCPSERVER_TEST_H

#include <QTemporaryDir>
#include <QJsonObject>
#include <QObject>

#include "showtreecommand.h"

class McpServer;
class McpBridge;
class AiAssist;
class Doc;

/**
 * AI Assist P3, the MCP server: an MCP client in this process (McpBridge,
 * what `qlcconsole --mcp` runs) over the real transport (a local socket;
 * no network at all).
 */
class McpServer_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void offByDefault();
    void listsTheToolsWithTheirSchemas();
    void readOnlyToolRuns();
    void changeIsProposedNotAppliedUntilConfirmed();
    void changesOffMeansNoChangeTools();
    void refusedInOperate();
    void localOnly();
    void anotherInstanceKeepsTheSocket();
    void settingsPage();

private:
    void enable(bool ai, bool mcp, bool changes = false);
    /** Send @a request through the bridge and wait for its answer. */
    QJsonObject call(const QString &method, const QJsonObject &params = QJsonObject());
    QString toolText(const QJsonObject &answer) const;
    ShowTreeCommand::Context smallShow();
    QByteArray treeXml() const;

    QTemporaryDir m_settingsDir;
    QString m_socket;
    int m_nextId = 1;
    Doc *m_doc = nullptr;
    AiAssist *m_ai = nullptr;
    McpServer *m_mcp = nullptr;
    McpBridge *m_bridge = nullptr;
};

#endif
