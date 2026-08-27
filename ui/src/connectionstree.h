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

#include <QString>
#include <QSet>

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

private slots:
    void slotContextMenu(const QPoint &pos);
    /** Commit an inline edit of a target's name. */
    void slotItemChanged(class QTreeWidgetItem *item, int column);

private:
    /** Universe ids patched to this plugin line. */
    QList<quint32> universesOn(const QString &pluginName, quint32 line, bool output) const;
    /** Universe ids whose FEEDBACK is routed to this plugin line. */
    QList<quint32> feedbackOn(const QString &pluginName, quint32 line) const;

    /** Patch an existing or new universe to this plugin line. */
    void patchUniverseTo(const QString &pluginName, quint32 line, bool output);
    /** Drop just this line's patch, leaving the universe itself alone. */
    void unpatchFromLine(quint32 universe, const QString &pluginName,
                         quint32 line, bool output);
    void renameUniverse(quint32 universe);
    void deleteUniverse(quint32 universe);

    /** Point this universe's feedback at a plugin line, or clear it. */
    void setFeedback(quint32 universe, const QString &pluginName, quint32 line);
    /** Apply an input profile to this universe, or clear it. */
    void setProfile(quint32 universe, const QString &profileName);
    /** Open a plugin's own configuration dialog. */
    void configurePlugin(const QString &pluginName);
    /** ArtNet transmit mode (Standard / Full / Partial) for one patch. */
    void setTransmitMode(quint32 universe, const QString &pluginName,
                         quint32 line, const QString &mode);
    /** Give an output target an operator-facing name. */
    void renameTarget(const QString &address);
    /** Patch a universe to a target typed in by hand. */
    void patchToNewTarget(const QString &pluginName, quint32 line);

    static QString itemPath(class QTreeWidgetItem *item);

    /** Patch a universe and aim it at one physical port of a discovered node. */
    void patchUniverseToPort(const QString &pluginName, quint32 line,
                             const QString &deviceAddress, quint32 portAddress);

    /** The port address an output patch is aimed at, or invalid if untargeted. */
    bool patchTarget(quint32 universe, const QString &pluginName, quint32 line,
                     QString &address, quint32 &portAddress) const;

private:
    Doc *m_doc;
    QCheckBox *m_showUnused;
    /* Collapse state survives the periodic rebuild; without this a refresh
       every 5 s reopened anything the operator had just closed. */
    QSet<QString> m_collapsed;
    bool m_populatedOnce;
    /** Suppresses itemChanged while refresh() rebuilds the tree. */
    bool m_rebuilding;
    QTreeWidget *m_tree;
    /* Nodes announce themselves about once a second, so a periodic rebuild
       keeps the view honest without a manual rescan. */
    QTimer *m_refreshTimer;
};

#endif
