/*
  Q Light Controller Plus - test support
  scalerig.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QVector3D>
#include <QColor>
#include <QSet>
#include <QSize>
#include <QElapsedTimer>

#include "scalerig.h"

#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "qlcpoint.h"
#include "monitorproperties.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "chaserstep.h"
#include "rgbalgorithm.h"
#include "rgbmatrix.h"
#include "efxfixture.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

namespace
{
QLCFixtureMode *modeOf(Doc *doc, const QString &maker, const QString &model, const QString &mode,
                       QString *why)
{
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(maker, model);
    QLCFixtureMode *m = def != nullptr ? def->mode(mode) : nullptr;
    if (m == nullptr && why != nullptr)
        *why = QStringLiteral("no %1 %2 (%3) in the fixture library").arg(maker, model, mode);
    return m;
}

/* What a look sets a channel to, by what the channel is: up, a colour, a
   position, an open shutter; the rest (macros, effects, speeds) left out. */
void setLook(Scene *scene, const Fixture *fx, int level, const QColor &colour)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
    {
        for (quint32 c = 0; c < fx->channels(); ++c)
            scene->setValue(fx->id(), c, uchar(level));
        return;
    }
    for (int c = 0; c < mode->channels().size(); ++c)
    {
        const QLCChannel *ch = mode->channels().at(c);
        int v = -1;
        switch (ch->group())
        {
            case QLCChannel::Intensity:
                switch (ch->colour())
                {
                    case QLCChannel::Red: v = colour.red(); break;
                    case QLCChannel::Green: v = colour.green(); break;
                    case QLCChannel::Blue: v = colour.blue(); break;
                    case QLCChannel::White: v = qMin(colour.red(), qMin(colour.green(), colour.blue())); break;
                    case QLCChannel::NoColour: v = level; break;
                    default: v = 0; break;
                }
                break;
            case QLCChannel::Pan:
            case QLCChannel::Tilt:
                v = 128;
                break;
            case QLCChannel::Shutter:
                v = 255;
                for (const QLCCapability *cap : ch->capabilities())
                    if (cap->preset() == QLCCapability::ShutterOpen)
                    {
                        v = cap->min();
                        break;
                    }
                break;
            default:
                break;
        }
        if (v >= 0)
            scene->setValue(fx->id(), quint32(c), uchar(v));
    }
}

ShowTree::Target groupTarget(quint32 g)
{
    ShowTree::Target t;
    t.groups << g;
    return t;
}
}

bool ScaleRig::build(Doc *doc, Built *out, QString *why)
{
    QLCFixtureMode *mover = modeOf(doc, "Martin", "ERA 300 Profile", "21 Channel", why);
    QLCFixtureMode *wash = modeOf(doc, "American DJ", "Micro Wash RGBW", "7 Channel", why);
    QLCFixtureMode *lantern = modeOf(doc, "WLED", "Lantern", "Effect (15ch)", why);
    if (mover == nullptr || wash == nullptr || lantern == nullptr)
        return false;

    QElapsedTimer clock;
    clock.start();
    auto lap = [&](const char *what) {
        out->timings << QStringLiteral("%1 %2 ms").arg(QLatin1String(what)).arg(clock.restart());
    };
    MonitorProperties *mp = doc->monitorProperties();
    // The plot: a 20 m wide, 14 m deep stage, 8 m up (metres).
    mp->setGridSize(QVector3D(20, 8, 14));

    int address = 0;
    auto place = [&](Fixture *fx, int universe, const QString &name, float xMm, float yMm, float zMm) {
        fx->setName(name);
        // Address first, then universe (Fixture::setAddress() keeps 7 universe bits)
        fx->setAddress(quint32(address));
        fx->setUniverse(quint32(universe));
        if (!doc->addFixture(fx))
        {
            delete fx;
            return quint32(Fixture::invalidId());
        }
        address += int(fx->channels());
        mp->setFixturePosition(fx->id(), 0, 0, QVector3D(xMm, yMm, zMm));
        return fx->id();
    };
    auto pixelBar = [&](int universe, const QString &name, int pixels, int widthMm, float x, float y, float z) {
        Fixture *fx = new Fixture(doc);
        QLCFixtureDef *def = fx->genericRGBPanelDef(pixels, Fixture::RGBW, false);
        fx->setFixtureDefinition(def, fx->genericRGBPanelMode(def, Fixture::RGBW, false, quint32(widthMm), 60));
        return place(fx, universe, name, x, y, z);
    };
    auto fromLibrary = [&](QLCFixtureMode *mode, int universe, const QString &name, float x, float y, float z) {
        Fixture *fx = new Fixture(doc);
        fx->setFixtureDefinition(mode->fixtureDef(), mode);
        return place(fx, universe, name, x, y, z);
    };
    auto keep = [](QList<quint32> &list, quint32 id) {
        if (id != Fixture::invalidId())
            list << id;
    };

    // 0-29: an upstage pixel bar each, on the floor along the cyc
    for (int u = 0; u < 30; ++u)
    {
        address = 0;
        keep(out->usts, pixelBar(u, QString("UST %1").arg(u + 1), 96, 560, 300 + u * 640, 13000, 0));
    }
    // 30-39: two movers each, on the mid truss; 40-49: two washes each
    for (int u = 30; u < 40; ++u)
    {
        address = 0;
        for (int i = 0; i < 2; ++i)
        {
            const int n = (u - 30) * 2 + i;
            keep(out->movers, fromLibrary(mover, u, QString("Mover %1").arg(n + 1), 600 + n * 1000, 7000, 6000));
        }
    }
    for (int u = 40; u < 50; ++u)
    {
        address = 0;
        for (int i = 0; i < 2; ++i)
        {
            const int n = (u - 40) * 2 + i;
            keep(out->washes, fromLibrary(wash, u, QString("Wash %1").arg(n + 1), 600 + n * 1000, 2000, 6000));
        }
    }
    // 50-75: three 42-pixel bars each, three rows across the stage
    for (int u = 50; u < 76; ++u)
    {
        address = 0;
        for (int i = 0; i < 3; ++i)
        {
            const int n = (u - 50) * 3 + i;
            keep(out->bars, pixelBar(u, QString("Bar %1").arg(n + 1), 42, 680,
                                     300 + (n % 26) * 750, 3000 + (n / 26) * 3500, 3000));
        }
    }
    // 76-77: 27 lanterns each, hung in three rows
    for (int u = 76; u < 78; ++u)
    {
        address = 0;
        for (int i = 0; i < 27; ++i)
        {
            const int n = (u - 76) * 27 + i;
            keep(out->lanterns, fromLibrary(lantern, u, QString("Lantern %1").arg(n + 1),
                                            500 + (n % 18) * 1100, 5000 + (n / 18) * 2000, 4500));
        }
    }
    // 78: the rest of the movers and washes, upstage truss
    address = 0;
    for (int i = 0; i < 12; ++i)
        keep(out->movers, fromLibrary(mover, 78, QString("Mover %1").arg(21 + i), 800 + i * 1600, 11000, 6000));
    for (int i = 0; i < 12; ++i)
        keep(out->washes, fromLibrary(wash, 78, QString("Wash %1").arg(21 + i), 1600 + i * 1600, 11000, 6000));
    // 79: the house dimmers, front of house
    address = 0;
    for (int i = 0; i < 34; ++i)
    {
        Fixture *fx = new Fixture(doc);
        fx->setChannels(1);
        keep(out->dimmers, place(fx, 79, QString("House %1").arg(i + 1), 300 + i * 560, 500, 7000));
    }

    if (doc->fixtures().size() != kFixtures)
    {
        if (why != nullptr)
            *why = QStringLiteral("%1 fixtures made, not %2").arg(doc->fixtures().size()).arg(kFixtures);
        return false;
    }

    lap("fixtures");

    // ---- groups: what the cues and the effects point at --------------------
    auto group = [&](const QString &name, const QList<quint32> &fixtures, const QSize &size) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        g->setSize(size);
        for (quint32 id : fixtures)
            g->assignFixture(id);
        if (!doc->addFixtureGroup(g))
        {
            delete g;
            return quint32(FixtureGroup::invalidId());
        }
        out->groups << g->id();
        return g->id();
    };
    // Every pixel: a row per bar, the USTs first
    {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName("Pixels");
        g->setSize(QSize(96, int(out->usts.size() + out->bars.size())));
        int row = 0;
        for (quint32 id : out->usts + out->bars)
        {
            Fixture *fx = doc->fixture(id);
            for (int h = 0; fx != nullptr && h < fx->heads(); ++h)
                g->assignHead(QLCPoint(h, row), GroupHead(id, h));
            ++row;
        }
        if (!doc->addFixtureGroup(g))
            delete g;
        else
            out->groups << (out->pixelGroup = g->id());
    }
    out->moverGroup = group("Movers", out->movers, QSize(16, 2));
    out->washGroup = group("Washes", out->washes, QSize(16, 2));
    out->lanternGroup = group("Lanterns", out->lanterns, QSize(18, 3));
    out->houseGroup = group("House", out->dimmers, QSize(34, 1));
    const quint32 ustGroup = group("USTs", out->usts, QSize(30, 1));
    const quint32 barGroup = group("Bars", out->bars, QSize(26, 3));

    lap("groups");

    // ---- the effects --------------------------------------------------------
    RGBMatrix *mtx = new RGBMatrix(doc);
    mtx->setName("Pixel Plasma");
    mtx->setFixtureGroup(out->pixelGroup);
    RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc, "Plasma");
    if (algo == nullptr)
    {
        delete mtx;
        if (why != nullptr)
            *why = QStringLiteral("no Plasma RGB script (are the scripts loaded?)");
        return false;
    }
    mtx->setAlgorithm(algo);
    mtx->setColor(0, QColor(255, 40, 0));
    mtx->setColor(1, QColor(0, 60, 255));
    mtx->setDuration(200);                  // a new map five times a second
    doc->addFunction(mtx);
    out->matrix = mtx->id();

    EFX *efx = new EFX(doc);
    efx->setName("Mover Circle");
    efx->setAlgorithm(EFX::Circle);
    efx->setWidth(100);
    efx->setHeight(80);
    efx->setPropagationMode(EFX::Serial);
    for (quint32 id : out->movers)
    {
        EFXFixture *ef = new EFXFixture(efx);
        ef->setHead(GroupHead(id, 0));
        ef->setMode(EFXFixture::PanTilt);
        efx->addFixture(ef);
    }
    efx->setDuration(4000);
    doc->addFunction(efx);
    out->efx = efx->id();

    // ---- three chasers of looks, per-step fades ----------------------------
    const QList<QColor> colours { QColor(255, 0, 0), QColor(255, 140, 0), QColor(0, 200, 255),
                                  QColor(120, 0, 255), QColor(255, 255, 255), QColor(0, 255, 80) };
    auto chaser = [&](const QString &name, const QList<quint32> &fixtures, int steps) {
        Chaser *ch = new Chaser(doc);
        ch->setName(name);
        ch->setDurationMode(Chaser::PerStep);
        ch->setFadeInMode(Chaser::PerStep);
        ch->setFadeOutMode(Chaser::PerStep);
        ch->setRunOrder(Function::Loop);
        for (int s = 0; s < steps; ++s)
        {
            Scene *look = new Scene(doc);
            look->setName(QString("%1 step %2").arg(name).arg(s + 1));
            for (int i = s; i < fixtures.size(); i += steps)
                setLook(look, doc->fixture(fixtures.at(i)), 255, colours.at((s + i) % colours.size()));
            doc->addFunction(look);
            ch->addStep(ChaserStep(look->id(), 300, 700, 300));
        }
        doc->addFunction(ch);
        out->chasers << ch->id();
    };
    chaser("Lantern chase", out->lanterns, 4);
    chaser("Wash chase", out->washes, 3);
    chaser("House chase", out->dimmers, 6);

    lap("effects and chasers");

    // ---- the Show Tree: 40 cues with fades over the groups ------------------
    QList<quint32> dimmers, cols, positions;
    for (int i = 0; i < 6; ++i)
    {
        QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, doc);
        dim->setName(QString("Level %1").arg(i + 1));
        dim->setValue(40 * i + 55);
        doc->addPalette(dim);
        dimmers << dim->id();
        QLCPalette *col = new QLCPalette(QLCPalette::Color, doc);
        col->setName(QString("Colour %1").arg(i + 1));
        col->setValue(QLCPalette::colorToString(colours.at(i), QColor(Qt::black)));
        doc->addPalette(col);
        cols << col->id();
        QLCPalette *pos = new QLCPalette(QLCPalette::PanTilt, doc);
        pos->setName(QString("Position %1").arg(i + 1));
        pos->setValue(30 * i + 20, 30 + 15 * i);
        doc->addPalette(pos);
        positions << pos->id();
    }
    const QList<quint32> lit { ustGroup, barGroup, out->washGroup, out->lanternGroup, out->moverGroup };
    ShowTreeController *st = doc->showTree();
    st->beginBatch("Scale show");
    const quint32 prod = st->addProduction("Scale show");
    out->cueList = st->addCueList(prod, "Act 1");
    QList<quint32> cueIds;
    for (int i = 0; i < kCues; ++i)
    {
        const quint32 c = st->addCue(out->cueList, i - 1, QString("Cue %1").arg(i + 1));
        cueIds << c;
        // The first cue sets every group; each later one changes two.
        const int from = i == 0 ? 0 : i % lit.size();
        const int count = i == 0 ? int(lit.size()) : 2;
        for (int k = 0; k < count; ++k)
        {
            const quint32 g = lit.at((from + k) % lit.size());
            st->setTargetLook(c, groupTarget(g), st->lookForPalette(dimmers.at((i + k) % dimmers.size())));
            st->setTargetLook(c, groupTarget(g), st->lookForPalette(cols.at((i * 5 + k) % cols.size())));
            if (g == out->moverGroup)
                st->setTargetLook(c, groupTarget(g), st->lookForPalette(positions.at(i % positions.size())));
        }
        if (i == 0 || i % 4 == 2)
            st->setTargetLook(c, groupTarget(out->houseGroup),
                              st->lookForPalette(dimmers.at(i % dimmers.size())));
    }
    for (int i = 0; i < cueIds.size(); ++i)
    {
        const ShowTree::Cue *cue = st->cue(cueIds.at(i));
        if (cue == nullptr)
            continue;
        ShowTree::Cue c = *cue;
        c.fadeIn = uint(2000 + (i % 3) * 500);
        c.fadeOut = c.fadeIn;
        st->setCue(c);
    }
    st->endBatch(true);
    lap("Show Tree");
    const ShowTree::CueList *list = st->cueList(out->cueList);
    out->cueChaser = list != nullptr ? list->chaser : 0;
    if (list == nullptr || doc->function(out->cueChaser) == nullptr)
    {
        if (why != nullptr)
            *why = QStringLiteral("the Show Tree list baked to no chaser");
        return false;
    }
    return true;
}

int ScaleRig::sparseUniverses(Doc *doc)
{
    QHash<quint32, int> perUniverse;
    for (Fixture *fx : doc->fixtures())
        perUniverse[fx->universe()]++;
    int sparse = 0;
    for (int n : perUniverse)
        if (n <= 2)
            ++sparse;
    return sparse;
}

int ScaleRig::channels(Doc *doc)
{
    int n = 0;
    for (Fixture *fx : doc->fixtures())
        n += int(fx->channels());
    return n;
}
