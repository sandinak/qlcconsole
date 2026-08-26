/*
  Q Light Controller Plus - qlcconsole
  connectionstree.h

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

#ifndef CONNECTIONSTREE_H
#define CONNECTIONSTREE_H

#include <QWidget>

class QTreeWidgetItem;
class QTreeWidget;
class QTimer;
class QCheckBox;
class Doc;

/**
 * Read-only "how is this console actually wired" view.
 *
 * The patch has always been presented universe-first: a flat list of plugin
 * lines named things like "ArtNet 1: 127.0.0.1" and "ArtNet 2: 192.168.1.245",
 * which does not tell you which is the show network, which is loopback, or
 * what is on the far end. This view inverts it and follows the signal outward
 * -- protocol, then interface, then the devices heard on it, then the
 * universes they carry -- so the question "where does universe 5 physically
 * come out" has a visible answer.
 *
 * Deliberately read-only. Editing stays in Overview (inline) and Detailed
 * (per-universe), which are the surfaces that already own it.
 */
class ConnectionsTree : public QWidget
{
    Q_OBJECT

public:
    ConnectionsTree(Doc *doc, QWidget *parent = 0);
    ~ConnectionsTree();

public slots:
    /** Rebuild from the current plugin/patch state. */
    void refresh();

private:
    /** Universes patched to this plugin line, as display strings. */
    QStringList universesOn(const QString &pluginName, quint32 line, bool output) const;

private:
    Doc *m_doc;
    QCheckBox *m_showUnused;
    QTreeWidget *m_tree;
    /* Nodes announce themselves about once a second, so a periodic rebuild
       keeps the view honest without a manual rescan. */
    QTimer *m_refreshTimer;
};

#endif
