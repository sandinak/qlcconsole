/*
  Q Light Controller Plus
  vceffectcontrol.cpp

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
#include <QCoreApplication>
#include <QPoint>
#include <cmath>

#include "programmingmanager.h"
#include "effectscriptrunner.h"
#include "vceffectcontrol.h"
#include "effectinstance.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "efxfixture.h"
#include "qlcpalette.h"
#include "rgbmatrix.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

#define tr(s) QCoreApplication::translate("VCEffectControl", s)

namespace
{
    const quint32 kInvalid = 0xFFFFFFFF;
    const int kMaxRecent = 64;

    /** A role's names in effect scripts, the first found wins. */
    QStringList scriptNames(VCEffectControl::Role role)
    {
        switch (role)
        {
            case VCEffectControl::Speed: return { "speed", "rate", "motion", "spin", "tempo" };
            case VCEffectControl::Size: return { "size", "width", "scale", "length", "panSize", "panRadius", "radius" };
            case VCEffectControl::Rate: return { "rate", "density", "frequency", "freq" };
            case VCEffectControl::Spread: return { "spread", "phaseSpread", "offset" };
            case VCEffectControl::Direction: return { "direction", "reverse" };
            case VCEffectControl::Intensity: return { "intensity", "level", "gain", "dimmer" };
            default: return {};
        }
    }

    /** Speed of a function: ×¼ .. ×4 around what it was, the middle as it was. */
    double speedFactor(int value) { return std::pow(2.0, (value - 128) / (value >= 128 ? 63.5 : 64.0)); }
    int speedSlider(double factor)
    {
        if (factor <= 0)
            return 128;
        const double l = std::log2(factor);
        return qBound(0, int(std::lround(128 + (l >= 0 ? 63.5 : 64.0) * l)), 255);
    }
}

/*****************************************************************************
 * Names
 *****************************************************************************/

QString VCEffectControl::roleToString(Role role)
{
    switch (role)
    {
        case Speed: return QStringLiteral("Speed");
        case Size: return QStringLiteral("Size");
        case Rate: return QStringLiteral("Rate");
        case Spread: return QStringLiteral("Spread");
        case Direction: return QStringLiteral("Direction");
        case Intensity: return QStringLiteral("Intensity");
        case Hue: return QStringLiteral("Hue");
        default: return QStringLiteral("Speed");
    }
}

VCEffectControl::Role VCEffectControl::stringToRole(const QString &str)
{
    for (int r = 0; r < RoleCount; ++r)
        if (roleToString(Role(r)) == str)
            return Role(r);
    return Speed;
}

QString VCEffectControl::roleName(Role role)
{
    switch (role)
    {
        case Speed: return tr("Speed");
        case Size: return tr("Size / Width");
        case Rate: return tr("Rate");
        case Spread: return tr("Offset / Spread");
        case Direction: return tr("Direction");
        case Intensity: return tr("Intensity");
        case Hue: return tr("Colour 1 hue");
        default: return QString();
    }
}

QString VCEffectControl::targetToString(Target target)
{
    switch (target)
    {
        case TargetPalette: return QStringLiteral("Palette");
        case TargetGroup: return QStringLiteral("Group");
        default: return QStringLiteral("Current");
    }
}

VCEffectControl::Target VCEffectControl::stringToTarget(const QString &str)
{
    if (str == QLatin1String("Palette"))
        return TargetPalette;
    if (str == QLatin1String("Group"))
        return TargetGroup;
    return TargetCurrent;
}

/*****************************************************************************
 * Resolving
 *****************************************************************************/

VCEffectControl::Resolved VCEffectControl::resolve(Doc *doc) const
{
    Resolved r;
    VCEffectTracker *tracker = VCEffectTracker::instance(doc);
    VCEffectTracker::Effect e;
    if (target == TargetPalette)
    {
        if (doc->palette(palette) != nullptr)
            e = { Resolved::Palette, palette };
    }
    else if (target == TargetGroup)
        e = tracker->current(group, false);
    else
        e = tracker->current(group, true);
    if (!e.isValid())
        return r;

    r.kind = e.kind;
    r.id = e.id;
    r.playing = tracker->isPlaying(e);
    if (e.kind == Resolved::Palette)
    {
        const QLCPalette *p = doc->palette(e.id);
        r.name = p != nullptr ? p->name() : QString();
        if (role == Hue)
        {
            r.available = true;
            r.param = EffectInstance::hueOverrideKey();
            r.min = 0;
            r.max = 1;
            return r;
        }
        const QList<EffectScript::ParamDef> defs = doc->effectScriptRunner()->paramDefs(e.id);
        for (const QString &name : scriptNames(role))
        {
            for (const EffectScript::ParamDef &d : defs)
            {
                if (d.name != name || (!d.type.isEmpty() && d.type != QLatin1String("float")))
                    continue;
                r.available = true;
                r.param = d.name;
                r.min = d.min;
                r.max = d.max > d.min ? d.max : d.min + 1;
                r.values = d.enumValues;
                return r;
            }
        }
        // No spread of its own: the host's Spread, in cycles.
        if (role == Spread && r.playing)
        {
            r.available = true;
            r.param = EffectInstance::spreadOverrideKey();
            r.min = 0;
            r.max = 8;
        }
        return r;
    }

    const Function *f = doc->function(e.id);
    r.name = f != nullptr ? f->name() : QString();
    switch (role)
    {
        case Speed:
        case Direction:
        case Intensity:
            r.available = true;
        break;
        case Size:
            r.available = (e.kind == Resolved::EFX);
        break;
        case Hue:
            r.available = (e.kind == Resolved::RGBMatrix);
        break;
        default:
        break;
    }
    return r;
}

/*****************************************************************************
 * Values
 *****************************************************************************/

int VCEffectControl::sliderValue(Doc *doc, const Resolved &r, Role role)
{
    if (!r.available || !r.playing)
        return 0;
    if (r.kind == Resolved::Palette)
    {
        bool ok = false;
        const double v = doc->effectScriptRunner()->paramValue(r.id, r.param, &ok);
        if (!ok)
            return 0;
        if (!r.values.isEmpty())
        {
            const int n = r.values.count();
            return n < 2 ? 0 : qBound(0, int(std::lround(v)) * 255 / (n - 1), 255);
        }
        return qBound(0, int(std::lround((v - r.min) / (r.max - r.min) * 255.0)), 255);
    }

    Function *f = doc->function(r.id);
    if (f == nullptr)
        return 0;
    VCEffectTracker *tracker = VCEffectTracker::instance(doc);
    switch (role)
    {
        case Speed:
        {
            if (!tracker->hasOriginal(r.id, Speed) || f->duration() == 0)
                return 128;
            return speedSlider(double(tracker->original(r.id, Speed).toUInt()) / double(f->duration()));
        }
        case Direction:
            return f->direction() == Function::Backward ? 255 : 0;
        case Intensity:
            return qBound(0, int(std::lround(f->getAttributeValue(Function::Intensity) * 255.0)), 255);
        case Size:
            if (EFX *efx = qobject_cast<EFX *>(f))
                return qBound(0, efx->width() * 255 / 127, 255);
            return 0;
        case Hue:
            if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
                return qBound(0, int(std::lround(qMax(0.0, m->getColor(0).hsvHueF()) * 255.0)), 255);
            return 0;
        default:
            return 0;
    }
}

QString VCEffectControl::valueText(Doc *doc, const Resolved &r, Role role)
{
    if (r.kind == Resolved::None)
        return tr("no effect");
    if (!r.playing)
        return tr("not playing");
    if (!r.available)
        return tr("n/a");
    if (r.kind == Resolved::Palette)
    {
        bool ok = false;
        const double v = doc->effectScriptRunner()->paramValue(r.id, r.param, &ok);
        if (!ok)
            return tr("n/a");
        if (!r.values.isEmpty())
            return r.values.value(qBound(0, int(std::lround(v)), r.values.count() - 1));
        if (r.param == EffectInstance::hueOverrideKey())
            return QStringLiteral("%1°").arg(int(std::lround(v * 360)));
        if (r.param == EffectInstance::spreadOverrideKey())
            return tr("%1 cycles").arg(v, 0, 'f', 2);
        return QString::number(v, 'f', r.max - r.min > 20 ? 0 : 2);
    }
    Function *f = doc->function(r.id);
    if (f == nullptr)
        return tr("n/a");
    switch (role)
    {
        case Speed:
            return QStringLiteral("×%1").arg(speedFactor(sliderValue(doc, r, role)), 0, 'f', 2);
        case Direction:
            return f->direction() == Function::Backward ? tr("Backward") : tr("Forward");
        case Intensity:
            return QStringLiteral("%1 %").arg(int(std::lround(f->getAttributeValue(Function::Intensity) * 100)));
        case Size:
            if (EFX *efx = qobject_cast<EFX *>(f))
                return QString::number(efx->width());
            return tr("n/a");
        case Hue:
            if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
                return QStringLiteral("%1°").arg(int(std::lround(qMax(0.0, m->getColor(0).hsvHueF()) * 360)));
            return tr("n/a");
        default:
            return tr("n/a");
    }
}

bool VCEffectControl::apply(Doc *doc, const Resolved &r, Role role, int value)
{
    if (!r.available || !r.playing)
        return false;
    value = qBound(0, value, 255);
    if (r.kind == Resolved::Palette)
    {
        double v;
        if (!r.values.isEmpty())
            v = r.values.count() < 2 ? 0 : std::lround(value * (r.values.count() - 1) / 255.0);
        else
            v = r.min + (r.max - r.min) * value / 255.0;
        return doc->effectScriptRunner()->setParamOverride(r.id, r.param, v);
    }

    Function *f = doc->function(r.id);
    if (f == nullptr)
        return false;
    VCEffectTracker *tracker = VCEffectTracker::instance(doc);
    switch (role)
    {
        case Speed:
        {
            if (!tracker->hasOriginal(r.id, Speed))
                tracker->rememberOriginal(r.id, Speed, f->duration());
            const uint orig = tracker->original(r.id, Speed).toUInt();
            f->setDuration(qMax(1u, uint(std::lround(orig / speedFactor(value)))));
            return true;
        }
        case Direction:
            if (!tracker->hasOriginal(r.id, Direction))
                tracker->rememberOriginal(r.id, Direction, int(f->direction()));
            f->setDirection(value < 128 ? Function::Forward : Function::Backward);
            return true;
        case Intensity:
        {
            const int id = tracker->intensityOverride(r.id);
            if (id < 0)
                tracker->setIntensityOverride(r.id, f->requestAttributeOverride(Function::Intensity, value / 255.0));
            else
                f->adjustAttribute(value / 255.0, id);
            return true;
        }
        case Size:
            if (EFX *efx = qobject_cast<EFX *>(f))
            {
                if (!tracker->hasOriginal(r.id, Size))
                    tracker->rememberOriginal(r.id, Size, QPoint(efx->width(), efx->height()));
                const QPoint o = tracker->original(r.id, Size).toPoint();
                const int w = value * 127 / 255;
                efx->setWidth(w);
                efx->setHeight(o.x() > 0 ? qBound(0, o.y() * w / o.x(), 127) : w);
                return true;
            }
            return false;
        case Hue:
            if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
            {
                if (!tracker->hasOriginal(r.id, Hue))
                    tracker->rememberOriginal(r.id, Hue, m->getColor(0));
                const QColor c = m->getColor(0).isValid() ? m->getColor(0) : QColor(Qt::red);
                m->setColor(0, QColor::fromHsvF(value / 255.0 * 359.0 / 360.0,
                                                c.hsvSaturationF() > 0 ? c.hsvSaturationF() : 1.0,
                                                c.valueF() > 0 ? c.valueF() : 1.0));
                m->updateColorDelta();
                return true;
            }
            return false;
        default:
            return false;
    }
}

bool VCEffectControl::isOverridden(Doc *doc, const Resolved &r)
{
    if (r.kind == Resolved::Palette)
        return !doc->effectScriptRunner()->paramOverrides(r.id).isEmpty();
    if (r.kind == Resolved::None)
        return false;
    VCEffectTracker *tracker = VCEffectTracker::instance(doc);
    for (int role = 0; role < RoleCount; ++role)
        if (tracker->hasOriginal(r.id, role))
            return true;
    return false;
}

bool VCEffectControl::store(Doc *doc, const Resolved &r)
{
    if (r.kind == Resolved::Palette)
    {
        EffectScriptRunner *runner = doc->effectScriptRunner();
        QLCPalette *p = doc->palette(r.id);
        const QHash<QString, double> over = runner->paramOverrides(r.id);
        if (p == nullptr || over.isEmpty())
            return false;
        bool stored = false;
        for (auto it = over.constBegin(); it != over.constEnd(); ++it)
        {
            // Colour 1's hue belongs to the look's colour palette, not the
            // effect: it stays a live override.
            if (it.key() == EffectInstance::hueOverrideKey())
                continue;
            p->setEffectParamValue(it.key(), it.value());
            runner->clearParamOverride(r.id, it.key());
            stored = true;
        }
        if (stored)
        {
            runner->updateEffectParams(r.id);
            doc->setModified();
        }
        return stored;
    }
    if (r.kind == Resolved::None)
        return false;
    // A function's values are already in it: forget what to put back.
    VCEffectTracker *tracker = VCEffectTracker::instance(doc);
    bool had = false;
    for (int role = 0; role < RoleCount; ++role)
        had |= tracker->hasOriginal(r.id, role);
    tracker->forgetOriginals(r.id);
    if (had)
        doc->setModified();
    return had;
}

/*****************************************************************************
 * Load & save
 *****************************************************************************/

bool VCEffectControl::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCEffectControl)
        return false;
    const QXmlStreamAttributes a = root.attributes();
    target = stringToTarget(a.value(KXMLQLCVCEffectControlTarget).toString());
    palette = a.hasAttribute(KXMLQLCVCEffectControlPalette)
              ? a.value(KXMLQLCVCEffectControlPalette).toUInt() : kInvalid;
    group = a.hasAttribute(KXMLQLCVCEffectControlGroup)
            ? a.value(KXMLQLCVCEffectControlGroup).toUInt() : kInvalid;
    role = stringToRole(a.value(KXMLQLCVCEffectControlRole).toString());
    root.skipCurrentElement();
    return true;
}

void VCEffectControl::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLQLCVCEffectControl);
    doc->writeAttribute(KXMLQLCVCEffectControlTarget, targetToString(target));
    if (target == TargetPalette && palette != kInvalid)
        doc->writeAttribute(KXMLQLCVCEffectControlPalette, QString::number(palette));
    if (target != TargetPalette && group != kInvalid)
        doc->writeAttribute(KXMLQLCVCEffectControlGroup, QString::number(group));
    doc->writeAttribute(KXMLQLCVCEffectControlRole, roleToString(role));
    doc->writeEndElement();
}

/*****************************************************************************
 * Tracker
 *****************************************************************************/

VCEffectTracker *VCEffectTracker::instance(Doc *doc)
{
    VCEffectTracker *t = doc->findChild<VCEffectTracker *>(QString(), Qt::FindDirectChildrenOnly);
    return t != nullptr ? t : new VCEffectTracker(doc);
}

VCEffectTracker::VCEffectTracker(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    if (EffectScriptRunner *runner = doc->effectScriptRunner())
    {
        connect(runner, &EffectScriptRunner::effectStarted, this, &VCEffectTracker::slotEffectStarted);
        connect(runner, &EffectScriptRunner::effectStopped, this, &VCEffectTracker::slotEffectStopped);
        connect(runner, &EffectScriptRunner::overridesChanged, this, &VCEffectTracker::changed);
    }
    connect(doc->masterTimer(), &MasterTimer::functionStarted, this, &VCEffectTracker::slotFunctionStarted);
    connect(doc->masterTimer(), &MasterTimer::functionStopped, this, &VCEffectTracker::slotFunctionStopped);
    connect(doc, &Doc::cleared, this, &VCEffectTracker::slotDocCleared);
    if (ProgrammingManager *pm = ProgrammingManager::instance())
        connect(pm, &ProgrammingManager::currentPaletteIdChanged, this, &VCEffectTracker::slotPaletteSelected);

    // What already plays, when made after it started.
    foreach (Function *f, doc->functions())
    {
        if (f == nullptr || !f->isRunning())
            continue;
        if (f->type() == Function::EFXType)
            m_recent << Effect{ VCEffectControl::Resolved::EFX, f->id() };
        else if (f->type() == Function::RGBMatrixType)
            m_recent << Effect{ VCEffectControl::Resolved::RGBMatrix, f->id() };
        else if (Scene *s = qobject_cast<Scene *>(f))
            for (quint32 pid : s->palettes())
                if (doc->effectScriptRunner() != nullptr && doc->effectScriptRunner()->isPlaying(pid))
                    m_recent << Effect{ VCEffectControl::Resolved::Palette, pid };
    }
    m_started = m_recent;
}

void VCEffectTracker::bringToFront(const Effect &e, bool started)
{
    m_recent.removeAll(e);
    m_recent.prepend(e);
    while (m_recent.count() > kMaxRecent)
        m_recent.removeLast();
    if (started)
    {
        m_started.removeAll(e);
        m_started.prepend(e);
        while (m_started.count() > kMaxRecent)
            m_started.removeLast();
    }
}

VCEffectTracker::Effect VCEffectTracker::current(quint32 group, bool selected) const
{
    QList<quint32> groupFixtures;
    if (group != kInvalid)
    {
        const FixtureGroup *g = m_doc->fixtureGroup(group);
        if (g == nullptr)
            return Effect();
        groupFixtures = g->fixtureList();
    }
    for (const Effect &e : selected ? m_recent : m_started)
    {
        if (!isPlaying(e))
            continue;
        if (group == kInvalid)
            return e;
        for (quint32 fid : fixturesOf(e))
            if (groupFixtures.contains(fid))
                return e;
    }
    return Effect();
}

bool VCEffectTracker::isPlaying(const Effect &e) const
{
    if (e.kind == VCEffectControl::Resolved::Palette)
        return m_doc->effectScriptRunner() != nullptr && m_doc->effectScriptRunner()->isPlaying(e.id);
    const Function *f = m_doc->function(e.id);
    return f != nullptr && f->isRunning() && !f->stopped();
}

QList<quint32> VCEffectTracker::fixturesOf(const Effect &e) const
{
    QList<quint32> out;
    if (e.kind == VCEffectControl::Resolved::Palette)
    {
        const quint32 sid = m_doc->effectScriptRunner() != nullptr
                            ? m_doc->effectScriptRunner()->sceneOf(e.id) : kInvalid;
        const Scene *s = qobject_cast<const Scene *>(m_doc->function(sid));
        if (s == nullptr)
            return out;
        out = s->fixtures();
        for (quint32 gid : s->fixtureGroups())
            if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
                out += g->fixtureList();
    }
    else if (const EFX *efx = qobject_cast<const EFX *>(m_doc->function(e.id)))
    {
        for (const EFXFixture *ef : efx->fixtures())
            out << ef->head().fxi;
    }
    else if (const RGBMatrix *m = qobject_cast<const RGBMatrix *>(m_doc->function(e.id)))
    {
        if (const FixtureGroup *g = m_doc->fixtureGroup(m->fixtureGroup()))
            out = g->fixtureList();
    }
    return out;
}

void VCEffectTracker::rememberOriginal(quint32 fid, int role, const QVariant &value)
{
    m_originals[fid].insert(role, value);
}

bool VCEffectTracker::hasOriginal(quint32 fid, int role) const
{
    return m_originals.value(fid).contains(role);
}

QVariant VCEffectTracker::original(quint32 fid, int role) const
{
    return m_originals.value(fid).value(role);
}

void VCEffectTracker::forgetOriginals(quint32 fid)
{
    m_originals.remove(fid);
    emit changed();
}

void VCEffectTracker::restoreFunction(quint32 fid)
{
    Function *f = m_doc->function(fid);
    const QHash<int, QVariant> orig = m_originals.take(fid);
    const int attr = m_intensity.contains(fid) ? m_intensity.take(fid) : -1;
    if (f == nullptr)
        return;
    if (attr >= 0)
        f->releaseAttributeOverride(attr);
    for (auto it = orig.constBegin(); it != orig.constEnd(); ++it)
    {
        switch (it.key())
        {
            case VCEffectControl::Speed:
                f->setDuration(it.value().toUInt());
            break;
            case VCEffectControl::Direction:
                f->setDirection(Function::Direction(it.value().toInt()));
            break;
            case VCEffectControl::Size:
                if (EFX *efx = qobject_cast<EFX *>(f))
                {
                    efx->setWidth(it.value().toPoint().x());
                    efx->setHeight(it.value().toPoint().y());
                }
            break;
            case VCEffectControl::Hue:
                if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
                {
                    m->setColor(0, it.value().value<QColor>());
                    m->updateColorDelta();
                }
            break;
            default:
            break;
        }
    }
}

void VCEffectTracker::slotPaletteSelected(quint32 paletteId)
{
    const QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr || p->type() != QLCPalette::Effect)
        return;
    const Effect e{ VCEffectControl::Resolved::Palette, paletteId };
    if (!m_recent.isEmpty() && m_recent.first() == e)
        return;                                 // a redraw of the same one
    bringToFront(e, false);
    emit changed();
}

void VCEffectTracker::slotEffectStarted(quint32 paletteId)
{
    bringToFront(Effect{ VCEffectControl::Resolved::Palette, paletteId }, true);
    emit changed();
}

void VCEffectTracker::slotEffectStopped(quint32 paletteId)
{
    Q_UNUSED(paletteId)
    emit changed();
}

void VCEffectTracker::slotFunctionStarted(quint32 fid)
{
    const Function *f = m_doc->function(fid);
    if (f == nullptr)
        return;
    if (f->type() == Function::EFXType)
        bringToFront(Effect{ VCEffectControl::Resolved::EFX, fid }, true);
    else if (f->type() == Function::RGBMatrixType)
        bringToFront(Effect{ VCEffectControl::Resolved::RGBMatrix, fid }, true);
    else
        return;
    emit changed();
}

void VCEffectTracker::slotFunctionStopped(quint32 fid)
{
    const Function *f = m_doc->function(fid);
    if (f == nullptr || (f->type() != Function::EFXType && f->type() != Function::RGBMatrixType))
        return;
    // A live override is for this run only.
    restoreFunction(fid);
    emit changed();
}

void VCEffectTracker::slotDocCleared()
{
    m_recent.clear();
    m_started.clear();
    m_originals.clear();
    m_intensity.clear();
    emit changed();
}
