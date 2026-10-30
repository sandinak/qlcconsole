/*
  QLC+ stress test - parametric show builder implementation
*/

#include "showbuilder.h"

#include <QRandomGenerator>
#include <QStringList>
#include <QColor>
#include <QSize>
#include <QDir>
#include <QtGlobal>
#include <QDebug>
#include <cstdio>

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "rgbmatrix.h"
#include "rgbalgorithm.h"
#include "rgbscriptscache.h"
#include "efx.h"
#include "efxfixture.h"
#include "collection.h"
#include "script.h"
#include "sequence.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "function.h"
#include "grouphead.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfile.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturehead.h"
#include "monitorproperties.h"
#include "powerdistribution.h"
#include "fixturepower.h"
#include <QVector3D>

// RGBMatrix script algorithms that ship with QLC+ (display names as used by
// RGBAlgorithm::algorithm()). Span cheap and expensive per-tick patterns.
static const char *kMatrixAlgos[] = {
    "Stripes", "Balls", "Plasma", "Noise", "Fireworks", "Gradient"
};
static const int kMatrixAlgoCount = int(sizeof(kMatrixAlgos) / sizeof(kMatrixAlgos[0]));

static QList<Function*> buildMixedRigShow(Doc *doc, const ShowSpec &spec);

QList<Function*> buildShow(Doc *doc, const ShowSpec &spec,
                           const QString &fixtureDir, const QString &scriptsDir)
{
    if (spec.rig == ShowSpec::MixedRig)
    {
        if (doc->rgbScriptsCache()->load(QDir(scriptsDir)) == false)
            qWarning() << "stress: RGB scripts NOT loaded from" << scriptsDir;
        QDir dir(fixtureDir);
        dir.setFilter(QDir::Files);
        dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
        if (doc->fixtureDefCache()->loadMap(dir) == false)
            qWarning() << "stress: failed to load fixture map from" << fixtureDir;
        return buildMixedRigShow(doc, spec);
    }

    QList<Function*> topLevel;
    QRandomGenerator rng(spec.seed);

    // RGB script cache must be loaded or RGBAlgorithm::algorithm() returns null
    // and every RGBMatrix becomes a no-op (no per-tick work).
    if (doc->rgbScriptsCache()->load(QDir(scriptsDir)) == false ||
        doc->rgbScriptsCache()->names().isEmpty())
        qWarning() << "stress: RGB scripts NOT loaded from" << scriptsDir
                   << "- matrices will be no-ops!";

    // ---- fixture definitions -------------------------------------------------
    QDir dir(fixtureDir);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    if (doc->fixtureDefCache()->loadMap(dir) == false)
        qWarning() << "stress: failed to load fixture map from" << fixtureDir;

    QLCFixtureDef *rgbDef = doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    QLCFixtureMode *rgbMode = nullptr;
    if (rgbDef != nullptr)
    {
        foreach (QLCFixtureMode *m, rgbDef->modes())
            if (m->name() == "RGB") { rgbMode = m; break; }
        if (rgbMode == nullptr && !rgbDef->modes().isEmpty())
            rgbMode = rgbDef->modes().first();
    }

    const int fpu = qMin(spec.fixturesPerUni, 512 / ShowSpec::channelsPerFix);

    // ---- fixtures ------------------------------------------------------------
    QList<quint32> fixtureIds;
    fixtureIds.reserve(spec.universes * fpu);
    for (int u = 0; u < spec.universes; u++)
    {
        for (int i = 0; i < fpu; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1-F%2").arg(u).arg(i));
            // Address first, then universe: Fixture::setAddress() keeps only 7
            // universe bits (m_address & 0xFE00), so setting the address after
            // the universe moves a fixture on universe 128+ to (universe % 128).
            // Fixture::loadXML() sets them in this order too.
            fx->setAddress(i * ShowSpec::channelsPerFix);
            fx->setUniverse(u);
            if (rgbDef != nullptr && rgbMode != nullptr)
                fx->setFixtureDefinition(rgbDef, rgbMode);
            else
                fx->setChannels(ShowSpec::channelsPerFix); // fallback: headless intensity
            if (doc->addFixture(fx) == false)
            {
                delete fx;
                continue;
            }
            fixtureIds.append(fx->id());
        }
    }
    fprintf(stdout, "stress: created %d fixtures across %d universes (%d channels)\n",
            int(fixtureIds.size()), spec.universes,
            int(fixtureIds.size()) * ShowSpec::channelsPerFix);

    if (fixtureIds.isEmpty())
        return topLevel;

    // ---- scenes --------------------------------------------------------------
    QList<quint32> sceneIds;
    for (int s = 0; s < spec.scenes; s++)
    {
        Scene *scene = new Scene(doc);
        scene->setName(QString("Scene %1").arg(s));
        // touch a random slice of fixtures so different scenes overlap channels
        int touch = spec.sceneFixtures > 0 ? qMin(spec.sceneFixtures, int(fixtureIds.size()))
                                           : qMax(1, int(fixtureIds.size()) / 8);
        for (int t = 0; t < touch; t++)
        {
            quint32 fid = fixtureIds.at(rng.bounded(fixtureIds.size()));
            for (int c = 0; c < ShowSpec::channelsPerFix; c++)
                scene->setValue(fid, c, uchar(rng.bounded(256)));
        }
        scene->setFadeInSpeed(rng.bounded(2000));
        scene->setFadeOutSpeed(rng.bounded(2000));
        if (doc->addFunction(scene) == false) { delete scene; continue; }
        sceneIds.append(scene->id());
        topLevel.append(scene);
    }

    // ---- chasers (step through scenes) --------------------------------------
    QList<quint32> chaserIds;
    for (int c = 0; c < spec.chasers && !sceneIds.isEmpty(); c++)
    {
        Chaser *chaser = new Chaser(doc);
        chaser->setName(QString("Chaser %1").arg(c));
        if (spec.chaserPerStep)
        {
            chaser->setDurationMode(Chaser::PerStep);
            chaser->setFadeInMode(Chaser::PerStep);
            chaser->setFadeOutMode(Chaser::PerStep);
        }
        else
            chaser->setDurationMode(Chaser::Common);   // duration 0: a step every tick
        int steps = 4 + rng.bounded(12);
        for (int st = 0; st < steps; st++)
        {
            quint32 sid = sceneIds.at(rng.bounded(sceneIds.size()));
            chaser->addStep(ChaserStep(sid, rng.bounded(500), 200 + rng.bounded(800), rng.bounded(500)));
        }
        chaser->setRunOrder(Function::Loop);
        chaser->setDirection(Function::Forward);
        if (doc->addFunction(chaser) == false) { delete chaser; continue; }
        chaserIds.append(chaser->id());
        topLevel.append(chaser);
    }

    // ---- RGB matrices (the CPU-heavy per-tick functions) --------------------
    QList<quint32> matrixIds;
    for (int m = 0; m < spec.matrices; m++)
    {
        // build a fixture group covering a chunk of fixtures laid out as a grid
        FixtureGroup *grp = new FixtureGroup(doc);
        grp->setName(QString("Group %1").arg(m));
        int side = 8 + rng.bounded(9);               // 8..16 -> up to 256 heads
        grp->setSize(QSize(side, side));
        int want = side * side;
        for (int k = 0; k < want; k++)
            grp->assignFixture(fixtureIds.at(rng.bounded(fixtureIds.size())));
        if (doc->addFixtureGroup(grp) == false) { delete grp; continue; }

        RGBMatrix *mtx = new RGBMatrix(doc);
        mtx->setName(QString("Matrix %1").arg(m));
        mtx->setFixtureGroup(grp->id());
        RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc, kMatrixAlgos[m % kMatrixAlgoCount]);
        if (algo != nullptr)
            mtx->setAlgorithm(algo);
        mtx->setColor(0, QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
        mtx->setColor(1, QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
        mtx->setFadeInSpeed(0);
        mtx->setDuration(100 + rng.bounded(400)); // step every 100-500ms
        if (doc->addFunction(mtx) == false) { delete mtx; continue; }
        matrixIds.append(mtx->id());
        topLevel.append(mtx);
    }

    // ---- EFX -----------------------------------------------------------------
    for (int e = 0; e < spec.efx; e++)
    {
        EFX *efx = new EFX(doc);
        efx->setName(QString("EFX %1").arg(e));
        efx->setAlgorithm(EFX::Algorithm(e % 6)); // Circle..Square
        efx->setWidth(64 + rng.bounded(64));
        efx->setHeight(64 + rng.bounded(64));
        int heads = 4 + rng.bounded(16);
        for (int h = 0; h < heads; h++)
        {
            EFXFixture *ef = new EFXFixture(efx);
            ef->setHead(GroupHead(fixtureIds.at(rng.bounded(fixtureIds.size())), 0));
            ef->setMode(EFXFixture::RGB);
            efx->addFixture(ef);
        }
        efx->setDuration(2000 + rng.bounded(3000));
        if (doc->addFunction(efx) == false) { delete efx; continue; }
        topLevel.append(efx);
    }

    // ---- collections (group scenes + chasers) -------------------------------
    if (spec.efxPerUniverse)
    {
        for (int u = 0; u < spec.universes; u++)
        {
            EFX *efx = new EFX(doc);
            efx->setName(QString("UniverseWave %1").arg(u));
            efx->setAlgorithm(EFX::Circle);
            efx->setWidth(127);
            efx->setHeight(127);
            // Serial: every head at its own phase, so some channel of the
            // universe changes on every tick (Parallel heads all sit at the
            // same point, and near the circle's extremes two ticks can round
            // to the same 8-bit values -- an unchanged universe sends no frame)
            efx->setPropagationMode(EFX::Serial);
            for (int i = 0; i < fpu; i++)
            {
                const int idx = u * fpu + i;
                if (idx >= fixtureIds.size())
                    break;
                EFXFixture *ef = new EFXFixture(efx);
                ef->setHead(GroupHead(fixtureIds.at(idx), 0));
                ef->setMode(EFXFixture::RGB);
                efx->addFixture(ef);
            }
            efx->setDuration(1000);
            if (doc->addFunction(efx) == false) { delete efx; continue; }
            topLevel.append(efx);
        }
    }

    for (int c = 0; c < spec.collections; c++)
    {
        Collection *col = new Collection(doc);
        col->setName(QString("Collection %1").arg(c));
        int members = 3 + rng.bounded(6);
        for (int k = 0; k < members; k++)
        {
            if (!sceneIds.isEmpty())
                col->addFunction(sceneIds.at(rng.bounded(sceneIds.size())));
            if (!chaserIds.isEmpty())
                col->addFunction(chaserIds.at(rng.bounded(chaserIds.size())));
        }
        if (doc->addFunction(col) == false) { delete col; continue; }
        topLevel.append(col);
    }

    // ---- Script functions (exercise the script executor every tick) --------
    for (int s = 0; s < spec.scripts; s++)
    {
        Script *scr = new Script(doc);
        scr->setName(QString("Script %1").arg(s));
        QString body = "label:loop\n";
        int sets = 4 + rng.bounded(8);
        for (int k = 0; k < sets; k++)
        {
            quint32 fid = fixtureIds.at(rng.bounded(fixtureIds.size()));
            body += QString("setfixture:%1 ch:%2 val:%3\n")
                        .arg(fid).arg(rng.bounded(ShowSpec::channelsPerFix)).arg(rng.bounded(256));
        }
        // occasionally drive another function, then wait and loop forever
        if (!sceneIds.isEmpty())
            body += QString("startfunction:%1\n").arg(sceneIds.at(rng.bounded(sceneIds.size())));
        body += "wait:40\njump:loop\n";
        scr->setData(body);
        if (doc->addFunction(scr) == false) { delete scr; continue; }
        topLevel.append(scr);
    }

    // ---- Sequences (scene-bound chasers) -----------------------------------
    QList<quint32> sequenceIds;
    for (int s = 0; s < spec.sequences; s++)
    {
        // each sequence owns a bound scene whose values its steps animate
        Scene *bound = new Scene(doc);
        bound->setName(QString("SeqScene %1").arg(s));
        int touch = qMax(1, fixtureIds.size() / 16);
        for (int t = 0; t < touch; t++)
        {
            quint32 fid = fixtureIds.at(rng.bounded(fixtureIds.size()));
            for (int c = 0; c < ShowSpec::channelsPerFix; c++)
                bound->setValue(fid, c, uchar(rng.bounded(256)));
        }
        if (doc->addFunction(bound) == false) { delete bound; continue; }

        Sequence *seq = new Sequence(doc);
        seq->setName(QString("Sequence %1").arg(s));
        seq->setBoundSceneID(bound->id());
        int steps = 4 + rng.bounded(10);
        for (int st = 0; st < steps; st++)
            seq->addStep(ChaserStep(bound->id(), rng.bounded(400), 150 + rng.bounded(600), rng.bounded(400)));
        seq->setRunOrder(Function::Loop);
        if (doc->addFunction(seq) == false) { delete seq; continue; }
        sequenceIds.append(seq->id());
        topLevel.append(seq);
    }

    // ---- Shows (timeline tracks of existing functions) ---------------------
    // candidate functions a show track can schedule
    QList<quint32> showable = sceneIds + chaserIds + sequenceIds;
    for (int sh = 0; sh < spec.shows && !showable.isEmpty(); sh++)
    {
        Show *show = new Show(doc);
        show->setName(QString("Show %1").arg(sh));
        if (doc->addFunction(show) == false) { delete show; continue; }

        int tracks = 2 + rng.bounded(4);
        for (int tr = 0; tr < tracks; tr++)
        {
            Track *track = new Track();
            track->setName(QString("Track %1.%2").arg(sh).arg(tr));
            show->addTrack(track);
            quint32 t = 0;
            int items = 3 + rng.bounded(5);
            for (int it = 0; it < items; it++)
            {
                quint32 fid = showable.at(rng.bounded(showable.size()));
                ShowFunction *sf = track->createShowFunction(fid);
                sf->setStartTime(t);
                quint32 dur = 1000 + rng.bounded(4000);
                sf->setDuration(dur);
                t += dur + rng.bounded(500); // stagger along the timeline
            }
        }
        topLevel.append(show);
    }

    // ---- Show Tree-style cue lists ------------------------------------------
    int cueListCount = 0;
    for (int cl = 0; cl < spec.cuelists; cl++)
    {
        Chaser *list = new Chaser(doc);
        list->setName(QString("CueList %1").arg(cl));
        list->setDurationMode(Chaser::PerStep);
        list->setFadeInMode(Chaser::PerStep);
        list->setFadeOutMode(Chaser::PerStep);
        list->setRunOrder(Function::Loop);
        list->setDirection(Function::Forward);
        for (int q = 0; q < spec.cuesPerList; q++)
        {
            Collection *cue = new Collection(doc);
            cue->setName(QString("CueList %1 Cue %2").arg(cl).arg(q + 1));
            for (int l = 0; l < spec.layersPerCue; l++)
            {
                Scene *layer = new Scene(doc);
                layer->setName(QString("CueList %1 Cue %2 Layer %3").arg(cl).arg(q + 1).arg(l));
                int touch = spec.sceneFixtures > 0 ? qMin(spec.sceneFixtures, int(fixtureIds.size()))
                                                   : qMax(1, int(fixtureIds.size()) / 8);
                // a layer is a block of neighbouring fixtures (a group), as a
                // Show Tree layer targets a group
                int start = rng.bounded(qMax(1, int(fixtureIds.size()) - touch + 1));
                for (int t = 0; t < touch; t++)
                    for (int c = 0; c < ShowSpec::channelsPerFix; c++)
                        layer->setValue(fixtureIds.at(start + t), c, uchar(rng.bounded(256)));
                if (doc->addFunction(layer) == false) { delete layer; continue; }
                cue->addFunction(layer->id());
            }
            if (doc->addFunction(cue) == false) { delete cue; continue; }
            list->addStep(ChaserStep(cue->id(), uint(spec.cueFadeMs), 2000 + rng.bounded(4000),
                                     uint(spec.cueFadeMs)));
        }
        if (doc->addFunction(list) == false) { delete list; continue; }
        topLevel.append(list);
        cueListCount++;
    }

    fprintf(stdout, "stress: created %d functions (%d scenes, %d chasers, %d matrices, %d efx, "
            "%d collections, %d scripts, %d sequences, %d shows)\n",
            int(doc->functions().size()), int(sceneIds.size()), int(chaserIds.size()),
            int(matrixIds.size()), spec.efx, spec.collections, spec.scripts,
            int(sequenceIds.size()), spec.shows);
    if (cueListCount)
        fprintf(stdout, "stress: %d Show Tree-style cue lists x %d cues x %d layers (fade %d ms)\n",
                cueListCount, spec.cuesPerList, spec.layersPerCue, spec.cueFadeMs);

    return topLevel;
}

/****************************************************************************
 * The mixed rig (ShowSpec::MixedRig)
 ****************************************************************************/

namespace
{

QLCFixtureMode *modeNamed(QLCFixtureDef *def, const QString &name)
{
    if (def == nullptr)
        return nullptr;
    QLCFixtureMode *m = def->mode(name);
    return m != nullptr ? m : (def->modes().isEmpty() ? nullptr : def->modes().first());
}

/* What a look sets a channel to: a level, a colour, a position, a gobo, an
   open shutter... by what the channel is, as a designer's group look would.
   -1: leave it out (maintenance, effects, macros). */
int lookValue(const QLCChannel *ch, QRandomGenerator &rng)
{
    switch (ch->group())
    {
        case QLCChannel::Intensity:
            if (ch->colour() == QLCChannel::NoColour && ch->controlByte() == QLCChannel::MSB)
                return 128 + int(rng.bounded(128));        // a dimmer: up
            return int(rng.bounded(256));
        case QLCChannel::Pan:
        case QLCChannel::Tilt:
        case QLCChannel::Beam:                             // zoom, focus
            return int(rng.bounded(256));
        case QLCChannel::Speed:
            return 0;                                      // pan/tilt speed: fastest
        case QLCChannel::Gobo:
        case QLCChannel::Colour:
            return int(rng.bounded(8)) * 16;
        case QLCChannel::Prism:
            return rng.bounded(2) ? 0 : 200;
        case QLCChannel::Shutter:
            foreach (QLCCapability *cap, ch->capabilities())
                if (cap->preset() == QLCCapability::ShutterOpen)
                    return cap->min();
            return 255;
        default:
            return -1;
    }
}

void setLook(Scene *scene, const Fixture *fx, QRandomGenerator &rng)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
    {
        for (quint32 c = 0; c < fx->channels(); c++)
            scene->setValue(fx->id(), c, uchar(rng.bounded(256)));
        return;
    }
    for (int c = 0; c < mode->channels().size(); c++)
    {
        const int v = lookValue(mode->channels().at(c), rng);
        if (v >= 0)
            scene->setValue(fx->id(), quint32(c), uchar(v));
    }
}

struct RigUniverse
{
    QList<quint32> movers, pars, bars, strips, dimmers;
};

} // namespace

/* Positions in the venue plot, in mm: universe u is truss position (u % 8)
   -- 1.5 m apart, upstage -- in the 12 m stage segment (u / 8). */
static QList<Function*> buildMixedRigShow(Doc *doc, const ShowSpec &spec)
{
    QList<Function*> topLevel;
    QRandomGenerator rng(spec.seed);
    QLCFixtureDefCache *cache = doc->fixtureDefCache();
    QLCFixtureDef *moverDef = cache->fixtureDef("Martin", "ERA 300 Profile");
    QLCFixtureDef *parDef = cache->fixtureDef("Chauvet", "SlimPAR Pro Q USB");
    QLCFixtureDef *barDef = cache->fixtureDef("Chauvet", "ColorBand Pix IP");
    QLCFixtureMode *moverMode = modeNamed(moverDef, "21 Channel");
    QLCFixtureMode *parMode = modeNamed(parDef, "5 Channel");
    QLCFixtureMode *barMode = modeNamed(barDef, "36 Channel");
    if (moverMode == nullptr || parMode == nullptr || barMode == nullptr)
    {
        qWarning() << "stress: the mixed rig's fixture definitions are missing";
        return topLevel;
    }

    MonitorProperties *mp = doc->monitorProperties();
    PowerDistribution *pd = doc->powerDistribution();
    PowerSource src;
    src.name = "Company switch";
    src.voltage = 208.0;
    pd->sources() << src;

    // Pars: measured, and held to a cap below their full draw.
    PowerProfile parPower;
    parPower.fullAmps = 0.8;
    parPower.standbyAmps = 0.05;
    parPower.maxAmps = 0.6;

    QList<RigUniverse> rigU;
    QHash<quint32, int> fixtureColumn;   // fixture -> its first column in a 120-column pixel row
    int dmx = 0;
    auto place = [&](Fixture *fx, int u, double xMm) -> bool {
        fx->setAddress(quint32(dmx));
        fx->setUniverse(quint32(u));
        if (doc->addFixture(fx) == false)
        {
            delete fx;
            return false;
        }
        dmx += int(fx->channels());
        const int row = u % 8, segment = u / 8;
        mp->setFixturePosition(fx->id(), 0, 0, QVector3D(float(segment * 12000 + xMm),
                                                         float(1000 + row * 1500), 6000.0f));
        // One 20 A circuit per universe
        pd->assignFixture(fx->id(), 0, u);
        return true;
    };

    for (int u = 0; u < spec.universes; u++)
    {
        PowerCircuit cir;
        cir.name = QString("U%1").arg(u + 1);
        cir.ratedAmps = 20;
        pd->sources()[0].circuits << cir;

        RigUniverse ru;
        dmx = 0;
        for (int i = 0; i < 4; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1 Mover %2").arg(u + 1).arg(i + 1));
            fx->setFixtureDefinition(moverDef, moverMode);
            if (place(fx, u, 1500 + i * 3000))
                ru.movers << fx->id();
        }
        for (int i = 0; i < 8; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1 Par %2").arg(u + 1).arg(i + 1));
            fx->setFixtureDefinition(parDef, parMode);
            fx->setPowerProfile(parPower);
            if (place(fx, u, 750 + i * 1500))
                ru.pars << fx->id();
        }
        // Pixels along the position: 4 bars (cols 0-59, 3 dark columns
        // between them), then 2 strips (cols 60-119), 100 mm a column.
        for (int i = 0; i < 4; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1 Bar %2").arg(u + 1).arg(i + 1));
            fx->setFixtureDefinition(barDef, barMode);
            if (place(fx, u, i * 1500 + 600))
            {
                ru.bars << fx->id();
                fixtureColumn.insert(fx->id(), i * 15);
            }
        }
        for (int i = 0; i < 2; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1 Strip %2").arg(u + 1).arg(i + 1));
            QLCFixtureDef *def = fx->genericRGBPanelDef(30, Fixture::RGB, false);
            fx->setFixtureDefinition(def, fx->genericRGBPanelMode(def, Fixture::RGB, false, 3000, 50));
            if (place(fx, u, 6000 + i * 3000 + 1500))
            {
                ru.strips << fx->id();
                fixtureColumn.insert(fx->id(), 60 + i * 30);
            }
        }
        for (int i = 0; i < 12; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1 Dimmer %2").arg(u + 1).arg(i + 1));
            fx->setChannels(1);
            if (place(fx, u, 500 + i * 1000))
                ru.dimmers << fx->id();
        }
        rigU << ru;
    }
    fprintf(stdout, "stress: mixed rig, %d fixtures across %d universes (%d channels)\n",
            int(doc->fixtures().size()), spec.universes, spec.totalChannels());

    // ---- groups across the stage ---------------------------------------------
    auto pixelHeads = [doc](quint32 fid) {
        Fixture *fx = doc->fixture(fid);
        return fx == nullptr ? 0 : fx->heads();
    };
    QList<FixtureGroup *> matrixGroups;
    QList<QList<quint32>> lookGroups;     // fixtures a group look covers
    QList<QList<quint32>> moverGroups;
    // A pixel row per position: 120 columns, the gaps between bars dark.
    for (int u = 0; u < rigU.size(); u++)
    {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(QString("Pixels U%1").arg(u + 1));
        g->setSize(QSize(120, 1));
        for (quint32 fid : rigU[u].bars + rigU[u].strips)
            for (int h = 0; h < pixelHeads(fid); h++)
                g->assignHead(QLCPoint(fixtureColumn.value(fid) + h, 0), GroupHead(fid, h));
        if (doc->addFixtureGroup(g)) matrixGroups << g; else delete g;
        lookGroups << rigU[u].pars + rigU[u].dimmers;
    }
    // Per 12 m segment: the pixel grid (8 positions deep), a par grid with
    // the gaps between pars, the movers.
    for (int seg = 0; seg * 8 < rigU.size(); seg++)
    {
        FixtureGroup *pix = new FixtureGroup(doc);
        pix->setName(QString("Stage pixels %1").arg(seg + 1));
        pix->setSize(QSize(120, 8));
        FixtureGroup *par = new FixtureGroup(doc);
        par->setName(QString("Stage pars %1").arg(seg + 1));
        par->setSize(QSize(16, 8));
        QList<quint32> movers, wash;
        for (int row = 0; row < 8 && seg * 8 + row < rigU.size(); row++)
        {
            const RigUniverse &ru = rigU[seg * 8 + row];
            for (quint32 fid : ru.bars + ru.strips)
                for (int h = 0; h < pixelHeads(fid); h++)
                    pix->assignHead(QLCPoint(fixtureColumn.value(fid) + h, row), GroupHead(fid, h));
            for (int i = 0; i < ru.pars.size(); i++)
                par->assignHead(QLCPoint(i * 2 + 1, row), GroupHead(ru.pars.at(i), 0));
            movers << ru.movers;
            wash << ru.pars;
        }
        if (doc->addFixtureGroup(pix)) matrixGroups << pix; else delete pix;
        if (doc->addFixtureGroup(par)) matrixGroups << par; else delete par;
        FixtureGroup *mov = new FixtureGroup(doc);
        mov->setName(QString("Stage movers %1").arg(seg + 1));
        mov->setSize(QSize(4, 8));
        for (quint32 fid : movers)
            mov->assignFixture(fid);
        if (!doc->addFixtureGroup(mov)) delete mov;
        moverGroups << movers;
        lookGroups << movers << wash;
    }

    // ---- group looks: every fixture of a position, by what it is ------------
    QList<quint32> sceneIds;
    for (int s = 0; s < spec.scenes; s++)
    {
        const RigUniverse &ru = rigU[s % rigU.size()];
        Scene *scene = new Scene(doc);
        scene->setName(QString("Look %1").arg(s));
        for (quint32 fid : ru.movers + ru.pars + ru.bars + ru.strips + ru.dimmers)
            setLook(scene, doc->fixture(fid), rng);
        scene->setFadeInSpeed(rng.bounded(2000));
        scene->setFadeOutSpeed(rng.bounded(2000));
        if (doc->addFunction(scene) == false) { delete scene; continue; }
        sceneIds << scene->id();
        topLevel << scene;
    }

    // ---- chasers of those looks (per-step times) ----------------------------
    for (int c = 0; c < spec.chasers && !sceneIds.isEmpty(); c++)
    {
        Chaser *chaser = new Chaser(doc);
        chaser->setName(QString("Chaser %1").arg(c));
        chaser->setDurationMode(Chaser::PerStep);
        chaser->setFadeInMode(Chaser::PerStep);
        chaser->setFadeOutMode(Chaser::PerStep);
        const int steps = 4 + rng.bounded(12);
        for (int st = 0; st < steps; st++)
            chaser->addStep(ChaserStep(sceneIds.at(rng.bounded(sceneIds.size())), rng.bounded(500),
                                       200 + rng.bounded(800), rng.bounded(500)));
        chaser->setRunOrder(Function::Loop);
        if (doc->addFunction(chaser) == false) { delete chaser; continue; }
        topLevel << chaser;
    }

    // ---- matrices over the stage groups --------------------------------------
    for (int m = 0; m < spec.matrices && !matrixGroups.isEmpty(); m++)
    {
        RGBMatrix *mtx = new RGBMatrix(doc);
        mtx->setName(QString("Matrix %1").arg(m));
        mtx->setFixtureGroup(matrixGroups.at(m % matrixGroups.size())->id());
        RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc, kMatrixAlgos[m % kMatrixAlgoCount]);
        if (algo != nullptr)
            mtx->setAlgorithm(algo);
        mtx->setColor(0, QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
        mtx->setColor(1, QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
        mtx->setFadeInSpeed(0);
        mtx->setDuration(100 + rng.bounded(400));
        if (doc->addFunction(mtx) == false) { delete mtx; continue; }
        topLevel << mtx;
    }

    // ---- EFX: pan/tilt on the movers (16-bit, fine channels written) ---------
    for (int e = 0; e < spec.efx && !moverGroups.isEmpty(); e++)
    {
        const QList<quint32> &movers = moverGroups.at(e % moverGroups.size());
        EFX *efx = new EFX(doc);
        efx->setName(QString("EFX %1").arg(e));
        efx->setAlgorithm(EFX::Algorithm(e % 6));
        efx->setWidth(64 + rng.bounded(64));
        efx->setHeight(64 + rng.bounded(64));
        efx->setPropagationMode(e % 2 ? EFX::Serial : EFX::Parallel);
        const int heads = qMin(int(movers.size()), 4 + int(rng.bounded(13)));
        const int from = int(rng.bounded(qMax(1, int(movers.size()) - heads + 1)));
        for (int h = 0; h < heads; h++)
        {
            EFXFixture *ef = new EFXFixture(efx);
            ef->setHead(GroupHead(movers.at(from + h), 0));
            ef->setMode(EFXFixture::PanTilt);
            efx->addFixture(ef);
        }
        efx->setDuration(2000 + rng.bounded(3000));
        if (doc->addFunction(efx) == false) { delete efx; continue; }
        topLevel << efx;
    }

    // ---- Show Tree-style cue lists: group-look layers ------------------------
    for (int cl = 0; cl < spec.cuelists && !lookGroups.isEmpty(); cl++)
    {
        Chaser *list = new Chaser(doc);
        list->setName(QString("CueList %1").arg(cl));
        list->setDurationMode(Chaser::PerStep);
        list->setFadeInMode(Chaser::PerStep);
        list->setFadeOutMode(Chaser::PerStep);
        list->setRunOrder(Function::Loop);
        for (int q = 0; q < spec.cuesPerList; q++)
        {
            Collection *cue = new Collection(doc);
            cue->setName(QString("CueList %1 Cue %2").arg(cl).arg(q + 1));
            for (int l = 0; l < spec.layersPerCue; l++)
            {
                Scene *layer = new Scene(doc);
                layer->setName(QString("CueList %1 Cue %2 Layer %3").arg(cl).arg(q + 1).arg(l));
                for (quint32 fid : lookGroups.at(rng.bounded(lookGroups.size())))
                    setLook(layer, doc->fixture(fid), rng);
                if (doc->addFunction(layer) == false) { delete layer; continue; }
                cue->addFunction(layer->id());
            }
            if (doc->addFunction(cue) == false) { delete cue; continue; }
            list->addStep(ChaserStep(cue->id(), uint(spec.cueFadeMs), 2000 + rng.bounded(4000),
                                     uint(spec.cueFadeMs)));
        }
        if (doc->addFunction(list) == false) { delete list; continue; }
        topLevel << list;
    }

    fprintf(stdout, "stress: mixed rig: %d functions, %d groups (%d over the stage), "
            "%d power circuits\n", int(doc->functions().size()), int(doc->fixtureGroups().size()),
            int(matrixGroups.size()), int(pd->sources().first().circuits.size()));
    return topLevel;
}
