/*
  Q Light Controller Plus - qlcconsole
  universepatcher.h

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

#ifndef UNIVERSEPATCHER_H
#define UNIVERSEPATCHER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>

class QLCIOPlugin;
class OutputPatch;
class QWidget;
class Doc;

/**
 * Patching a universe's OUTPUT -- which interface, which node, which port --
 * and taking it off again: the questions, the rules and the change itself.
 *
 * Grew up inside the Connections tab (ConnectionsTree), and lives here so
 * that every place a universe can be right-clicked asks the same questions
 * in the same order, refuses the same things, and makes one undo step of the
 * same shape. Two copies of a patch dialog drift; this is the one.
 *
 * Owns no view. Anything that changes a patch emits patchChanged() once,
 * after the change, so the caller rebuilds whatever it shows.
 */
class UniversePatcher : public QObject
{
    Q_OBJECT

public:
    /** @p dialogParent: the widget the questions are asked over. */
    UniversePatcher(Doc *doc, QWidget *dialogParent);

    /*********************************************************************
     * Words
     *********************************************************************/

    /** A port address the way the port rows and a node's own panel print it. */
    static QString portAddressText(quint32 pa);
    /** "port N · universe n:s:u", or "universe n:s:u" when the socket is unknown. */
    static QString portWhereText(int number, quint32 pa);
    /** net:subnet:universe back to a port address; false if it is not one. */
    static bool parsePortAddress(const QString &txt, quint32 &pa);

    /** The plural menu entries, worded once for every list that offers them. */
    static QString patchSelectionText(int count);
    static QString unpatchSelectionText(int patchedCount);
    /** The menu entry that runs retargetPatch() for a patch to @p pluginName. */
    static QString retargetText(const QString &pluginName);

    /*********************************************************************
     * Where a patch goes
     *********************************************************************/

    /** A device this console knows on one interface: heard from, declared by
     *  hand, or only named by a patch that already points at it. */
    struct TargetChoice
    {
        QString address;
        QString label;          ///< as the device row reads: "name (address)"
        QList<quint32> ports;   ///< its port addresses, each once, in port order
        QList<int> numbers;     ///< physical port number per entry, 1-based; 0 unknown
        bool heard = false;     ///< answered (discovered), not just configured
    };
    /** Every device listed under @p line of @p pluginName, in tree order. */
    QList<TargetChoice> visibleTargets(const QString &pluginName, quint32 line) const;

    /** One port of one visible device, as the target picker offers it. */
    struct TargetPortChoice
    {
        QString address;
        quint32 port = 0;       ///< port address
        quint32 owner = 0;      ///< who already sends there, or invalidUniverse()
        QString text;           ///< the picker's line for it
    };
    /** Every port of visibleTargets(), free ones first then the ones another
     *  universe than @p universe already sends to, each saying which. */
    QList<TargetPortChoice> targetPortChoices(const QString &pluginName, quint32 line,
                                              quint32 universe) const;

    /** The port address an output patch is aimed at, or false if untargeted. */
    bool patchTarget(quint32 universe, const QString &pluginName, quint32 line,
                     QString &address, quint32 &portAddress) const;

    /** Which universe already sends to a node port, or invalidUniverse().
     *
     *  The destination is NOT the plugin line. For Art-Net and sACN a line is a
     *  network INTERFACE, and one interface carries as many universes as you
     *  like -- what distinguishes them is the node address plus the port
     *  address. Two universes on one NIC is ordinary; two universes on the same
     *  node port is a genuine fault, and the one worth refusing. */
    quint32 universeOnTarget(const QString &pluginName, quint32 line,
                             const QString &address, quint32 portAddress,
                             quint32 exceptUniverse) const;

    /** Write outputIP / outputUni onto the patch of this universe on this
     *  line. Refuses (false) a port another universe already sends to. */
    bool applyTarget(quint32 universe, const QString &pluginName, quint32 line,
                     const QString &address, quint32 portAddress);

    /** Where an output leg goes, in words: "Art-Net → CS1 (172.18.2.211) ·
     *  port 1 · universe 0:0:0". @p cache: visibleTargets() per line, for a
     *  caller describing many legs at once. */
    QString outputLegText(OutputPatch *op,
                          QHash<QString, QList<TargetChoice> > *cache = nullptr) const;
    /** Every output leg of @p universe in those words, empty if unpatched. */
    QStringList outputText(quint32 universe) const;

    /*********************************************************************
     * Many universes at once
     *********************************************************************/

    /** One universe of a multi-universe patch, and where it lands. */
    struct OrderedPatchEntry
    {
        quint32 universe = 0;
        quint32 line = 0;       ///< the plugin line it goes out on
        bool targeted = false;  ///< false: broadcast, or no target concept
        quint32 port = 0;       ///< port address on the target, if targeted
        QString portName;       ///< how the port reads to the operator
        quint32 owner = 0;      ///< conflicts only: who already sends there
    };
    /** "Patch these N universes to that device", worked out before anything
     *  is changed, so it can be shown, confirmed, and tested without a
     *  dialog. Universe k goes to port k; what does not fit is left alone. */
    struct OrderedPatch
    {
        QString pluginName;
        QString address;                    ///< empty: no target / broadcast
        QList<OrderedPatchEntry> entries;   ///< will be patched, in order
        QList<quint32> overflow;            ///< more universes than ports
        QList<OrderedPatchEntry> conflicts; ///< port already another's
    };
    /** Pair @p universes (in this order) with @p ports (in this order) on
     *  @p address. @p linesAsPorts: a plugin with no targets, whose
     *  consecutive LINES are the ports, starting at @p line. An empty
     *  @p address without @p linesAsPorts patches every one to @p line with
     *  no target (broadcast). */
    OrderedPatch planOrderedPatch(const QList<quint32> &universes,
                                  const QString &pluginName, quint32 line,
                                  const QString &address,
                                  const QList<quint32> &ports,
                                  const QStringList &portNames,
                                  bool linesAsPorts) const;
    /** Carry out a plan as ONE undo step. Returns how many universes were
     *  patched. */
    int applyOrderedPatch(const OrderedPatch &plan);

    /** One patch to remove, as "Unpatch" on a row means it. */
    struct UnpatchLeg
    {
        quint32 universe = 0;
        QString pluginName;
        quint32 line = 0;
        bool output = true;
        bool pending = false;   ///< a pending patch: clear its pending legs
    };
    /** Every OUTPUT leg of these universes, pending ones included -- what
     *  "Unpatch output" on a universe takes away. Input and feedback stay. */
    QList<UnpatchLeg> outputLegs(const QList<quint32> &universes) const;
    /** Remove all of them as ONE undo step. */
    void unpatchLegs(const QList<UnpatchLeg> &legs);

    /*********************************************************************
     * The questions
     *********************************************************************/

    /** Which of a plugin's lines: the only one if there is only one,
     *  otherwise a picker opened on @p current. False if it has none in that
     *  direction, or the picker was cancelled. */
    bool pickPluginLine(QLCIOPlugin *p, bool output, quint32 &lineOut, int current = -1);

    /** "Send it where?" for one universe: a visible device's port, another
     *  address typed by hand, or (with @p allowBroadcast) broadcast. In:
     *  the current target to preselect (@p address empty: none). Out: the
     *  choice, @p address empty for broadcast. False when cancelled, or when
     *  the port picked carries another universe already (said, nothing done). */
    bool askTargetPort(const QString &title, const QString &pluginName, quint32 line,
                       quint32 universe, bool allowBroadcast,
                       QString &address, quint32 &port);

    /** Patch one universe's output from scratch: protocol, then interface,
     *  then (for a protocol with targets) node and port -- each opened on
     *  what it has now. Replaces its patch rather than adding a second. */
    void patchUniverse(quint32 universe);

    /** Repoint an existing output patch: its interface (asked again, the
     *  current one preselected), then its node / port on that interface. */
    void retargetPatch(quint32 universe, const QString &pluginName, quint32 line);

    /** "Patch N selected universes to…": protocol, interface, device, first
     *  port -- then the plan shown and confirmed, in the order given. */
    void patchSelectionTo(const QList<quint32> &universes);

    /** True if @p pluginName's outputs can be aimed at a node and port. */
    bool supportsTargets(const QString &pluginName) const;

signals:
    /** A patch changed (one signal per action, after the change). */
    void patchChanged();

private:
    Doc *m_doc;
    QWidget *m_parent;
};

#endif
