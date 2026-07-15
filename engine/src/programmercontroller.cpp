/*
  Q Light Controller Plus
  programmercontroller.cpp

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#include <QtAlgorithms>
#include <QDebug>
#include <algorithm>
#include <climits>

#include <QMutexLocker>

#include "programmercontroller.h"

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "function.h"
#include "scene.h"
#include "scenevalue.h"
#include "chaser.h"
#include "chaseraction.h"
#include "collection.h"
#include "qlcpalette.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturemode.h"
#include "mastertimer.h"
#include "programmerflasher.h"
#include "highlighteffect.h"
#include "capturemanager.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "qlcioplugin.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "truss.h"
#include <QtMath>
#include <QVector3D>

ProgrammerController::ProgrammerController(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    qDebug() << "[PC] ProgrammerController constructed";
    m_programmerFlasher = new ProgrammerFlasher(m_doc);
    m_highlightEffect = new HighlightEffect(m_doc);

    connect(this, &ProgrammerController::programmerSelectionChanged,
            this, &ProgrammerController::slotSyncEffectFixtures);
    connect(m_doc, &Doc::modeChanged,
            this, &ProgrammerController::slotDocModeChanged);
    if (m_doc->monitorProperties())
        connect(m_doc->monitorProperties(), &MonitorProperties::rigPropsChanged,
                this, &ProgrammerController::slotRigPropsChanged);

    // Drive joystick aim at a steady 50 Hz so a HELD stick keeps moving (velocity
    // ∝ deflection) — input events only fire while the value is changing, so
    // integrating in the event handler made it move only while the stick moved.
    m_designJoyTimer = new QTimer(this);
    m_designJoyTimer->setInterval(20);   // 50 Hz, matches the dt used in the math
    connect(m_designJoyTimer, &QTimer::timeout,
            this, &ProgrammerController::applyDesignJoystick);
    m_designJoyTimer->start();
}

/*****************************************************************************
 * Programmer selection
 *****************************************************************************/

QList<quint32> ProgrammerController::programmerSelection() const
{
    // Read from the DMX thread (VCSlider::writeDMXParameter).
    QMutexLocker locker(&m_stateMutex);
    return m_programmerSelection;
}

void ProgrammerController::setProgrammerSelection(const QList<quint32>& fixtureIds)
{
    QList<quint32> deduped;
    QSet<quint32> seen;
    for (quint32 fid : fixtureIds)
    {
        if (seen.contains(fid))
            continue;
        deduped.append(fid);
        seen.insert(fid);
    }

    {
        QMutexLocker locker(&m_stateMutex);
        if (deduped == m_programmerSelection)
            return;
        m_programmerSelection = deduped;
        m_programmerSelectionLookup = seen;
    }
    emit programmerSelectionChanged();
}

void ProgrammerController::addToProgrammerSelection(const QList<quint32>& fixtureIds)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        for (quint32 fid : fixtureIds)
        {
            if (m_programmerSelectionLookup.contains(fid))
                continue;
            m_programmerSelection.append(fid);
            m_programmerSelectionLookup.insert(fid);
            changed = true;
        }
    }
    if (changed)
        emit programmerSelectionChanged();
}

void ProgrammerController::removeFromProgrammerSelection(const QList<quint32>& fixtureIds)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        for (quint32 fid : fixtureIds)
        {
            if (!m_programmerSelectionLookup.contains(fid))
                continue;
            m_programmerSelection.removeAll(fid);
            m_programmerSelectionLookup.remove(fid);
            changed = true;
        }
    }
    if (changed)
        emit programmerSelectionChanged();
}

void ProgrammerController::toggleInProgrammerSelection(const QList<quint32>& fixtureIds)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        for (quint32 fid : fixtureIds)
        {
            if (m_programmerSelectionLookup.contains(fid))
            {
                m_programmerSelection.removeAll(fid);
                m_programmerSelectionLookup.remove(fid);
            }
            else
            {
                m_programmerSelection.append(fid);
                m_programmerSelectionLookup.insert(fid);
            }
            changed = true;
        }
    }
    if (changed)
        emit programmerSelectionChanged();
}

void ProgrammerController::clearProgrammerSelection()
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_programmerSelection.isEmpty())
            return;
        m_programmerSelection.clear();
        m_programmerSelectionLookup.clear();
    }
    emit programmerSelectionChanged();
}

bool ProgrammerController::isInProgrammerSelection(quint32 fixtureId) const
{
    QMutexLocker locker(&m_stateMutex);
    return m_programmerSelectionLookup.contains(fixtureId);
}

bool ProgrammerController::allInProgrammerSelection(const QList<quint32>& fixtureIds) const
{
    if (fixtureIds.isEmpty())
        return false;
    QMutexLocker locker(&m_stateMutex);
    for (quint32 fid : fixtureIds)
    {
        if (!m_programmerSelectionLookup.contains(fid))
            return false;
    }
    return true;
}

QColor ProgrammerController::programmerColor() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_programmerColor;
}

void ProgrammerController::setProgrammerColorComponent(int qlcPrimaryColour, uchar value)
{
    // Written from the DMX thread (VCSlider colour-component sliders).
    QMutexLocker locker(&m_stateMutex);
    if (!m_programmerColor.isValid())
        m_programmerColor = QColor(0, 0, 0, 0);
    switch (qlcPrimaryColour)
    {
    case QLCChannel::Red:   m_programmerColor.setRed(value); break;
    case QLCChannel::Green: m_programmerColor.setGreen(value); break;
    case QLCChannel::Blue:  m_programmerColor.setBlue(value); break;
    // White rides in alpha — VCSlider's wheel-match consumer uses RGB
    // only, but stashing W here lets a future white-aware match use it.
    case QLCChannel::White: m_programmerColor.setAlpha(value); break;
    default: break;
    }
}

/*****************************************************************************
 * Programmer Values
 *****************************************************************************/

void ProgrammerController::setProgrammerValue(quint32 fixtureId, quint32 channel, uchar value)
{
    // Written from the DMX thread (VCSlider::writeDMXParameter).
    bool wasEmpty = false;
    {
        QMutexLocker locker(&m_stateMutex);
        wasEmpty = m_programmerValues.isEmpty();
        m_programmerValues[fixtureId][channel] = value;
    }
    if (wasEmpty)
        emit programmerDirtyChanged(true);
}

void ProgrammerController::clearProgrammerValues()
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_programmerValues.isEmpty() && m_editedScenes.isEmpty())
            return;
        // Caller (Save) has persisted everything elsewhere — drop the
        // values map AND the in-memory edited-scenes tracking. Snapshots
        // are no longer needed because the post-save scenes ARE the new
        // baseline.
        m_programmerValues.clear();
        m_editedScenes.clear();
        m_sceneSnapshots.clear();
    }
    emit programmerDirtyChanged(false);
}

bool ProgrammerController::isProgrammerDirty() const
{
    QMutexLocker locker(&m_stateMutex);
    return !m_programmerValues.isEmpty() || !m_editedScenes.isEmpty();
}

quint32 ProgrammerController::saveProgrammerAsScene(const QString &name)
{
    const QHash<quint32, QHash<quint32, uchar>> values = programmerValues();
    if (values.isEmpty())
        return Function::invalidId();

    Scene *scene = new Scene(m_doc);
    scene->setName(name.isEmpty() ? tr("Programmer scene") : name);

    for (auto fixIt = values.constBegin();
         fixIt != values.constEnd(); ++fixIt)
    {
        const quint32 fid = fixIt.key();
        const QHash<quint32, uchar> &channels = fixIt.value();
        for (auto chIt = channels.constBegin();
             chIt != channels.constEnd(); ++chIt)
        {
            scene->setValue(fid, chIt.key(), chIt.value());
        }
    }

    if (!m_doc->addFunction(scene))
    {
        delete scene;
        return Function::invalidId();
    }
    m_doc->setModified();
    return scene->id();
}

void ProgrammerController::slotProgrammerFunctionStarted(quint32 fid)
{
    Function *f = m_doc->function(fid);
    if (f == NULL)
        return;
    // qDebug, not qCritical: this runs on the MasterTimer thread (DirectConnection)
    // while the function-list mutex is held, and a chaser stepping fires it on
    // every step — unconditional stderr writes there cost tick budget.
    qDebug() << "[PC] functionStarted:" << fid << f->name()
             << "type=" << f->type()
             << "mode=" << (m_doc->mode() == Doc::Design ? "Design" : "Operate");
    // Pure container work only: this slot runs on the MasterTimer thread with
    // MasterTimer::m_functionListMutex already held, so it must not call out
    // to anything that could take another engine lock while holding this one.
    const Function::Type type = f->type();
    QMutexLocker locker(&m_stateMutex);
    if (type == Function::SceneType)
    {
        // Re-running an already-tracked scene: bump it to most-recent.
        m_runningScenes.removeAll(fid);
        m_runningScenes.append(fid);
    }
    else if (type == Function::ChaserType)
    {
        m_runningChasers.removeAll(fid);
        m_runningChasers.append(fid);
    }
    else if (type == Function::CollectionType)
    {
        m_runningCollections.removeAll(fid);
        m_runningCollections.append(fid);
    }
}

void ProgrammerController::slotProgrammerFunctionStopped(quint32 fid)
{
    Function *f = m_doc->function(fid);
    qDebug() << "[PC] functionStopped:" << fid
             << (f ? f->name() : "(null)")
             << "mode=" << (m_doc->mode() == Doc::Design ? "Design" : "Operate");

    // As in slotProgrammerFunctionStarted: DMX thread, container work only.
    QMutexLocker locker(&m_stateMutex);
    m_runningScenes.removeAll(fid);
    m_runningChasers.removeAll(fid);
    m_runningCollections.removeAll(fid);
}

namespace {

/** Collect the Scenes that are *actively contributing to the stage*
    under a running function, appending newest-relevant first.
      - Scene:      itself.
      - Chaser:     only its current running step's scene (that's what's
                    visible and what the user is editing). Recurses so a
                    chaser-of-chasers still resolves to a leaf scene.
      - Collection: all member functions run at once, so recurse into
                    every member.
    Only top-level running functions are tracked in Doc's running lists;
    member scenes/chasers inside a chaser or collection are started
    directly (bypassing the MasterTimer), so they never appear there.
    Recursing from the tracked top-level function is how we reach them. */
void collectActiveScenes(Doc *doc, Function *f, QList<Scene*>& out)
{
    if (f == NULL)
        return;
    if (Scene *s = qobject_cast<Scene*>(f))
    {
        if (!out.contains(s))
            out.append(s);
    }
    else if (Chaser *c = qobject_cast<Chaser*>(f))
    {
        ChaserRunnerStep step = c->currentRunningStep();
        collectActiveScenes(doc, step.m_function, out);
    }
    else if (Collection *coll = qobject_cast<Collection*>(f))
    {
        const QList<quint32> members = coll->functions();
        for (quint32 mid : members)
            collectActiveScenes(doc, doc->function(mid), out);
    }
}

} // namespace

quint32 ProgrammerController::routeProgrammerEdit(quint32 fid, quint32 ch, uchar value)
{
    // Build the set of running scenes that could own this channel,
    // newest → oldest. A scene counts whether it's running on its own,
    // as the active step of a running chaser, or as a member of a
    // running collection (including a collection that runs a chaser,
    // which is the common "play a look" structure).
    // Copy the running lists under the lock (they are mutated from the same
    // DMX thread by the functionStarted/Stopped slots, and read here), then
    // walk them unlocked — collectActiveScenes() calls into Doc.
    QList<quint32> scenes, chasers, collections;
    {
        QMutexLocker locker(&m_stateMutex);
        scenes = m_runningScenes;
        chasers = m_runningChasers;
        collections = m_runningCollections;
    }

    QList<Scene*> candidates;
    for (int i = scenes.size() - 1; i >= 0; --i)
        collectActiveScenes(m_doc, m_doc->function(scenes.at(i)), candidates);
    for (int i = chasers.size() - 1; i >= 0; --i)
        collectActiveScenes(m_doc, m_doc->function(chasers.at(i)), candidates);
    for (int i = collections.size() - 1; i >= 0; --i)
        collectActiveScenes(m_doc, m_doc->function(collections.at(i)), candidates);

    // Palette-aware routing first: editing a whole group scene live
    // updates the palette (so the group follows) instead of writing a
    // per-channel override. Falls through only for pad sub-selections
    // or partial group selections.
    const quint32 paletteRouted = tryRoutePaletteEdit(fid, ch, value, candidates);
    if (paletteRouted != Function::invalidId())
        return paletteRouted;

    // LTP: route to the first (newest) running Scene that already has
    // this (fid, ch) set.
    for (Scene *scene : qAsConst(candidates))
    {
        SceneValue probe(fid, ch);
        if (!scene->checkValue(probe))
            continue;

        const quint32 sid = scene->id();
        // First time this scene becomes edited this round: snapshot it so
        // Revert can restore it.
        beginSceneEdit(scene);
        // checkHTP=false → the running fader REPLACES the channel
        // value with ours rather than HTP-stacking. Without this,
        // intensity-grouped channels (R/G/B/W/Dimmer) ignore lowered
        // values — the existing higher target wins HTP, the user
        // sees their slider tweak briefly then revert on the next tick.
        scene->setValue(SceneValue(fid, ch, value),
                        /*blind=*/false, /*checkHTP=*/false);
        return sid;
    }
    return Function::invalidId();
}

bool ProgrammerController::selectionCoversGroups(const QList<quint32> &groups) const
{
    if (groups.isEmpty())
        return false;
    const QList<quint32> selection = programmerSelection();   // locked copy
    QSet<quint32> sel(selection.begin(), selection.end());
    QSet<quint32> grp;
    for (quint32 gid : groups)
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g == NULL)
            continue;
        const QList<quint32> fl = g->fixtureList();
        grp.unite(QSet<quint32>(fl.begin(), fl.end()));
    }
    return !grp.isEmpty() && sel == grp;
}

ProgrammerController::PaletteEditOutcome
ProgrammerController::updatePaletteForEdit(QLCPalette *pal, uchar value)
{
    switch (pal->type())
    {
    case QLCPalette::Color:
    {
        // Use the composite programmer color (RGB). White/amber/uv live
        // edits aren't represented by an RGB-only Color palette and
        // simply don't match its expansion (so they never reach here).
        const QColor c(m_programmerColor.red(),
                       m_programmerColor.green(),
                       m_programmerColor.blue());
        const QString nv = c.name(); // "#rrggbb"
        if (pal->value().toString() == nv)
            return PaletteUnchanged;
        pal->setValue(nv);
        return PaletteChanged;
    }
    case QLCPalette::Dimmer:
    case QLCPalette::Gobo:
    case QLCPalette::Shutter:
        if (pal->intValue1() == int(value))
            return PaletteUnchanged;
        pal->setValue(int(value));
        return PaletteChanged;
    default:
        // Pan/Tilt and anything else: a single channel edit can't be
        // cleanly back-derived to the palette value.
        return PaletteCantDerive;
    }
}

quint32 ProgrammerController::tryRoutePaletteEdit(quint32 fid, quint32 ch,
                                                 uchar value,
                                                 const QList<Scene*> &candidates)
{
    // Only when editing the whole group — a pad sub-selection means the
    // user wants to deviate individual fixtures, so keep per-channel.
    if (!m_programmerSubSelection.isEmpty())
        return Function::invalidId();

    for (Scene *scene : candidates)
    {
        const QList<quint32> pids = scene->palettes();
        if (pids.isEmpty())
            continue;
        const QList<quint32> groups = scene->fixtureGroups();
        if (!selectionCoversGroups(groups))
            continue;

        for (quint32 pid : pids)
        {
            QLCPalette *pal = m_doc->palette(pid);
            if (pal == NULL)
                continue;

            // Does this palette currently drive (fid, ch)? Pass the owning
            // scene: this runs on the DMX thread, where the Doc-wide function
            // scan in the Aim branch would be a race.
            bool drives = false;
            const QList<SceneValue> exp =
                pal->valuesFromFixtureGroups(m_doc, groups, scene);
            for (const SceneValue &sv : exp)
            {
                if (sv.fxi == fid && sv.channel == ch)
                {
                    drives = true;
                    break;
                }
            }
            if (!drives)
                continue;

            // Snapshot BEFORE touching the palette: updatePaletteForEdit
            // mutates the shared QLCPalette in place, so a snapshot taken
            // afterwards would capture the already-edited value and Revert
            // would be a no-op.
            bool wasEdited = false;
            {
                QMutexLocker locker(&m_stateMutex);
                wasEdited = m_editedScenes.contains(scene->id());
            }
            beginSceneEdit(scene);

            PaletteEditOutcome outcome = updatePaletteForEdit(pal, value);
            if (outcome == PaletteCantDerive)
            {
                // updatePaletteForEdit can't back-derive this palette type
                // from a raw DMX value alone (e.g. PanTilt). Try the
                // channel-group-aware path so we update the palette rather
                // than writing a raw baked value into the scene alongside it.
                Fixture *fxi = m_doc->fixture(fid);
                const QLCChannel *qch = fxi ? fxi->channel(ch) : nullptr;
                if (qch)
                    outcome = updatePaletteForChannelType(pal, qch->group(), 0, value, fxi);
                if (outcome == PaletteCantDerive)
                {
                    // Nothing was mutated after all — undo the speculative
                    // snapshot so we don't leave the scene falsely dirty.
                    if (!wasEdited)
                    {
                        bool nowClean = false;
                        {
                            QMutexLocker locker(&m_stateMutex);
                            m_sceneSnapshots.remove(scene->id());
                            m_editedScenes.remove(scene->id());
                            nowClean = m_programmerValues.isEmpty() &&
                                       m_editedScenes.isEmpty();
                        }
                        if (nowClean)
                            emit programmerDirtyChanged(false);
                    }
                    return Function::invalidId(); // genuinely can't route
                }
            }

            const quint32 sid = scene->id();
            if (outcome == PaletteChanged)
            {
                // Re-expand the palette into the LIVE faders on the next write
                // tick — the whole group follows the edit with no scene/engine
                // change. In-place, not resetRuntime(): this is the DMX thread
                // and it runs on every tick of a slider drag.
                scene->requestPaletteRefresh();
                m_doc->setModified();
            }
            return sid; // took the edit (changed or already-matching)
        }
    }
    return Function::invalidId();
}

int ProgrammerController::rerouteProgrammerValues()
{
    int routed = 0;
    // Snapshot first — routeProgrammerEdit() mutates m_programmerValues
    // indirectly only via our removals below, but iterate a copy to be
    // safe against any reentrancy (and against the DMX thread inserting).
    const QHash<quint32, QHash<quint32, uchar>> snapshot = programmerValues();
    for (auto fixIt = snapshot.constBegin(); fixIt != snapshot.constEnd(); ++fixIt)
    {
        const quint32 fid = fixIt.key();
        const QHash<quint32, uchar>& chans = fixIt.value();
        for (auto chIt = chans.constBegin(); chIt != chans.constEnd(); ++chIt)
        {
            const quint32 ch = chIt.key();
            const uchar val = chIt.value();
            if (routeProgrammerEdit(fid, ch, val) != Function::invalidId())
            {
                // Now owned by a running scene → drop from the raw map
                // so Save doesn't also bundle it into a new scene.
                QMutexLocker locker(&m_stateMutex);
                m_programmerValues[fid].remove(ch);
                if (m_programmerValues[fid].isEmpty())
                    m_programmerValues.remove(fid);
                ++routed;
            }
        }
    }
    return routed;
}

QSet<quint32> ProgrammerController::editedSceneIds() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_editedScenes;
}

bool ProgrammerController::hasProgrammerValues() const
{
    QMutexLocker locker(&m_stateMutex);
    return !m_programmerValues.isEmpty();
}

QHash<quint32, QHash<quint32, uchar>> ProgrammerController::programmerValues() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_programmerValues;
}

namespace {

/** Classify a fixture's channel into the smart-save category that
    determines which folder the resulting scene lands in. */
Doc::SaveCategory categorizeChannel(const QLCChannel *qch)
{
    if (qch == NULL)
        return Doc::SaveCatUnknown;
    switch (qch->group())
    {
    case QLCChannel::Pan:
    case QLCChannel::Tilt:
        return Doc::SaveCatPosition;
    case QLCChannel::Colour:
        return Doc::SaveCatColor;
    case QLCChannel::Intensity:
        return (qch->colour() == QLCChannel::NoColour)
                ? Doc::SaveCatIntensity
                : Doc::SaveCatColor;
    case QLCChannel::Gobo:
    case QLCChannel::Speed:
    case QLCChannel::Shutter:
    case QLCChannel::Prism:
    case QLCChannel::Beam:
    case QLCChannel::Effect:
    case QLCChannel::Maintenance:
        return Doc::SaveCatSpecial;
    default:
        return Doc::SaveCatUnknown;
    }
}

QString categoryFolderName(Doc::SaveCategory cat)
{
    switch (cat)
    {
    case Doc::SaveCatPosition:  return QStringLiteral("Positions");
    case Doc::SaveCatColor:     return QStringLiteral("Colors");
    case Doc::SaveCatSpecial:   return QStringLiteral("Specials");
    case Doc::SaveCatIntensity: return QStringLiteral("Intensity");
    default:                    return QStringLiteral("Programmer");
    }
}

/** Best-effort named-color guess from RGBW values. The Save dialog
    shows this as the default scene name; the user can rename. */
QString guessColorName(int r, int g, int b, int w)
{
    // Treat any value >= 32 as "on enough to count" for category checks.
    const bool anyRGB = (r >= 32 || g >= 32 || b >= 32);
    if (!anyRGB && w >= 32)
        return QStringLiteral("White");
    if (!anyRGB && w == 0)
        return QStringLiteral("Black");

    struct NamedColor { const char *name; int r, g, b; };
    static const NamedColor palette[] = {
        {"Red",        255,   0,   0},
        {"Orange",     255, 128,   0},
        {"Yellow",     255, 255,   0},
        {"Lime",       128, 255,   0},
        {"Green",        0, 255,   0},
        {"Teal",         0, 255, 128},
        {"Cyan",         0, 255, 255},
        {"Sky",          0, 128, 255},
        {"Blue",         0,   0, 255},
        {"Indigo",     128,   0, 255},
        {"Magenta",    255,   0, 255},
        {"Pink",       255,   0, 128},
        {"White",      255, 255, 255},
        {"Pale",       180, 180, 180},
        {"Amber",      255, 160,  40},
    };
    int best = 0;
    int bestDist = INT_MAX;
    for (size_t i = 0; i < sizeof(palette) / sizeof(palette[0]); ++i)
    {
        const int dr = palette[i].r - r;
        const int dg = palette[i].g - g;
        const int db = palette[i].b - b;
        const int d = dr * dr + dg * dg + db * db;
        if (d < bestDist)
        {
            bestDist = d;
            best = int(i);
        }
    }
    QString base = QString::fromLatin1(palette[best].name);
    if (w >= 96 && base != QStringLiteral("White"))
        base += QStringLiteral(" + W");
    return base;
}

/** Read a fixture's color-wheel capability name at the given DMX
    value, if the fixture has one. Returns empty if not applicable. */
QString readColorWheelName(Fixture *fxi, const QHash<quint32, uchar> &values)
{
    if (fxi == NULL || fxi->fixtureMode() == NULL)
        return QString();
    quint32 wheelCh = fxi->fixtureMode()->channelNumber(
        QLCChannel::Colour, QLCChannel::MSB);
    if (wheelCh == QLCChannel::invalid())
        return QString();
    if (!values.contains(wheelCh))
        return QString();
    const QLCChannel *qch = fxi->channel(wheelCh);
    if (qch == NULL)
        return QString();
    const uchar v = values.value(wheelCh);
    for (QLCCapability *cap : qch->capabilities())
    {
        if (cap == NULL)
            continue;
        if (v >= cap->min() && v <= cap->max())
            return cap->name();
    }
    return QString();
}

} // anonymous namespace

QList<Doc::SaveBucket> ProgrammerController::proposedSaveBuckets() const
{
    QList<Doc::SaveBucket> out;
    if (m_programmerValues.isEmpty())
        return out;

    // Phase A: bin every programmer value by category, tracking the
    // (fid → ch → val) values and the fixture set per category.
    QHash<Doc::SaveCategory, QHash<quint32, QHash<quint32, uchar>>> byCat;
    for (auto fxIt = m_programmerValues.constBegin();
         fxIt != m_programmerValues.constEnd(); ++fxIt)
    {
        const quint32 fid = fxIt.key();
        Fixture *fxi = m_doc->fixture(fid);
        if (fxi == NULL)
            continue;
        for (auto chIt = fxIt.value().constBegin();
             chIt != fxIt.value().constEnd(); ++chIt)
        {
            const Doc::SaveCategory cat = categorizeChannel(fxi->channel(chIt.key()));
            byCat[cat][fid][chIt.key()] = chIt.value();
        }
    }

    // Phase B: per category, pick the *most inclusive* grouping — the
    // single largest fixture group that contains every edited fixture
    // in that category. If one exists, the whole category collapses to
    // one bucket. Otherwise fall back to one bucket per smallest group
    // containing each fixture (the same split the UI can request later).
    QHash<QPair<quint32, Doc::SaveCategory>, Doc::SaveBucket> map;
    for (auto catIt = byCat.constBegin(); catIt != byCat.constEnd(); ++catIt)
    {
        const Doc::SaveCategory cat = catIt.key();
        const QHash<quint32, QHash<quint32, uchar>> &vals = catIt.value();
        const QList<quint32> catFix = vals.keys();

        // Largest group covering ALL of catFix.
        quint32 incId = Function::invalidId();
        QString incName;
        int incSize = -1;
        for (FixtureGroup *g : m_doc->fixtureGroups())
        {
            if (g == NULL)
                continue;
            const QList<quint32> flist = g->fixtureList();
            bool coversAll = true;
            for (quint32 fid : catFix)
            {
                if (!flist.contains(fid)) { coversAll = false; break; }
            }
            if (coversAll && flist.size() > incSize)
            {
                incSize = flist.size();
                incId = g->id();
                incName = g->name();
            }
        }

        if (incId != Function::invalidId())
        {
            Doc::SaveBucket &b = map[qMakePair(incId, cat)];
            b.category = cat;
            b.fixtureGroupId = incId;
            b.groupName = incName;
            for (auto fxIt = vals.constBegin(); fxIt != vals.constEnd(); ++fxIt)
                for (auto chIt = fxIt.value().constBegin();
                     chIt != fxIt.value().constEnd(); ++chIt)
                    b.values[fxIt.key()][chIt.key()] = chIt.value();
        }
        else
        {
            // No single covering group → per-fixture smallest group.
            for (auto fxIt = vals.constBegin(); fxIt != vals.constEnd(); ++fxIt)
            {
                const quint32 fid = fxIt.key();
                quint32 fgId = Function::invalidId();
                QString fgName;
                int bestSize = INT_MAX;
                for (FixtureGroup *g : m_doc->fixtureGroups())
                {
                    if (g == NULL)
                        continue;
                    const QList<quint32> flist = g->fixtureList();
                    if (!flist.contains(fid))
                        continue;
                    if (flist.size() < bestSize)
                    {
                        bestSize = flist.size();
                        fgId = g->id();
                        fgName = g->name();
                    }
                }
                if (fgName.isEmpty())
                    fgName = QStringLiteral("Custom");
                Doc::SaveBucket &b = map[qMakePair(fgId, cat)];
                b.category = cat;
                b.fixtureGroupId = fgId;
                b.groupName = fgName;
                for (auto chIt = fxIt.value().constBegin();
                     chIt != fxIt.value().constEnd(); ++chIt)
                    b.values[fid][chIt.key()] = chIt.value();
            }
        }
    }

    for (auto it = map.begin(); it != map.end(); ++it)
        out.append(it.value());
    finalizeSaveBuckets(out);

    // Stable order: Position, Color, Special, Intensity by group name.
    std::sort(out.begin(), out.end(),
              [](const Doc::SaveBucket &a, const Doc::SaveBucket &c) {
                  if (a.category != c.category)
                      return a.category < c.category;
                  return a.groupName < c.groupName;
              });
    return out;
}

QList<Doc::SaveBucket> ProgrammerController::splitBucketByGroup(const Doc::SaveBucket &bucket) const
{
    // Re-bin the bucket's fixtures by their smallest containing group,
    // producing one sub-bucket per group (category preserved).
    QHash<quint32, Doc::SaveBucket> map; // groupId → bucket
    for (auto fxIt = bucket.values.constBegin();
         fxIt != bucket.values.constEnd(); ++fxIt)
    {
        const quint32 fid = fxIt.key();
        quint32 fgId = Function::invalidId();
        QString fgName;
        int bestSize = INT_MAX;
        for (FixtureGroup *g : m_doc->fixtureGroups())
        {
            if (g == NULL)
                continue;
            const QList<quint32> flist = g->fixtureList();
            if (!flist.contains(fid))
                continue;
            if (flist.size() < bestSize)
            {
                bestSize = flist.size();
                fgId = g->id();
                fgName = g->name();
            }
        }
        if (fgName.isEmpty())
            fgName = QStringLiteral("Custom");
        Doc::SaveBucket &b = map[fgId];
        b.category = bucket.category;
        b.fixtureGroupId = fgId;
        b.groupName = fgName;
        for (auto chIt = fxIt.value().constBegin();
             chIt != fxIt.value().constEnd(); ++chIt)
            b.values[fid][chIt.key()] = chIt.value();
    }

    QList<Doc::SaveBucket> out = map.values();
    finalizeSaveBuckets(out);
    std::sort(out.begin(), out.end(),
              [](const Doc::SaveBucket &a, const Doc::SaveBucket &c) {
                  return a.groupName < c.groupName;
              });
    return out;
}

void ProgrammerController::finalizeSaveBuckets(QList<Doc::SaveBucket> &buckets) const
{
    // Generate default name + path per bucket.
    for (auto it = buckets.begin(); it != buckets.end(); ++it)
    {
        Doc::SaveBucket &b = *it;
        b.defaultPath = QStringLiteral("%1/%2")
                            .arg(categoryFolderName(b.category))
                            .arg(b.groupName);

        switch (b.category)
        {
        case Doc::SaveCatColor:
        {
            // Aggregate RGBW across the bucket's fixtures (avg).
            int rsum = 0, gsum = 0, bsum = 0, wsum = 0;
            int rN = 0, gN = 0, bN = 0, wN = 0;
            QString wheelName;
            for (auto fxIt2 = b.values.constBegin();
                 fxIt2 != b.values.constEnd(); ++fxIt2)
            {
                Fixture *fxi = m_doc->fixture(fxIt2.key());
                if (fxi == NULL)
                    continue;
                if (wheelName.isEmpty())
                    wheelName = readColorWheelName(fxi, fxIt2.value());
                for (auto chIt = fxIt2.value().constBegin();
                     chIt != fxIt2.value().constEnd(); ++chIt)
                {
                    const QLCChannel *qch = fxi->channel(chIt.key());
                    if (qch == NULL)
                        continue;
                    if (qch->group() != QLCChannel::Intensity)
                        continue;
                    switch (qch->colour())
                    {
                    case QLCChannel::Red:   rsum += chIt.value(); ++rN; break;
                    case QLCChannel::Green: gsum += chIt.value(); ++gN; break;
                    case QLCChannel::Blue:  bsum += chIt.value(); ++bN; break;
                    case QLCChannel::White: wsum += chIt.value(); ++wN; break;
                    default: break;
                    }
                }
            }
            int r = rN ? rsum / rN : 0;
            int g = gN ? gsum / gN : 0;
            int blu = bN ? bsum / bN : 0;
            int w = wN ? wsum / wN : 0;
            QString colorName = guessColorName(r, g, blu, w);
            if (rN == 0 && gN == 0 && bN == 0 && !wheelName.isEmpty())
                colorName = wheelName;
            b.defaultName = colorName;
        }
        break;

        case Doc::SaveCatPosition:
            b.defaultName = QStringLiteral("Pos");
            break;

        case Doc::SaveCatIntensity:
        {
            // Average dimmer to derive an "Int 75%" style label.
            int sum = 0, n = 0;
            for (auto fxIt2 = b.values.constBegin();
                 fxIt2 != b.values.constEnd(); ++fxIt2)
            {
                for (auto chIt = fxIt2.value().constBegin();
                     chIt != fxIt2.value().constEnd(); ++chIt)
                {
                    sum += chIt.value();
                    ++n;
                }
            }
            const int pct = n ? int((sum / n) * 100.0 / 255.0 + 0.5) : 0;
            b.defaultName = QStringLiteral("Int %1%").arg(pct);
        }
        break;

        case Doc::SaveCatSpecial:
            b.defaultName = QStringLiteral("Special");
            break;

        default:
            b.defaultName = QStringLiteral("Programmer");
            break;
        }

        // Append a number if a function with that name already exists.
        QString candidate = b.defaultName;
        int suffix = 2;
        while (true)
        {
            bool collision = false;
            for (Function *fn : m_doc->functions())
            {
                if (fn != NULL && fn->name() == candidate
                    && fn->path() == b.defaultPath)
                {
                    collision = true;
                    break;
                }
            }
            if (!collision)
                break;
            candidate = QStringLiteral("%1 %2").arg(b.defaultName).arg(suffix++);
        }
        b.defaultName = candidate;
    }
}

void ProgrammerController::stepCurrentChaser(int direction)
{
    if (direction == 0)
        return;
    quint32 cid = Function::invalidId();
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_runningChasers.isEmpty())
            return;
        cid = m_runningChasers.last();
    }
    Chaser *chaser = qobject_cast<Chaser*>(m_doc->function(cid));
    if (chaser == NULL)
        return;
    ChaserAction action;
    action.m_action = (direction > 0) ? ChaserNextStep : ChaserPreviousStep;
    chaser->setAction(action);
}

quint32 ProgrammerController::findMatchingScene(
    const QHash<quint32, QHash<quint32, uchar>> &values,
    quint32 excludeSceneId) const
{
    if (values.isEmpty())
        return Function::invalidId();

    // Flatten our values into a sorted list for quick exact-compare.
    auto flatten = [](const QHash<quint32, QHash<quint32, uchar>> &v) {
        QList<QPair<QPair<quint32, quint32>, uchar>> out;
        for (auto fxIt = v.constBegin(); fxIt != v.constEnd(); ++fxIt)
            for (auto chIt = fxIt.value().constBegin();
                 chIt != fxIt.value().constEnd(); ++chIt)
                out.append({{fxIt.key(), chIt.key()}, chIt.value()});
        std::sort(out.begin(), out.end());
        return out;
    };
    const auto target = flatten(values);

    for (Function *fn : m_doc->functions())
    {
        if (fn == NULL || fn->type() != Function::SceneType)
            continue;
        if (fn->id() == excludeSceneId)
            continue;
        Scene *scene = qobject_cast<Scene*>(fn);
        if (scene == NULL)
            continue;

        QHash<quint32, QHash<quint32, uchar>> sceneVals;
        for (const SceneValue &sv : scene->values())
            sceneVals[sv.fxi][sv.channel] = sv.value;

        if (flatten(sceneVals) == target)
            return scene->id();
    }
    return Function::invalidId();
}

QList<quint32> ProgrammerController::collectionsContaining(quint32 sceneId) const
{
    QList<quint32> out;
    for (Function *fn : m_doc->functions())
    {
        if (fn == NULL || fn->type() != Function::CollectionType)
            continue;
        Collection *coll = qobject_cast<Collection*>(fn);
        if (coll == NULL)
            continue;
        if (coll->functions().contains(sceneId))
            out.append(coll->id());
    }
    return out;
}

bool ProgrammerController::replaceSceneInCollection(quint32 collectionId,
                                                    quint32 oldSceneId,
                                                    quint32 newSceneId)
{
    Collection *coll = qobject_cast<Collection*>(m_doc->function(collectionId));
    if (coll == NULL)
        return false;
    const QList<quint32> children = coll->functions();
    const int idx = children.indexOf(oldSceneId);
    if (idx < 0)
        return false;
    coll->removeFunction(oldSceneId);
    coll->addFunction(newSceneId, idx);
    m_doc->setModified();
    return true;
}

void ProgrammerController::revertSceneFromSnapshot(quint32 sceneId)
{
    SceneSnapshot snap;
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_sceneSnapshots.contains(sceneId))
            return;
        snap = m_sceneSnapshots.value(sceneId);
    }

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (scene == NULL)
        return;
    restoreSceneFromSnapshot(scene, snap);   // calls out — not under the lock

    bool nowClean = false;
    {
        QMutexLocker locker(&m_stateMutex);
        m_sceneSnapshots.remove(sceneId);
        m_editedScenes.remove(sceneId);
        nowClean = m_programmerValues.isEmpty() && m_editedScenes.isEmpty();
    }
    if (nowClean)
        emit programmerDirtyChanged(false);
}

quint32 ProgrammerController::singleRunningCollection() const
{
    QMutexLocker locker(&m_stateMutex);
    if (m_runningCollections.size() == 1)
        return m_runningCollections.first();
    return Function::invalidId();
}

void ProgrammerController::flashFixture(quint32 fixtureId, int durationMs)
{
    if (m_programmerFlasher != nullptr)
        m_programmerFlasher->flashFixture(fixtureId, durationMs);
}

/*****************************************************************************
 * Highlight mode
 *****************************************************************************/

bool ProgrammerController::isHighlightActive() const
{
    return m_highlightActive;
}

void ProgrammerController::setHighlightActive(bool active)
{
    if (m_highlightActive == active)
        return;
    m_highlightActive = active;
    // HighlightEffect registers/unregisters a DMXSource, which takes
    // MasterTimer::m_dmxSourceListMutex — so take the locked copy of the
    // selection FIRST and call out with no lock held.
    const QList<quint32> selection = programmerSelection();
    if (m_highlightEffect)
        m_highlightEffect->setFixtures(active ? selection : QList<quint32>());
    emit highlightActiveChanged(active);
}

/*****************************************************************************
 * Followspot effect
 *****************************************************************************/

/*********************************************************************
 * Controller axis binding
 *********************************************************************/

void ProgrammerController::bindPanAxis()
{
    m_boundFromProfile = false; // switching to manual mode
    connectControllerInput();
    m_capturingPan  = true;
    m_capturingTilt = false;
}

void ProgrammerController::bindTiltAxis()
{
    m_boundFromProfile = false; // switching to manual mode
    connectControllerInput();
    m_capturingPan  = false;
    m_capturingTilt = true;
}

void ProgrammerController::clearAxisBindings()
{
    m_panBound  = false;
    m_tiltBound = false;
    m_boundFromProfile = false;
    m_capturingPan  = false;
    m_capturingTilt = false;
    emit axisBindingChanged();
}

QString ProgrammerController::axisLabel(quint32 universe, quint32 channel) const
{
    if (!m_doc->inputOutputMap())
        return QString("Universe %1, Ch %2").arg(universe + 1).arg(channel + 1);

    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    if (!ip)
        return QString("Universe %1, Ch %2").arg(universe + 1).arg(channel + 1);

    // Ask the plugin for a profile-derived channel name (e.g. "Left Stick X")
    const QString chanName = ip->plugin()
        ? ip->plugin()->inputChannelName(ip->input(), channel)
        : QString();

    const QString devName = ip->inputName(); // e.g. "Microsoft Controller"

    if (!chanName.isEmpty() && !devName.isEmpty())
        return QString("%1 — %2").arg(devName).arg(chanName);
    if (!chanName.isEmpty())
        return chanName;
    if (!devName.isEmpty())
        return QString("%1  axis %2").arg(devName).arg(channel + 1);
    return QString("Universe %1, Ch %2").arg(universe + 1).arg(channel + 1);
}

QString ProgrammerController::panAxisLabel() const
{
    if (!m_panBound) return tr("—");
    return axisLabel(m_panUniverse, m_panChannel);
}

QString ProgrammerController::tiltAxisLabel() const
{
    if (!m_tiltBound) return tr("—");
    return axisLabel(m_tiltUniverse, m_tiltChannel);
}

void ProgrammerController::autoBindFromProfile()
{
    if (!m_doc->inputOutputMap())
        return;

    bool changed = false;
    const auto universeList = m_doc->inputOutputMap()->universes();
    qDebug() << "[AutoBind] scanning" << universeList.size() << "universes; panBound=" << m_panBound << "tiltBound=" << m_tiltBound;
    for (quint32 u = 0; u < (quint32)universeList.size(); ++u)
    {
        InputPatch *ip = m_doc->inputOutputMap()->inputPatch(u);
        if (!ip)
        {
            qDebug() << "[AutoBind] universe" << u << "- no input patch";
            continue;
        }
        QLCIOPlugin *plugin = ip->plugin();
        if (!plugin)
            continue;

        // inputAxisForSlot matches profile by device VID/PID, not by the named profile
        // assignment in the I/O panel — so no hidProfileName() check needed here.
        const int panCh = plugin->inputAxisForSlot(ip->input(), QStringLiteral("pan"));
        const int tiltCh = plugin->inputAxisForSlot(ip->input(), QStringLiteral("tilt"));
        qDebug() << "[AutoBind] universe" << u << "plugin=" << plugin->name()
                    << "input=" << ip->input() << "profile=" << ip->hidProfileName()
                    << "panCh=" << panCh << "tiltCh=" << tiltCh;

        if (panCh >= 0 && !m_panBound)
        {
            m_panUniverse = u;
            m_panChannel  = (quint32)panCh;
            m_panBound    = true;
            changed = true;
        }
        if (tiltCh >= 0 && !m_tiltBound)
        {
            m_tiltUniverse = u;
            m_tiltChannel  = (quint32)tiltCh;
            m_tiltBound    = true;
            changed = true;
        }
    }

    if (changed)
    {
        m_boundFromProfile = true;

        // Capture the device's global deadzone from the matched HID profile so it
        // reaches effect scripts (followspot.js) via the "joystick" data channel —
        // otherwise they fall back to a hard-coded default and feel inconsistent.
        for (quint32 u = 0; u < (quint32)universeList.size(); ++u)
        {
            InputPatch *ip2 = m_doc->inputOutputMap()->inputPatch(u);
            if (!ip2) continue;
            QLCIOPlugin *plugin2 = ip2->plugin();
            if (!plugin2) continue;
            if (plugin2->inputAxisForSlot(ip2->input(), QStringLiteral("pan"))  < 0 &&
                plugin2->inputAxisForSlot(ip2->input(), QStringLiteral("tilt")) < 0)
                continue;
            m_joystickDeadzone = qBound(0.0f, float(plugin2->inputProfileDeadzone(ip2->input())), 0.4f);
            break;
        }

        updateFollowspotJsBindings();
        emit axisBindingChanged();
    }

    // Always connect input so we receive events even when no profile is assigned.
    // Without this, manual bind capture (bindPanAxis/bindTiltAxis) never fires.
    connectControllerInput();
    qDebug() << "[AutoBind] RESULT: panBound=" << m_panBound << "(u=" << m_panUniverse << "ch=" << m_panChannel << ")"
                << "tiltBound=" << m_tiltBound << "(u=" << m_tiltUniverse << "ch=" << m_tiltChannel << ")"
                << "inputConnected=" << m_controllerInputConnected;
}

void ProgrammerController::connectControllerInput()
{
    if (m_controllerInputConnected || !m_doc->inputOutputMap())
        return;
    connect(m_doc->inputOutputMap(),
            SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)),
            this,
            SLOT(slotControllerInputChanged(quint32, quint32, uchar, const QString&)));
    m_controllerInputConnected = true;
}

void ProgrammerController::updateFollowspotJsBindings()
{
    // Followspot ties to a joystick DEVICE, not to per-effect x/y axis settings:
    // pan/tilt arrive globally through the "joystick" data channel (fed from the
    // device's HID profile), so the effect palette must not carry its own x/y
    // input bindings.  Clear any stale per-effect axis bindings so the config
    // stays minimal and device-bound.  (Generic, non-joystick effects keep
    // their own input bindings — only followspot is device-bound here.)
    for (QLCPalette *pal : m_doc->palettes())
    {
        const QString sp = pal->scriptPath();
        if (!sp.contains(QLatin1String("followspot"), Qt::CaseInsensitive))
            continue;
        pal->clearEffectInputBindings();
    }
}

void ProgrammerController::slotControllerInputChanged(quint32 universe, quint32 channel,
                                                       uchar value, const QString &)
{
    if (m_capturingPan)
    {
        m_panUniverse = universe;
        m_panChannel  = channel;
        m_panBound    = true;
        m_capturingPan = false;
        updateFollowspotJsBindings();
        emit axisBindingChanged();
        return;
    }
    if (m_capturingTilt)
    {
        m_tiltUniverse = universe;
        m_tiltChannel  = channel;
        m_tiltBound    = true;
        m_capturingTilt = false;
        updateFollowspotJsBindings();
        emit axisBindingChanged();
        return;
    }

    const float norm = value / 255.0f;

    // Trace every event so we can see incoming u/ch vs bound u/ch.
    // One-shot per (universe, channel) pair to avoid 50Hz flood.
    {
        static QSet<quint64> seen;
        const quint64 key = ((quint64)universe << 32) | channel;
        if (!seen.contains(key))
        {
            seen.insert(key);
            qDebug() << "[Input] first event: u=" << universe << "ch=" << channel
                        << "val=" << value
                        << " | panBound=" << m_panBound
                        << "(u=" << m_panUniverse << "ch=" << m_panChannel << ")"
                        << " | tiltBound=" << m_tiltBound
                        << "(u=" << m_tiltUniverse << "ch=" << m_tiltChannel << ")"
                        << " | mode=" << (m_doc->mode() == Doc::Design ? "Design" : "Operate");
        }
    }

    // Input events only update the latest stick value; the 50 Hz timer
    // (m_designJoyTimer → applyDesignJoystick) integrates it continuously so a
    // held stick keeps moving at a rate set by its deflection.
    if (m_panBound && universe == m_panUniverse && channel == m_panChannel)
    {
        m_panNorm = norm;
        emit joystickUpdated(m_panNorm, m_tiltNorm);
        return;
    }
    if (m_tiltBound && universe == m_tiltUniverse && channel == m_tiltChannel)
    {
        m_tiltNorm = norm;
        emit joystickUpdated(m_panNorm, m_tiltNorm);
        return;
    }

    /* Button action dispatch: only on press (value > 0) */
    if (value > 0 && m_doc->inputOutputMap())
    {
        InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
        if (ip && ip->plugin())
        {
            const QString action = ip->plugin()->inputButtonAction(ip->input(), channel);
            if (!action.isEmpty())
                emit buttonActionTriggered(action);
        }
    }
}

void ProgrammerController::applyDesignJoystick()
{
    // Run-mode joystick: for an Aim look, drag the stage target in floor space
    // (the scene re-resolves per-fixture pan/tilt from the moved target).  A
    // Standard Pan/Tilt look is fixed; a followspot.js effect, when present,
    // owns the joystick instead (this path yields to it below).
    if (m_focusedSceneId == Function::invalidId())
        return;
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_focusedSceneId));
    if (!scene)
        return;
    if (!m_panBound && !m_tiltBound)
        return;

    // The joystick drives the focused scene in BOTH modes now: it authors the
    // Aim target (Edit, persisted) or tracks it live (Operate).  Ownership, not
    // mode, decides who gets the stick — see the effect yield below.

    // A followspot JS effect in this scene OWNS the joystick.  Pan/tilt reach it
    // through the "joystick" data channel.  Decide who owns the stick:
    //  - Followspot + NO aim target = FREE-FLY: the effect flies each head's angle
    //    itself, so this C++ path yields and lets it be the sole driver.
    //  - Followspot + aim target = CONVERGED FOLLOW: the joystick instead moves
    //    the shared aim target below and the scene's Aim look re-aims every head
    //    at it (heads stay converged on the spot).  The effect defers (emits no
    //    pan/tilt when it has a target), so the two never fight.  Don't yield.
    {
        bool hasFollowspot = false, hasAimTarget = false;
        for (quint32 pid : scene->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (!p) continue;
            if (p->type() == QLCPalette::Effect &&
                p->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
                hasFollowspot = true;
            if (p->type() == QLCPalette::Aim)
                hasAimTarget = true;
        }
        if (hasFollowspot && !hasAimTarget)
            return;
    }

    // Working selection: programmer selection first, then all scene fixtures.
    QList<quint32> workingSelection = programmerSelection();   // locked copy
    if (workingSelection.isEmpty())
    {
        for (quint32 fid : scene->fixtures())
            workingSelection << fid;
        for (quint32 gid : scene->fixtureGroups())
        {
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g)
                for (quint32 fid : g->fixtureList())
                    if (!workingSelection.contains(fid)) workingSelection << fid;
        }
    }
    if (workingSelection.isEmpty())
        return;

    // Find the controlling position palette (Aim or PanTilt, whichever is last in list).
    QLCPalette *panTiltPal = nullptr;
    QLCPalette *aimPal     = nullptr;
    if (m_focusedPaletteId != QLCPalette::invalidId())
    {
        QLCPalette *p = m_doc->palette(m_focusedPaletteId);
        if (p && p->type() == QLCPalette::PanTilt) panTiltPal = p;
        if (p && p->type() == QLCPalette::Aim)     aimPal     = p;
    }
    if (!panTiltPal && !aimPal)
    {
        for (quint32 pid : scene->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (!p) continue;
            if (p->type() == QLCPalette::PanTilt) panTiltPal = p;
            if (p->type() == QLCPalette::Aim)     aimPal     = p;
        }
        if (panTiltPal && aimPal)
        {
            const QList<quint32> &order = scene->palettes();
            if (order.lastIndexOf(aimPal->id()) > order.lastIndexOf(panTiltPal->id()))
                panTiltPal = nullptr;
            else
                aimPal = nullptr;
        }
    }

    if (aimPal)
    {
        // Aim palette: move the stage target in XY space (metres) so the fixture
        // beam traces a straight line on the floor — avoids the circular arc that
        // pan/tilt angle accumulation produces (equal angle steps ≠ equal floor steps).
        MonitorProperties *mProps = m_doc->monitorProperties();
        StageTarget *tgt = mProps ? mProps->stageTarget(aimPal->stageTargetId()) : nullptr;
        if (tgt)
        {
            if (!m_stageAimValid)
            {
                QVector3D pos = tgt->position();
                m_stageAimX = pos.x();
                m_stageAimY = pos.y();
                m_stageAimValid = true;
            }

            const float dt    = 1.0f / 50.0f;
            const float dz    = 0.12f;   // ±6% deadzone per axis
            const float speed = m_joystickSensitivity * 2.0f;  // m/s at full deflection
            float xVel =  (m_panNorm  - 0.5f) * 2.0f;
            float yVel = -(m_tiltNorm - 0.5f) * 2.0f;  // negated: stick-up → target up on stage
            if (qAbs(xVel) < dz) xVel = 0.0f;
            if (qAbs(yVel) < dz) yVel = 0.0f;

            // Stick centred → no velocity → nothing to do. Return BEFORE
            // resetRuntime()/emits so the 50 Hz timer doesn't churn while idle;
            // the heads simply hold their last aimed position.
            if (xVel == 0.0f && yVel == 0.0f)
                return;

            m_stageAimX = qBound(-50.0f, m_stageAimX + xVel * speed * dt, 50.0f);
            m_stageAimY = qBound(-50.0f, m_stageAimY + yVel * speed * dt, 50.0f);
            m_beamHeldValid = true;   // a real beam position now exists (lastPosition)

            tgt->setPosition(QVector3D(m_stageAimX, m_stageAimY, 0.0f));

            // Both Design and Operate modes: the scene is running (preview or
            // operate-scene).  resetRuntime() clears the fader map so the next
            // write() tick re-evaluates the Aim palette with the updated target
            // position — re-aiming every head at the moved spot (convergence).
            // A followspot.js effect on the scene defers (emits no pan/tilt when a
            // target is present), so it does not fight this.
            // Re-resolve ONLY the position palettes into the existing faders
            // (requestReaim) rather than tearing down the whole map — the latter
            // rebuilds the dimmer/colour faders every 50 Hz tick and flashes the
            // LEDs.  requestReaim replaces pan/tilt in place at 0-time fade.
            scene->requestReaim();

            emit designPositionWritten();
            emit followSpotPinChanged(true, m_stageAimX, m_stageAimY);
            return;
        }
        // No stage target — fall through to baked path
    }

    if (panTiltPal)
    {
        // Standard Pan/Tilt look (fixed angles): the joystick does not fly it.
        // Live position flying is the followspot.js effect (single engine); raw
        // P/T authoring will arrive with the Edit-mode gamepad map.  No-op here.
        emit followSpotPinChanged(false, 0.0f, 0.0f);  // PanTilt has no floor geometry
        return;
    }

    // No position palette — bake values directly into scene channels.
    //
    // Idle-stick guard (mirrors the aim branch above): applyDesignJoystick()
    // runs at 50 Hz whenever a joystick is bound.  A resting analog stick that
    // drifts slightly off-centre would otherwise re-bake identical pan/tilt and
    // call resetRuntime() every tick — tearing down the scene's whole fader map
    // 50×/s, which makes dimmer/colour faders re-fade and the rig visibly blink.
    // Within the centre deadzone → nothing has changed, hold and return before
    // any resetRuntime().  A followspot with an Aim look takes the guarded aim
    // branch instead, which is why applying an Aim target also stops the blink.
    {
        const float dz = 0.12f;   // ±6% per axis, matches the aim branch
        if (qAbs((m_panNorm  - 0.5f) * 2.0f) < dz &&
            qAbs((m_tiltNorm - 0.5f) * 2.0f) < dz)
            return;
    }

    bool firstEdit = false;
    {
        QMutexLocker locker(&m_stateMutex);
        firstEdit = !m_editedScenes.contains(m_focusedSceneId);
    }
    for (quint32 fid : workingSelection)
    {
        Fixture *f = m_doc->fixture(fid);
        if (!f || !f->fixtureMode()) continue;
        const QLCPhysical &phy = f->fixtureMode()->physical();
        // checkHTP=false → Scene::setValue REPLACES the channel in the live fader
        // instead of HTP-merging it. HTP would refuse to lower a pan/tilt value
        // (the existing higher target wins), which is the only reason this used to
        // need a resetRuntime() afterwards — and that tore the whole fader map down
        // 50×/s while the stick was deflected, re-fading every dimmer/colour
        // channel and making the rig blink. Replace-in-place needs no teardown.
        if (m_panBound)
        {
            float panMax = phy.focusPanMax() > 0 ? phy.focusPanMax() : 360.0f;
            for (const SceneValue &sv : f->positionToValues(QLCChannel::Pan, m_panNorm * panMax))
                scene->setValue(SceneValue(sv.fxi, sv.channel, sv.value),
                                /*blind=*/false, /*checkHTP=*/false);
        }
        if (m_tiltBound)
        {
            float tiltMax = phy.focusTiltMax() > 0 ? phy.focusTiltMax() : 270.0f;
            for (const SceneValue &sv : f->positionToValues(QLCChannel::Tilt, m_tiltNorm * tiltMax))
                scene->setValue(SceneValue(sv.fxi, sv.channel, sv.value),
                                /*blind=*/false, /*checkHTP=*/false);
        }
    }
    if (firstEdit)
        markSceneEdited(m_focusedSceneId);
    emit designPositionWritten();
}

void ProgrammerController::commitDesignJoystick()
{
    if (m_focusedSceneId == Function::invalidId()) return;
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_focusedSceneId));
    if (!scene) return;

    // Aim palette: the target was already updated live on every joystick tick.
    // Just finalize (mark edited if not already) and reset stage-aim state.
    if (m_stageAimValid)
    {
        bool alreadyEdited = false;
        {
            QMutexLocker locker(&m_stateMutex);
            alreadyEdited = m_editedScenes.contains(m_focusedSceneId);
        }
        if (!alreadyEdited)
            markSceneEdited(m_focusedSceneId);
        m_stageAimValid = false;
    }
}

void ProgrammerController::slotSyncEffectFixtures()
{
    if (m_highlightActive && m_highlightEffect)
        m_highlightEffect->setFixtures(programmerSelection());
}

void ProgrammerController::slotDocModeChanged(Doc::Mode mode)
{
    Q_UNUSED(mode);
    // Centre the joystick on any mode switch: a deflection left over from Edit
    // (where the stick is read but the integrator is disabled) would otherwise
    // immediately walk the beam off the target the instant Run starts. The next
    // real stick event re-reads the live position.
    m_panNorm  = 0.5f;
    m_tiltNorm = 0.5f;
    // Push the centred value into the effect-script "joystick" data channel too
    // (followspot.js reads its velocity from there), else it keeps the stale
    // deflection and drifts.
    emit joystickUpdated(m_panNorm, m_tiltNorm);

    m_stageAimValid = false;
    // Start the aim/pin on the scene's target so on entering Run the beam is
    // already on the target and the pin shows there — no jump, no need to nudge
    // the stick first.
    seedStageAimFromScene(m_focusedSceneId);
}

void ProgrammerController::slotRigPropsChanged(quint32 fid)
{
    Q_UNUSED(fid);
    // Mounting / pan-zero facing / truss changed.  Clear the fader map of the
    // focused (previewed/operate) scene and any running scenes so their Aim looks
    // re-resolve against the new geometry on the next tick — otherwise the change
    // only shows after a scene restart ("I changed it and nothing happened").
    auto reaim = [this](quint32 sceneId) {
        if (sceneId == Function::invalidId()) return;
        if (Scene *s = qobject_cast<Scene*>(m_doc->function(sceneId)))
            s->requestReaim();   // position-only; no LED flash
    };
    reaim(m_focusedSceneId);
    QList<quint32> running;
    {
        QMutexLocker locker(&m_stateMutex);
        running = m_runningScenes;
    }
    for (quint32 sid : running)
        if (sid != m_focusedSceneId)
            reaim(sid);
}

bool ProgrammerController::isShowLocked() const
{
    return m_showLocked;
}

void ProgrammerController::setShowLocked(bool locked)
{
    if (m_showLocked == locked)
        return;
    m_showLocked = locked;
    emit showLockedChanged(locked);
}

quint32 ProgrammerController::saveBucketAsScene(const Doc::SaveBucket &bucket,
                               const QString &name, const QString &path)
{
    if (bucket.values.isEmpty())
        return Function::invalidId();
    Scene *scene = new Scene(m_doc);
    scene->setName(name.isEmpty() ? bucket.defaultName : name);
    if (!path.isEmpty())
        scene->setPath(path);
    for (auto fxIt = bucket.values.constBegin();
         fxIt != bucket.values.constEnd(); ++fxIt)
    {
        for (auto chIt = fxIt.value().constBegin();
             chIt != fxIt.value().constEnd(); ++chIt)
        {
            scene->setValue(fxIt.key(), chIt.key(), chIt.value());
        }
    }
    if (!m_doc->addFunction(scene))
    {
        delete scene;
        return Function::invalidId();
    }
    m_doc->setModified();
    return scene->id();
}

QLCPalette *ProgrammerController::buildPaletteFromBucket(const Doc::SaveBucket &bucket) const
{
    switch (bucket.category)
    {
    case Doc::SaveCatColor:
    {
        // Average RGB + W/A/UV across the bucket's fixtures.
        int rs = 0, gs = 0, bs = 0, ws = 0, as = 0, us = 0;
        int rn = 0, gn = 0, bn = 0, wn = 0, an = 0, un = 0;
        for (auto fxIt = bucket.values.constBegin();
             fxIt != bucket.values.constEnd(); ++fxIt)
        {
            Fixture *fxi = m_doc->fixture(fxIt.key());
            if (fxi == NULL)
                continue;
            for (auto chIt = fxIt.value().constBegin();
                 chIt != fxIt.value().constEnd(); ++chIt)
            {
                const QLCChannel *qch = fxi->channel(chIt.key());
                if (qch == NULL || qch->group() != QLCChannel::Intensity)
                    continue;
                switch (qch->colour())
                {
                case QLCChannel::Red:   rs += chIt.value(); ++rn; break;
                case QLCChannel::Green: gs += chIt.value(); ++gn; break;
                case QLCChannel::Blue:  bs += chIt.value(); ++bn; break;
                case QLCChannel::White: ws += chIt.value(); ++wn; break;
                case QLCChannel::Amber: as += chIt.value(); ++an; break;
                case QLCChannel::UV:    us += chIt.value(); ++un; break;
                default: break;
                }
            }
        }
        // Need at least one mixable colour component to be a Color palette.
        if (rn + gn + bn + wn + an + un == 0)
            return NULL;
        QColor rgb(rn ? rs / rn : 0, gn ? gs / gn : 0, bn ? bs / bn : 0);
        QColor wauv(wn ? ws / wn : 0, an ? as / an : 0, un ? us / un : 0);
        QLCPalette *p = new QLCPalette(QLCPalette::Color);
        p->setValue(QLCPalette::colorToString(rgb, wauv));
        return p;
    }

    case Doc::SaveCatIntensity:
    {
        int sum = 0, n = 0;
        for (auto fxIt = bucket.values.constBegin();
             fxIt != bucket.values.constEnd(); ++fxIt)
            for (auto chIt = fxIt.value().constBegin();
                 chIt != fxIt.value().constEnd(); ++chIt)
            { sum += chIt.value(); ++n; }
        if (n == 0)
            return NULL;
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer);
        p->setValue(sum / n);
        return p;
    }

    case Doc::SaveCatPosition:
    {
        // Palettes store degrees; convert a representative fixture's
        // captured Pan/Tilt DMX back to degrees via its physical range.
        for (auto fxIt = bucket.values.constBegin();
             fxIt != bucket.values.constEnd(); ++fxIt)
        {
            Fixture *fxi = m_doc->fixture(fxIt.key());
            if (fxi == NULL || fxi->fixtureMode() == NULL)
                continue;
            const QLCPhysical phy = fxi->fixtureMode()->physical();
            qreal panMax = phy.focusPanMax();  if (panMax == 0) panMax = 360;
            qreal tiltMax = phy.focusTiltMax(); if (tiltMax == 0) tiltMax = 270;

            int panMSB = -1, panLSB = -1, tiltMSB = -1, tiltLSB = -1;
            for (auto chIt = fxIt.value().constBegin();
                 chIt != fxIt.value().constEnd(); ++chIt)
            {
                const QLCChannel *qch = fxi->channel(chIt.key());
                if (qch == NULL)
                    continue;
                const bool msb = (qch->controlByte() == QLCChannel::MSB);
                if (qch->group() == QLCChannel::Pan)
                    (msb ? panMSB : panLSB) = chIt.value();
                else if (qch->group() == QLCChannel::Tilt)
                    (msb ? tiltMSB : tiltLSB) = chIt.value();
            }
            if (panMSB < 0 && tiltMSB < 0)
                continue;

            const int panDmx16 = (qMax(panMSB, 0) << 8) | qMax(panLSB, 0);
            const int tiltDmx16 = (qMax(tiltMSB, 0) << 8) | qMax(tiltLSB, 0);
            const int panDeg = int(panDmx16 * panMax / 65535.0 + 0.5);
            const int tiltDeg = int(tiltDmx16 * tiltMax / 65535.0 + 0.5);
            QLCPalette *p = new QLCPalette(QLCPalette::PanTilt);
            p->setValue(panDeg, tiltDeg);
            return p;
        }
        return NULL;
    }

    case Doc::SaveCatSpecial:
    {
        // Only a *uniform* sub-type can be one palette. Detect whether
        // every channel is Gobo, or every channel is Shutter.
        bool allGobo = true, allShutter = true, any = false;
        int repValue = 0;
        for (auto fxIt = bucket.values.constBegin();
             fxIt != bucket.values.constEnd(); ++fxIt)
        {
            Fixture *fxi = m_doc->fixture(fxIt.key());
            if (fxi == NULL)
                continue;
            for (auto chIt = fxIt.value().constBegin();
                 chIt != fxIt.value().constEnd(); ++chIt)
            {
                const QLCChannel *qch = fxi->channel(chIt.key());
                if (qch == NULL)
                    continue;
                any = true;
                repValue = chIt.value();
                if (qch->group() != QLCChannel::Gobo)    allGobo = false;
                if (qch->group() != QLCChannel::Shutter) allShutter = false;
            }
        }
        if (!any || (!allGobo && !allShutter))
            return NULL;
        QLCPalette *p = new QLCPalette(allGobo ? QLCPalette::Gobo
                                               : QLCPalette::Shutter);
        p->setValue(repValue);
        return p;
    }

    default:
        return NULL;
    }
}

quint32 ProgrammerController::saveBucketAsGroupScene(const Doc::SaveBucket &bucket,
                                    const QString &name, const QString &path)
{
    if (bucket.values.isEmpty()
        || bucket.fixtureGroupId == Function::invalidId()
        || m_doc->fixtureGroup(bucket.fixtureGroupId) == NULL)
        return Function::invalidId();

    QLCPalette *palette = buildPaletteFromBucket(bucket);

    Scene *scene = new Scene(m_doc);
    scene->setName(name.isEmpty() ? bucket.defaultName : name);
    if (!path.isEmpty())
        scene->setPath(path);

    // (fid << 32 | ch) keys for channels the palette already drives, so
    // the per-fixture fallback below doesn't double-write them.
    auto key = [](quint32 fid, quint32 ch) -> quint64 {
        return (quint64(fid) << 32) | quint64(ch);
    };
    QSet<quint64> covered;

    if (palette != NULL)
    {
        if (m_doc->addPalette(palette))
        {
            scene->addPalette(palette->id());
            scene->addFixtureGroup(bucket.fixtureGroupId);
            const QList<SceneValue> driven =
                palette->valuesFromFixtureGroups(m_doc, { bucket.fixtureGroupId });
            for (const SceneValue &sv : driven)
                covered.insert(key(sv.fxi, sv.channel));
        }
        else
        {
            delete palette;
            palette = NULL;
        }
    }

    // Per-fixture fallback: bake every bucket channel the palette didn't
    // cover (e.g. colour-wheel channels under a Color palette, or whole
    // fixtures of a type the palette can't drive).
    bool baked = false;
    for (auto fxIt = bucket.values.constBegin();
         fxIt != bucket.values.constEnd(); ++fxIt)
    {
        for (auto chIt = fxIt.value().constBegin();
             chIt != fxIt.value().constEnd(); ++chIt)
        {
            if (covered.contains(key(fxIt.key(), chIt.key())))
                continue;
            scene->setValue(fxIt.key(), chIt.key(), chIt.value());
            baked = true;
        }
    }

    if (palette == NULL && !baked)
    {
        delete scene;
        return Function::invalidId();
    }

    if (!m_doc->addFunction(scene))
    {
        delete scene;
        if (palette != NULL)
            m_doc->deletePalette(palette->id());
        return Function::invalidId();
    }
    m_doc->setModified();
    return scene->id();
}

Doc::PadMode ProgrammerController::padMode() const
{
    return m_padMode;
}

void ProgrammerController::setPadMode(Doc::PadMode mode)
{
    if (m_padMode == mode)
        return;
    m_padMode = mode;
    emit padModeChanged(mode);
}

quint32 ProgrammerController::activeProgrammerGroup() const
{
    const QList<quint32> selection = programmerSelection();   // locked copy
    if (selection.isEmpty())
        return Function::invalidId();
    QSet<quint32> selSet(selection.begin(), selection.end());
    for (FixtureGroup *g : m_doc->fixtureGroups())
    {
        if (g == NULL)
            continue;
        const QList<quint32> flist = g->fixtureList();
        if (flist.size() != selSet.size())
            continue;
        QSet<quint32> gSet(flist.begin(), flist.end());
        if (gSet == selSet)
            return g->id();
    }
    return Function::invalidId();
}

QSet<quint32> ProgrammerController::programmerSubSelection() const
{
    // Read from the DMX thread (VCSlider::writeDMXParameter).
    QMutexLocker locker(&m_stateMutex);
    return m_programmerSubSelection;
}

bool ProgrammerController::isInProgrammerSubSelection(quint32 fid) const
{
    QMutexLocker locker(&m_stateMutex);
    return m_programmerSubSelection.contains(fid);
}

void ProgrammerController::toggleInProgrammerSubSelection(quint32 fid)
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_programmerSubSelection.contains(fid))
            m_programmerSubSelection.remove(fid);
        else
            m_programmerSubSelection.insert(fid);
    }
    emit programmerSubSelectionChanged();
}

void ProgrammerController::clearProgrammerSubSelection()
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_programmerSubSelection.isEmpty())
            return;
        m_programmerSubSelection.clear();
    }
    emit programmerSubSelectionChanged();
}

void ProgrammerController::revertProgrammer()
{
    if (!isProgrammerDirty())
        return;

    // Snapshot the edit set under the lock, then restore outside it — the
    // restore calls into Scene/Doc and must not hold m_stateMutex.
    QHash<quint32, SceneSnapshot> snapshots;
    {
        QMutexLocker locker(&m_stateMutex);
        snapshots = m_sceneSnapshots;
        m_editedScenes.clear();
        m_sceneSnapshots.clear();
        m_programmerValues.clear();
    }

    for (auto it = snapshots.constBegin(); it != snapshots.constEnd(); ++it)
    {
        Scene *scene = qobject_cast<Scene*>(m_doc->function(it.key()));
        if (scene == NULL)
            continue;
        restoreSceneFromSnapshot(scene, it.value());
    }

    emit programmerDirtyChanged(false);
}

/*****************************************************************************
 * Look-level routing
 *****************************************************************************/

void ProgrammerController::setFocusedScene(quint32 sceneId)
{
    m_focusedSceneId = sceneId;
    m_focusedPaletteId = QLCPalette::invalidId();
    m_stageAimValid = false;

    // Entering a follow-spot scene that aims at an existing target: start the
    // beam pin AT the target so it never begins from a stale/centre position.
    seedStageAimFromScene(sceneId);
}

void ProgrammerController::seedStageAimFromScene(quint32 sceneId)
{
    // Restore any target a previous Operate run moved transiently, so the saved
    // target returns to its authored position before we (re)seed.
    restoreAim();

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    MonitorProperties *mProps = m_doc->monitorProperties();
    if (scene != NULL && mProps != NULL)
    {
        foreach (quint32 pid, scene->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (p == NULL || p->type() != QLCPalette::Aim)
                continue;
            StageTarget *tgt = mProps->stageTarget(p->stageTargetId());
            if (tgt == NULL)
                continue;

            const bool operate = (m_doc->mode() == Doc::Operate);
            // lastPosition (default): in Run, HOLD the beam where the previous
            // scene left it — move the new scene's target (transiently) to the
            // held floor position so the heads don't jump on transition.
            const bool hold = operate && m_beamHeldValid &&
                              sceneFollowspotLastPosition(scene);

            if (hold)
            {
                m_aimRestoreTgtId = p->stageTargetId();
                m_aimRestoreOrig  = tgt->position();
                m_aimRestoreValid = true;
                tgt->setPosition(QVector3D(m_stageAimX, m_stageAimY, 0.0f));
                m_stageAimValid = true;
                if (Scene *s = qobject_cast<Scene*>(m_doc->function(sceneId)))
                    s->requestReaim();
            }
            else
            {
                // snapToTarget, or first activation: seed from the scene's target.
                const QVector3D pos = tgt->position();
                m_stageAimX = pos.x();
                m_stageAimY = pos.y();
                m_stageAimValid = true;
                if (operate)
                {
                    m_aimRestoreTgtId = p->stageTargetId();
                    m_aimRestoreOrig  = pos;
                    m_aimRestoreValid = true;
                }
            }
            emit followSpotPinChanged(true, m_stageAimX, m_stageAimY);
            return;
        }
    }
    // No aim target in this scene → no pin.
    emit followSpotPinChanged(false, 0.0f, 0.0f);
}

bool ProgrammerController::sceneFollowspotLastPosition(Scene *scene) const
{
    if (!scene)
        return false;
    for (quint32 pid : scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p && p->type() == QLCPalette::Effect &&
            p->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
        {
            // followMode: 0 = lastPosition (default), 1 = snapToTarget.
            const double m = p->effectParamValues().value(QStringLiteral("followMode"), 0.0);
            return m < 0.5;
        }
    }
    return false; // no followspot effect → snap (seed straight from the target)
}

void ProgrammerController::restoreAim()
{
    if (!m_aimRestoreValid)
        return;
    MonitorProperties *mProps = m_doc->monitorProperties();
    StageTarget *tgt = mProps ? mProps->stageTarget(m_aimRestoreTgtId) : nullptr;
    if (tgt)
    {
        tgt->setPosition(m_aimRestoreOrig);
        // Re-resolve only the position so heads return to the authored aim
        // without flashing the LEDs.
        if (Scene *scene = qobject_cast<Scene*>(m_doc->function(m_focusedSceneId)))
            scene->requestReaim();
    }
    m_aimRestoreValid = false;
}

bool ProgrammerController::hasFollowSpotInput() const
{
    return m_controllerInputConnected && (m_panBound || m_tiltBound);
}

void ProgrammerController::setJoystickSensitivity(float s)
{
    m_joystickSensitivity = qBound(0.1f, s, 10.0f);
    emit joystickSensitivityChanged(m_joystickSensitivity);
}

void ProgrammerController::setFocusedPalette(quint32 paletteId)
{
    m_focusedPaletteId = paletteId;
}

QLCPalette::PaletteType ProgrammerController::channelGroupToPaletteType(int g)
{
    switch (g)
    {
    case QLCChannel::Intensity: return QLCPalette::Dimmer;
    case QLCChannel::Red:
    case QLCChannel::Green:
    case QLCChannel::Blue:
    case QLCChannel::White:
    case QLCChannel::Amber:
    case QLCChannel::UV:
    case QLCChannel::Cyan:
    case QLCChannel::Magenta:
    case QLCChannel::Yellow:
    case QLCChannel::Colour:    return QLCPalette::Color;
    case QLCChannel::Pan:       return QLCPalette::Pan;   // also accepts PanTilt
    case QLCChannel::Tilt:      return QLCPalette::Tilt;  // also accepts PanTilt
    case QLCChannel::Shutter:   return QLCPalette::Shutter;
    case QLCChannel::Gobo:      return QLCPalette::Gobo;
    case QLCChannel::Beam:      return QLCPalette::Beam;
    case QLCChannel::Effect:    return QLCPalette::Effect;
    default:                    return QLCPalette::Undefined;
    }
}

bool ProgrammerController::paletteTypeMatchesChannel(QLCPalette::PaletteType palType,
                                                     int qlcChannelGroup,
                                                     int /*groupIndex*/)
{
    QLCPalette::PaletteType wanted = channelGroupToPaletteType(qlcChannelGroup);
    if (wanted == QLCPalette::Undefined)
        return false;
    if (palType == wanted)
        return true;
    // Pan and Tilt channels can both be served by a combined PanTilt palette.
    if ((wanted == QLCPalette::Pan || wanted == QLCPalette::Tilt)
            && palType == QLCPalette::PanTilt)
        return true;
    return false;
}

int ProgrammerController::positionMaxDegrees(int qlcChannelGroup, const Fixture *ref)
{
    // Mirrors Fixture::positionToValues(), which is the consumer of the value
    // we are about to store.
    const int fallback = (qlcChannelGroup == QLCChannel::Pan) ? 360 : 270;
    if (ref == NULL || ref->fixtureMode() == NULL)
        return fallback;

    QLCPhysical phy = ref->fixtureMode()->physical();
    const int max = (qlcChannelGroup == QLCChannel::Pan)
                        ? phy.focusPanMax()
                        : phy.focusTiltMax();
    return (max > 0) ? max : fallback;
}

ProgrammerController::PaletteEditOutcome
ProgrammerController::updatePaletteForChannelType(QLCPalette *pal,
                                                   int qlcChannelGroup,
                                                   int groupIndex,
                                                   uchar rawValue,
                                                   const Fixture *ref)
{
    switch (pal->type())
    {
    case QLCPalette::Color:
    {
        // Use the composite programmer color, same as updatePaletteForEdit.
        const QColor c(m_programmerColor.red(),
                       m_programmerColor.green(),
                       m_programmerColor.blue());
        const QString nv = c.name();
        if (pal->value().toString() == nv)
            return PaletteUnchanged;
        pal->setValue(nv);
        return PaletteChanged;
    }

    case QLCPalette::Dimmer:
    case QLCPalette::Gobo:
    case QLCPalette::Shutter:
        if (pal->intValue1() == int(rawValue))
            return PaletteUnchanged;
        pal->setValue(int(rawValue));
        return PaletteChanged;

    // Pan/Tilt palettes store DEGREES, not a percentage: valuesFromFixtures()
    // hands the stored value to Fixture::positionToValues() as degrees. Scaling
    // the raw slider value to 0-100 would aim a 540-degree mover at 100 degrees
    // (~18% of travel) at full-fader, and bake that wrong number into the file.
    case QLCPalette::Pan:
    case QLCPalette::Tilt:
    {
        const int maxDeg = positionMaxDegrees(qlcChannelGroup, ref);
        const int deg = int(rawValue) * maxDeg / 255;
        if (pal->intValue1() == deg)
            return PaletteUnchanged;
        pal->setValue(deg);
        return PaletteChanged;
    }

    case QLCPalette::PanTilt:
    {
        const int maxDeg = positionMaxDegrees(qlcChannelGroup, ref);
        const int deg = int(rawValue) * maxDeg / 255;
        if (qlcChannelGroup == QLCChannel::Pan)
        {
            if (pal->intValue1() == deg)
                return PaletteUnchanged;
            pal->setValue(deg, pal->intValue2());
        }
        else // Tilt
        {
            if (pal->intValue2() == deg)
                return PaletteUnchanged;
            pal->setValue(pal->intValue1(), deg);
        }
        return PaletteChanged;
    }

    case QLCPalette::Beam:
    {
        // Beam palette stores [focus, frost, iris] as intValue1/2/3.
        // groupIndex 0 = focus (primary Beam), 1 = zoom/secondary.
        const int idx = (groupIndex <= 0) ? 0 : 1;
        QVariantList vals = pal->values();
        while (vals.size() <= idx)
            vals.append(0);
        if (vals.at(idx).toInt() == int(rawValue))
            return PaletteUnchanged;
        vals[idx] = int(rawValue);
        pal->setValues(vals);
        return PaletteChanged;
    }

    default:
        return PaletteCantDerive;
    }
}

void ProgrammerController::beginSceneEdit(Scene *scene)
{
    if (scene == NULL)
        return;

    {
        QMutexLocker locker(&m_stateMutex);
        if (m_editedScenes.contains(scene->id()))
            return;
    }

    // Built OUTSIDE the lock: these are calls out into Scene and Doc, and
    // m_stateMutex must never be held across a call that could take another
    // engine lock (see the m_stateMutex comment in the header).
    SceneSnapshot snap;
    snap.values = scene->values();
    snap.fixtures = scene->fixtures();
    snap.fixtureGroups = scene->fixtureGroups();
    snap.palettes = scene->palettes();
    foreach (quint32 pid, snap.palettes)
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (pal != NULL)
            snap.paletteValues.insert(pid, pal->values());
    }

    bool wasClean = false;
    {
        QMutexLocker locker(&m_stateMutex);
        // Re-check under the lock: another thread may have flagged this scene
        // while we were building the snapshot above.
        if (m_editedScenes.contains(scene->id()))
            return;
        wasClean = m_programmerValues.isEmpty() && m_editedScenes.isEmpty();
        m_sceneSnapshots.insert(scene->id(), snap);
        m_editedScenes.insert(scene->id());
    }
    if (wasClean)
        emit programmerDirtyChanged(true);
}

void ProgrammerController::restoreSceneFromSnapshot(Scene *scene, const SceneSnapshot &snap)
{
    if (scene == NULL)
        return;

    scene->clear();
    for (const SceneValue &sv : snap.values)
    {
        // checkHTP=false → replace semantics. HTP would otherwise refuse to
        // lower an intensity value back to its pre-edit level if the
        // post-edit one is higher.
        scene->setValue(SceneValue(sv.fxi, sv.channel, sv.value),
                        /*blind=*/false, /*checkHTP=*/false);
    }
    foreach (quint32 id, snap.fixtures)
        scene->addFixture(id);
    foreach (quint32 id, snap.fixtureGroups)
        scene->addFixtureGroup(id);
    foreach (quint32 pid, snap.palettes)
    {
        scene->addPalette(pid);
        QLCPalette *pal = m_doc->palette(pid);
        if (pal != NULL && snap.paletteValues.contains(pid))
            pal->setValues(snap.paletteValues.value(pid));
    }

    // Drop the running faders so the next tick re-initializes from the
    // now-restored values and palettes.
    scene->resetRuntime();
}

void ProgrammerController::markSceneEdited(quint32 sceneId)
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (!scene)
        return;
    // Refresh the palettes into the LIVE faders rather than resetRuntime()'ing
    // the fader map. This runs on the DMX thread once per tick for the whole
    // duration of a slider drag; a teardown would rebuild every baked value and
    // re-expand every palette from scratch, 50 times a second, and restart the
    // scene's fade-in each time.
    scene->requestPaletteRefresh();
    beginSceneEdit(scene);
    m_doc->setModified();
}

quint32 ProgrammerController::routeProgrammerEditByChannelType(int qlcChannelGroup,
                                                               int groupIndex,
                                                               uchar rawValue)
{
    // Determine which palette to target.
    QLCPalette *pal = nullptr;
    quint32 owningSceneId = Function::invalidId();

    if (m_focusedPaletteId != QLCPalette::invalidId())
    {
        // User has a specific look selected → use it if the type matches.
        pal = m_doc->palette(m_focusedPaletteId);
        if (!pal || !paletteTypeMatchesChannel(pal->type(), qlcChannelGroup, groupIndex))
            return Function::invalidId();

        // Find which scene owns this palette so we can mark it edited.
        if (m_focusedSceneId != Function::invalidId())
        {
            Scene *s = qobject_cast<Scene*>(m_doc->function(m_focusedSceneId));
            if (s && s->palettes().contains(m_focusedPaletteId))
                owningSceneId = m_focusedSceneId;
        }
        // Fallback: search running scenes.
        if (owningSceneId == Function::invalidId())
        {
            QList<Scene*> candidates;
            QList<quint32> runningScenes;
            {
                QMutexLocker locker(&m_stateMutex);
                runningScenes = m_runningScenes;
            }
            for (int i = runningScenes.size() - 1; i >= 0; --i)
                collectActiveScenes(m_doc, m_doc->function(runningScenes.at(i)), candidates);
            for (Scene *s : qAsConst(candidates))
            {
                if (s->palettes().contains(m_focusedPaletteId))
                {
                    owningSceneId = s->id();
                    break;
                }
            }
        }
    }
    else if (m_focusedSceneId != Function::invalidId())
    {
        // Scene selected but no specific look → find the first palette in
        // the scene whose type matches the channel group being edited.
        Scene *scene = qobject_cast<Scene*>(m_doc->function(m_focusedSceneId));
        if (!scene)
            return Function::invalidId();
        for (quint32 pid : scene->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (p && paletteTypeMatchesChannel(p->type(), qlcChannelGroup, groupIndex))
            {
                pal = p;
                owningSceneId = m_focusedSceneId;
                break;
            }
        }
    }

    if (!pal)
        return Function::invalidId();

    // Pan/Tilt values are stored in degrees, so the conversion needs a fixture
    // to read the pan/tilt range from. Use the first fixture of the current
    // programmer selection as the reference — that is what the user is aiming.
    const Fixture *ref = nullptr;
    {
        const QList<quint32> selection = programmerSelection();
        if (!selection.isEmpty())
            ref = m_doc->fixture(selection.first());
    }

    const PaletteEditOutcome outcome =
        updatePaletteForChannelType(pal, qlcChannelGroup, groupIndex, rawValue, ref);

    if (outcome == PaletteCantDerive)
        return Function::invalidId();

    if (outcome == PaletteChanged && owningSceneId != Function::invalidId())
        markSceneEdited(owningSceneId);

    // Return a valid id so VCSlider knows the edit was taken.
    return (owningSceneId != Function::invalidId()) ? owningSceneId : pal->id();
}
