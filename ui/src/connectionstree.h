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
#include <QVariant>
#include <QPair>
#include <QList>

#include <QWidget>

class QTreeWidgetItem;
class QTreeWidget;
class QTimer;
class QCheckBox;
class QShowEvent;
class QHideEvent;
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
    /** Flash the universe row that just received input. */
    void slotInputActivity(quint32 universe, quint32 channel, uchar value);
    void slotActivityTimeout();

protected:
    /* The rebuild timer is only worth running while the tab is on screen.
       Started unconditionally in the constructor it clears and repopulates
       the whole tree every five seconds for the entire session, including
       right through a show, with nobody looking at it. */
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    void slotContextMenu(const QPoint &pos);
    /** Poll every plugin for hardware now, rather than waiting for a tick. */
    void slotRescan();
    /** Put the last patch change back. */
    void slotUndoPatch();
    /** Enable/label the undo button from what PatchUndo is holding. */
    void slotUndoAvailabilityChanged();
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
    /** Set a plugin parameter on this universe's patch on this line. */
    void setPatchParameter(quint32 universe, const QString &pluginName,
                           quint32 line, bool output,
                           const QString &prop, const QVariant &value);
    /** Current value of a plugin parameter on that patch, or an invalid QVariant. */
    QVariant patchParameter(quint32 universe, const QString &pluginName,
                            quint32 line, bool output, const QString &prop) const;
    /** Repoint the FEEDBACK patch of this universe at a node / port. */
    void retargetFeedback(quint32 universe);

    /** Repoint an existing output patch at a different node / port. */
    void retargetPatch(quint32 universe, const QString &pluginName, quint32 line);
    /** Repoint every selected universe at one node, numbering the ports up. */
    void retargetSelection(const QString &pluginName);
    /** Universes selected in the tree, in row order, deduplicated. */
    QList<quint32> selectedUniverses(QString &pluginName, QList<quint32> &lines) const;
    /** Write outputIP / outputUni onto the patch of this universe on this line. */
    bool applyTarget(quint32 universe, const QString &pluginName, quint32 line,
                     const QString &address, quint32 portAddress);

    /** Give an interface an operator-facing name. */
    void renameLine(const QString &pluginName, const QString &lineName);
    /** Give an output target an operator-facing name. */
    void renameTarget(const QString &address);
    /** Patch a universe to a target typed in by hand. */
    void patchToNewTarget(const QString &pluginName, quint32 line);

    /** Declare a target this console cannot discover for itself. */
    void addManualTarget(const QString &pluginName, quint32 line);
    /** Add a port (net:sub:universe) to a hand-declared target. */
    void addManualPort(const QString &address);
    /** Drop a hand-declared target. Patches aimed at it are left alone. */
    void forgetTarget(const QString &address);

    static QString itemPath(class QTreeWidgetItem *item);

    /** Ordered name/value pairs as a tooltip table, or empty if there are none. */
    static QString propertyTooltip(const QString &title,
                                   const QList<QPair<QString, QString> > &props);
    /** Put a tooltip on every column, so it shows wherever the pointer lands. */
    static void setRowTooltip(class QTreeWidgetItem *item, const QString &html);

    /** Patch a universe and aim it at one physical port of a discovered node. */
    void patchUniverseToPort(const QString &pluginName, quint32 line,
                             const QString &deviceAddress, quint32 portAddress);

    /** The port address an output patch is aimed at, or invalid if untargeted. */
    bool patchTarget(quint32 universe, const QString &pluginName, quint32 line,
                     QString &address, quint32 &portAddress) const;

private:
    Doc *m_doc;
    QCheckBox *m_showUnused;
    class QPushButton *m_rescan;
    class QPushButton *m_undo;
    class QLabel *m_status;
    /* Which branches are open, by full path, so the periodic rebuild does not
       fight the operator. Tracked as EXPANDED rather than collapsed because
       the default is now "protocol and interface open, devices closed": with a
       default that is closed in places and open in others, remembering only
       one direction cannot express the other. */
    QSet<QString> m_expanded;
    /* Line identities ("plugin|line") seen on the previous rebuild, so a line
       that was not there before can be recognised as newly plugged in. */
    QSet<QString> m_knownLines;
    bool m_linesBaselined;
    /* New lines stay visible for as long as the tab is open, rather than for
       the single rebuild in which they first appeared. */
    QSet<QString> m_newLines;
    bool m_populatedOnce;
    /* First show is the moment to go looking; the constructor may run long
       before anyone opens the tab. */
    bool m_shownOnce;
    /** Suppresses itemChanged while refresh() rebuilds the tree. */
    bool m_rebuilding;
    /* Nodes announce roughly once a second, so an empty tree in the first
       moments means "not yet", not "nothing there". */
    class QElapsedTimer *m_since;
    QTreeWidget *m_tree;
    /* Nodes announce themselves about once a second, so a periodic rebuild
       keeps the view honest without a manual rescan. */
    QTimer *m_refreshTimer;
    /* Rows tinted by live input, and the timer that fades them. Keyed by
       universe id: the row a universe occupies is rebuilt every few seconds,
       so a pointer would dangle. */
    QSet<quint32> m_activeUniverses;
    QTimer *m_activityTimer;
};

#endif
