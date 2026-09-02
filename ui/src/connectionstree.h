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
    /** Resolve which of a plugin's lines an action should apply to: the only
     *  one if there is only one, otherwise a picker. False if the plugin has
     *  no line in that direction at all, or the user cancelled the picker.
     *  Exists because the protocol-level "add a connection" / "patch a
     *  universe" actions have no line context of their own to work from --
     *  unlike everything else in this menu, which is reached by right-
     *  clicking a specific line row. */
    bool pickPluginLine(class QLCIOPlugin *p, bool output, quint32 &lineOut);
    /** "Add an interface…" -- resolve a line on @p p (asking direction and
     *  which line as needed) and pin it visible. Shared by the protocol
     *  row's own action and the host row's "Add a protocol" picker, so the
     *  two stay identical instead of drifting apart. */
    void revealInterface(class QLCIOPlugin *p);
    /** "Add a target…" -- reveal a line on @p p the same way
     *  revealInterface() does, then declare a target on it. Shared the same
     *  way revealInterface() is. */
    void addTargetOnNewInterface(class QLCIOPlugin *p);
    /** Drop just this line's patch, leaving the universe itself alone. */
    void unpatchFromLine(quint32 universe, const QString &pluginName,
                         quint32 line, bool output);
    /** Give up on a pending patch's interface ever coming back and clear it
     *  -- the one action that makes sense on a KIND_PENDING_UNIVERSE row
     *  besides renaming/deleting the universe itself (see
     *  OutputPatch::isPending()). */
    void forgetPendingPatch(quint32 universe);
    void renameUniverse(quint32 universe);
    void deleteUniverse(quint32 universe);
    /** Append a new universe (always the last one — see deleteUniverse()'s
     *  own "only the last can go" rule for why numbering never has gaps). */
    void addUniverse();
    /** Shared handling for the "Add/Delete Universe" and "Collapse/Expand
     *  everything below" actions that every per-kind menu in
     *  slotContextMenu() carries in addition to its own row-specific ones --
     *  offered on every row, not just empty space, since a fully-patched
     *  tree (the common case) has no empty space left to right-click.
     *  Returns true if @p chosen was one of these (and has already been
     *  fully acted on), so the caller knows to stop and return. */
    bool handleUniversalMenuAction(class QAction *chosen, QTreeWidgetItem *item,
                                   class QAction *collapseAll, class QAction *expandAll,
                                   class QAction *addUniv, class QAction *delUniv);
    /** Add the Collapse/Expand and Add/Delete Universe actions to an
     *  already-built per-kind menu, right before it is shown -- kept last so
     *  a menu reads top-down by hierarchy: what this exact row offers
     *  first, the actions that apply regardless of what was clicked last. */
    void appendUniversalMenuActions(class QMenu &menu, QTreeWidgetItem *item,
                                    class QAction *&collapseAll, class QAction *&expandAll,
                                    class QAction *&addUniv, class QAction *&delUniv);
    /** Move every universe currently output-patched to (pluginName, oldLine)
     *  onto a different line of the same plugin, in one step, preserving
     *  each patch's own settings (target IP, transmit mode, ...). */
    void rerouteLine(const QString &pluginName, quint32 oldLine,
                     const QList<quint32> &universes);

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

    /** Unicast a discovery probe at every address we believe in but have not
        heard from, so "silent" means no answer rather than no question. */
    void probeUnheardTargets();

    static QString itemPath(class QTreeWidgetItem *item);

    /** Ordered name/value pairs as a tooltip table, or empty if there are none. */
    static QString propertyTooltip(const QString &title,
                                   const QList<QPair<QString, QString> > &props);
    /** Put a tooltip on every column, so it shows wherever the pointer lands. */
    static void setRowTooltip(class QTreeWidgetItem *item, const QString &html);

    /** Fill in a universe row's Carries cell with what is actually patched
     *  into it -- fixture count, head count (only when it differs from the
     *  fixture count -- most rigs are single-head and it would just repeat
     *  the same number), and channel usage out of 512, red when over. A
     *  no-op if nothing is patched to this universe. Shared by every kind
     *  of universe row (patched, pending, unpatched) so "what's configured
     *  here" reads the same regardless of whether the network path behind
     *  it currently resolves -- fixture assignment does not depend on
     *  that. */
    void setCarriesFixtures(class QTreeWidgetItem *item, quint32 universeId) const;

    /** Patch a universe and aim it at one physical port of a discovered node. */
    void patchUniverseToPort(const QString &pluginName, quint32 line,
                             const QString &deviceAddress, quint32 portAddress);
    /** Patch an UNPATCHED universe by choosing everything from scratch --
     *  protocol, then interface, then (for a target-capable protocol) an
     *  address and port -- since there is no line/target/port row to have
     *  clicked to get here in the first place. */
    void patchUnpatchedUniverseTo(quint32 universe);

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
    /* Lines ("plugin|line") explicitly kept visible from the protocol row's
       "Add an interface..." action, session-persistently rather than just
       until the next rebuild -- unlike m_newLines. This is the closest this
       engine gets to "create an interface": a real network interface is a
       host NIC that already exists whether QLC+ has noticed it or not, so
       there is nothing to fabricate, only something to stop hiding. */
    QSet<QString> m_pinnedLines;
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
