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

#include <climits>
#include <algorithm>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <QStringList>
#include <QString>
#include <QSet>
#include <QThread>
#include <QDebug>
#include <QList>
#include <QTime>
#include <QDir>
#include <QRandomGenerator>

#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "programmerflasher.h"
#include "effectscriptrunner.h"
#include "showembedding.h"

#include "monitorproperties.h"
#include "powerdistribution.h"
#include "networktopology.h"
#include "timecodesource.h"
#include "ltcreceiver.h"
#include "timecodecues.h"
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
#include "speedmasters.h"
#include "showcontrol.h"
#include "showtreecontroller.h"
#include "paletteops.h"
#include "doc.h"
#include "powerlimiter.h"
#include "shutterautoopen.h"
#include "showwebaccess.h"
#include "controlmap.h"
#include "showoperate.h"
#include "teachingsettings.h"
#include "outputrulekeeper.h"
#include "bus.h"
#include "qlcpalette.h"
#include "qlcphysical.h"
#include "fixturegroup.h"
#include "programmercontroller.h"
#include "venuecorrections.h"
#include "parkeffect.h"
#include "focuscalls.h"
#include "markeffect.h"
#include "markplanner.h"
#include "lastlookeffect.h"

 #include "audiocapture_qt6.h"

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
    , m_powerDist(NULL)
    , m_speedMasters(NULL)
    , m_showControl(NULL)
    , m_networkTopology(NULL)
    , m_timecodeSource(NULL)
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
    
    m_autosaveTimer.setInterval(AUTOSAVE_TIMEOUT * 1000);
    m_autosaveTimer.setSingleShot(true);

    connect(&m_autosaveTimer, SIGNAL(timeout()), this, SIGNAL(needAutosave()));

    // All programmer-mode state and logic live in a fork-owned
    // controller (QObject child of Doc, auto-deleted). Doc keeps thin
    // forwarder methods so no call sites / connect() sites elsewhere
    // change. Created after m_masterTimer so it can wire to it.
    m_programmer = new ProgrammerController(this);

    // Speed masters read the main beat clock, so after m_masterTimer too.
    m_speedMasters = new SpeedMasters(this, this);
    connect(m_ioMap, &InputOutputMap::inputValueChanged,
            m_speedMasters, &SpeedMasters::slotInputValue);

    // Cues fired from outside (QLab): MSC arrives as SysEx on a patched
    // MIDI input, as MTC does.
    m_showControl = new ShowControl(this, this);
    m_paletteOps = new PaletteOps(this);
    m_showTree = new ShowTreeController(this);
    connect(m_ioMap, &InputOutputMap::inputSysExReceived,
            m_showControl, &ShowControl::handleSysEx);
    // Which masters are used changes only with an edit: let them know.
    // (modified() can come from the DMX thread; marking is atomic.)
    auto usage = [this]() { m_speedMasters->markUsageDirty(); };
    connect(this, &Doc::modified, m_speedMasters, usage, Qt::DirectConnection);
    connect(this, &Doc::functionAdded, m_speedMasters, usage);
    connect(this, &Doc::functionRemoved, m_speedMasters, usage);
    connect(this, &Doc::functionChanged, m_speedMasters, usage);
    connect(this, &Doc::paletteAdded, m_speedMasters, usage);
    connect(this, &Doc::paletteRemoved, m_speedMasters, usage);

    // Fixtures held to a maximum draw: kept in step with the show.
    m_powerLimiter = new PowerLimiter(this);
    // Shutters opened for a lit head that nothing else opens.
    m_shutterAutoOpen = new ShutterAutoOpen(this);
    // Web access started with the show (off unless the show says so).
    m_webAccess = new ShowWebAccess(this);
    // MIDI / OSC controls bound straight to what they fire (no VC widget).
    m_controlMap = new ControlMap(this);
    // Open in Operate, and the PIN to leave it (unset unless the show says).
    m_operate = new ShowOperate(this);
    m_teaching = new TeachingSettings(this);
    // Relays, house and work lights: Blackout / GM exempt, switch, on at open.
    m_outputRuleKeeper = new OutputRuleKeeper(this);

    // Track running Scenes for programmer edit-routing (LTP).
    // DirectConnection: the signal is emitted from MasterTimer's worker
    // thread and the read side (VCSlider::writeDMXParameter →
    // routeProgrammerEdit) ALSO runs in that thread. A queued
    // connection would land the m_runningScenes update on the main
    // thread asynchronously, so a slider moved immediately after a
    // chaser/collection step would see a stale-empty list and route
    // to the new-scene bucket instead of the running scenes.
    connect(m_masterTimer, SIGNAL(functionStarted(quint32)),
            m_programmer, SLOT(slotProgrammerFunctionStarted(quint32)),
            Qt::DirectConnection);
    connect(m_masterTimer, SIGNAL(functionStopped(quint32)),
            m_programmer, SLOT(slotProgrammerFunctionStopped(quint32)),
            Qt::DirectConnection);

    // Last-look persistence: a stopped show holds its final look (Operate) via
    // this DMXSource holder; any newly started function replaces it (whole-look
    // replace). The hold is captured in Show::postRun.
    m_lastLook = new LastLookEffect(this);
    connect(m_masterTimer, SIGNAL(functionStarted(quint32)),
            this, SLOT(slotClearLastLookOnStart(quint32)),
            Qt::DirectConnection);

    // Route absolute input timecode (e.g. MIDI Time Code) into the shared
    // timecode source that the Show timeline can follow.
    connect(m_ioMap, SIGNAL(inputTimeCodeChanged(quint32,quint32,uchar)),
            timecodeSource(), SLOT(updateTimeCode(quint32,quint32,uchar)));

    // Effect script runner — drives JS effect instances for scenes that
    // reference Effect-type palettes. Queued connections are fine here
    // because the runner only needs to know about starts/stops to create
    // or destroy instances; the actual DMX work is driven by its QTimer.
    m_effectScriptRunner = new EffectScriptRunner(this);
    m_showEmbedding = new ShowEmbedding(this);
    connect(m_masterTimer, SIGNAL(functionStarted(quint32)),
            m_effectScriptRunner, SLOT(slotFunctionStarted(quint32)));
    connect(m_masterTimer, SIGNAL(functionStopped(quint32)),
            m_effectScriptRunner, SLOT(slotFunctionStopped(quint32)));
    connect(m_masterTimer, SIGNAL(functionStartedAgain(quint32,quint32)),
            m_effectScriptRunner, SLOT(slotFunctionStartedAgain(quint32,quint32)));
    // deleteFunction() removes the function from the MasterTimer without
    // emitting functionStopped, and clearContents() deletes every function at
    // once — neither would tear down the effect instances, which then leak a
    // QJSEngine each and keep pinning their last frame onto the output at
    // Override priority. Both are emitted before the functions are destroyed.
    connect(this, SIGNAL(functionRemoved(quint32)),
            m_effectScriptRunner, SLOT(slotFunctionRemoved(quint32)));
    connect(this, SIGNAL(cleared()),
            m_effectScriptRunner, SLOT(slotDocCleared()));
    // A deleted Effect look must not leave a persistent engine parked under its id.
    connect(this, SIGNAL(paletteRemoved(quint32)),
            m_effectScriptRunner, SLOT(slotPaletteRemoved(quint32)));

    // Feed real-time joystick data into the effect script data channel system.
    connect(m_programmer, &ProgrammerController::joystickUpdated,
            this, [this](float pan, float tilt)
    {
        QVariantMap jd;
        jd[QStringLiteral("pan")]  = double(pan);
        jd[QStringLiteral("tilt")] = double(tilt);
        // Sensitivity and deadzone are global joystick settings (not per-effect):
        // pass them through so effect scripts honour the I/O Manager / device
        // profile instead of falling back to hard-coded defaults.
        jd[QStringLiteral("sensitivity")] = double(m_programmer->joystickSensitivity());
        jd[QStringLiteral("deadzone")]    = double(m_programmer->joystickDeadzone());
        m_effectScriptRunner->setDataChannel(QStringLiteral("joystick"), jd);
    });

    // Relay the controller's signals as Doc's own signals so external
    // code keeps connecting to `doc` unchanged.
    connect(m_programmer, &ProgrammerController::programmerSelectionChanged,
            this, &Doc::programmerSelectionChanged);
    connect(m_programmer, &ProgrammerController::programmerDirtyChanged,
            this, &Doc::programmerDirtyChanged);
    connect(m_programmer, &ProgrammerController::padModeChanged,
            this, &Doc::padModeChanged);
    connect(m_programmer, &ProgrammerController::programmerSubSelectionChanged,
            this, &Doc::programmerSubSelectionChanged);
    connect(m_programmer, &ProgrammerController::showLockedChanged,
            this, &Doc::showLockedChanged);

    // Programmer sub-selection (pad-grid fixture refinement) is
    // scoped to the current programmer selection — clear it on every
    // selection change so a leftover sub-selection from an old group
    // can't silently filter writes against a different group.
    connect(this, &Doc::programmerSelectionChanged,
            this, &Doc::clearProgrammerSubSelection);

    m_captureManager = new CaptureManager(this, this);
}

Doc::~Doc()
{
    delete m_powerLimiter;
    m_powerLimiter = nullptr;
    delete m_shutterAutoOpen;
    m_shutterAutoOpen = nullptr;
    delete m_webAccess;
    m_webAccess = nullptr;
    delete m_controlMap;
    m_controlMap = nullptr;
    delete m_operate;
    m_operate = nullptr;
    delete m_outputRuleKeeper;
    m_outputRuleKeeper = nullptr;

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

    delete m_powerDist;
    m_powerDist = NULL;

    delete m_networkTopology;
    m_networkTopology = NULL;
}

void Doc::clearContents()
{
    emit clearing();

    m_loadedFromWeb.store(false);

    if (m_lastLook != NULL)
        m_lastLook->clear();

    m_clipboard->resetContents();

    if (m_monitorProps != NULL)
        m_monitorProps->reset();

    if (m_powerDist != NULL)
        m_powerDist->reset();

    m_speedMasters->reset();
    if (m_shutterAutoOpen != nullptr)
        m_shutterAutoOpen->reset();
    if (m_webAccess != nullptr)
        m_webAccess->reset();
    if (m_controlMap != nullptr)
        m_controlMap->reset();
    if (m_operate != nullptr)
        m_operate->reset();
    if (m_teaching != nullptr)
        m_teaching->reset();
    m_showControl->reset();
    m_showTree->reset();
    m_paletteOps->reset();
    // The timecode source is the show's (a universe of its patch).
    if (m_timecodeSource != NULL)
        m_timecodeSource->setSourceUniverse(-1);

    if (m_networkTopology != NULL)
        m_networkTopology->reset();

    // Drop any parked fixtures so a quarantine doesn't bleed into the next
    // workspace (loadXML re-populates from <Park> if present).
    if (m_programmer != NULL)
    {
        m_programmer->unparkAllFixtures();
        // Move in black is the show's: a show without <MoveInBlack> starts
        // from a new show's, not the last one's.
        m_programmer->resetMoveInBlack();
    }

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
        // Invalidate the cached fixtures() list BEFORE emitting: the fixture is
        // already out of m_fixtures, so a listener that calls fixtures() during
        // the signal rebuilds from the live map and never sees this freed
        // pointer (was a use-after-free / shutdown crash).
        m_fixturesListCacheUpToDate = false;
        delete fxi;
        emit fixtureRemoved(fxID);
    }
    m_fixturesListCacheUpToDate = false;

    m_orderedGroups.clear();

    m_latestFunctionId = 0;
    m_startupFunctionId = Function::invalidId();
    m_latestFixtureId = 0;
    m_latestFixtureGroupId = 0;
    m_latestChannelsGroupId = 0;
    m_latestPaletteId = 0;
    m_addresses.clear();
    m_loadStatus = Cleared;

    m_programmer->clearProgrammerSelection();

    /* The show is gone: this computer's definitions and scripts again. */
    if (m_showEmbedding != nullptr)
        m_showEmbedding->restoreComputer();

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

void Doc::setComponentPathOverrides(const QMap<QString, QString> &overrides)
{
    m_componentPathOverrides = overrides;
}

bool Doc::hasComponentPathOverride(const QString &absolutePath) const
{
    return m_componentPathOverrides.contains(QFileInfo(absolutePath).absoluteFilePath());
}

QString Doc::normalizeComponentPath(const QString& filePath) const
{
    if (filePath.isEmpty())
        return filePath;

    QFileInfo f(filePath);

    if (!m_componentPathOverrides.isEmpty())
    {
        const auto it = m_componentPathOverrides.constFind(f.absoluteFilePath());
        if (it != m_componentPathOverrides.constEnd())
            return it.value();
    }

    // No workspace path (an unsaved show): keep the absolute path, rather
    // than one relative to whatever the current directory happens to be.
    // Under the show's folder: the folder itself or one inside it (not a
    // sibling whose name merely starts the same, /Shows/A vs /Shows/AB).
    const QString ws = QDir(workspacePath()).absolutePath();
    const QString in = f.absolutePath();
    if (!workspacePath().isEmpty()
        && (in == ws || in.startsWith(ws.endsWith(QLatin1Char('/')) ? ws : ws + QLatin1Char('/'))))
    {
        return QDir(workspacePath()).relativeFilePath(f.absoluteFilePath());
    }
    else
    {
        return f.absoluteFilePath();
    }
}

QString Doc::keepWithShow(const QString& filePath, const QString& subdir) const
{
    if (filePath.isEmpty() || workspacePath().isEmpty())
        return filePath;

    QFileInfo src(filePath);
    if (!src.isAbsolute() || !src.isFile())
        return filePath;
    if (normalizeComponentPath(src.absoluteFilePath()) != src.absoluteFilePath())
        return filePath;                    // already in the show's folder

    QDir dir(workspacePath());
    if (!dir.mkpath(subdir) || !dir.cd(subdir))
        return filePath;

    QFile in(src.absoluteFilePath());
    if (!in.open(QIODevice::ReadOnly))
        return filePath;
    const QByteArray bytes = in.readAll();

    for (int n = 1; n < 1000; n++)
    {
        const QString name = n == 1 ? src.fileName()
            : QStringLiteral("%1-%2.%3").arg(src.completeBaseName()).arg(n).arg(src.suffix());
        const QString dest = dir.absoluteFilePath(name);
        QFile out(dest);
        if (out.exists())
        {
            // The same picture copied before: use it.
            if (out.open(QIODevice::ReadOnly) && out.size() == bytes.size() && out.readAll() == bytes)
                return dest;
            continue;
        }
        if (out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size())
            return dest;
        out.remove();
        return filePath;
    }
    return filePath;
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
    return m_programmer->programmerSelection();
}

void Doc::setProgrammerSelection(const QList<quint32>& fixtureIds)
{
    m_programmer->setProgrammerSelection(fixtureIds);
}

void Doc::addToProgrammerSelection(const QList<quint32>& fixtureIds)
{
    m_programmer->addToProgrammerSelection(fixtureIds);
}

void Doc::removeFromProgrammerSelection(const QList<quint32>& fixtureIds)
{
    m_programmer->removeFromProgrammerSelection(fixtureIds);
}

void Doc::toggleInProgrammerSelection(const QList<quint32>& fixtureIds)
{
    m_programmer->toggleInProgrammerSelection(fixtureIds);
}

void Doc::clearProgrammerSelection()
{
    m_programmer->clearProgrammerSelection();
}

bool Doc::isInProgrammerSelection(quint32 fixtureId) const
{
    return m_programmer->isInProgrammerSelection(fixtureId);
}

bool Doc::allInProgrammerSelection(const QList<quint32>& fixtureIds) const
{
    return m_programmer->allInProgrammerSelection(fixtureIds);
}

QColor Doc::programmerColor() const
{
    return m_programmer->programmerColor();
}

void Doc::setProgrammerColorComponent(int qlcPrimaryColour, uchar value)
{
    m_programmer->setProgrammerColorComponent(qlcPrimaryColour, value);
}

/*****************************************************************************
 * Programmer Values
 *****************************************************************************/

void Doc::setProgrammerValue(quint32 fixtureId, quint32 channel, uchar value)
{
    m_programmer->setProgrammerValue(fixtureId, channel, value);
}

void Doc::clearProgrammerValues()
{
    m_programmer->clearProgrammerValues();
}

bool Doc::isProgrammerDirty() const
{
    return m_programmer->isProgrammerDirty();
}

quint32 Doc::saveProgrammerAsScene(const QString &name)
{
    return m_programmer->saveProgrammerAsScene(name);
}

quint32 Doc::routeProgrammerEdit(quint32 fid, quint32 ch, uchar value)
{
    return m_programmer->routeProgrammerEdit(fid, ch, value);
}

void Doc::setFocusedScene(quint32 sceneId)
{
    m_programmer->setFocusedScene(sceneId);
}

void Doc::setFocusedPalette(quint32 paletteId)
{
    m_programmer->setFocusedPalette(paletteId);
}

quint32 Doc::routeProgrammerEditByChannelType(int qlcChannelGroup, int groupIndex, uchar rawValue)
{
    return m_programmer->routeProgrammerEditByChannelType(qlcChannelGroup, groupIndex, rawValue);
}

int Doc::rerouteProgrammerValues()
{
    return m_programmer->rerouteProgrammerValues();
}

QSet<quint32> Doc::editedSceneIds() const
{
    return m_programmer->editedSceneIds();
}

bool Doc::hasProgrammerValues() const
{
    return m_programmer->hasProgrammerValues();
}

QHash<quint32, QHash<quint32, uchar>> Doc::programmerValues() const
{
    return m_programmer->programmerValues();
}

QList<Doc::SaveBucket> Doc::proposedSaveBuckets() const
{
    return m_programmer->proposedSaveBuckets();
}

QList<Doc::SaveBucket> Doc::splitBucketByGroup(const SaveBucket &bucket) const
{
    return m_programmer->splitBucketByGroup(bucket);
}

void Doc::stepCurrentChaser(int direction)
{
    m_programmer->stepCurrentChaser(direction);
}

quint32 Doc::findMatchingScene(
    const QHash<quint32, QHash<quint32, uchar>> &values,
    quint32 excludeSceneId) const
{
    return m_programmer->findMatchingScene(values, excludeSceneId);
}

QList<quint32> Doc::collectionsContaining(quint32 sceneId) const
{
    return m_programmer->collectionsContaining(sceneId);
}

bool Doc::replaceSceneInCollection(quint32 collectionId,
                                   quint32 oldSceneId,
                                   quint32 newSceneId)
{
    return m_programmer->replaceSceneInCollection(collectionId, oldSceneId, newSceneId);
}

void Doc::revertSceneFromSnapshot(quint32 sceneId)
{
    m_programmer->revertSceneFromSnapshot(sceneId);
}

quint32 Doc::singleRunningCollection() const
{
    return m_programmer->singleRunningCollection();
}

void Doc::flashFixture(quint32 fixtureId, int durationMs)
{
    m_programmer->flashFixture(fixtureId, durationMs);
}

bool Doc::isShowLocked() const
{
    return m_programmer->isShowLocked();
}

void Doc::setShowLocked(bool locked)
{
    m_programmer->setShowLocked(locked);
}

QString Doc::nextUniqueModelName(const QString &name,
                                 const std::function<bool(const QString &)> &isTaken)
{
    if (name.isEmpty() || isTaken(name) == false)
        return name;
    for (int n = 2; n < 100000; ++n)
    {
        const QString candidate = QStringLiteral("%1 #%2").arg(name).arg(n);
        if (isTaken(candidate) == false)
            return candidate;
    }
    return name;
}

QString Doc::nextUniqueName(const QString &name,
                            const std::function<bool(const QString &)> &isTaken)
{
    // The name is free as-is — keep it. (Duplication passes the source's own
    // name, which IS taken, so it always bumps; multi-add passes fresh
    // candidates that may already be free.)
    if (name.isEmpty() || isTaken(name) == false)
        return name;

    // Locate the rightmost run of digits in the name.
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
        for (int bump = 1; bump < 100000; ++bump)
        {
            const QString numStr = QString::number(base + bump)
                                       .rightJustified(width, QChar('0'));
            const QString candidate = prefix + numStr + suffix;
            if (isTaken(candidate) == false)
                return candidate;
        }
    }

    // Fallback: append " 2", " 3", … (no digits found, or the bump loop was
    // exhausted — both very unlikely).
    for (int n = 2; n < 100000; ++n)
    {
        const QString candidate = QStringLiteral("%1 %2").arg(name).arg(n);
        if (isTaken(candidate) == false)
            return candidate;
    }
    return name + QStringLiteral(" (Copy)");
}

bool Doc::fixtureNameTaken(const QString &name, quint32 except) const
{
    foreach (Fixture *f, m_fixtures)
        if (f != NULL && f->id() != except && f->name() == name)
            return true;
    return false;
}

QString Doc::uniqueFixtureName(const QString &name, quint32 except) const
{
    return nextUniqueModelName(name, [this, except](const QString &c) {
        return fixtureNameTaken(c, except);
    });
}

QMap<QString, QList<quint32> > Doc::duplicateFixtureNames() const
{
    QMap<QString, QList<quint32> > byName;
    foreach (Fixture *f, m_fixtures)
        if (f != NULL)
            byName[f->name()] << f->id();
    QMap<QString, QList<quint32> > out;
    for (auto it = byName.constBegin(); it != byName.constEnd(); ++it)
    {
        if (it.value().size() < 2)
            continue;
        QList<quint32> ids = it.value();
        std::sort(ids.begin(), ids.end());
        out.insert(it.key(), ids);
    }
    return out;
}

int Doc::makeFixtureNamesUnique(const QStringList &only)
{
    int renamed = 0;
    const QMap<QString, QList<quint32> > dups = duplicateFixtureNames();
    for (auto it = dups.constBegin(); it != dups.constEnd(); ++it)
    {
        if (!only.isEmpty() && !only.contains(it.key()))
            continue;
        // The one in the show keeps the name: patched first, then lowest ID.
        quint32 keeper = it.value().first();
        foreach (quint32 fid, it.value())
            if (Fixture *f = fixture(fid))
                if (f->isPatched()) { keeper = fid; break; }
        foreach (quint32 fid, it.value())
        {
            Fixture *f = fixture(fid);
            if (f == NULL || fid == keeper)
                continue;
            f->setName(uniqueFixtureName(f->name(), fid));
            ++renamed;
        }
    }
    if (renamed > 0)
        setModified();
    return renamed;
}

QString Doc::nextDuplicateName(const Function *src) const
{
    if (src == NULL)
        return QString();
    const QString path = src->path();

    // A candidate collides if any OTHER function in the same folder owns it.
    auto isTaken = [this, &path](const QString &candidate) {
        for (Function *fn : m_functions.values())
        {
            if (fn != NULL && fn->name() == candidate && fn->path() == path)
                return true;
        }
        return false;
    };

    return nextUniqueName(src->name(), isTaken);
}

quint32 Doc::saveBucketAsScene(const SaveBucket &bucket,
                               const QString &name, const QString &path)
{
    return m_programmer->saveBucketAsScene(bucket, name, path);
}

quint32 Doc::saveBucketAsGroupScene(const SaveBucket &bucket,
                                    const QString &name, const QString &path)
{
    return m_programmer->saveBucketAsGroupScene(bucket, name, path);
}

Doc::PadMode Doc::padMode() const
{
    return m_programmer->padMode();
}

void Doc::setPadMode(PadMode mode)
{
    m_programmer->setPadMode(mode);
}

quint32 Doc::activeProgrammerGroup() const
{
    return m_programmer->activeProgrammerGroup();
}

QSet<quint32> Doc::programmerSubSelection() const
{
    return m_programmer->programmerSubSelection();
}

bool Doc::isInProgrammerSubSelection(quint32 fid) const
{
    return m_programmer->isInProgrammerSubSelection(fid);
}

void Doc::toggleInProgrammerSubSelection(quint32 fid)
{
    m_programmer->toggleInProgrammerSubSelection(fid);
}

void Doc::clearProgrammerSubSelection()
{
    m_programmer->clearProgrammerSubSelection();
}

void Doc::revertProgrammer()
{
    m_programmer->revertProgrammer();
}

QSharedPointer<AudioCapture> Doc::audioInputCapture() const
{
    if (!m_inputCapture)
    {
        qDebug() << "Creating new audio capture";
        m_inputCapture = QSharedPointer<AudioCapture>(
            new AudioCaptureQt6()
            );
    }
    return m_inputCapture;
}

QSharedPointer<AudioCapture> Doc::existingAudioInputCapture() const
{
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

    // setModified() is reached from the MasterTimer (DMX) thread via the
    // programmer edit routing (VCSlider::writeDMXParameter -> markSceneEdited).
    // QTimer::start() cannot be called from a thread other than the timer's own:
    // Qt refuses it ("Timers can only be used with threads started with QThread")
    // and the timer is simply never armed — so autosave silently stopped
    // tracking programmer edits. Marshal the start back to the timer's thread.
    if (QThread::currentThread() == m_autosaveTimer.thread())
        m_autosaveTimer.start();
    else
        QMetaObject::invokeMethod(&m_autosaveTimer, "start", Qt::QueuedConnection);

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

    // Leaving Operate (→ Design) drops any held last look: you're building
    // again, not running a show.
    if (mode == Design && m_lastLook != NULL)
        m_lastLook->clear();

    // Show-mode Lock is part of Operate (board Q80): programmer parameter
    // writes and timeline edits are locked while the show runs.
    if (m_programmer != NULL)
        m_programmer->setShowLocked(mode == Operate);

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

void Doc::setLastLookEnabled(bool enable)
{
    m_lastLookEnabled = enable;
    if (!enable && m_lastLook != NULL)
        m_lastLook->clear();
}

// Recursively collect the fixture IDs a function ultimately drives. Leaf types
// (Scene/EFX/RGBMatrix) report fixtures via components(); container types
// (Chaser/Sequence/Collection/Show) report member function IDs, recursed into.
static void collectFunctionFixtures(const Doc *doc, quint32 fid,
                                    QSet<quint32> &fixtures,
                                    QSet<quint32> &visited, int depth)
{
    if (depth > 8 || doc == NULL || visited.contains(fid))
        return;
    visited.insert(fid);

    Function *f = doc->function(fid);
    if (f == NULL)
        return;

    switch (f->type())
    {
        case Function::SceneType:
        case Function::EFXType:
        case Function::RGBMatrixType:
            foreach (quint32 fxi, f->components())
                fixtures.insert(fxi);
            break;
        case Function::ChaserType:
        case Function::SequenceType:
        case Function::CollectionType:
        case Function::ShowType:
            foreach (quint32 mid, f->components())
                collectFunctionFixtures(doc, mid, fixtures, visited, depth + 1);
            break;
        default:
            break;
    }
}

QList<quint32> Doc::functionFixtures(quint32 fid) const
{
    QSet<quint32> fixtures;
    QSet<quint32> visited;
    collectFunctionFixtures(this, fid, fixtures, visited, 0);
    return fixtures.values();
}

// Mirrors collectFunctionFixtures() above, but collects function IDs
// (including fid itself) instead of resolving containers through to
// fixtures.
static void collectFunctionFunctions(const Doc *doc, quint32 fid,
                                     QSet<quint32> &functions,
                                     QSet<quint32> &visited, int depth)
{
    if (depth > 8 || doc == NULL || visited.contains(fid))
        return;
    visited.insert(fid);

    Function *f = doc->function(fid);
    if (f == NULL)
        return;

    functions.insert(fid);

    switch (f->type())
    {
        case Function::ChaserType:
        case Function::SequenceType:
        case Function::CollectionType:
        case Function::ShowType:
            foreach (quint32 mid, f->components())
                collectFunctionFunctions(doc, mid, functions, visited, depth + 1);
            break;
        default:
            break;
    }
}

QList<quint32> Doc::functionFunctions(quint32 fid) const
{
    QSet<quint32> functions;
    QSet<quint32> visited;
    collectFunctionFunctions(this, fid, functions, visited, 0);
    return functions.values();
}

void Doc::captureLastLook(quint32 fid, const QList<Universe*> &universes,
                          bool append) const
{
    if (m_lastLook == NULL)
        return;

    QList<LastLookEffect::ChannelHold> holds;
    foreach (quint32 fxiId, functionFixtures(fid))
    {
        Fixture *fxi = fixture(fxiId);
        if (fxi == NULL)
            continue;
        const int uniId = int(fxi->universe());
        if (uniId < 0 || uniId >= universes.size())
            continue;
        Universe *uni = universes.at(uniId);
        if (uni == NULL)
            continue;
        const int base = int(fxi->address());
        const int chans = int(fxi->channels());
        for (int c = 0; c < chans; c++)
        {
            const int addr = base + c;
            if (addr < 0 || addr >= UNIVERSE_SIZE)
                continue;
            LastLookEffect::ChannelHold h;
            h.fixtureId = fxiId;
            h.universeId = uniId;
            h.address = addr;
            h.value = uni->preGMValue(addr);
            holds.append(h);
        }
    }
    if (holds.isEmpty() == false)
    {
        if (append)
            m_lastLook->addHold(holds);
        else
            m_lastLook->hold(holds);
    }
}

void Doc::slotClearLastLookOnStart(quint32 fid)
{
    // Per-channel (per-fixture) yield: a newly started cue drops only the held
    // channels on the fixtures it drives; the rest of the last look keeps
    // holding. A full new state covering every held fixture clears it entirely.
    if (m_lastLook != NULL && m_lastLook->isActive())
        m_lastLook->releaseFixtures(functionFixtures(fid));
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
    /* An id a venue's house fixture uses (docs/design/VENUES.md) is skipped too: a
       house light of one venue never shares an id with a tour light. */
    VenueCorrections *venues = m_programmer != NULL ? m_programmer->venueCorrections() : NULL;
    while (m_fixtures.contains(m_latestFixtureId) == true ||
           m_latestFixtureId == Fixture::invalidId() ||
           (venues != NULL && venues->isReservedFixtureId(m_latestFixtureId)))
    {
        m_latestFixtureId++;
    }

    return m_latestFixtureId;
}

bool Doc::isVenueFixture(quint32 id) const
{
    return m_programmer != NULL && m_programmer->venueCorrections() != NULL
           && m_programmer->venueCorrections()->isVenueFixture(id);
}

bool Doc::addFixture(Fixture* fixture, quint32 id, bool crossUniverse)
{
    Q_ASSERT(fixture != NULL);

    quint32 i;

    // No ID given, this method can assign one
    if (id == Fixture::invalidId())
        id = createFixtureId();

    if (m_fixtures.contains(id) == true || id == Fixture::invalidId())
    {
        qWarning() << Q_FUNC_INFO << "a fixture with ID" << id << "already exists!";
        return false;
    }

    /* An inventory fixture holds no address: no overlap check, no address
       registration, no universe growth. Its 0/0 is a placeholder, not a
       claim on universe 1's first channels. */
    if (fixture->isPatched())
    {
        /* Past the most universes a show can have: refused, said, rather than
           added with no universe under it (a function writing to it would
           index past the universe list and crash). */
        if (!fitsUniverseLimit(fixture))
        {
            qWarning() << Q_FUNC_INFO << "fixture" << id << "on universe" << fixture->universe() + 1
                       << "is past the limit of" << InputOutputMap::maxUniverses() << "universes";
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
        /* Nor on channels the venue applied keeps for itself, as
           canPatchFixtures() refuses (docs/design/VENUES.md section 5). */
        if (isOnNotOurs(fixture->universe(), fixture->address(), fixture->channels()))
        {
            qWarning() << Q_FUNC_INFO << "fixture" << id << "is on channels the venue keeps (not ours)";
            return false;
        }
    }

    fixture->setID(id);
    m_fixtures.insert(id, fixture);
    m_fixturesListCacheUpToDate = false;

    /* Patch fixture change signals thru Doc */
    connect(fixture, SIGNAL(changed(quint32)),
            this, SLOT(slotFixtureChanged(quint32)));

    if (fixture->isPatched())
    {
        /* Keep track of fixture addresses */
        for (i = fixture->universeAddress();
             i < fixture->universeAddress() + fixture->channels(); i++)
        {
            m_addresses[i] = id;
        }

        applyFixtureToUniverses(fixture, crossUniverse);
    }

    emit fixtureAdded(id);
    setModified();

    return true;
}

bool Doc::fitsUniverseLimit(const Fixture *fixture)
{
    if (fixture == NULL || !fixture->isPatched())
        return true;
    const quint32 channels = qMax(fixture->channels(), quint32(1));
    /* The last channel's universe, so one running over a universe's end
       (cross-universe) counts where it ends. 64-bit: no wrap at the top. */
    const quint64 last = quint64(fixture->universeAddress()) + channels - 1;
    return (last >> 9) < InputOutputMap::maxUniverses();
}

void Doc::applyFixtureToUniverses(Fixture *fixture, bool crossUniverse)
{
    quint32 i;
    quint32 uni = fixture->universe();

    if (crossUniverse)
        uni = floor((fixture->universeAddress() + fixture->channels()) / 512);

    /* Past the most universes a show can have (Fixture::loadXML() unpatches
       one read from a file): adding universes up to it would hang. */
    if (uni >= InputOutputMap::maxUniverses())
    {
        qWarning() << Q_FUNC_INFO << "fixture" << fixture->name() << "is on universe" << uni
                   << ", past the limit of" << InputOutputMap::maxUniverses();
        return;
    }

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

        // the fixture may not fit in the universe (e.g. address too high)
        if (uni >= quint32(universes.count()) || addr >= 512)
            continue;

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
}

bool Doc::patchFixture(quint32 id, quint32 universe, quint32 address)
{
    return patchFixtures({ FixturePatch{ id, universe, address } });
}

bool Doc::isOnNotOurs(quint32 universe, quint32 address, quint32 channels, quint32 *first) const
{
    VenueCorrections *venues = m_programmer != NULL ? m_programmer->venueCorrections() : NULL;
    for (quint32 i = 0; venues != NULL && i < channels; i++)
    {
        if (venues->isNotOurs(universe, address + i))
        {
            if (first != NULL)
                *first = address + i;
            return true;
        }
    }
    return false;
}

bool Doc::canPatchFixtures(const QList<FixturePatch> &patches, QString *why,
                           const QHash<quint32, quint32> &channels) const
{
    auto refuse = [why](const QString &text) {
        if (why != NULL)
            *why = text;
        return false;
    };

    QSet<quint32> moving;
    foreach (const FixturePatch &p, patches)
    {
        Fixture *fxi = fixture(p.fixtureId);
        if (fxi == NULL)
            return refuse(QStringLiteral("no fixture with id %1").arg(p.fixtureId));
        if (moving.contains(p.fixtureId))
            return refuse(QStringLiteral("%1 is listed twice").arg(fxi->name()));
        moving.insert(p.fixtureId);
        if (fxi->isPatched() && mode() == Operate
            && (fxi->universe() != p.universe || fxi->address() != p.address))
            return refuse(QStringLiteral("%1 is patched and the show is running -- "
                                         "switch to Design mode to move it").arg(fxi->name()));
    }

    QHash<quint32, quint32> claimed;    // absolute channel -> fixture, after the move
    QHashIterator<quint32, quint32> it(m_addresses);
    while (it.hasNext())
    {
        it.next();
        if (!moving.contains(it.value()))
            claimed.insert(it.key(), it.value());
    }

    foreach (const FixturePatch &p, patches)
    {
        Fixture *fxi = fixture(p.fixtureId);
        const quint32 n = channels.value(p.fixtureId, fxi->channels());
        if (n == 0 || p.address + n > 512)
            return refuse(QStringLiteral("%1 (%2 ch) does not fit at universe %3, address %4")
                              .arg(fxi->name()).arg(n).arg(p.universe + 1).arg(p.address + 1));
        if (p.universe >= InputOutputMap::maxUniverses())
            return refuse(QStringLiteral("%1: universe %2 is past the limit of %3 universes")
                              .arg(fxi->name()).arg(p.universe + 1).arg(InputOutputMap::maxUniverses()));
        const quint32 base = (p.universe << 9) | p.address;
        /* Channels the venue applied keeps for itself (docs/design/VENUES.md section 5):
           nothing of ours is sent there, so nothing is patched there. */
        quint32 at = 0;
        if (isOnNotOurs(p.universe, p.address, n, &at))
            return refuse(QStringLiteral("%1 at universe %2, address %3 is on channels %4 keeps (not ours)")
                              .arg(fxi->name()).arg(p.universe + 1).arg(at + 1)
                              .arg(m_programmer->venueCorrections()->activeVenue()));
        for (quint32 i = base; i < base + n; i++)
        {
            if (claimed.contains(i))
            {
                Fixture *other = fixture(claimed.value(i));
                return refuse(QStringLiteral("%1 at universe %2, address %3 overlaps %4")
                                  .arg(fxi->name()).arg(p.universe + 1).arg(p.address + 1)
                                  .arg(other != NULL ? other->name() : QStringLiteral("another fixture")));
            }
            claimed.insert(i, p.fixtureId);
        }
    }
    return true;
}

bool Doc::patchFixtures(const QList<FixturePatch> &patches, QString *why)
{
    if (!canPatchFixtures(patches, why))
        return false;

    // Release every claim first, so the order of the list cannot matter.
    QSet<quint32> moving;
    foreach (const FixturePatch &p, patches)
        moving.insert(p.fixtureId);
    QMutableHashIterator <quint32, quint32> it(m_addresses);
    while (it.hasNext() == true)
    {
        it.next();
        if (moving.contains(it.value()))
            it.remove();
    }

    foreach (const FixturePatch &p, patches)
    {
        Fixture *fxi = fixture(p.fixtureId);
        fxi->blockSignals(true);
        fxi->setUniverse(p.universe);
        fxi->setAddress(p.address);
        fxi->setPatched(true);
        fxi->setAddressClash(false);
        fxi->blockSignals(false);

        const quint32 base = (p.universe << 9) | p.address;
        for (quint32 i = base; i < base + fxi->channels(); i++)
            m_addresses[i] = p.fixtureId;
        applyFixtureToUniverses(fxi, false);
    }

    setModified();
    foreach (const FixturePatch &p, patches)
        emit fixtureChanged(p.fixtureId);
    return true;
}

quint32 Doc::findFreeAddress(quint32 universe, quint32 channels, quint32 from,
                             const QSet<quint32> &ignore) const
{
    if (channels == 0 || channels > 512)
        return UINT_MAX;
    quint32 run = 0;
    for (quint32 a = from; a < 512; a++)
    {
        const quint32 abs = (universe << 9) | a;
        const bool taken = m_addresses.contains(abs) && !ignore.contains(m_addresses.value(abs));
        run = taken ? 0 : run + 1;
        if (run == channels)
            return a - channels + 1;
    }
    return UINT_MAX;
}

QList<quint32> Doc::addressClashes() const
{
    QList<quint32> out;
    foreach (Fixture *fxi, m_fixtures)
        if (fxi->addressClash())
            out << fxi->id();
    std::sort(out.begin(), out.end());
    return out;
}

bool Doc::unpatchFixture(quint32 id)
{
    Fixture *fxi = fixture(id);
    if (fxi == NULL || !fxi->isPatched() || mode() != Design)
        return false;

    QMutableHashIterator <uint,uint> it(m_addresses);
    while (it.hasNext() == true)
    {
        it.next();
        if (it.value() == id)
            it.remove();
    }

    // Back to the placeholder 0/0 a freshly staged fixture holds.
    fxi->blockSignals(true);
    fxi->setPatched(false);
    fxi->setAddressClash(false);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    fxi->blockSignals(false);

    setModified();
    emit fixtureChanged(id);
    return true;
}

bool Doc::canChangeFixtureMode(quint32 id, const QLCFixtureMode *newMode, QString *why) const
{
    Fixture *fxi = fixture(id);
    if (fxi == NULL)
    {
        if (why != NULL) *why = QStringLiteral("no such fixture");
        return false;
    }

    /* Fixture groups lay out heads by number: a mode with fewer heads would
       leave a group pointing at heads that are gone. Only then -- a fixture
       in no group, or one whose groups use only heads the new mode keeps,
       can change ("change the mode to accommodate where possible"). */
    if (newMode != NULL && fxi->fixtureMode() != NULL
        && newMode->heads().size() != fxi->fixtureMode()->heads().size())
    {
        const int keeps = qMax(1, int(newMode->heads().size()));
        foreach (FixtureGroup *grp, fixtureGroups())
        {
            foreach (const GroupHead &h, grp->headList())
            {
                if (h.fxi == id && h.head >= keeps)
                {
                    if (why != NULL)
                        *why = QStringLiteral("that mode has %1 head(s), this one has %2, and group "
                                              "\"%3\" lays out head %4")
                                   .arg(newMode->heads().size()).arg(fxi->fixtureMode()->heads().size())
                                   .arg(grp->name()).arg(h.head + 1);
                    return false;
                }
            }
        }
    }

    foreach (Function *f, functionsByType(Function::SceneType))
    {
        Scene *scene = static_cast<Scene *>(f);
        foreach (const SceneValue &sv, scene->values())
        {
            if (sv.fxi == id)
            {
                if (why != NULL)
                    *why = QStringLiteral("scene \"%1\" sets its channels directly, by number")
                               .arg(scene->name());
                return false;
            }
        }
    }
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
        {
            m_monitorProps->removeFixture(id);
            // The glass goes with the unit; a reused id starts with none.
            m_monitorProps->clearGoboSwaps(id);
        }

        if (m_powerDist != NULL)
            m_powerDist->removeFixture(id);

        m_programmer->removeFromProgrammerSelection(QList<quint32>() << id);
        m_programmer->unparkFixtures(QList<quint32>() << id);

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
        newFixture->setPatched(fixture->isPatched());
        newFixture->setConnection(fixture->connection());
        newFixture->setIPAddress(fixture->ipAddress());
        newFixture->setTransport(fixture->transport());
        newFixture->setStageLook(fixture->stageLook());
        newFixture->setOwner(fixture->owner());
        newFixture->setPowerProfile(fixture->powerProfile());
        newFixture->setOutputRules(fixture->outputRules());

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
             newFixture->isPatched() && i < newFixture->universeAddress() + newFixture->channels(); i++)
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
            // Not a light (a curtain): no load, and nothing missing either.
            if (!phys.emitsLight())
                continue;
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
    quint32 oldBase = UINT_MAX;     // where it was, if it was patched
    QMutableHashIterator <uint,uint> it(m_addresses);
    while (it.hasNext() == true)
    {
        it.next();
        if (it.value() == id)
        {
            qDebug() << Q_FUNC_INFO << " remove: " << it.key() << " val: " << it.value();
            oldBase = qMin(oldBase, quint32(it.key()));
            it.remove();
        }
    }

    /* A patched fixture moved onto channels the venue applied keeps (not
       ours) is refused, as canPatchFixtures() refuses it: it goes back to
       where it was. One that was nowhere goes in unpatched, keeping the
       address it asked for, as a clash on load does. A fixture that was
       already there (the venue came on after it) stays: it's the show's. */
    if (fxi->isPatched() && fxi->universeAddress() != oldBase
        && (!fitsUniverseLimit(fxi)
            || isOnNotOurs(fxi->universe(), fxi->address(), fxi->channels())))
    {
        if (!fitsUniverseLimit(fxi))
            qWarning() << Q_FUNC_INFO << "fixture" << id << "moved past the limit of"
                       << InputOutputMap::maxUniverses() << "universes: refused";
        else
            qWarning() << Q_FUNC_INFO << "fixture" << id << "moved onto channels the venue keeps (not ours): refused";
        fxi->blockSignals(true);
        if (oldBase != UINT_MAX)
        {
            fxi->setUniverse(oldBase >> 9);
            fxi->setAddress(oldBase & 0x01FF);
        }
        else
        {
            fxi->setPatched(false);
            fxi->setAddressClash(true);
        }
        fxi->blockSignals(false);
    }

    // An inventory fixture claims nothing (its 0/0 is a placeholder).
    for (uint i = fxi->universeAddress(); fxi->isPatched() && i < fxi->universeAddress() + fxi->channels(); i++)
    {
        /*
         * setting new universe and address calls this twice,
         * with an tmp wrong address after the first call (old address() + new universe()).
         * we only add if the channel is free, to prevent messing up things
         */
        if (m_addresses.contains(i))
            continue;

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

        // Detach from any scene that references this palette so no
        // dangling references remain after deletion.
        foreach (Function *f, functions())
        {
            Scene *s = qobject_cast<Scene*>(f);
            if (s != NULL && s->palettes().contains(id))
                s->removePalette(id);
        }
        // ...and from any effect look that binds it to a slot, so a palette
        // made later under the same id isn't picked up by it. (The effect
        // then falls back to its look colours, as with no binding.)
        foreach (QLCPalette *other, m_palettes)
        {
            if (other == NULL)
                continue;
            const QMap<QString, quint32> binds = other->effectPaletteBindings();
            for (auto it = binds.constBegin(); it != binds.constEnd(); ++it)
                if (it.value() == id)
                    other->clearEffectPaletteBinding(it.key());
        }

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
        Function* func = m_functions.value(id);
        Q_ASSERT(func != NULL);

        // Remove it from the MasterTimer's running list / start queue first
        // (synchronised with the tick) so the timer thread cannot be ticking it,
        // then delete it here on the Doc thread (preserving QObject affinity).
        // Deleting it without this removal would be a use-after-free in
        // timerTickFunctions if the function is running. A running one is
        // stopped there -- while it is still in the Doc, so anything its
        // stopping asks the Doc about (a chaser's steps) is still found.
        masterTimer()->removeFunction(func);
        m_functions.remove(id);

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

void Doc::notifyFunctionChanged(quint32 fid)
{
    slotFunctionChanged(fid);
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
    {
        m_monitorProps = new MonitorProperties();
        connect(m_monitorProps, &MonitorProperties::autoStageLooksChanged,
                this, [this](bool) { emit stageLooksChanged(); });
    }

    return m_monitorProps;
}

/*********************************************************************
 * Power Distribution
 *********************************************************************/

PowerDistribution *Doc::powerDistribution()
{
    if (m_powerDist == NULL)
        m_powerDist = new PowerDistribution();

    return m_powerDist;
}

/*********************************************************************
 * Network Topology
 *********************************************************************/

NetworkTopology *Doc::networkTopology()
{
    if (m_networkTopology == NULL)
        m_networkTopology = new NetworkTopology();

    return m_networkTopology;
}

TimecodeSource *Doc::timecodeSource()
{
    if (m_timecodeSource == NULL)
    {
        m_timecodeSource = new TimecodeSource(this);
        // LTC from the audio input, when it is the picked source
        new LtcReceiver(this, m_timecodeSource);
        m_timecodeCues = new TimecodeCues(this, m_timecodeSource);
    }

    return m_timecodeSource;
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

    /* Auto-open shutters as for a show saved without the setting (QLC+:
       off); its <ShutterAutoOpen>, when there is one, overrides. */
    m_shutterAutoOpen->beginLoad();

    /* Progress reporting: loading a big workspace is dominated by fixtures and
       fixture groups, and it happens with the main window already up, so
       without this the app just looks hung. Emit at most every 8 items to keep
       the signal (and the repaint it triggers) off the hot path. */
    QString progressStage;
    int progressCount = 0;
    auto reportProgress = [&](const QString &stage)
    {
        if (stage != progressStage)
        {
            progressStage = stage;
            progressCount = 0;
        }
        if ((++progressCount % 8) == 0)
            emit loadProgress(stage, progressCount);
    };

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
            reportProgress(tr("Loading fixtures"));
        }
        else if (doc.name() == KXMLQLCStagedFixtures)
        {
            // The Inventory's fixtures, as a .qxw keeps them (saveXML())
            while (doc.readNextStartElement())
            {
                if (doc.name() == KXMLFixture)
                    Fixture::loader(doc, this);
                else
                    doc.skipCurrentElement();
            }
            reportProgress(tr("Loading fixtures"));
        }
        else if (doc.name() == KXMLQLCFixtureGroup)
        {
            FixtureGroup::loader(doc, this);
            reportProgress(tr("Loading fixture groups"));
        }
        else if (doc.name() == KXMLQLCChannelsGroup)
        {
            ChannelsGroup::loader(doc, this);
        }
        else if (doc.name() == KXMLQLCPalette)
        {
            QLCPalette::loader(doc, this);
            reportProgress(tr("Loading palettes"));
        }
        else if (doc.name() == KXMLQLCFunction)
        {
            //qDebug() << doc.attributes().value("Name").toString();
            Function::loader(doc, this);
            reportProgress(tr("Loading functions"));
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
        else if (doc.name() == KXMLQLCSpeedMasters)
        {
            m_speedMasters->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCShowControl)
        {
            m_showControl->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCShowTree)
        {
            m_showTree->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCControlMap)
        {
            m_controlMap->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCTimecode)
        {
            timecodeSource()->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCPowerDistribution)
        {
            powerDistribution()->loadXML(doc, this);
        }
        else if (doc.name() == KXMLQLCNetworkTopology)
        {
            networkTopology()->loadXML(doc, this);
        }
        else if (doc.name() == KXMLQLCPark)
        {
            if (m_programmer != NULL)
                m_programmer->loadParkXML(doc);
            else
                doc.skipCurrentElement();
        }
        else if (doc.name() == KXMLQLCFocusCalls)
        {
            if (m_programmer != NULL)
                m_programmer->loadFocusCallsXML(doc);
            else
                doc.skipCurrentElement();
        }
        else if (doc.name() == KXMLQLCMark)
        {
            if (m_programmer != NULL)
                m_programmer->loadMarkXML(doc);
            else
                doc.skipCurrentElement();
        }
        else if (doc.name() == KXMLQLCOutputRuleSettings)
        {
            m_outputRuleKeeper->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCShutterAutoOpen)
        {
            m_shutterAutoOpen->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCWebAccess)
        {
            m_webAccess->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCTeaching)
        {
            m_teaching->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCOperate)
        {
            m_operate->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCMoveInBlack)
        {
            if (m_programmer != NULL)
                m_programmer->loadMoveInBlackXML(doc);
            else
                doc.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown engine tag:" << doc.name();
            doc.skipCurrentElement();
        }
    }

    // Palette folders from an older show, now the whole show is read.
    QLCPalette::migrateLegacyPaths(palettes());

    postLoad();

    /* An object's lock is its own again, beside its layer's: a show from
       when it only mirrored the layer is put right, and its "Locked" layer
       taken apart (MonitorProperties::migrateLegacyLocks()). */
    if (m_monitorProps != NULL)
        m_monitorProps->migrateLegacyLocks(powerDistribution(), networkTopology());

    /* A timecode source naming no universe of this show is as good as none:
       "any", as before it was saved. */
    if (m_timecodeSource != NULL && m_timecodeSource->sourceUniverse() >= 0
        && m_timecodeSource->sourceUniverse() != TimecodeSource::LtcUniverse)
    {
        bool found = false;
        for (quint32 i = 0; i < m_ioMap->universesCount() && !found; i++)
            found = (qint32(m_ioMap->getUniverseID(i)) == m_timecodeSource->sourceUniverse());
        if (!found)
            m_timecodeSource->setSourceUniverse(-1);
    }

    m_loadStatus = Loaded;
    emit loaded();

    /* Cut short (a truncated copy, a disk that filled up while saving) or
       not well formed: what came before the fault is loaded, the rest is
       not there. Said, and false, so the caller can keep the file from
       being saved over with less. */
    if (doc.hasError())
    {
        qWarning() << Q_FUNC_INFO << "XML error at line" << doc.lineNumber() << ":" << doc.errorString();
        appendToErrorLog(damagedFileMessage(doc));
        return false;
    }

    return true;
}

QString Doc::damagedFileMessage(const QXmlStreamReader &doc)
{
    return tr("This show file is damaged or incomplete (line %1: %2). "
              "It was opened as far as it could be read.")
            .arg(doc.lineNumber()).arg(doc.errorString().toHtmlEscaped());
}

bool Doc::saveXML(QXmlStreamWriter *doc, bool forStockQlc)
{
    Q_ASSERT(doc != NULL);

    /* Create the master Engine node */
    doc->writeStartElement(KXMLQLCEngine);

    if (startupFunction() != Function::invalidId())
        doc->writeAttribute(KXMLQLCStartupFunction, QString::number(startupFunction()));

    // First, so a loading show holds real output before its patch opens.
    m_teaching->saveXML(doc);

    m_ioMap->saveXML(doc);

    /* Write fixtures into an XML document */
    QList<Fixture *> staged;
    QListIterator <Fixture*> fxit(fixtures());
    while (fxit.hasNext() == true)
    {
        Fixture *fxi(fxit.next());
        Q_ASSERT(fxi != NULL);
        // A venue's house fixture is in the venue's file (docs/design/VENUES.md).
        if (isVenueFixture(fxi->id()))
            continue;
        if (forStockQlc && !fxi->isPatched() && !fxi->addressClash())
        {
            staged << fxi;
            continue;
        }
        fxi->saveXML(doc);
    }
    /* QLC+ 4 has no unpatched fixtures (see the header): they go where it
       doesn't look, right after the others, so they load in the same order. */
    if (!staged.isEmpty())
    {
        doc->writeStartElement(KXMLQLCStagedFixtures);
        foreach (Fixture *fxi, staged)
            fxi->saveXML(doc);
        doc->writeEndElement();
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

    if (m_powerDist != NULL)
        m_powerDist->saveXML(doc);

    m_speedMasters->saveXML(doc);
    m_shutterAutoOpen->saveXML(doc);
    m_webAccess->saveXML(doc);
    m_operate->saveXML(doc);
    m_outputRuleKeeper->saveXML(doc);
    m_showControl->saveXML(doc);
    m_showTree->saveXML(doc);
    m_controlMap->saveXML(doc);
    if (m_timecodeSource != NULL)
        m_timecodeSource->saveXML(doc);

    if (m_networkTopology != NULL)
        m_networkTopology->saveXML(doc);

    if (m_programmer != NULL)
        m_programmer->saveParkXML(doc);

    if (m_programmer != NULL)
        m_programmer->saveMarkXML(doc);

    if (m_programmer != NULL)
        m_programmer->saveFocusCallsXML(doc);

    if (m_programmer != NULL)
        m_programmer->saveMoveInBlackXML(doc);

    if (m_monitorProps != NULL)
    {
        m_monitorProps->keepPatternsWithShow(this);
        m_monitorProps->saveXML(doc, this, true);
    }

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

QHash<quint32, QString> Doc::effectFailures() const
{
    if (m_effectScriptRunner == nullptr)
        return QHash<quint32, QString>();
    return m_effectScriptRunner->failures();
}

/*****************************************************************************
 * Show Tree: the current production (forwarders)
 *****************************************************************************/

quint32 Doc::currentProduction() const
{
    return m_showTree->currentProduction();
}

bool Doc::setCurrentProduction(quint32 productionId)
{
    return m_showTree->setCurrentProduction(productionId);
}
