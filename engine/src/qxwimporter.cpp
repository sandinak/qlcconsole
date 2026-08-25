/*
  Q Light Controller
  qxwimporter.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QByteArray>
#include <QHash>
#include <QSet>

#include "qxwimporter.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "function.h"
#include "qlcpalette.h"
#include "scene.h"
#include "scenevalue.h"
#include "chaser.h"
#include "chaserstep.h"
#include "sequence.h"
#include "collection.h"
#include "efx.h"
#include "efxfixture.h"
#include "rgbmatrix.h"
#include "script.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "audio.h"
#include "video.h"

/*****************************************************************************
 * XML round-trip cloning
 *
 * Each of these takes an object living in sourceDoc, serializes it through
 * its own (already proven-correct, used for every real file save) saveXML(),
 * then re-parses that fragment into a brand new object owned by targetDoc.
 * The ID inserted is the CALLER's choice, not whatever the source XML says --
 * that's the whole point: Doc::loadXML()/Function::loader()/etc. always use
 * the embedded ID and silently drop the object if it collides, which is
 * exactly what Import must not do.
 *****************************************************************************/

static Fixture *cloneFixtureXml(Fixture *src, Doc *targetDoc)
{
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    src->saveXML(&writer);

    QXmlStreamReader reader(buf);
    reader.readNextStartElement();

    Fixture *copy = new Fixture(targetDoc);
    if (copy->loadXML(reader, targetDoc, targetDoc->fixtureDefCache()) == false)
    {
        delete copy;
        return NULL;
    }
    return copy;
}

static FixtureGroup *cloneFixtureGroupXml(FixtureGroup *src, Doc *targetDoc)
{
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    src->saveXML(&writer);

    QXmlStreamReader reader(buf);
    reader.readNextStartElement();

    FixtureGroup *copy = new FixtureGroup(targetDoc);
    if (copy->loadXML(reader) == false)
    {
        delete copy;
        return NULL;
    }
    return copy;
}

static QLCPalette *clonePaletteXml(QLCPalette *src, Doc *targetDoc)
{
    Q_UNUSED(targetDoc)
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    src->saveXML(&writer);

    QXmlStreamReader reader(buf);
    reader.readNextStartElement();

    QLCPalette *copy = new QLCPalette(src->type());
    if (copy->loadXML(reader) == false)
    {
        delete copy;
        return NULL;
    }
    return copy;
}

// Mirrors Function::loader()'s dispatch (function.cpp), but takes the ID to
// insert with as a parameter instead of reading it from the XML.
static Function *cloneFunctionXml(Function *src, Doc *targetDoc)
{
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    src->saveXML(&writer);

    QXmlStreamReader reader(buf);
    reader.readNextStartElement(); // <Function ...>

    QXmlStreamAttributes attrs = reader.attributes();
    QString name = attrs.value(KXMLQLCFunctionName).toString();
    Function::Type type = Function::stringToType(attrs.value(KXMLQLCFunctionType).toString());
    QString path;
    bool visible = true;
    Universe::BlendMode blendMode = Universe::NormalBlend;

    if (attrs.hasAttribute(KXMLQLCFunctionPath))
        path = attrs.value(KXMLQLCFunctionPath).toString();
    if (attrs.hasAttribute(KXMLQLCFunctionHidden))
        visible = false;
    if (attrs.hasAttribute(KXMLQLCFunctionBlendMode))
        blendMode = Universe::stringToBlendMode(attrs.value(KXMLQLCFunctionBlendMode).toString());

    Function *function = NULL;
    if (type == Function::SceneType)
        function = new Scene(targetDoc);
    else if (type == Function::ChaserType)
        function = new Chaser(targetDoc);
    else if (type == Function::CollectionType)
        function = new Collection(targetDoc);
    else if (type == Function::EFXType)
        function = new EFX(targetDoc);
    else if (type == Function::ScriptType)
        function = new Script(targetDoc);
    else if (type == Function::RGBMatrixType)
        function = new RGBMatrix(targetDoc);
    else if (type == Function::ShowType)
        function = new Show(targetDoc);
    else if (type == Function::SequenceType)
        function = new Sequence(targetDoc);
    else if (type == Function::AudioType)
        function = new Audio(targetDoc);
    else if (type == Function::VideoType)
        function = new Video(targetDoc);
    else
        return NULL;

    function->setName(name);
    function->setPath(path);
    function->setVisible(visible);
    function->setBlendMode(blendMode);

    if (function->loadXML(reader) == false)
    {
        delete function;
        return NULL;
    }
    return function;
}

/*****************************************************************************
 * Import
 *****************************************************************************/

QxwImportResult QxwImporter::import(Doc *sourceDoc, Doc *targetDoc,
                                     const QList<quint32> &fixtureIds,
                                     const QList<quint32> &fixtureGroupIds,
                                     const QList<quint32> &functionIds)
{
    QxwImportResult result;
    if (sourceDoc == NULL || targetDoc == NULL)
        return result;

    /*** 1. Expand the selection to its full dependency closure ***/
    QSet<quint32> allFixtures;
    QSet<quint32> allFunctions;
    QSet<quint32> allGroups;

    foreach (quint32 id, fixtureIds)
        allFixtures.insert(id);
    foreach (quint32 id, fixtureGroupIds)
        allGroups.insert(id);

    foreach (quint32 fid, functionIds)
    {
        foreach (quint32 mid, sourceDoc->functionFunctions(fid))
            allFunctions.insert(mid);
        foreach (quint32 fxi, sourceDoc->functionFixtures(fid))
            allFixtures.insert(fxi);
    }

    // Any RGBMatrix among the functions needs its FixtureGroup along too --
    // components()/functionFixtures() already resolve it to real fixture
    // IDs, but the group ID itself (and hence the group's own identity /
    // head layout) isn't referenced by that walk.
    foreach (quint32 fid, allFunctions)
    {
        RGBMatrix *rgb = qobject_cast<RGBMatrix *>(sourceDoc->function(fid));
        if (rgb != NULL && rgb->fixtureGroup() != FixtureGroup::invalidId())
            allGroups.insert(rgb->fixtureGroup());
    }

    // A Scene applies palettes by ID, so an imported Scene needs them too or it
    // lands referring to palettes that don't exist in the target.
    QSet<quint32> allPalettes;
    foreach (quint32 fid, allFunctions)
    {
        Scene *sc = qobject_cast<Scene *>(sourceDoc->function(fid));
        if (sc != NULL)
        {
            foreach (quint32 pid, sc->palettes())
                allPalettes.insert(pid);
        }
    }

    // Any selected/depended-on FixtureGroup pulls in its own member fixtures.
    foreach (quint32 gid, allGroups)
    {
        FixtureGroup *grp = sourceDoc->fixtureGroup(gid);
        if (grp != NULL)
        {
            foreach (quint32 fxi, grp->fixtureList())
                allFixtures.insert(fxi);
        }
    }

    /*** 2. Clone fixtures. Doc::addFixture() auto-allocates a fresh ID
     *      when passed invalidId(), so the collision decision and the
     *      actual ID assignment happen together here -- keep the source ID
     *      when it's free in the target Doc, otherwise let addFixture()
     *      pick one and read back whatever it picked. Also relocates to a
     *      free DMX address if the original one is already occupied. ***/
    QHash<quint32, quint32> fixtureMap;
    QHash<quint32, quint32> functionMap;
    QHash<quint32, quint32> groupMap;
    QHash<quint32, quint32> paletteMap;

    foreach (quint32 oldId, allFixtures)
    {
        Fixture *src = sourceDoc->fixture(oldId);
        if (src == NULL)
            continue;

        Fixture *copy = cloneFixtureXml(src, targetDoc);
        if (copy == NULL)
        {
            result.warnings << QObject::tr("Skipped fixture \"%1\": could not be read.").arg(src->name());
            continue;
        }

        bool collides = (targetDoc->fixture(oldId) != NULL);
        quint32 requestedId = collides ? Fixture::invalidId() : oldId;
        if (targetDoc->addFixture(copy, requestedId) == false)
        {
            // Only ID collisions were pre-checked above; a failure here
            // means the original DMX address overlaps something already
            // patched in the target -- find it a new home in the same
            // universe.
            quint32 freeAddr = targetDoc->findFreeAddress(copy->universe(), copy->channels());
            bool relocated = false;
            if (freeAddr != UINT_MAX)
            {
                copy->setAddress(freeAddr);
                relocated = targetDoc->addFixture(copy, requestedId);
            }
            if (relocated == false)
            {
                result.warnings << QObject::tr("Skipped fixture \"%1\": no free DMX address in universe %2.")
                                    .arg(src->name()).arg(copy->universe() + 1);
                delete copy;
                continue;
            }
            result.fixturesRelocated++;
        }
        if (collides == true)
            result.idsRemapped++;
        fixtureMap.insert(oldId, copy->id());
        result.fixturesImported++;
    }

    /*** 3. Clone fixture groups, remapping each head's fixture ID to match
     *      what fixtures actually got imported as. ***/
    foreach (quint32 oldId, allGroups)
    {
        FixtureGroup *src = sourceDoc->fixtureGroup(oldId);
        if (src == NULL)
            continue;

        FixtureGroup *copy = cloneFixtureGroupXml(src, targetDoc);
        if (copy == NULL)
        {
            result.warnings << QObject::tr("Skipped fixture group \"%1\": could not be read.").arg(src->name());
            continue;
        }

        QMap<QLCPoint, GroupHead> oldHeads = copy->headsMap();
        for (auto it = oldHeads.constBegin(); it != oldHeads.constEnd(); ++it)
        {
            GroupHead remapped(fixtureMap.value(it.value().fxi, it.value().fxi), it.value().head);
            copy->assignHead(it.key(), remapped);
        }

        bool collides = (targetDoc->fixtureGroup(oldId) != NULL);
        if (targetDoc->addFixtureGroup(copy, collides ? FixtureGroup::invalidId() : oldId) == false)
        {
            result.warnings << QObject::tr("Skipped fixture group \"%1\": could not be placed.").arg(src->name());
            delete copy;
            continue;
        }
        if (collides == true)
            result.idsRemapped++;
        groupMap.insert(oldId, copy->id());
        result.fixtureGroupsImported++;
    }

    /*** 3b. Clone palettes. Standalone -- nothing inside a palette refers to a
     *       fixture or function, so this only needs its own ID space remapped;
     *       the Scenes that reference them are fixed up in step 5. ***/
    foreach (quint32 oldId, allPalettes)
    {
        QLCPalette *src = sourceDoc->palette(oldId);
        if (src == NULL)
            continue;

        QLCPalette *copy = clonePaletteXml(src, targetDoc);
        if (copy == NULL)
        {
            result.warnings << QObject::tr("Skipped palette \"%1\": could not be read.").arg(src->name());
            continue;
        }

        bool collides = (targetDoc->palette(oldId) != NULL);
        if (targetDoc->addPalette(copy, collides ? QLCPalette::invalidId() : oldId) == false)
        {
            result.warnings << QObject::tr("Skipped palette \"%1\": could not be placed.").arg(src->name());
            delete copy;
            continue;
        }
        if (collides == true)
            result.idsRemapped++;
        paletteMap.insert(oldId, copy->id());
        result.palettesImported++;
    }

    /*** 4. Clone every function (IDs only -- cross-references still point
     *      at the SOURCE file's original IDs at this point) ***/
    foreach (quint32 oldId, allFunctions)
    {
        Function *src = sourceDoc->function(oldId);
        if (src == NULL)
            continue;

        Function *copy = cloneFunctionXml(src, targetDoc);
        if (copy == NULL)
        {
            result.warnings << QObject::tr("Skipped function \"%1\": could not be read.").arg(src->name());
            continue;
        }

        bool collides = (targetDoc->function(oldId) != NULL);
        if (targetDoc->addFunction(copy, collides ? Function::invalidId() : oldId) == false)
        {
            result.warnings << QObject::tr("Skipped function \"%1\": could not be placed.").arg(src->name());
            delete copy;
            continue;
        }
        if (collides == true)
            result.idsRemapped++;
        functionMap.insert(oldId, copy->id());
        result.functionsImported++;
    }

    /*** 5. Rewrite every cross-reference inside the newly-cloned functions
     *      to point at the imported (possibly remapped) IDs, per type ***/
    foreach (quint32 oldId, allFunctions)
    {
        quint32 newId = functionMap.value(oldId, oldId);
        Function *f = targetDoc->function(newId);
        if (f == NULL)
            continue;

        switch (f->type())
        {
        case Function::SceneType:
        {
            Scene *scene = qobject_cast<Scene *>(f);
            QList<SceneValue> values = scene->values();
            // Capture BEFORE clear(): it wipes m_palettes and m_paletteFade
            // along with the values.
            const QList<quint32> oldPalettes = scene->palettes();
            const QHash<quint32, Scene::PaletteFade> oldFades = scene->paletteFades();
            scene->clear();
            foreach (const SceneValue &scv, values)
            {
                SceneValue remapped(fixtureMap.value(scv.fxi, scv.fxi), scv.channel, scv.value);
                scene->setValue(remapped);
            }

            // Palette references, and the per-palette fade overrides keyed by
            // the same ids. Re-added in the captured order because order is
            // precedence: later entries override earlier ones.
            foreach (quint32 pid, oldPalettes)
                scene->addPalette(paletteMap.value(pid, pid));
            for (auto it = oldFades.constBegin(); it != oldFades.constEnd(); ++it)
                scene->setPaletteFade(paletteMap.value(it.key(), it.key()),
                                      it.value().fadeInMs, it.value().fadeOutMs);
            break;
        }
        case Function::EFXType:
        {
            EFX *efx = qobject_cast<EFX *>(f);
            foreach (EFXFixture *ef, efx->fixtures())
            {
                const GroupHead &h = ef->head();
                if (fixtureMap.contains(h.fxi))
                    ef->setHead(GroupHead(fixtureMap.value(h.fxi), h.head));
            }
            break;
        }
        case Function::RGBMatrixType:
        {
            RGBMatrix *rgb = qobject_cast<RGBMatrix *>(f);
            quint32 grp = rgb->fixtureGroup();
            if (groupMap.contains(grp))
                rgb->setFixtureGroup(groupMap.value(grp));
            break;
        }
        case Function::ChaserType:
        case Function::SequenceType:
        {
            Chaser *chaser = qobject_cast<Chaser *>(f);
            QList<ChaserStep> steps = chaser->steps();
            for (int i = 0; i < steps.size(); i++)
            {
                if (functionMap.contains(steps.at(i).fid))
                {
                    ChaserStep step = steps.at(i);
                    step.fid = functionMap.value(step.fid);
                    chaser->replaceStep(step, i);
                }
            }
            break;
        }
        case Function::CollectionType:
        {
            Collection *coll = qobject_cast<Collection *>(f);
            foreach (quint32 memberId, coll->components())
            {
                if (functionMap.contains(memberId) && functionMap.value(memberId) != memberId)
                {
                    coll->removeFunction(memberId);
                    coll->addFunction(functionMap.value(memberId));
                }
            }
            break;
        }
        case Function::ShowType:
        {
            Show *show = qobject_cast<Show *>(f);
            foreach (Track *track, show->tracks())
            {
                quint32 sceneId = track->getSceneID();
                if (functionMap.contains(sceneId))
                    track->setSceneID(functionMap.value(sceneId));

                foreach (ShowFunction *sf, track->showFunctions())
                {
                    if (functionMap.contains(sf->functionID()))
                        sf->setFunctionID(functionMap.value(sf->functionID()));
                }
            }
            break;
        }
        default:
            // Script/Audio/Video: no fixture/function ID fields to rewrite
            // (Script may reference IDs as literal numbers in its text --
            // not safely rewritable; imported verbatim, same as upstream
            // QLC+'s own copy/paste does).
            break;
        }
    }

    return result;
}
