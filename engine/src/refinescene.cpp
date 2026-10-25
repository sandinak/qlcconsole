/*
  Q Light Controller Plus
  refinescene.cpp

  Copyright (C) Branson Matheson

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

/*
  Refine on a Build scene (FOCUS_REFINE_DESIGN.md section 6B): the same
  session, the same controller, on a scene of Build's Workbench instead of
  a Show Tree cue.

  The nearest equivalents of the cue's parts:
  - the scene is the LAYER (the Workbench's stack of scenes: the triggers);
  - its group rows (and its loose fixtures, as one) are the GROUPS;
  - its looks are the scene's palettes, which play on everything it lights
    but the fixtures each one leaves out (Scene::excludedFixtures);
  - an EXCEPTION is a fixture left out of that attribute's palettes with
    the value as the scene's own raw values on it -- what the Show Tree's
    child layers bake to (ShowTreeController::bake()).
*/

#include "showtreecontroller.h"
#include "refinesession.h"
#include "fixturegroup.h"
#include "qlcfixturemode.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scenevalue.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
const quint32 kInvalid = 0xFFFFFFFF;

/** The palette types that set @p key. */
QList<int> typesFor(const QString &key)
{
    if (key == ShowTree::KeyPosition)
        return { QLCPalette::PanTilt, QLCPalette::Pan, QLCPalette::Tilt, QLCPalette::Aim };
    if (key == ShowTree::KeyZoom)
        return { QLCPalette::Zoom };
    if (key == ShowTree::KeyBeam)
        return { QLCPalette::Beam };
    if (key == ShowTree::KeyColour)
        return { QLCPalette::Color };
    if (key == ShowTree::KeyGobo)
        return { QLCPalette::Gobo };
    if (key == ShowTree::KeyDimmer)
        return { QLCPalette::Dimmer };
    return {};
}

/** Whether channel @p ch carries @p key. */
bool channelOf(const QLCChannel *ch, const QString &key)
{
    if (ch == nullptr)
        return false;
    if (key == ShowTree::KeyPosition)
        return ch->group() == QLCChannel::Pan || ch->group() == QLCChannel::Tilt;
    if (key == ShowTree::KeyZoom)
        return ch->preset() == QLCChannel::BeamZoomSmallBig || ch->preset() == QLCChannel::BeamZoomBigSmall;
    if (key == ShowTree::KeyBeam)
        return ch->preset() == QLCChannel::BeamFocusNearFar || ch->preset() == QLCChannel::BeamFocusFarNear;
    if (key == ShowTree::KeyColour)
        return ch->group() == QLCChannel::Colour
            || (ch->group() == QLCChannel::Intensity && ch->colour() != QLCChannel::NoColour);
    if (key == ShowTree::KeyGobo)
        return ch->group() == QLCChannel::Gobo;
    return false;
}
}

bool RefineSession::startScene(const QList<quint32> &sceneIds, quint32 first, QString *why)
{
    auto refuse = [why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        return false;
    };
    QList<quint32> scenes;
    for (quint32 sid : sceneIds)
        if (qobject_cast<Scene *>(m_doc->function(sid)) != nullptr && !scenes.contains(sid))
            scenes << sid;
    if (scenes.isEmpty())
        return refuse(tr("Choose a scene to focus."));
    QString busy;
    if (m_startGuard && !m_startGuard(&busy))
        return refuse(busy);
    if (m_active)
        stop();
    m_sceneMode = true;
    m_layers = scenes;
    bool any = false;
    for (int i = 0; i < scenes.size() && !any; ++i)
    {
        m_layerIndex = i;
        any = !sceneFixtures().isEmpty();
    }
    if (!any)
    {
        m_sceneMode = false;
        m_layers.clear();
        m_layerIndex = -1;
        return refuse(tr("This scene lights no fixtures to focus."));
    }
    m_cue = kInvalid;
    m_layerIndex = qMax(0, scenes.indexOf(first));
    m_scope = LayerScope;
    m_groupIndex = m_fixtureIndex = 0;
    m_page = BeamPage;
    m_edits.clear();
    m_editOrder.clear();
    m_kept.clear();
    m_levelCache.clear();
    m_active = true;
    rebuildWrites();
    emit started();
    emit changed();
    flashSelection();
    say(tr("Focusing scene %1. %2").arg(layerName(currentLayer()), wherePhrase()));
    return true;
}

Scene *RefineSession::sceneAt(quint32 sceneId) const
{
    return m_sceneMode ? qobject_cast<Scene *>(m_doc->function(sceneId)) : nullptr;
}

QList<quint32> RefineSession::sceneFixtures() const
{
    QList<quint32> out;
    Scene *s = sceneAt(currentLayer());
    if (s == nullptr)
        return out;
    for (quint32 gid : s->fixtureGroups())
        if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
            for (quint32 fid : g->fixtureList())
                if (!out.contains(fid) && m_doc->fixture(fid) != nullptr)
                    out << fid;
    for (quint32 fid : s->fixtures())
        if (!out.contains(fid) && m_doc->fixture(fid) != nullptr)
            out << fid;
    return out;
}

QList<ShowTree::Target> RefineSession::sceneGroups() const
{
    QList<ShowTree::Target> out;
    Scene *s = sceneAt(currentLayer());
    if (s == nullptr)
        return out;
    QList<quint32> inGroups;
    for (quint32 gid : s->fixtureGroups())
        if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
        {
            ShowTree::Target t;
            t.groups << gid;
            out << t;
            inGroups << g->fixtureList();
        }
    ShowTree::Target loose;
    for (quint32 fid : s->fixtures())
        if (!inGroups.contains(fid))
            loose.fixtures << fid;
    if (!loose.fixtures.isEmpty())
        out << loose;
    // The scene's rows as one piece is the scene itself: no Group scope.
    return out.size() > 1 ? out : QList<ShowTree::Target>();
}

const ShowTree::Look *RefineSession::sceneLookFor(quint32 fid, const QString &key, QList<quint32> *lit) const
{
    Scene *s = sceneAt(currentLayer());
    if (s == nullptr)
        return nullptr;
    const QList<int> types = typesFor(key);
    m_sceneLook = ShowTree::Look();
    m_sceneLook.key = key;
    for (quint32 pid : s->palettes())
        if (const QLCPalette *p = m_doc->palette(pid))
            if (types.contains(int(p->type())) && !s->excludedFixtures(pid).contains(fid))
                m_sceneLook.palettes << pid;
    if (m_sceneLook.palettes.isEmpty())
        return nullptr;
    if (lit != nullptr)
        *lit = sceneFixtures();
    return &m_sceneLook;
}

bool RefineSession::sceneHasOwn(quint32 fid, const QString &key) const
{
    Scene *s = sceneAt(currentLayer());
    if (s == nullptr || sceneLookFor(fid, key) != nullptr)
        return false;
    Fixture *fx = m_doc->fixture(fid);
    for (const SceneValue &sv : s->values())
        if (sv.fxi == fid && fx != nullptr && channelOf(fx->channel(sv.channel), key))
            return true;
    return false;
}

QHash<quint32, uchar> RefineSession::sceneRawValues(quint32 fid) const
{
    QHash<quint32, uchar> out;
    if (Scene *s = sceneAt(currentLayer()))
        for (const SceneValue &sv : s->values())
            if (sv.fxi == fid)
                out.insert(sv.channel, sv.value);
    return out;
}

QStringList RefineSession::paletteUsers(quint32 paletteId, quint32 exceptScene) const
{
    QStringList out;
    for (const Function *f : m_doc->functions())
        if (const Scene *sc = qobject_cast<const Scene *>(f))
            if (sc->id() != exceptScene && sc->palettes().contains(paletteId))
                out << sc->name();
    out.sort();
    return out;
}

bool RefineSession::sceneKeep(QString *why)
{
    Scene *s = sceneAt(currentLayer());
    PaletteOps *ops = m_doc->paletteOps();
    if (s == nullptr || ops == nullptr)
        return false;
    QList<QString> order = m_editOrder;
    std::stable_sort(order.begin(), order.end(), [this](const QString &a, const QString &b) {
        return int(m_edits[a].scope) < int(m_edits[b].scope);
    });
    const quint32 sid = s->id();

    /* The palettes a Layer-scope edit moves in place (as Build's Look
       Editor does), each with how it moves. */
    struct InPlace { quint32 pid; QString key; Edit edit; };
    QList<InPlace> inPlace;
    QHash<QString, QStringList> rawKeys;     // edit key -> attributes as raw values
    for (const QString &k : order)
    {
        const Edit &e = m_edits[k];
        if (e.scope != LayerScope)
            continue;
        for (const QPair<bool, QString> &a : { qMakePair(e.hasPos, ShowTree::KeyPosition),
                                               qMakePair(e.hasZoom, ShowTree::KeyZoom),
                                               qMakePair(e.hasFocus, ShowTree::KeyBeam) })
        {
            if (!a.first)
                continue;
            const ShowTree::Look *look = sceneLookFor(e.fixtures.value(0, kInvalid), a.second);
            if (look == nullptr)
                rawKeys[k] << a.second;
            else
                for (quint32 pid : look->palettes)
                    inPlace << InPlace{ pid, a.second, e };
        }
    }

    /* Board Q116: a palette other scenes (or cues) use changes there too --
       say which, and only go on when A is pressed again. */
    if (!m_sharedConfirmed)
    {
        QStringList users;
        QStringList names;
        for (const InPlace &ip : inPlace)
        {
            const QStringList u = paletteUsers(ip.pid, sid);
            if (u.isEmpty())
                continue;
            if (const QLCPalette *p = m_doc->palette(ip.pid))
                if (!names.contains(p->name()))
                    names << p->name();
            for (const QString &n : u)
                if (!users.contains(n))
                    users << n;
        }
        if (!users.isEmpty())
        {
            m_sharedWarning = users.size() == 1
                ? tr("%1 is also used by %2. Press A again to change it there too.")
                  .arg(names.join(QStringLiteral(", ")), users.first())
                : tr("%1 is also used by %2 and %3 more. Press A again to change them all.")
                  .arg(names.join(QStringLiteral(", ")), users.first()).arg(users.size() - 1);
            m_sharedUsers = users;
            m_sharedConfirmed = true;           // the next A goes on
            emit changed();
            warn(m_sharedWarning);
            if (why != nullptr)
                *why = m_sharedWarning;
            return false;
        }
    }

    // What each edit sends, read before anything in the scene changes.
    QHash<QString, QList<SceneValue>> values;
    for (const QString &k : order)
        values.insert(k, previewValues(m_edits[k]));
    if (m_doc->mode() == Doc::Operate)
        for (const QString &k : order)
            m_kept << values.value(k);

    QList<quint32> pids;
    for (const InPlace &ip : inPlace)
        if (!pids.contains(ip.pid))
            pids << ip.pid;

    // Board Q117: everything A keeps -- palettes and the scene -- one step.
    const bool ok = ops->sceneValueStep(tr("Focus this look: %1").arg(s->name()), { sid }, pids, [&]() {
        for (const InPlace &ip : inPlace)
        {
            QLCPalette *p = m_doc->palette(ip.pid);
            if (p == nullptr)
                continue;
            const Edit &e = ip.edit;
            QVariantList v = p->values();
            if (ip.key == ShowTree::KeyPosition)
            {
                // Degrees moved by the nudge (an Aim was refused): the fan kept.
                if (p->type() == QLCPalette::PanTilt && v.size() >= 2)
                {
                    v[0] = qRound(v.at(0).toFloat() + e.pan);
                    v[1] = qRound(v.at(1).toFloat() + e.tilt);
                }
                else if (!v.isEmpty() && (p->type() == QLCPalette::Pan || p->type() == QLCPalette::Tilt))
                    v[0] = qRound(v.at(0).toFloat() + (p->type() == QLCPalette::Pan ? e.pan : e.tilt));
            }
            else if (ip.key == ShowTree::KeyZoom)
                v = QVariantList{ double(e.zoom) };
            else if (v.isEmpty())
                v << e.focus;
            else
                v[0] = e.focus;
            p->setValues(v);
        }
        for (const QString &k : order)
        {
            const Edit &e = m_edits[k];
            if (e.scope == LayerScope)
            {
                // A colour or gobo: that palette in place of the scene's.
                for (const QPair<quint32, int> &sw : { qMakePair(e.colour, int(QLCPalette::Color)),
                                                       qMakePair(e.gobo, int(QLCPalette::Gobo)) })
                {
                    if (sw.first == kInvalid)
                        continue;
                    for (quint32 pid : s->palettes())
                        if (const QLCPalette *p = m_doc->palette(pid))
                            if (int(p->type()) == sw.second)
                                s->removePalette(pid);
                    s->addPalette(sw.first);
                }
                // No palette for it: the scene's raw values.
                for (const SceneValue &sv : values.value(k))
                    if (Fixture *fx = m_doc->fixture(sv.fxi))
                        for (const QString &key : rawKeys.value(k))
                            if (channelOf(fx->channel(sv.channel), key))
                                s->setValue(sv);
                continue;
            }
            /* A group or a fixture: the exception -- left out of that
               attribute's palettes, with the value as the scene's own. */
            QStringList keys;
            if (e.hasPos)
                keys << ShowTree::KeyPosition;
            if (e.hasZoom)
                keys << ShowTree::KeyZoom;
            if (e.hasFocus)
                keys << ShowTree::KeyBeam;
            if (e.colour != kInvalid)
                keys << ShowTree::KeyColour;
            if (e.gobo != kInvalid)
                keys << ShowTree::KeyGobo;
            for (const QString &key : keys)
            {
                const QList<int> types = typesFor(key);
                for (quint32 pid : s->palettes())
                {
                    const QLCPalette *p = m_doc->palette(pid);
                    if (p == nullptr || !types.contains(int(p->type())))
                        continue;
                    QList<quint32> ex = s->excludedFixtures(pid);
                    for (quint32 fid : e.fixtures)
                        if (!ex.contains(fid))
                            ex << fid;
                    s->setExcludedFixtures(pid, ex);
                }
            }
            for (quint32 fid : e.fixtures)
                if (!s->fixtures().contains(fid))
                    s->addFixture(fid);
            for (const SceneValue &sv : values.value(k))
                if (Fixture *fx = m_doc->fixture(sv.fxi))
                    for (const QString &key : keys)
                        if (channelOf(fx->channel(sv.channel), key))
                            s->setValue(sv);
        }
        return true;
    });
    m_sharedConfirmed = false;
    m_sharedWarning.clear();
    m_sharedUsers.clear();
    for (const Function *f : m_doc->functions())
        if (Scene *sc = const_cast<Scene *>(qobject_cast<const Scene *>(f)))
            if (sc->isRunning())
                sc->requestPaletteRefresh();
    if (!ok)
    {
        if (why != nullptr && why->isEmpty())
            *why = tr("The scene did not take the change.");
        m_kept.clear();
        warn(tr("Not kept."));
        return false;
    }
    const QString light = keptLight();
    m_edits.clear();
    m_editOrder.clear();
    m_levelCache.clear();
    rebuildWrites();
    emit changed();
    emit kept();
    const QString scene = s->name();
    confirm(m_doc->mode() == Doc::Operate
            ? tr("%1 kept in %2. It plays from the scene's next start.").arg(light, scene)
            : tr("%1 kept in %2.").arg(light, scene));
    return true;
}

bool RefineSession::sceneClearException()
{
    Scene *s = sceneAt(currentLayer());
    PaletteOps *ops = m_doc->paletteOps();
    if (s == nullptr || ops == nullptr)
        return false;
    const QString key = editKey();
    if (m_edits.remove(key) > 0)
        m_editOrder.removeAll(key);
    QStringList keys;
    if (m_page == BeamPage)
        keys << ShowTree::KeyPosition << ShowTree::KeyZoom << ShowTree::KeyBeam;
    else
        keys << (m_page == ColourPage ? ShowTree::KeyColour : ShowTree::KeyGobo);
    const QList<quint32> fixtures = scopeFixtures();
    bool any = false;
    for (quint32 fid : fixtures)
        for (const QString &k : keys)
            any = any || sceneHasOwn(fid, k) || [&]() {
                for (quint32 pid : s->palettes())
                    if (s->excludedFixtures(pid).contains(fid))
                        return true;
                return false;
            }();
    if (!any)
    {
        rebuildWrites();
        emit changed();
        warn(tr("No exception here."));
        return false;
    }
    ops->sceneStep(tr("Focus this look: %1: clear exception").arg(s->name()), { s->id() }, [&]() {
        for (const QString &k : keys)
        {
            const QList<int> types = typesFor(k);
            for (quint32 pid : s->palettes())
            {
                const QLCPalette *p = m_doc->palette(pid);
                if (p == nullptr || !types.contains(int(p->type())))
                    continue;
                QList<quint32> ex = s->excludedFixtures(pid);
                for (quint32 fid : fixtures)
                    ex.removeAll(fid);
                s->setExcludedFixtures(pid, ex);
            }
            for (quint32 fid : fixtures)
                if (Fixture *fx = m_doc->fixture(fid))
                    for (quint32 c = 0; c < fx->channels(); ++c)
                        if (channelOf(fx->channel(c), k))
                            s->unsetValue(fid, c);
        }
        return true;
    });
    if (s->isRunning())
        s->requestPaletteRefresh();
    rebuildWrites();
    emit changed();
    confirm(tr("Exception cleared."));
    return true;
}
