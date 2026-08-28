/*
  Q Light Controller Plus - qlcconsole
  patchundo.h

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

#ifndef PATCHUNDO_H
#define PATCHUNDO_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QList>
#include <QMap>

class InputOutputMap;

/**
 * One step of undo for patching.
 *
 * Patch operations were immediate and permanent: the only way back from a
 * mis-aimed retarget was to remember what it used to be. That is tolerable
 * for one universe and not at all tolerable for a bulk change across sixteen,
 * which is the feature this exists to make safe.
 *
 * Snapshot-and-restore rather than a command hierarchy. A universe's whole
 * patch state is small and completely enumerable -- an input patch, an ordered
 * list of output patches, a feedback patch, each a plugin, a line and a bag of
 * parameters -- so capturing it costs nothing and restoring it needs no
 * hand-written inverse for every operation. Writing one inverse per action
 * (retarget, unpatch, set-parameter, change-profile, and whatever comes next)
 * is where undo implementations usually rot: the inverses drift from the
 * actions and nobody notices until one of them is wrong.
 *
 * Deliberately ONE step deep. Every additional step is a state that might be
 * restored onto a rig which has since changed underneath it, and a stale
 * restore is worse than no restore -- it looks like it worked.
 *
 * Deliberately does NOT cover adding or removing universes. Undoing a deleted
 * universe means recreating it with its id intact so fixture references still
 * resolve; undoing an added one means removing it without renumbering the
 * rest. Both are real work and neither is what bulk patching needs, so this
 * says so plainly instead of half-supporting them.
 *
 * Restoring is NOT side-effect free: setting a patch closes and reopens plugin
 * lines, so an undo interrupts output on the universes it touches. That is why
 * the action driving it is named for what it does rather than offered as a
 * reflexive Ctrl+Z.
 */
class PatchUndo : public QObject
{
    Q_OBJECT

public:
    PatchUndo(InputOutputMap *ioMap, QObject *parent = 0);
    ~PatchUndo();

    /** One patch: which plugin line, and everything hanging off it. */
    struct Patch
    {
        Patch() : line(0), present(false) {}
        QString plugin;
        quint32 line;
        QString profile;                    //!< input patches only
        QMap<QString, QVariant> parameters;
        /** False means "there was no patch here", which restores as a clear
            rather than as a patch to an empty plugin. */
        bool present;
    };

    /** Everything about one universe that patching can change. */
    struct State
    {
        State() : id(0) {}
        quint32 id;
        Patch input;
        QList<Patch> outputs;
        Patch feedback;
    };

    /**
     * Remember the current state of these universes.
     *
     * Call BEFORE the change. Replaces whatever was held: one step deep.
     * `summary` is shown to the operator, so it should name the thing being
     * undone ("retarget 16 universes"), not the mechanism.
     */
    void capture(const QList<quint32> &universes, const QString &summary);

    bool canUndo() const;
    /** What capture() was told it was about to do. Empty if !canUndo(). */
    QString summary() const;

    /** Put it back. Returns false if there was nothing held. */
    bool undo();

    /** Forget the held state -- e.g. after loading a different workspace,
        where restoring it would write one show's patch onto another's. */
    void clear();

signals:
    /** canUndo()/summary() changed; menus should re-read them. */
    void changed();

private:
    State captureOne(quint32 universe) const;
    void restoreOne(const State &state);

private:
    InputOutputMap *m_ioMap;
    QList<State> m_held;
    QString m_summary;
    bool m_valid;
};

#endif
