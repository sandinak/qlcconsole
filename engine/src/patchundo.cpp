/*
  Q Light Controller Plus - qlcconsole
  patchundo.cpp

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

#include <QDebug>

#include "patchundo.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"

PatchUndo::PatchUndo(InputOutputMap *ioMap, QObject *parent)
    : QObject(parent)
    , m_ioMap(ioMap)
    , m_valid(false)
    , m_holdsUniverseList(false)
{
    Q_ASSERT(ioMap != NULL);
}

PatchUndo::~PatchUndo()
{
}

/****************************************************************************
 * Capture
 ****************************************************************************/

PatchUndo::State PatchUndo::captureOne(quint32 universe) const
{
    State st;
    st.id = universe;

    Universe *uni = m_ioMap->universe(universe);
    if (uni != NULL)
    {
        st.name = uni->name();
        st.passthrough = uni->passthrough();
    }

    InputPatch *ip = m_ioMap->inputPatch(universe);
    if (ip != NULL && ip->plugin() != NULL)
    {
        st.input.present = true;
        st.input.plugin = ip->pluginName();
        st.input.line = ip->input();
        st.input.profile = ip->profileName();
        st.input.parameters = ip->getPluginParameters();
    }

    for (int i = 0; i < m_ioMap->outputPatchesCount(universe); i++)
    {
        OutputPatch *op = m_ioMap->outputPatch(universe, i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        Patch p;
        p.present = true;
        p.plugin = op->pluginName();
        p.line = op->output();
        p.parameters = op->getPluginParameters();
        if (op->isPending())
        {
            p.pending = true;
            p.pendingPlugin = op->plugin();
            p.pendingUid = op->pendingUid();
            p.pendingLine = op->pendingLine();
            p.pendingNetwork = op->pendingNetwork();
            p.pendingByNumber = op->pendingByNumber();
            p.holdKind = int(op->holdKind());
            p.heldReason = op->heldReason();
            p.reconnectHeld = op->reconnectHeld();
        }
        st.outputs << p;
    }

    OutputPatch *fb = m_ioMap->feedbackPatch(universe);
    if (fb != NULL && fb->plugin() != NULL)
    {
        st.feedback.present = true;
        st.feedback.plugin = fb->pluginName();
        st.feedback.line = fb->output();
        st.feedback.parameters = fb->getPluginParameters();
    }

    return st;
}

void PatchUndo::capture(const QList<quint32> &universes, const QString &summary)
{
    m_held.clear();
    /* Duplicates would restore the same universe twice -- harmless but
       wasteful, and it makes the held list lie about how much was touched. */
    QList<quint32> seen;
    foreach (quint32 u, universes)
    {
        if (seen.contains(u))
            continue;
        seen << u;
        if (m_ioMap->universe(u) == NULL)
            continue;
        m_held << captureOne(u);
    }

    m_summary = summary;
    m_holdsUniverseList = false;
    m_valid = (m_held.isEmpty() == false);
    emit changed();
}

void PatchUndo::captureUniverses(const QString &summary)
{
    QList<quint32> all;
    foreach (Universe *uni, m_ioMap->universes())
    {
        if (uni != NULL)
            all << uni->id();
    }

    capture(all, summary);
    /* Set after capture(), which clears it: this is the flag that tells undo()
       to reconcile the LIST as well as the patches. */
    m_holdsUniverseList = true;
    /* An empty workspace is still a state worth restoring to -- undoing "add
       the first universe" has to be able to get back to none. */
    m_valid = true;
    emit changed();
}

/****************************************************************************
 * Restore
 ****************************************************************************/

void PatchUndo::restoreOne(const State &st)
{
    /* --- Output patches ---------------------------------------------------

       Order matters twice over, and each leg is restored in one step.

       First, trim surplus patches from the END. Removing index 0 first
       shuffles every later patch down, so each subsequent removal takes the
       wrong one.

       Then re-point and re-parameterise, in index order, so a universe
       fanned out to several nodes gets each leg back on its own node rather
       than in some other arrangement of the same set. Each leg goes back
       with ALL its parameters at once (review 21): on its line still, by
       OutputPatch::replacePluginParameters() -- one closing frame, to the
       target it has now, then the captured one -- or onto another line by
       setOutputPatch(..., parameters), opened aimed. Clearing everything
       first and setting each parameter after (as this did) put the line on
       the plugin's default target -- broadcast, Port-Address 0 -- between
       the two, and the first parameter set was a retarget whose closing
       frame went there. A parameter the undone operation ADDED is cleared
       by the same call (the captured map is the whole of what it had). */
    for (int i = m_ioMap->outputPatchesCount(st.id) - 1; i >= st.outputs.count(); i--)
        m_ioMap->setOutputPatch(st.id, QString(), QString(), 0, false, i);

    for (int i = 0; i < st.outputs.count(); i++)
    {
        const Patch &p = st.outputs.at(i);
        OutputPatch *op = m_ioMap->outputPatch(st.id, i);
        if (p.pending)
        {
            restorePending(st.id, i, p);
            continue;
        }
        const bool sameLine = op != NULL && op->isPatched() && op->pluginName() == p.plugin
                              && op->output() == p.line;
        if (sameLine == false
            && m_ioMap->setOutputPatch(st.id, p.plugin, QString(), p.line, false, i, p.parameters) == false)
        {
            // Refused (N1, F4) or not opened: whatever it is on is left as it is.
            op = m_ioMap->outputPatch(st.id, i);
            if (op == NULL || op->pluginName() != p.plugin || op->output() != p.line)
                continue;
        }

        op = m_ioMap->outputPatch(st.id, i);
        if (op != NULL && op->plugin() != NULL && op->pluginName() == p.plugin)
            op->replacePluginParameters(p.parameters);
    }

    /* --- Input patch ------------------------------------------------------ */
    if (st.input.present)
    {
        m_ioMap->setInputPatch(st.id, st.input.plugin, QString(),
                               st.input.line, st.input.profile);
        InputPatch *ip = m_ioMap->inputPatch(st.id);
        if (ip != NULL)
        {
            QMapIterator<QString, QVariant> it(st.input.parameters);
            while (it.hasNext())
            {
                it.next();
                ip->setPluginParameter(it.key(), it.value());
            }
        }
    }
    else
    {
        /* An empty plugin name is how this map clears a patch. */
        m_ioMap->setInputPatch(st.id, QString(), QString(), 0);
    }

    /* --- Feedback patch --------------------------------------------------- */
    if (st.feedback.present)
    {
        OutputPatch *fb = m_ioMap->feedbackPatch(st.id);
        // As an output leg: on its line still, only its parameters, in one step.
        if (fb == NULL || fb->isPatched() == false || fb->pluginName() != st.feedback.plugin
            || fb->output() != st.feedback.line)
            m_ioMap->setOutputPatch(st.id, st.feedback.plugin, QString(),
                                    st.feedback.line, true, 0);
        fb = m_ioMap->feedbackPatch(st.id);
        if (fb != NULL)
            fb->replacePluginParameters(st.feedback.parameters);
    }
    else
    {
        m_ioMap->setOutputPatch(st.id, QString(), QString(), 0, true, 0);
    }
}

/** A leg that was pending goes back pending, with its identity -- not through
 *  setOutputPatch() on an invalid line, which deletes it. */
void PatchUndo::restorePending(quint32 universe, int index, const Patch &p)
{
    Universe *uni = m_ioMap->universe(universe);
    if (uni == NULL || p.pendingPlugin == NULL)
        return;
    OutputPatch *op = m_ioMap->outputPatch(universe, index);
    const bool same = op != NULL && op->isPending() && op->plugin() == p.pendingPlugin
                      && op->pendingUid() == p.pendingUid && op->pendingLine() == p.pendingLine
                      && op->pendingNetwork() == p.pendingNetwork
                      && op->pendingByNumber() == p.pendingByNumber;
    if (same == false
        && uni->setOutputPatchPending(p.pendingPlugin, p.pendingUid, index, p.pendingLine,
                                      p.pendingNetwork, p.pendingByNumber) == false)
        return;
    op = m_ioMap->outputPatch(universe, index);
    if (op == NULL)
        return;
    op->markHold(OutputPatch::HoldKind(p.holdKind), p.reconnectHeld);
    op->setHeldReason(p.heldReason);
    op->replacePluginParameters(p.parameters);
}

/** Make the universe list match what was held.
 *
 *  Only ever at the end, in both directions, because that is the only place
 *  InputOutputMap will change it: removeUniverse() refuses anything but the
 *  last entry rather than leave a gap in the numbering. That restriction is
 *  what makes this safe -- no id is ever renumbered, so a restored universe
 *  reclaims exactly the id its fixtures still reference.
 */
void PatchUndo::restoreUniverseList()
{
    /* Too many now: drop from the end until the count matches. */
    while (quint32(m_ioMap->universesCount()) > quint32(m_held.count()))
    {
        const int last = m_ioMap->universesCount() - 1;
        if (m_ioMap->removeUniverse(last) == false)
        {
            qWarning() << Q_FUNC_INFO << "could not remove universe" << last
                       << "-- stopping rather than looping";
            break;
        }
    }

    /* Too few: add back, in the order they were held, so ids land where they
       were. addUniverse() takes the id explicitly for exactly this. */
    for (int i = m_ioMap->universesCount(); i < m_held.count(); i++)
    {
        if (m_ioMap->addUniverse(m_held.at(i).id) == false)
        {
            qWarning() << Q_FUNC_INFO << "could not restore universe"
                       << m_held.at(i).id;
            break;
        }
    }
}

bool PatchUndo::undo()
{
    if (m_valid == false)
        return false;

    if (m_holdsUniverseList)
        restoreUniverseList();

    foreach (const State &st, m_held)
    {
        Universe *uni = m_ioMap->universe(st.id);
        if (uni == NULL)
        {
            /* Either the universe was deleted after the capture and this step
               does not hold the list (a patch-only capture cannot honestly
               recreate it), or restoring the list failed above. Skipping is
               better than inventing it back with no record of what it was;
               the rest of the step still restores. */
            qWarning() << Q_FUNC_INFO << "universe" << st.id
                       << "no longer exists; skipping its patch restore";
            continue;
        }

        /* Name and passthrough only when this step owns the list. A patch-only
           capture must not quietly revert a rename somebody made in between --
           it never claimed to be holding that. */
        if (m_holdsUniverseList)
        {
            uni->setName(st.name);
            uni->setPassthrough(st.passthrough);
        }

        restoreOne(st);
    }

    clear();
    return true;
}

void PatchUndo::clear()
{
    m_held.clear();
    m_summary.clear();
    m_valid = false;
    m_holdsUniverseList = false;
    emit changed();
}

bool PatchUndo::canUndo() const
{
    return m_valid;
}

QString PatchUndo::summary() const
{
    return m_valid ? m_summary : QString();
}
