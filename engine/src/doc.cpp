/*
  Q Light Controller Plus
  doc.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <QStringList>
#include <QString>
#include <QDebug>
#include <QList>
#include <QTime>
#include <QDir>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "programmerflasher.h"

#include "monitorproperties.h"
#include "audioplugincache.h"
#include "capturemanager.h"
#include "rgbscriptscache.h"
#include "channelsgroup.h"
#include "scriptwrapper.h"
#include "collection.h"
#include "function.h"
#include "universe.h"
#include "sequence.h"
#include "fixture.h"
#include "chaser.h"
#include "chaseraction.h"
#include "scene.h"
#include "show.h"
#include "doc.h"
#include "bus.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
 #include "audiocapture_qt5.h"
#else
 #include "audiocapture_qt6.h"
#endif

#define AUTOSAVE_TIMEOUT    30 // seconds

Doc::Doc(QObject* parent, int universes)
    : QObject(parent)
    , m_workspacePath("")
    , m_fixtureDefCache(new QLCFixtureDefCache)
    , m_modifiersCache(new QLCModifiersCache)
    , m_rgbScriptsCache(new RGBScriptsCache(this))
    , m_ioPluginCache(new IOPluginCache(this))
    , m_audioPluginCache(new AudioPluginCache(this))
    , m_masterTimer(new MasterTimer(this))
    , m_captureManager(NULL)
    , m_ioMap(new InputOutputMap(this, universes))
    , m_monitorProps(NULL)
    , m_mode(Design)
    , m_kiosk(false)
    , m_loadStatus(Cleared)
    , m_clipboard(new QLCClipboard(this))
    , m_fixturesListCacheUpToDate(false)
    , m_latestFixtureId(0)
    , m_latestFixtureGroupId(0)
    , m_latestChannelsGroupId(0)
    , m_latestPaletteId(0)
    , m_latestFunctionId(0)
    , m_startupFunctionId(Function::invalidId())
{
    Bus::init(this);
    resetModified();
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QTime::currentTime().msec());
#endif
    
    m_autosaveTimer.setInterval(AUTOSAVE_TIMEOUT * 1000);
    m_autosaveTimer.setSingleShot(true);

    connect(&m_autosaveTimer, SIGNAL(timeout()), this, SIGNAL(needAutosave()));

    // Track running Scenes for programmer edit-routing (LTP).
    // DirectConnection: the signal is emitted from MasterTimer's worker
    // thread and the read side (VCSlider::writeDMXParameter →
    // routeProgrammerEdit) ALSO runs in that thread. A queued
    // connection would land the m_runningScenes update on the main
    // thread asynchronously, so a slider moved immediately after a
    // chaser/collection step would see a stale-empty list and route
    // to the new-scene bucket instead of the running scenes.
    connect(m_masterTimer, SIGNAL(functionStarted(quint32)),
            this, SLOT(slotProgrammerFunctionStarted(quint32)),
            Qt::DirectConnection);
    connect(m_masterTimer, SIGNAL(functionStopped(quint32)),
            this, SLOT(slotProgrammerFunctionStopped(quint32)),
            Qt::DirectConnection);

    // Programmer sub-selection (pad-grid fixture refinement) is
    // scoped to the current programmer selection — clear it on every
    // selection change so a leftover sub-selection from an old group
    // can't silently filter writes against a different group.
    connect(this, &Doc::programmerSelectionChanged,
            this, &Doc::clearProgrammerSubSelection);

    m_programmerFlasher = new ProgrammerFlasher(this);

    m_captureManager = new CaptureManager(this, this);
}

Doc::~Doc()
{
    delete m_masterTimer;
    m_masterTimer = NULL;

    clearContents();

    if (isKiosk() == false)
    {
        // TODO: is this still needed ??
        //m_ioMap->saveDefaults();
    }
    delete m_ioMap;
    m_ioMap = NULL;

    delete m_ioPluginCache;
    m_ioPluginCache = NULL;

    delete m_modifiersCache;
    m_modifiersCache = NULL;

    delete m_fixtureDefCache;
    m_fixtureDefCache = NULL;

    delete m_rgbScriptsCache;
    m_rgbScriptsCache = NULL;
}

void Doc::clearContents()
{
    emit clearing();

    m_clipboard->resetContents();

    if (m_monitorProps != NULL)
        m_monitorProps->reset();

    destroyAudioCapture();

    // Delete all function instances
    QListIterator <quint32> funcit(m_functions.keys());
    while (funcit.hasNext() == true)
    {
        Function* func = m_functions.take(funcit.next());
        if (func == NULL)
            continue;
        emit functionRemoved(func->id());
        delete func;
    }

    // Delete all palettes
    QListIterator <quint32> palIt(m_palettes.keys());
    while (palIt.hasNext() == true)
    {
        QLCPalette *palette = m_palettes.take(palIt.next());
        emit paletteRemoved(palette->id());
        delete palette;
    }

    // Delete all channel groups
    QListIterator <quint32> grpchans(m_channelsGroups.keys());
    while (grpchans.hasNext() == true)
    {
        ChannelsGroup* grp = m_channelsGroups.take(grpchans.next());
        emit channelsGroupRemoved(grp->id());
        delete grp;
    }

    // Delete all fixture groups
    QListIterator <quint32> grpit(m_fixtureGroups.keys());
    while (grpit.hasNext() == true)
    {
        FixtureGroup* grp = m_fixtureGroups.take(grpit.next());
        quint32 grpID = grp->id();
        delete grp;
        emit fixtureGroupRemoved(grpID);
    }

    // Delete all fixture instances
    QListIterator <quint32> fxit(m_fixtures.keys());
    while (fxit.hasNext() == true)
    {
        Fixture* fxi = m_fixtures.take(fxit.next());
        quint32 fxID = fxi->id();
        delete fxi;
        emit fixtureRemoved(fxID);
    }
    m_fixturesListCacheUpToDate = false;

    m_orderedGroups.clear();

    m_latestFunctionId = 0;
    m_latestFixtureId = 0;
    m_latestFixtureGroupId = 0;
    m_latestChannelsGroupId = 0;
    m_latestPaletteId = 0;
    m_addresses.clear();
    m_loadStatus = Cleared;

    if (!m_programmerSelection.isEmpty())
    {
        m_programmerSelection.clear();
        m_programmerSelectionLookup.clear();
        emit programmerSelectionChanged();
    }

    emit cleared();
}

void Doc::setWorkspacePath(QString path)
{
    m_workspacePath = path;
}

QString Doc::workspacePath() const
{
    return m_workspacePath;
}

QString Doc::normalizeComponentPath(const QString& filePath) const
{
    if (filePath.isEmpty())
        return filePath;

    QFileInfo f(filePath);

    if (f.absolutePath().startsWith(workspacePath()))
    {
        return QDir(workspacePath()).relativeFilePath(f.absoluteFilePath());
    }
    else
    {
        return f.absoluteFilePath();
    }
}

QString Doc::denormalizeComponentPath(const QString& filePath) const
{
    if (filePath.isEmpty())
        return filePath;

    return QFileInfo(QDir(workspacePath()), filePath).absoluteFilePath();
}

/*****************************************************************************
 * Engine components
 *****************************************************************************/

QLCFixtureDefCache* Doc::fixtureDefCache() const
{
    return m_fixtureDefCache;
}

void Doc::setFixtureDefinitionCache(QLCFixtureDefCache *cache)
{
    m_fixtureDefCache = cache;
}

QLCModifiersCache* Doc::modifiersCache() const
{
    return m_modifiersCache;
}

RGBScriptsCache* Doc::rgbScriptsCache() const
{
    return m_rgbScriptsCache;
}

IOPluginCache* Doc::ioPluginCache() const
{
    return m_ioPluginCache;
}

AudioPluginCache *Doc::audioPluginCache() const
{
    return m_audioPluginCache;
}

InputOutputMap* Doc::inputOutputMap() const
{
    return m_ioMap;
}

MasterTimer* Doc::masterTimer() const
{
    return m_masterTimer;
}

CaptureManager* Doc::captureManager() const
{
    return m_captureManager;
}

/*****************************************************************************
 * Programmer selection
 *****************************************************************************/

QList<quint32> Doc::programmerSelection() const
{
    return m_programmerSelection;
}

void Doc::setProgrammerSelection(const QList<quint32>& fixtureIds)
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
    if (deduped == m_programmerSelection)
        return;
    m_programmerSelection = deduped;
    m_programmerSelectionLookup = seen;
    emit programmerSelectionChanged();
}

void Doc::addToProgrammerSelection(const QList<quint32>& fixtureIds)
{
    bool changed = false;
    for (quint32 fid : fixtureIds)
    {
        if (m_programmerSelectionLookup.contains(fid))
            continue;
        m_programmerSelection.append(fid);
        m_programmerSelectionLookup.insert(fid);
        changed = true;
    }
    if (changed)
        emit programmerSelectionChanged();
}

void Doc::removeFromProgrammerSelection(const QList<quint32>& fixtureIds)
{
    bool changed = false;
    for (quint32 fid : fixtureIds)
    {
        if (!m_programmerSelectionLookup.contains(fid))
            continue;
        m_programmerSelection.removeAll(fid);
        m_programmerSelectionLookup.remove(fid);
        changed = true;
    }
    if (changed)
        emit programmerSelectionChanged();
}

void Doc::toggleInProgrammerSelection(const QList<quint32>& fixtureIds)
{
    bool changed = false;
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
    if (changed)
        emit programmerSelectionChanged();
}

void Doc::clearProgrammerSelection()
{
    if (m_programmerSelection.isEmpty())
        return;
    m_programmerSelection.clear();
    m_programmerSelectionLookup.clear();
    emit programmerSelectionChanged();
}

bool Doc::isInProgrammerSelection(quint32 fixtureId) const
{
    return m_programmerSelectionLookup.contains(fixtureId);
}

bool Doc::allInProgrammerSelection(const QList<quint32>& fixtureIds) const
{
    if (fixtureIds.isEmpty())
        return false;
    for (quint32 fid : fixtureIds)
    {
        if (!m_programmerSelectionLookup.contains(fid))
            return false;
    }
    return true;
}

QColor Doc::programmerColor() const
{
    return m_programmerColor;
}

void Doc::setProgrammerColorComponent(int qlcPrimaryColour, uchar value)
{
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

void Doc::setProgrammerValue(quint32 fixtureId, quint32 channel, uchar value)
{
    const bool wasEmpty = m_programmerValues.isEmpty();
    m_programmerValues[fixtureId][channel] = value;
    if (wasEmpty)
        emit programmerDirtyChanged(true);
}

void Doc::clearProgrammerValues()
{
    if (!isProgrammerDirty())
        return;
    // Caller (Save) has persisted everything elsewhere — drop the
    // values map AND the in-memory edited-scenes tracking. Snapshots
    // are no longer needed because the post-save scenes ARE the new
    // baseline.
    m_programmerValues.clear();
    m_editedScenes.clear();
    m_sceneSnapshots.clear();
    emit programmerDirtyChanged(false);
}

bool Doc::isProgrammerDirty() const
{
    return !m_programmerValues.isEmpty() || !m_editedScenes.isEmpty();
}

quint32 Doc::saveProgrammerAsScene(const QString &name)
{
    if (m_programmerValues.isEmpty())
        return Function::invalidId();

    Scene *scene = new Scene(this);
    scene->setName(name.isEmpty() ? tr("Programmer scene") : name);

    for (auto fixIt = m_programmerValues.constBegin();
         fixIt != m_programmerValues.constEnd(); ++fixIt)
    {
        const quint32 fid = fixIt.key();
        const QHash<quint32, uchar> &channels = fixIt.value();
        for (auto chIt = channels.constBegin();
             chIt != channels.constEnd(); ++chIt)
        {
            scene->setValue(fid, chIt.key(), chIt.value());
        }
    }

    if (!addFunction(scene))
    {
        delete scene;
        return Function::invalidId();
    }
    setModified();
    return scene->id();
}

void Doc::slotProgrammerFunctionStarted(quint32 fid)
{
    Function *f = function(fid);
    if (f == NULL)
        return;
    if (f->type() == Function::SceneType)
    {
        // Re-running an already-tracked scene: bump it to most-recent.
        m_runningScenes.removeAll(fid);
        m_runningScenes.append(fid);
    }
    else if (f->type() == Function::ChaserType)
    {
        m_runningChasers.removeAll(fid);
        m_runningChasers.append(fid);
    }
    else if (f->type() == Function::CollectionType)
    {
        m_runningCollections.removeAll(fid);
        m_runningCollections.append(fid);
    }
}

void Doc::slotProgrammerFunctionStopped(quint32 fid)
{
    m_runningScenes.removeAll(fid);
    m_runningChasers.removeAll(fid);
    m_runningCollections.removeAll(fid);
}

quint32 Doc::routeProgrammerEdit(quint32 fid, quint32 ch, uchar value)
{
    // LTP: walk newest → oldest and pick the first running Scene that
    // already has this (fid, ch) set.
    for (int i = m_runningScenes.size() - 1; i >= 0; --i)
    {
        const quint32 sid = m_runningScenes.at(i);
        Function *f = function(sid);
        if (f == NULL || f->type() != Function::SceneType)
            continue;
        Scene *scene = qobject_cast<Scene*>(f);
        if (scene == NULL)
            continue;
        SceneValue probe(fid, ch);
        if (!scene->checkValue(probe))
            continue;

        // First time this scene becomes edited this round: snapshot
        // its current values so Revert can restore them.
        if (!m_editedScenes.contains(sid))
        {
            m_sceneSnapshots.insert(sid, scene->values());
            const bool wasClean = !isProgrammerDirty();
            m_editedScenes.insert(sid);
            if (wasClean)
                emit programmerDirtyChanged(true);
        }
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

QSet<quint32> Doc::editedSceneIds() const
{
    return m_editedScenes;
}

bool Doc::hasProgrammerValues() const
{
    return !m_programmerValues.isEmpty();
}

QHash<quint32, QHash<quint32, uchar>> Doc::programmerValues() const
{
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

QList<Doc::SaveBucket> Doc::proposedSaveBuckets() const
{
    QList<SaveBucket> out;
    if (m_programmerValues.isEmpty())
        return out;

    // Collect (fixtureGroup, category) → bucket
    QHash<QPair<quint32, SaveCategory>, SaveBucket> map;

    for (auto fxIt = m_programmerValues.constBegin();
         fxIt != m_programmerValues.constEnd(); ++fxIt)
    {
        const quint32 fid = fxIt.key();
        Fixture *fxi = fixture(fid);
        if (fxi == NULL)
            continue;

        // Pick the smallest fixture group that contains this fixture
        // (most specific). Falls back to invalidId if the fixture
        // isn't in any group.
        quint32 fgId = Function::invalidId();
        QString fgName;
        int bestSize = INT_MAX;
        QMapIterator<quint32, FixtureGroup*> grpIt(m_fixtureGroups);
        while (grpIt.hasNext())
        {
            grpIt.next();
            FixtureGroup *g = grpIt.value();
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

        for (auto chIt = fxIt.value().constBegin();
             chIt != fxIt.value().constEnd(); ++chIt)
        {
            const quint32 ch = chIt.key();
            const uchar val = chIt.value();
            const SaveCategory cat = categorizeChannel(fxi->channel(ch));

            SaveBucket &b = map[qMakePair(fgId, cat)];
            b.category = cat;
            b.fixtureGroupId = fgId;
            b.groupName = fgName;
            b.values[fid][ch] = val;
        }
    }

    // Generate default name + path per bucket.
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        SaveBucket &b = it.value();
        b.defaultPath = QStringLiteral("%1/%2")
                            .arg(categoryFolderName(b.category))
                            .arg(b.groupName);

        switch (b.category)
        {
        case SaveCatColor:
        {
            // Aggregate RGBW across the bucket's fixtures (avg).
            int rsum = 0, gsum = 0, bsum = 0, wsum = 0;
            int rN = 0, gN = 0, bN = 0, wN = 0;
            QString wheelName;
            for (auto fxIt2 = b.values.constBegin();
                 fxIt2 != b.values.constEnd(); ++fxIt2)
            {
                Fixture *fxi = fixture(fxIt2.key());
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

        case SaveCatPosition:
            b.defaultName = QStringLiteral("Pos");
            break;

        case SaveCatIntensity:
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

        case SaveCatSpecial:
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
            for (Function *fn : m_functions.values())
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

        out.append(b);
    }

    // Stable order: Position, Color, Special, Intensity by group name.
    std::sort(out.begin(), out.end(),
              [](const SaveBucket &a, const SaveBucket &c) {
                  if (a.category != c.category)
                      return a.category < c.category;
                  return a.groupName < c.groupName;
              });
    return out;
}

void Doc::stepCurrentChaser(int direction)
{
    if (m_runningChasers.isEmpty() || direction == 0)
        return;
    const quint32 cid = m_runningChasers.last();
    Chaser *chaser = qobject_cast<Chaser*>(function(cid));
    if (chaser == NULL)
        return;
    ChaserAction action;
    action.m_action = (direction > 0) ? ChaserNextStep : ChaserPreviousStep;
    chaser->setAction(action);
}

quint32 Doc::findMatchingScene(
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

    QMapIterator<quint32, Function*> it(m_functions);
    while (it.hasNext())
    {
        it.next();
        Function *fn = it.value();
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

QList<quint32> Doc::collectionsContaining(quint32 sceneId) const
{
    QList<quint32> out;
    QMapIterator<quint32, Function*> it(m_functions);
    while (it.hasNext())
    {
        it.next();
        Function *fn = it.value();
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

bool Doc::replaceSceneInCollection(quint32 collectionId,
                                   quint32 oldSceneId,
                                   quint32 newSceneId)
{
    Collection *coll = qobject_cast<Collection*>(function(collectionId));
    if (coll == NULL)
        return false;
    const QList<quint32> children = coll->functions();
    const int idx = children.indexOf(oldSceneId);
    if (idx < 0)
        return false;
    coll->removeFunction(oldSceneId);
    coll->addFunction(newSceneId, idx);
    setModified();
    return true;
}

void Doc::revertSceneFromSnapshot(quint32 sceneId)
{
    if (!m_sceneSnapshots.contains(sceneId))
        return;
    Scene *scene = qobject_cast<Scene*>(function(sceneId));
    if (scene == NULL)
        return;
    scene->clear();
    for (const SceneValue &sv : m_sceneSnapshots.value(sceneId))
    {
        scene->setValue(SceneValue(sv.fxi, sv.channel, sv.value),
                        /*blind=*/false, /*checkHTP=*/false);
    }
    scene->resetRuntime();
    m_sceneSnapshots.remove(sceneId);
    m_editedScenes.remove(sceneId);
    if (!isProgrammerDirty())
        emit programmerDirtyChanged(false);
}

quint32 Doc::singleRunningCollection() const
{
    if (m_runningCollections.size() == 1)
        return m_runningCollections.first();
    return Function::invalidId();
}

void Doc::flashFixture(quint32 fixtureId, int durationMs)
{
    if (m_programmerFlasher != nullptr)
        m_programmerFlasher->flashFixture(fixtureId, durationMs);
}

bool Doc::isShowLocked() const
{
    return m_showLocked;
}

void Doc::setShowLocked(bool locked)
{
    if (m_showLocked == locked)
        return;
    m_showLocked = locked;
    emit showLockedChanged(locked);
}

QString Doc::nextDuplicateName(const Function *src) const
{
    if (src == NULL)
        return QString();
    const QString name = src->name();
    const QString path = src->path();

    auto isUnique = [this, &path](const QString &candidate) {
        for (Function *fn : m_functions.values())
        {
            if (fn != NULL && fn->name() == candidate && fn->path() == path)
                return false;
        }
        return true;
    };

    // Locate the rightmost run of digits in the source name.
    QRegularExpression re(QStringLiteral("(\\d+)"));
    QRegularExpressionMatchIterator it = re.globalMatch(name);
    QRegularExpressionMatch lastMatch;
    while (it.hasNext())
        lastMatch = it.next();

    if (lastMatch.hasMatch())
    {
        const int width = lastMatch.captured(1).length();
        const int base = lastMatch.captured(1).toInt();
        const QString prefix = name.left(lastMatch.capturedStart(1));
        const QString suffix = name.mid(lastMatch.capturedEnd(1));
        // Try base+1, base+2, ... until we find one that's not taken.
        for (int bump = 1; bump < 1000; ++bump)
        {
            const QString numStr = QString::number(base + bump)
                                       .rightJustified(width, QChar('0'));
            const QString candidate = prefix + numStr + suffix;
            if (isUnique(candidate))
                return candidate;
        }
    }

    // Fallback: append " 2", " 3", … (no digits found, or 1000 bumps
    // exhausted — both very unlikely).
    for (int n = 2; n < 1000; ++n)
    {
        const QString candidate = QStringLiteral("%1 %2").arg(name).arg(n);
        if (isUnique(candidate))
            return candidate;
    }
    return name + QStringLiteral(" (Copy)");
}

quint32 Doc::saveBucketAsScene(const SaveBucket &bucket,
                               const QString &name, const QString &path)
{
    if (bucket.values.isEmpty())
        return Function::invalidId();
    Scene *scene = new Scene(this);
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
    if (!addFunction(scene))
    {
        delete scene;
        return Function::invalidId();
    }
    setModified();
    return scene->id();
}

Doc::PadMode Doc::padMode() const
{
    return m_padMode;
}

void Doc::setPadMode(PadMode mode)
{
    if (m_padMode == mode)
        return;
    m_padMode = mode;
    emit padModeChanged(mode);
}

quint32 Doc::activeProgrammerGroup() const
{
    if (m_programmerSelection.isEmpty())
        return Function::invalidId();
    QSet<quint32> selSet(m_programmerSelection.begin(),
                         m_programmerSelection.end());
    QMapIterator<quint32, FixtureGroup*> it(m_fixtureGroups);
    while (it.hasNext())
    {
        it.next();
        FixtureGroup *g = it.value();
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

QSet<quint32> Doc::programmerSubSelection() const
{
    return m_programmerSubSelection;
}

bool Doc::isInProgrammerSubSelection(quint32 fid) const
{
    return m_programmerSubSelection.contains(fid);
}

void Doc::toggleInProgrammerSubSelection(quint32 fid)
{
    if (m_programmerSubSelection.contains(fid))
        m_programmerSubSelection.remove(fid);
    else
        m_programmerSubSelection.insert(fid);
    emit programmerSubSelectionChanged();
}

void Doc::clearProgrammerSubSelection()
{
    if (m_programmerSubSelection.isEmpty())
        return;
    m_programmerSubSelection.clear();
    emit programmerSubSelectionChanged();
}

void Doc::revertProgrammer()
{
    if (!isProgrammerDirty())
        return;

    // Restore each edited scene from its pre-edit snapshot.
    for (const quint32 sid : qAsConst(m_editedScenes))
    {
        Scene *scene = qobject_cast<Scene*>(function(sid));
        if (scene == NULL)
            continue;
        scene->clear();
        const QList<SceneValue> &snap = m_sceneSnapshots.value(sid);
        for (const SceneValue &sv : snap)
        {
            // checkHTP=false → replace semantics. HTP would otherwise
            // refuse to lower an intensity value back to its pre-edit
            // level if the post-edit one is higher.
            scene->setValue(SceneValue(sv.fxi, sv.channel, sv.value),
                            /*blind=*/false, /*checkHTP=*/false);
        }
        // Drop the running faders so the next tick re-initializes
        // from the now-restored m_values. Without this, FadeChannels
        // added during the edit (channels not in the original scene)
        // would keep asserting their post-edit values.
        scene->resetRuntime();
    }

    m_editedScenes.clear();
    m_sceneSnapshots.clear();
    m_programmerValues.clear();
    emit programmerDirtyChanged(false);
}

QSharedPointer<AudioCapture> Doc::audioInputCapture() const
{
    if (!m_inputCapture)
    {
        qDebug() << "Creating new audio capture";
        m_inputCapture = QSharedPointer<AudioCapture>(
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            new AudioCaptureQt5()
#else
            new AudioCaptureQt6()
#endif
            );
    }
    return m_inputCapture;
}

void Doc::destroyAudioCapture()
{
    if (m_inputCapture.isNull() == false)
    {
        qDebug() << "Destroying audio capture";
        m_inputCapture.clear();
    }
}

/*****************************************************************************
 * Modified status
 *****************************************************************************/
Doc::LoadStatus Doc::loadStatus() const
{
    return m_loadStatus;
}

bool Doc::isModified() const
{
    return m_modified;
}

void Doc::setModified()
{
    m_modified = true;
    m_autosaveTimer.start();
    emit modified(true);
}

void Doc::resetModified()
{
    m_modified = false;
    m_autosaveTimer.stop();
    emit modified(false);
}

/*****************************************************************************
 * Main operating mode
 *****************************************************************************/

void Doc::setMode(Doc::Mode mode)
{
    /* Don't do mode switching twice */
    if (m_mode == mode)
        return;

    m_mode = mode;

    if (mode == Operate)
        m_autosaveTimer.stop();
    else if (m_modified)
        m_autosaveTimer.start();

    // Run startup function
    if (m_mode == Operate && m_startupFunctionId != Function::invalidId())
    {
        Function *func = function(m_startupFunctionId);
        if (func != NULL)
        {
            qDebug() << Q_FUNC_INFO << "Starting startup function. (" << m_startupFunctionId << ")";
            func->start(masterTimer(), FunctionParent::master());
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Startup function does not exist, erasing. (" << m_startupFunctionId << ")";
            m_startupFunctionId = Function::invalidId();
        }
    }

    emit modeChanged(m_mode);
}

Doc::Mode Doc::mode() const
{
    return m_mode;
}

void Doc::setKiosk(bool state)
{
    m_kiosk = state;
}

bool Doc::isKiosk() const
{
    return m_kiosk;
}

/*********************************************************************
 * Clipboard
 *********************************************************************/

QLCClipboard *Doc::clipboard()
{
    return m_clipboard;
}

/*****************************************************************************
 * Fixtures
 *****************************************************************************/

quint32 Doc::createFixtureId()
{
    /* This results in an endless loop if there are UINT_MAX-1 fixtures. That,
       however, seems a bit unlikely. Are there even 4294967295-1 fixtures in
       total in the whole world? */
    while (m_fixtures.contains(m_latestFixtureId) == true ||
           m_latestFixtureId == Fixture::invalidId())
    {
        m_latestFixtureId++;
    }

    return m_latestFixtureId;
}

bool Doc::addFixture(Fixture* fixture, quint32 id, bool crossUniverse)
{
    Q_ASSERT(fixture != NULL);

    quint32 i;
    quint32 uni = fixture->universe();

    // No ID given, this method can assign one
    if (id == Fixture::invalidId())
        id = createFixtureId();

    if (m_fixtures.contains(id) == true || id == Fixture::invalidId())
    {
        qWarning() << Q_FUNC_INFO << "a fixture with ID" << id << "already exists!";
        return false;
    }

    /* Check for overlapping address */
    for (i = fixture->universeAddress();
         i < fixture->universeAddress() + fixture->channels(); i++)
    {
        if (m_addresses.contains(i))
        {
            qWarning() << Q_FUNC_INFO << "fixture" << id << "overlapping with fixture" << m_addresses[i] << "@ channel" << i;
            return false;
        }
    }

    fixture->setID(id);
    m_fixtures.insert(id, fixture);
    m_fixturesListCacheUpToDate = false;

    /* Patch fixture change signals thru Doc */
    connect(fixture, SIGNAL(changed(quint32)),
            this, SLOT(slotFixtureChanged(quint32)));

    /* Keep track of fixture addresses */
    for (i = fixture->universeAddress();
         i < fixture->universeAddress() + fixture->channels(); i++)
    {
        m_addresses[i] = id;
    }

    if (crossUniverse)
        uni = floor((fixture->universeAddress() + fixture->channels()) / 512);

    if (uni >= inputOutputMap()->universesCount())
    {
        for (i = inputOutputMap()->universesCount(); i <= uni; i++)
            inputOutputMap()->addUniverse(i);
        inputOutputMap()->startUniverses();
    }

    // Add the fixture channels capabilities to the universe they belong
    QList<Universe *> universes = inputOutputMap()->claimUniverses();

    QList<int> forcedHTP = fixture->forcedHTPChannels();
    QList<int> forcedLTP = fixture->forcedLTPChannels();
    quint32 fxAddress = fixture->address();

    for (i = 0; i < fixture->channels(); i++)
    {
        const QLCChannel *channel(fixture->channel(i));
        quint32 addr = fxAddress + i;

        if (crossUniverse)
        {
            uni = floor((fixture->universeAddress() + i) / 512);
            addr = (fixture->universeAddress() + i) - (uni * 512);
        }

        // Inform Universe of any HTP/LTP forcing
        if (forcedHTP.contains(int(i)))
            universes.at(uni)->setChannelCapability(addr, channel->group(), Universe::HTP);
        else if (forcedLTP.contains(int(i)))
            universes.at(uni)->setChannelCapability(addr, channel->group(), Universe::LTP);
        else
            universes.at(uni)->setChannelCapability(addr, channel->group());

        // Apply the default value BEFORE modifiers
        universes.at(uni)->setChannelDefaultValue(addr, channel->defaultValue());

        // Apply a channel modifier, if defined
        ChannelModifier *mod = fixture->channelModifier(i);
        universes.at(uni)->setChannelModifier(addr, mod);
    }
    inputOutputMap()->releaseUniverses(true);

    emit fixtureAdded(id);
    setModified();

    return true;
}

bool Doc::deleteFixture(quint32 id)
{
    if (m_fixtures.contains(id) == true)
    {
        Fixture* fxi = m_fixtures.take(id);
        Q_ASSERT(fxi != NULL);
        m_fixturesListCacheUpToDate = false;

        /* Keep track of fixture addresses */
        QMutableHashIterator <uint,uint> it(m_addresses);
        while (it.hasNext() == true)
        {
            it.next();
            if (it.value() == id)
                it.remove();
        }
        if (m_monitorProps != NULL)
            m_monitorProps->removeFixture(id);

        if (m_programmerSelectionLookup.contains(id))
        {
            m_programmerSelection.removeAll(id);
            m_programmerSelectionLookup.remove(id);
            emit programmerSelectionChanged();
        }

        emit fixtureRemoved(id);
        setModified();
        delete fxi;

        if (m_fixtures.count() == 0)
            m_latestFixtureId = 0;

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No fixture with id" << id;
        return false;
    }
}

bool Doc::replaceFixtures(QList<Fixture*> newFixturesList)
{
    // Delete all fixture instances
    QListIterator <quint32> fxit(m_fixtures.keys());
    while (fxit.hasNext() == true)
    {
        Fixture* fxi = m_fixtures.take(fxit.next());
        disconnect(fxi, SIGNAL(changed(quint32)),
                   this, SLOT(slotFixtureChanged(quint32)));
        delete fxi;
        m_fixturesListCacheUpToDate = false;
    }
    m_latestFixtureId = 0;
    m_addresses.clear();

    foreach (Fixture *fixture, newFixturesList)
    {
        quint32 id = fixture->id();
        // create a copy of the original cause remapping will
        // destroy it later
        Fixture *newFixture = new Fixture(this);
        newFixture->setID(id);
        newFixture->setName(fixture->name());
        newFixture->setAddress(fixture->address());
        newFixture->setUniverse(fixture->universe());

        if (fixture->fixtureDef() == NULL ||
            (fixture->fixtureDef()->manufacturer() == KXMLFixtureGeneric &&
             fixture->fixtureDef()->model() == KXMLFixtureGeneric))
        {
            // Generic dimmers just need to know the number of channels
            newFixture->setChannels(fixture->channels());
        }
        else if (fixture->fixtureDef() == NULL ||
            (fixture->fixtureDef()->manufacturer() == KXMLFixtureGeneric &&
             fixture->fixtureDef()->model() == KXMLFixtureRGBPanel))
        {
            // RGB Panels definitions are not cached or shared, so
            // let's make a deep copy of them
            QLCFixtureDef *fixtureDef = new QLCFixtureDef();
            *fixtureDef = *fixture->fixtureDef();
            QLCFixtureMode *mode = new QLCFixtureMode(fixtureDef);
            *mode = *fixture->fixtureMode();
            newFixture->setFixtureDefinition(fixtureDef, mode);
        }
        else
        {
            QLCFixtureDef *def = fixtureDefCache()->fixtureDef(fixture->fixtureDef()->manufacturer(),
                                                               fixture->fixtureDef()->model());
            QLCFixtureMode *mode = NULL;
            if (def != NULL)
                mode = def->mode(fixture->fixtureMode()->name());
            newFixture->setFixtureDefinition(def, mode);
        }

        newFixture->setExcludeFadeChannels(fixture->excludeFadeChannels());
        newFixture->setForcedHTPChannels(fixture->forcedHTPChannels());
        newFixture->setForcedLTPChannels(fixture->forcedLTPChannels());

        for (quint32 s = 0; s < fixture->channels(); s++)
        {
            ChannelModifier *chMod = fixture->channelModifier(s);
            if (chMod != NULL)
                newFixture->setChannelModifier(s, chMod);
        }

        m_fixtures.insert(id, newFixture);
        m_fixturesListCacheUpToDate = false;

        /* Patch fixture change signals thru Doc */
        connect(newFixture, SIGNAL(changed(quint32)),
                this, SLOT(slotFixtureChanged(quint32)));

        /* Keep track of fixture addresses */
        for (uint i = newFixture->universeAddress();
             i < newFixture->universeAddress() + newFixture->channels(); i++)
        {
            m_addresses[i] = id;
        }
        m_latestFixtureId = id;
    }
    return true;
}

bool Doc::updateFixtureChannelCapabilities(quint32 id, QList<int> forcedHTP, QList<int> forcedLTP)
{
    if (m_fixtures.contains(id) == false)
        return false;

    Fixture* fixture = m_fixtures[id];
    // get exclusive access to the universes list
    QList<Universe *> universes = inputOutputMap()->claimUniverses();
    Universe *universe = universes.at(fixture->universe());
    quint32 fxAddress = fixture->address();

    // Set forced HTP channels
    fixture->setForcedHTPChannels(forcedHTP);

    // Set forced LTP channels
    fixture->setForcedLTPChannels(forcedLTP);

    // Update the Fixture Universe with the current channel states
    for (quint32 i = 0 ; i < fixture->channels(); i++)
    {
        const QLCChannel *channel(fixture->channel(i));

        // Inform Universe of any HTP/LTP forcing
        if (forcedHTP.contains(int(i)))
            universe->setChannelCapability(fxAddress + i, channel->group(), Universe::HTP);
        else if (forcedLTP.contains(int(i)))
            universe->setChannelCapability(fxAddress + i, channel->group(), Universe::LTP);
        else
            universe->setChannelCapability(fxAddress + i, channel->group());

        // Apply the default value BEFORE modifiers
        universe->setChannelDefaultValue(fxAddress + i, channel->defaultValue());

        // Apply a channel modifier, if defined
        ChannelModifier *mod = fixture->channelModifier(i);
        universe->setChannelModifier(fxAddress + i, mod);
    }

    inputOutputMap()->releaseUniverses(true);

    return true;
}

QList<Fixture*> const& Doc::fixtures() const
{
    if (!m_fixturesListCacheUpToDate)
    {
        // Sort fixtures by id
        QMap <quint32, Fixture*> fixturesMap;
        QHashIterator <quint32, Fixture*> hashIt(m_fixtures);
        while (hashIt.hasNext())
        {
            hashIt.next();
            fixturesMap.insert(hashIt.key(), hashIt.value());
        }
        const_cast<QList<Fixture*>&>(m_fixturesListCache) = fixturesMap.values();
        const_cast<bool&>(m_fixturesListCacheUpToDate) = true;
    }
    return m_fixturesListCache;
}

int Doc::fixturesCount() const
{
    return m_fixtures.count();
}

Fixture* Doc::fixture(quint32 id) const
{
    return m_fixtures.value(id, NULL);
}

quint32 Doc::fixtureForAddress(quint32 universeAddress) const
{
    return m_addresses.value(universeAddress, Fixture::invalidId());
}

int Doc::totalPowerConsumption(int& fuzzy) const
{
    int totalPowerConsumption = 0;

    // Make sure fuzzy starts from zero
    fuzzy = 0;

    QListIterator <Fixture*> fxit(fixtures());
    while (fxit.hasNext() == true)
    {
        Fixture* fxi(fxit.next());
        Q_ASSERT(fxi != NULL);

        if (fxi->fixtureMode() != NULL)
        {
            QLCPhysical phys = fxi->fixtureMode()->physical();
            if (phys.powerConsumption() > 0)
                totalPowerConsumption += phys.powerConsumption();
            else
                fuzzy++;
        }
        else
        {
            fuzzy++;
        }
    }

    return totalPowerConsumption;
}

void Doc::slotFixtureChanged(quint32 id)
{
    /* Keep track of fixture addresses */
    Fixture* fxi = fixture(id);

    // remove it
    QMutableHashIterator <uint,uint> it(m_addresses);
    while (it.hasNext() == true)
    {
        it.next();
        if (it.value() == id)
        {
            qDebug() << Q_FUNC_INFO << " remove: " << it.key() << " val: " << it.value();
            it.remove();
        }
    }

    for (uint i = fxi->universeAddress(); i < fxi->universeAddress() + fxi->channels(); i++)
    {
        /*
         * setting new universe and address calls this twice,
         * with an tmp wrong address after the first call (old address() + new universe()).
         * we only add if the channel is free, to prevent messing up things
         */
        Q_ASSERT(!m_addresses.contains(i));
        m_addresses[i] = id;
    }

    setModified();
    emit fixtureChanged(id);
}

/*****************************************************************************
 * Fixture groups
 *****************************************************************************/

bool Doc::addFixtureGroup(FixtureGroup* grp, quint32 id)
{
    Q_ASSERT(grp != NULL);

    // No ID given, this method can assign one
    if (id == FixtureGroup::invalidId())
        id = createFixtureGroupId();

    if (m_fixtureGroups.contains(id) == true || id == FixtureGroup::invalidId())
    {
        qWarning() << Q_FUNC_INFO << "a fixture group with ID" << id << "already exists!";
        return false;
    }
    else
    {
        grp->setId(id);
        m_fixtureGroups[id] = grp;

        /* Patch fixture group change signals thru Doc */
        connect(grp, SIGNAL(changed(quint32)),
                this, SLOT(slotFixtureGroupChanged(quint32)));

        emit fixtureGroupAdded(id);
        setModified();

        return true;
    }
}

bool Doc::deleteFixtureGroup(quint32 id)
{
    if (m_fixtureGroups.contains(id) == true)
    {
        FixtureGroup* grp = m_fixtureGroups.take(id);
        Q_ASSERT(grp != NULL);

        emit fixtureGroupRemoved(id);
        setModified();
        delete grp;

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No fixture group with id" << id;
        return false;
    }
}

FixtureGroup* Doc::fixtureGroup(quint32 id) const
{
    return m_fixtureGroups.value(id, NULL);
}

QList <FixtureGroup*> Doc::fixtureGroups() const
{
    return m_fixtureGroups.values();
}

quint32 Doc::createFixtureGroupId()
{
    /* This results in an endless loop if there are UINT_MAX-1 fixture groups. That,
       however, seems a bit unlikely. Are there even 4294967295-1 fixtures in
       total in the whole world? */
    while (m_fixtureGroups.contains(m_latestFixtureGroupId) == true ||
           m_latestFixtureGroupId == FixtureGroup::invalidId())
    {
        m_latestFixtureGroupId++;
    }

    return m_latestFixtureGroupId;
}

void Doc::slotFixtureGroupChanged(quint32 id)
{
    setModified();
    emit fixtureGroupChanged(id);
}

/*********************************************************************
 * Channels groups
 *********************************************************************/
bool Doc::addChannelsGroup(ChannelsGroup *grp, quint32 id)
{
    Q_ASSERT(grp != NULL);

    // No ID given, this method can assign one
    if (id == ChannelsGroup::invalidId())
        id = createChannelsGroupId();

     grp->setId(id);
     m_channelsGroups[id] = grp;
     if (m_orderedGroups.contains(id) == false)
        m_orderedGroups.append(id);

     emit channelsGroupAdded(id);
     setModified();

     return true;
}

bool Doc::deleteChannelsGroup(quint32 id)
{
    if (m_channelsGroups.contains(id) == true)
    {
        ChannelsGroup* grp = m_channelsGroups.take(id);
        Q_ASSERT(grp != NULL);

        emit channelsGroupRemoved(id);
        setModified();
        delete grp;

        int idx = m_orderedGroups.indexOf(id);
        if (idx != -1)
            m_orderedGroups.takeAt(idx);

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No channels group with id" << id;
        return false;
    }
}

bool Doc::moveChannelGroup(quint32 id, int direction)
{
    if (direction == 0 || m_orderedGroups.contains(id) == false)
        return false;

    int idx = m_orderedGroups.indexOf(id);

    if (idx + direction < 0 || idx + direction >= m_orderedGroups.count())
        return false;

    qDebug() << Q_FUNC_INFO << m_orderedGroups;
    m_orderedGroups.takeAt(idx);
    m_orderedGroups.insert(idx + direction, id);
    qDebug() << Q_FUNC_INFO << m_orderedGroups;

    setModified();
    return true;
}

ChannelsGroup* Doc::channelsGroup(quint32 id) const
{
    return m_channelsGroups.value(id, NULL);
}

QList <ChannelsGroup*> Doc::channelsGroups() const
{
    QList <ChannelsGroup*> orderedList;

    for (int i = 0; i < m_orderedGroups.count(); i++)
    {
        orderedList.append(m_channelsGroups[m_orderedGroups.at(i)]);
    }
    return orderedList;
}

quint32 Doc::createChannelsGroupId()
{
    while (m_channelsGroups.contains(m_latestChannelsGroupId) == true ||
           m_latestChannelsGroupId == ChannelsGroup::invalidId())
    {
        m_latestChannelsGroupId++;
    }

    return m_latestChannelsGroupId;
}

/*********************************************************************
 * Palettes
 *********************************************************************/

bool Doc::addPalette(QLCPalette *palette, quint32 id)
{
    Q_ASSERT(palette != NULL);

    // No ID given, this method can assign one
    if (id == QLCPalette::invalidId())
        id = createPaletteId();

    if (m_palettes.contains(id) == true || id == QLCPalette::invalidId())
    {
        qWarning() << Q_FUNC_INFO << "a palette with ID" << id << "already exists!";
        return false;
    }
    else
    {
        palette->setID(id);
        m_palettes[id] = palette;

        emit paletteAdded(id);
        setModified();
    }

    return true;
}

bool Doc::deletePalette(quint32 id)
{
    if (m_palettes.contains(id) == true)
    {
        QLCPalette *palette = m_palettes.take(id);
        Q_ASSERT(palette != NULL);

        emit paletteRemoved(id);
        setModified();
        delete palette;

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No palette with id" << id;
        return false;
    }
}

QLCPalette *Doc::palette(quint32 id) const
{
    return m_palettes.value(id, NULL);
}

QList<QLCPalette *> Doc::palettes() const
{
    return m_palettes.values();
}

quint32 Doc::createPaletteId()
{
    while (m_palettes.contains(m_latestPaletteId) == true ||
           m_latestPaletteId == FixtureGroup::invalidId())
    {
        m_latestPaletteId++;
    }

    return m_latestPaletteId;
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

quint32 Doc::createFunctionId()
{
    /* This results in an endless loop if there are UINT_MAX-1 functions. That,
       however, seems a bit unlikely. Are there even 4294967295-1 functions in
       total in the whole world? */
    while (m_functions.contains(m_latestFunctionId) == true ||
           m_latestFunctionId == Fixture::invalidId())
    {
        m_latestFunctionId++;
    }

    return m_latestFunctionId;
}

bool Doc::addFunction(Function* func, quint32 id)
{
    Q_ASSERT(func != NULL);

    if (id == Function::invalidId())
        id = createFunctionId();

    if (m_functions.contains(id) == true || id == Fixture::invalidId())
    {
        qWarning() << Q_FUNC_INFO << "a function with ID" << id << "already exists!";
        return false;
    }
    else
    {
        // Listen to function changes
        connect(func, SIGNAL(changed(quint32)),
                this, SLOT(slotFunctionChanged(quint32)));

        // Listen to function name changes
        connect(func, SIGNAL(nameChanged(quint32)),
                this, SLOT(slotFunctionNameChanged(quint32)));

        // Make the function listen to fixture removals
        connect(this, SIGNAL(fixtureRemoved(quint32)),
                func, SLOT(slotFixtureRemoved(quint32)));

        // Place the function in the map and assign it the new ID
        m_functions[id] = func;
        func->setID(id);
        emit functionAdded(id);
        setModified();

        return true;
    }
}

QList <Function*> Doc::functions() const
{
    return m_functions.values();
}

QList<Function *> Doc::functionsByType(Function::Type type) const
{
    QList <Function*> list;
    foreach (Function *f, m_functions)
    {
        if (f != NULL && f->type() == type)
            list.append(f);
    }
    return list;
}

Function *Doc::functionByName(QString name)
{
    foreach (Function *f, m_functions)
    {
        if (f != NULL && f->name() == name)
            return f;
    }
    return NULL;
}

bool Doc::deleteFunction(quint32 id)
{
    if (m_functions.contains(id) == true)
    {
        Function* func = m_functions.take(id);
        Q_ASSERT(func != NULL);

        if (m_startupFunctionId == id)
            m_startupFunctionId = Function::invalidId();

        emit functionRemoved(id);
        setModified();
        delete func;

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No function with id" << id;
        return false;
    }
}

Function* Doc::function(quint32 id) const
{
    return m_functions.value(id, NULL);
}

quint32 Doc::nextFunctionID()
{
    quint32 tmpFID = m_latestFunctionId;
    while (m_functions.contains(tmpFID) == true ||
           tmpFID == Fixture::invalidId())
    {
        tmpFID++;
    }

    return tmpFID;
}

void Doc::setStartupFunction(quint32 fid)
{
    m_startupFunctionId = fid;
}

quint32 Doc::startupFunction()
{
    return m_startupFunctionId;
}

QList<quint32> Doc::getUsage(quint32 fid)
{
    QList<quint32> usageList;

    foreach (Function *f, m_functions)
    {
        if (f->id() == fid)
            continue;

        switch(f->type())
        {
            case Function::CollectionType:
            {
                Collection *c = qobject_cast<Collection *>(f);
                int pos = c->functions().indexOf(fid);
                if (pos != -1)
                {
                    usageList.append(f->id());
                    usageList.append(pos);
                }
            }
            break;
            case Function::ChaserType:

            {
                Chaser *c = qobject_cast<Chaser *>(f);
                for (int i = 0; i < c->stepsCount(); i++)
                {
                    ChaserStep *cs = c->stepAt(i);
                    if (cs->fid == fid)
                    {
                        usageList.append(f->id());
                        usageList.append(i);
                    }
                }
            }
            break;
            case Function::SequenceType:
            {
                Sequence *s = qobject_cast<Sequence *>(f);
                if (s->boundSceneID() == fid)
                {
                    usageList.append(f->id());
                    usageList.append(0);
                }
            }
            break;
            case Function::ScriptType:
            {
                Script *s = qobject_cast<Script *>(f);
                QList<quint32> l = s->functionList();
                for (int i = 0; i < l.count(); i+=2)
                {
                    if (l.at(i) == fid)
                    {
                        if (i + 1 >= l.count()) {
                            qDebug() << "Doc::getUsage: Index entry missing on " << f->name();
                            break;
                        }
                        usageList.append(s->id());
                        usageList.append(l.at(i + 1)); // line number
                    }
                }
            }
            break;
            case Function::ShowType:
            {
                Show *s = qobject_cast<Show *>(f);
                foreach (Track *t, s->tracks())
                {
                    foreach (ShowFunction *sf, t->showFunctions())
                    {
                        if (sf->functionID() == fid)
                        {
                            usageList.append(f->id());
                            usageList.append(t->id());
                        }
                    }
                }
            }
            break;
            default:
            break;
        }
    }

    return usageList;
}

void Doc::slotFunctionChanged(quint32 fid)
{
    setModified();
    emit functionChanged(fid);
}

void Doc::slotFunctionNameChanged(quint32 fid)
{
    setModified();
    emit functionNameChanged(fid);
}

/*********************************************************************
 * Monitor Properties
 *********************************************************************/

MonitorProperties *Doc::monitorProperties()
{
    if (m_monitorProps == NULL)
        m_monitorProps = new MonitorProperties();

    return m_monitorProps;
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool Doc::loadXML(QXmlStreamReader &doc, bool loadIO)
{
    clearErrorLog();

    if (doc.name() != KXMLQLCEngine)
    {
        qWarning() << Q_FUNC_INFO << "Engine node not found";
        return false;
    }

    m_loadStatus = Loading;
    emit loading();

    if (doc.attributes().hasAttribute(KXMLQLCStartupFunction))
    {
        quint32 sID = doc.attributes().value(KXMLQLCStartupFunction).toString().toUInt();
        if (sID != Function::invalidId())
            setStartupFunction(sID);
    }

    while (doc.readNextStartElement())
    {
        //qDebug() << "Doc tag:" << doc.name();
        if (doc.name() == KXMLFixture)
        {
            Fixture::loader(doc, this);
        }
        else if (doc.name() == KXMLQLCFixtureGroup)
        {
            FixtureGroup::loader(doc, this);
        }
        else if (doc.name() == KXMLQLCChannelsGroup)
        {
            ChannelsGroup::loader(doc, this);
        }
        else if (doc.name() == KXMLQLCPalette)
        {
            QLCPalette::loader(doc, this);
            doc.skipCurrentElement();
        }
        else if (doc.name() == KXMLQLCFunction)
        {
            //qDebug() << doc.attributes().value("Name").toString();
            Function::loader(doc, this);
        }
        else if (doc.name() == KXMLQLCBus)
        {
            /* LEGACY */
            Bus::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLIOMap && loadIO)
        {
            m_ioMap->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCMonitorProperties)
        {
            monitorProperties()->loadXML(doc, this);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown engine tag:" << doc.name();
            doc.skipCurrentElement();
        }
    }

    postLoad();

    m_loadStatus = Loaded;
    emit loaded();

    return true;
}

bool Doc::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    /* Create the master Engine node */
    doc->writeStartElement(KXMLQLCEngine);

    if (startupFunction() != Function::invalidId())
        doc->writeAttribute(KXMLQLCStartupFunction, QString::number(startupFunction()));

    m_ioMap->saveXML(doc);

    /* Write fixtures into an XML document */
    QListIterator <Fixture*> fxit(fixtures());
    while (fxit.hasNext() == true)
    {
        Fixture *fxi(fxit.next());
        Q_ASSERT(fxi != NULL);
        fxi->saveXML(doc);
    }

    /* Write fixture groups into an XML document */
    QListIterator <FixtureGroup*> grpit(fixtureGroups());
    while (grpit.hasNext() == true)
    {
        FixtureGroup *grp(grpit.next());
        Q_ASSERT(grp != NULL);
        grp->saveXML(doc);
    }

    /* Write channel groups into an XML document */
    QListIterator <ChannelsGroup*> chanGroups(channelsGroups());
    while (chanGroups.hasNext() == true)
    {
        ChannelsGroup *grp(chanGroups.next());
        Q_ASSERT(grp != NULL);
        grp->saveXML(doc);
    }

    /* Write palettes into an XML document */
    QListIterator <QLCPalette*> paletteIt(palettes());
    while (paletteIt.hasNext() == true)
    {
        QLCPalette *palette(paletteIt.next());
        Q_ASSERT(palette != NULL);
        palette->saveXML(doc);
    }

    /* Write functions into an XML document */
    QListIterator <Function*> funcit(functions());
    while (funcit.hasNext() == true)
    {
        Function *func(funcit.next());
        Q_ASSERT(func != NULL);
        func->saveXML(doc);
    }

    if (m_monitorProps != NULL)
        m_monitorProps->saveXML(doc, this);

    /* End the <Engine> tag */
    doc->writeEndElement();

    return true;
}

void Doc::appendToErrorLog(QString error)
{
    if (m_errorLog.contains(error))
        return;

    m_errorLog.append(error);
    m_errorLog.append("<br>");
}

void Doc::clearErrorLog()
{
    m_errorLog = "";
}

QString Doc::errorLog()
{
    return m_errorLog;
}

void Doc::postLoad()
{
    QListIterator <Function*> functionit(functions());
    while (functionit.hasNext() == true)
    {
        Function* function(functionit.next());
        Q_ASSERT(function != NULL);
        function->postLoad();
    }
}
