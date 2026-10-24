/*
  Q Light Controller Plus
  venuecorrections.cpp

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
#include <QSaveFile>
#include <QFileInfo>
#include <QFile>
#include <QDir>

#include "venuecorrections.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "stagetarget.h"
#include "fixture.h"
#include "doc.h"

namespace
{
const QString kRoot = QStringLiteral("QlcVenues");
const QString kVenue = QStringLiteral("Venue");
const QString kFixture = QStringLiteral("Fixture");
const int kVersion = 1;

QString num(float v, int places)
{
    return QString::number(double(v), 'f', places);
}
}

VenueCorrections::VenueCorrections(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    /* Board Q111: with a venue applied, a calibration changed anywhere --
       the fixture dialog, the Lighting Studio -- is that venue's, and goes
       into its file, not the show's. */
    if (m_doc != nullptr && m_doc->monitorProperties() != nullptr)
        connect(m_doc->monitorProperties(), &MonitorProperties::rigPropsChanged,
                this, &VenueCorrections::onRigPropsChanged);
    // A target moved or a truss trimmed while a venue is applied (Q128).
    if (m_doc != nullptr && m_doc->monitorProperties() != nullptr)
    {
        connect(m_doc->monitorProperties(), &MonitorProperties::stageTargetsChanged,
                this, &VenueCorrections::syncStructure);
        connect(m_doc->monitorProperties(), &MonitorProperties::rigPropsChanged,
                this, &VenueCorrections::syncStructure);
    }
    // A fixture added while a venue is applied: its calibration is held too.
    if (m_doc != nullptr)
        connect(m_doc, &Doc::fixtureAdded, this, [this](quint32 fid) {
            if (isActive())
                ensureBase(fid);
        });
}

void VenueCorrections::onRigPropsChanged(quint32 fid)
{
    if (!isActive() || m_writing || m_held.contains(fid) || m_doc->fixture(fid) == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    const RigCalibration now = RigCalibration::of(props->fixtureRigProps(fid));
    // A skipped correction (Q133) is not on the rig: compare with the base.
    const RigCalibration was = (hasCorrection(fid) && !needsFocus(fid)) ? correction(fid) : baseOf(fid);
    if (now == was)
        return;     // something else about the fixture: its place, its mount
    record(fid);
}

void VenueCorrections::setHeld(const QList<quint32> &fids)
{
    m_held = QSet<quint32>(fids.begin(), fids.end());
}

VenueCorrections::~VenueCorrections()
{
}

/*****************************************************************************
 * The sidecar file
 *****************************************************************************/

QString VenueCorrections::sidecarPath(const QString &showFile)
{
    if (showFile.isEmpty())
        return QString();
    const QFileInfo fi(showFile);
    return fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".venues.xml"));
}

void VenueCorrections::setShowFile(const QString &showFile, bool opened)
{
    if (opened)
    {
        reset();
        m_confirmed = false;
        m_showFile = showFile;
        if (!load())
            return;
        // The venue it was focused for, applied again.
        const QString wanted = m_active;
        m_active.clear();
        if (!wanted.isEmpty() && hasVenue(wanted))
        {
            m_active = wanted;
            apply();
            emit activeVenueChanged(m_active);
        }
        emit venuesChanged();
        return;
    }
    if (showFile == m_showFile)
        return;
    m_showFile = showFile;
    // Saved under a new name: the venues go with it.
    if (!m_venues.isEmpty())
        save();
}

bool VenueCorrections::load(QString *why)
{
    const QString path = filePath();
    if (path.isEmpty() || !QFile::exists(path))
    {
        m_venues.clear();
        m_active.clear();
        return true;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (why != nullptr)
            *why = tr("Cannot read %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
        return false;
    }
    return fromXml(f.readAll(), why);
}

bool VenueCorrections::save(QString *why)
{
    const QString path = filePath();
    if (path.isEmpty())
    {
        if (why != nullptr)
            *why = tr("Save the show first: a venue's corrections are kept next to it.");
        return false;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
    {
        if (why != nullptr)
            *why = tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
        return false;
    }
    f.write(toXml());
    if (!f.commit())
    {
        if (why != nullptr)
            *why = tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
        return false;
    }
    return true;
}

QByteArray VenueCorrections::toXml() const
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(1);
    w.writeStartDocument();
    w.writeStartElement(kRoot);
    w.writeAttribute(QStringLiteral("Version"), QString::number(kVersion));
    if (!m_showFile.isEmpty())
        w.writeAttribute(QStringLiteral("Show"), QFileInfo(m_showFile).fileName());
    if (!m_active.isEmpty())
        w.writeAttribute(QStringLiteral("Active"), m_active);
    for (const Venue &v : m_venues)
    {
        w.writeStartElement(kVenue);
        w.writeAttribute(QStringLiteral("Name"), v.name);
        if (v.created.isValid())
            w.writeAttribute(QStringLiteral("Created"), v.created.toString(Qt::ISODate));
        if (v.modified.isValid())
            w.writeAttribute(QStringLiteral("Modified"), v.modified.toString(Qt::ISODate));
        for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
        {
            const RigCalibration &c = it.value();
            w.writeStartElement(kFixture);
            w.writeAttribute(QStringLiteral("ID"), QString::number(it.key()));
            if (v.names.contains(it.key()))
                w.writeAttribute(QStringLiteral("Name"), v.names.value(it.key()));
            // What it was when focused (Q133), so a swapped fixture is caught.
            if (!v.models.value(it.key()).isEmpty())
            {
                w.writeAttribute(QStringLiteral("Manufacturer"), v.makers.value(it.key()));
                w.writeAttribute(QStringLiteral("Model"), v.models.value(it.key()));
                w.writeAttribute(QStringLiteral("Mode"), v.modes.value(it.key()));
            }
            w.writeAttribute(QStringLiteral("PanOfs"), num(c.panOffsetDeg, 2));
            w.writeAttribute(QStringLiteral("TiltOfs"), num(c.tiltOffsetDeg, 2));
            if (c.fixedAim)
            {
                w.writeAttribute(QStringLiteral("AimPan"), num(c.panZeroDir, 1));
                w.writeAttribute(QStringLiteral("AimDown"), num(c.aimDown, 1));
            }
            else
            {
                w.writeAttribute(QStringLiteral("PanZero"), num(c.panZeroDir, 1));
            }
            if (c.lensDeg > 0.0f)
                w.writeAttribute(QStringLiteral("Lens"), num(c.lensDeg, 1));
            w.writeEndElement();
        }
        for (auto it = v.trims.constBegin(); it != v.trims.constEnd(); ++it)
        {
            w.writeStartElement(QStringLiteral("Trim"));
            w.writeAttribute(QStringLiteral("Truss"), QString::number(it.key()));
            if (const Truss *t = m_doc != nullptr && m_doc->monitorProperties() != nullptr
                                 ? m_doc->monitorProperties()->truss(it.key()) : nullptr)
                w.writeAttribute(QStringLiteral("Name"), t->name());
            w.writeAttribute(QStringLiteral("Z"), num(it.value(), 3));
            w.writeEndElement();
        }
        for (auto it = v.targets.constBegin(); it != v.targets.constEnd(); ++it)
        {
            w.writeStartElement(QStringLiteral("Target"));
            w.writeAttribute(QStringLiteral("ID"), QString::number(it.key()));
            if (const StageTarget *t = m_doc != nullptr && m_doc->monitorProperties() != nullptr
                                       ? m_doc->monitorProperties()->stageTarget(it.key()) : nullptr)
                w.writeAttribute(QStringLiteral("Name"), t->name());
            w.writeAttribute(QStringLiteral("X"), num(it.value().x(), 3));
            w.writeAttribute(QStringLiteral("Y"), num(it.value().y(), 3));
            w.writeAttribute(QStringLiteral("Z"), num(it.value().z(), 3));
            w.writeEndElement();
        }
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

bool VenueCorrections::fromXml(const QByteArray &xml, QString *why)
{
    QXmlStreamReader r(xml);
    if (!r.readNextStartElement() || r.name() != kRoot)
    {
        if (why != nullptr)
            *why = tr("This is not a venue corrections file.");
        return false;
    }
    QList<Venue> venues;
    const QString active = r.attributes().value(QStringLiteral("Active")).toString();
    while (r.readNextStartElement())
    {
        if (r.name() != kVenue)
        {
            r.skipCurrentElement();
            continue;
        }
        Venue v;
        const QXmlStreamAttributes va = r.attributes();
        v.name = va.value(QStringLiteral("Name")).toString();
        v.created = QDateTime::fromString(va.value(QStringLiteral("Created")).toString(), Qt::ISODate);
        v.modified = QDateTime::fromString(va.value(QStringLiteral("Modified")).toString(), Qt::ISODate);
        while (r.readNextStartElement())
        {
            if (r.name() == kFixture)
            {
                const QXmlStreamAttributes a = r.attributes();
                bool ok = false;
                const quint32 fid = a.value(QStringLiteral("ID")).toUInt(&ok);
                if (ok)
                {
                    RigCalibration c;
                    c.panOffsetDeg = a.value(QStringLiteral("PanOfs")).toFloat();
                    c.tiltOffsetDeg = a.value(QStringLiteral("TiltOfs")).toFloat();
                    c.fixedAim = a.hasAttribute(QStringLiteral("AimDown"));
                    c.panZeroDir = c.fixedAim ? a.value(QStringLiteral("AimPan")).toFloat()
                                              : a.value(QStringLiteral("PanZero")).toFloat();
                    if (c.fixedAim)
                        c.aimDown = a.value(QStringLiteral("AimDown")).toFloat();
                    c.lensDeg = a.value(QStringLiteral("Lens")).toFloat();
                    v.fixtures.insert(fid, c);
                    if (a.hasAttribute(QStringLiteral("Name")))
                        v.names.insert(fid, a.value(QStringLiteral("Name")).toString());
                    if (a.hasAttribute(QStringLiteral("Model")))
                    {
                        v.makers.insert(fid, a.value(QStringLiteral("Manufacturer")).toString());
                        v.models.insert(fid, a.value(QStringLiteral("Model")).toString());
                        v.modes.insert(fid, a.value(QStringLiteral("Mode")).toString());
                    }
                }
            }
            else if (r.name() == QLatin1String("Trim"))
            {
                bool ok = false;
                const quint32 id = r.attributes().value(QStringLiteral("Truss")).toUInt(&ok);
                if (ok)
                    v.trims.insert(id, r.attributes().value(QStringLiteral("Z")).toFloat());
            }
            else if (r.name() == QLatin1String("Target"))
            {
                bool ok = false;
                const QXmlStreamAttributes a = r.attributes();
                const quint32 id = a.value(QStringLiteral("ID")).toUInt(&ok);
                if (ok)
                    v.targets.insert(id, QVector3D(a.value(QStringLiteral("X")).toFloat(),
                                                   a.value(QStringLiteral("Y")).toFloat(),
                                                   a.value(QStringLiteral("Z")).toFloat()));
            }
            r.skipCurrentElement();
        }
        if (!v.name.isEmpty())
            venues << v;
    }
    if (r.hasError())
    {
        if (why != nullptr)
            *why = tr("The venue corrections file is damaged: %1").arg(r.errorString());
        return false;
    }
    m_venues = venues;
    m_active = hasVenue(active) ? active : QString();
    return true;
}

/*****************************************************************************
 * Venues
 *****************************************************************************/

QStringList VenueCorrections::venueNames() const
{
    QStringList out;
    for (const Venue &v : m_venues)
        out << v.name;
    return out;
}

bool VenueCorrections::hasVenue(const QString &name) const
{
    return venueNames().contains(name);
}

VenueCorrections::Venue VenueCorrections::venue(const QString &name) const
{
    for (const Venue &v : m_venues)
        if (v.name == name)
            return v;
    return Venue();
}

QString VenueCorrections::defaultVenueName(const QDateTime &when)
{
    return tr("Venue %1").arg(when.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
}

QString VenueCorrections::addVenue(const QString &name)
{
    QString n = name.trimmed();
    if (!n.isEmpty() && hasVenue(n))
        return n;
    if (n.isEmpty())
    {
        n = defaultVenueName();
        const QString stem = n;
        for (int i = 2; hasVenue(n); ++i)
            n = QStringLiteral("%1 (%2)").arg(stem).arg(i);
    }
    Venue v;
    v.name = n;
    v.created = v.modified = QDateTime::currentDateTime();
    m_venues << v;
    save();
    emit venuesChanged();
    return n;
}

bool VenueCorrections::renameVenue(const QString &from, const QString &to)
{
    const QString n = to.trimmed();
    if (n.isEmpty() || hasVenue(n) || !hasVenue(from))
        return false;
    for (Venue &v : m_venues)
        if (v.name == from)
            v.name = n;
    if (m_active == from)
    {
        m_active = n;
        emit activeVenueChanged(m_active);
    }
    save();
    emit venuesChanged();
    return true;
}

bool VenueCorrections::removeVenue(const QString &name)
{
    if (!hasVenue(name))
        return false;
    if (m_active == name)
        setActiveVenue(QString());
    for (int i = 0; i < m_venues.size(); ++i)
        if (m_venues.at(i).name == name)
            m_venues.removeAt(i--);
    save();
    emit venuesChanged();
    return true;
}

bool VenueCorrections::setActiveVenue(const QString &name)
{
    if (!name.isEmpty() && !hasVenue(name))
        return false;
    if (name == m_active)
        return true;
    unapply();
    m_active = name;
    // Chosen just now: the room it is.
    m_confirmed = !name.isEmpty();
    apply();
    save();
    emit activeVenueChanged(m_active);
    emit correctionsChanged();
    return true;
}

QDateTime VenueCorrections::lastChanged(const QString &name) const
{
    const Venue v = venue(name);
    return v.modified.isValid() ? v.modified : v.created;
}

bool VenueCorrections::needsConfirm(const QDateTime &now) const
{
    if (!isActive() || m_confirmed)
        return false;
    const QDateTime when = lastChanged(m_active);
    return !when.isValid() || when.secsTo(now) > qint64(staleAfterHours()) * 3600;
}

/*****************************************************************************
 * Corrections
 *****************************************************************************/

void VenueCorrections::writeRig(quint32 fid, const RigCalibration &c)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (RigCalibration::of(rp) == c)
        return;
    c.applyTo(rp);
    // Its own writes are not edits to record.
    const bool was = m_writing;
    m_writing = true;
    props->setFixtureRigProps(fid, rp);
    m_writing = was;
}

void VenueCorrections::ensureBase(quint32 fid)
{
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr || !isActive() || props->hasCalibrationBase(fid))
        return;
    props->setCalibrationBase(fid, RigCalibration::of(props->fixtureRigProps(fid)));
}

RigCalibration VenueCorrections::baseOf(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props->hasCalibrationBase(fid))
        return props->calibrationBase(fid);
    return RigCalibration::of(props->fixtureRigProps(fid));
}

void VenueCorrections::apply()
{
    if (!isActive() || m_doc == nullptr || m_doc->monitorProperties() == nullptr)
        return;
    /* The show's calibration of every fixture is held from here on, so
       nothing done to it while the venue is applied reaches the show file
       -- not only what the venue corrects now. */
    for (const Fixture *fx : m_doc->fixtures())
        ensureBase(fx->id());
    /* Trims and targets (Q128): the show's own held, the venue's on. */
    {
        MonitorProperties *props = m_doc->monitorProperties();
        const Venue vv = venue(m_active);
        const bool was = m_writing;
        m_writing = true;
        for (Truss *t : props->trusses())
        {
            if (t->isChildBar())
                continue;
            if (!t->hasBaseTrim())
                t->setBaseTrim(t->origin().z());
            if (vv.trims.contains(t->id()))
            {
                QVector3D o = t->origin();
                o.setZ(vv.trims.value(t->id()));
                t->setOrigin(o);
            }
        }
        for (StageTarget *t : props->stageTargets())
        {
            if (!t->hasBasePosition())
                t->setBasePosition(t->position());
            if (vv.targets.contains(t->id()))
                t->setPosition(vv.targets.value(t->id()));
        }
        if (!vv.trims.isEmpty())
            props->recomputeChildTrusses();
        m_writing = was;
    }
    m_needsFocus.clear();
    QList<quint32> unchecked;
    const Venue v = venue(m_active);
    for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
    {
        // A fixture removed from the show since: kept, listed (orphaned()).
        if (m_doc->fixture(it.key()) == nullptr)
            continue;
        /* Board Q133: focused as another fixture (definition or mode
           changed, a lamp swapped for a mover): its correction would aim
           it wrong. Skipped, and it needs focusing again. */
        QString maker, model, mode;
        identify(it.key(), maker, model, mode);
        if (v.models.contains(it.key()))
        {
            if (v.makers.value(it.key()) != maker || v.models.value(it.key()) != model
                || v.modes.value(it.key()) != mode)
            {
                m_needsFocus << it.key();
                continue;
            }
        }
        else
            unchecked << it.key();
        ensureBase(it.key());
        writeRig(it.key(), it.value());
    }
    /* A file from before definitions were kept: applied as it is, said
       once, and from now on it remembers what each light is. */
    if (!unchecked.isEmpty())
    {
        for (Venue &w : m_venues)
            if (w.name == m_active)
                for (quint32 fid : unchecked)
                {
                    QString maker, model, mode;
                    identify(fid, maker, model, mode);
                    w.makers.insert(fid, maker);
                    w.models.insert(fid, model);
                    w.modes.insert(fid, mode);
                }
        save();
        emit note(tr("%1 was saved before venues remembered each light's fixture type: its "
                     "corrections were applied as they are. Check any light that was swapped "
                     "since.").arg(m_active));
    }
}

void VenueCorrections::identify(quint32 fid, QString &maker, QString &model, QString &mode) const
{
    const Fixture *fx = m_doc != nullptr ? m_doc->fixture(fid) : nullptr;
    maker.clear();
    model.clear();
    mode.clear();
    if (fx == nullptr)
        return;
    if (const QLCFixtureDef *def = fx->fixtureDef())
    {
        maker = def->manufacturer();
        model = def->model();
    }
    if (const QLCFixtureMode *m = fx->fixtureMode())
        mode = m->name();
}

QList<quint32> VenueCorrections::orphaned() const
{
    QList<quint32> out;
    if (!isActive())
        return out;
    const Venue v = venue(m_active);
    for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
        if (m_doc->fixture(it.key()) == nullptr)
            out << it.key();
    return out;
}

QString VenueCorrections::correctedAs(quint32 fid) const
{
    const Venue v = venue(m_active);
    const QString name = v.names.value(fid, tr("Fixture %1").arg(fid));
    if (v.models.value(fid).isEmpty())
        return name;
    return tr("%1 (%2 %3, %4)").arg(name, v.makers.value(fid), v.models.value(fid), v.modes.value(fid));
}

void VenueCorrections::unapply()
{
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    m_needsFocus.clear();
    if (props == nullptr)
        return;
    {
        const bool was = m_writing;
        m_writing = true;
        bool trimmed = false;
        for (Truss *t : props->trusses())
            if (t->hasBaseTrim())
            {
                if (t->origin().z() != t->baseTrim())
                {
                    QVector3D o = t->origin();
                    o.setZ(t->baseTrim());
                    t->setOrigin(o);
                    trimmed = true;
                }
                t->clearBaseTrim();
            }
        for (StageTarget *t : props->stageTargets())
            if (t->hasBasePosition())
            {
                t->setPosition(t->basePosition());
                t->clearBasePosition();
            }
        if (trimmed)
            props->recomputeChildTrusses();
        m_writing = was;
    }
    for (quint32 fid : props->calibrationBaseFixtures())
    {
        if (m_doc->fixture(fid) != nullptr)
            writeRig(fid, props->calibrationBase(fid));
        props->clearCalibrationBase(fid);
    }
}

bool VenueCorrections::record(quint32 fid, QString *why)
{
    if (!isActive())
    {
        if (why != nullptr)
            *why = tr("No venue is applied.");
        return false;
    }
    MonitorProperties *props = m_doc->monitorProperties();
    ensureBase(fid);
    const RigCalibration now = RigCalibration::of(props->fixtureRigProps(fid));
    for (Venue &v : m_venues)
    {
        if (v.name != m_active)
            continue;
        if (now == props->calibrationBase(fid))
        {
            v.fixtures.remove(fid);
            v.names.remove(fid);
            v.makers.remove(fid);
            v.models.remove(fid);
            v.modes.remove(fid);
        }
        else
        {
            v.fixtures.insert(fid, now);
            if (const Fixture *fx = m_doc->fixture(fid))
                v.names.insert(fid, fx->name());
            QString maker, model, mode;
            identify(fid, maker, model, mode);
            v.makers.insert(fid, maker);
            v.models.insert(fid, model);
            v.modes.insert(fid, mode);
        }
        v.modified = QDateTime::currentDateTime();
    }
    // Focused again here: no longer skipped.
    m_needsFocus.removeAll(fid);
    emit correctionsChanged();
    return save(why);
}

bool VenueCorrections::recordTarget(quint32 id, QString *why)
{
    StageTarget *t = isActive() ? m_doc->monitorProperties()->stageTarget(id) : nullptr;
    if (t == nullptr)
    {
        if (why != nullptr)
            *why = tr("No venue is applied.");
        return false;
    }
    if (!t->hasBasePosition())
        t->setBasePosition(t->position());
    for (Venue &v : m_venues)
        if (v.name == m_active)
        {
            if (t->position() == t->basePosition())
                v.targets.remove(id);
            else
                v.targets.insert(id, t->position());
            v.modified = QDateTime::currentDateTime();
        }
    emit correctionsChanged();
    return save(why);
}

bool VenueCorrections::recordTrim(quint32 id, QString *why)
{
    Truss *t = isActive() ? m_doc->monitorProperties()->truss(id) : nullptr;
    if (t == nullptr)
    {
        if (why != nullptr)
            *why = tr("No venue is applied.");
        return false;
    }
    if (!t->hasBaseTrim())
        t->setBaseTrim(t->origin().z());
    for (Venue &v : m_venues)
        if (v.name == m_active)
        {
            if (qFuzzyCompare(1.0f + t->origin().z(), 1.0f + t->baseTrim()))
                v.trims.remove(id);
            else
                v.trims.insert(id, t->origin().z());
            v.modified = QDateTime::currentDateTime();
        }
    emit correctionsChanged();
    return save(why);
}

bool VenueCorrections::hasTargetOverride(quint32 id) const
{
    return isActive() && venue(m_active).targets.contains(id);
}

bool VenueCorrections::hasTrimOverride(quint32 id) const
{
    return isActive() && venue(m_active).trims.contains(id);
}

bool VenueCorrections::clearTarget(quint32 id)
{
    StageTarget *t = hasTargetOverride(id) ? m_doc->monitorProperties()->stageTarget(id) : nullptr;
    if (t == nullptr)
        return false;
    const bool was = m_writing;
    m_writing = true;
    t->setPosition(t->basePosition());
    m_writing = was;
    const bool ok = recordTarget(id);
    m_doc->monitorProperties()->notifyStageTargetsChanged();
    return ok;
}

bool VenueCorrections::clearTrim(quint32 id)
{
    Truss *t = hasTrimOverride(id) ? m_doc->monitorProperties()->truss(id) : nullptr;
    if (t == nullptr)
        return false;
    QVector3D o = t->origin();
    o.setZ(t->baseTrim());
    t->setOrigin(o);
    m_doc->monitorProperties()->recomputeChildTrusses();
    return recordTrim(id);
}

void VenueCorrections::syncStructure()
{
    if (!isActive() || m_writing || m_doc == nullptr || m_doc->monitorProperties() == nullptr)
        return;
    const Venue v = venue(m_active);
    MonitorProperties *props = m_doc->monitorProperties();
    for (StageTarget *t : props->stageTargets())
    {
        const QVector3D expected = v.targets.value(t->id(), t->basePosition());
        if (t->position() != expected)
            recordTarget(t->id());
    }
    for (Truss *t : props->trusses())
    {
        // A bar on a truss follows its parent: the parent's trim is the one.
        if (t->isChildBar())
            continue;
        const float expected = v.trims.value(t->id(), t->baseTrim());
        if (!qFuzzyCompare(1.0f + t->origin().z(), 1.0f + expected))
            recordTrim(t->id());
    }
}

bool VenueCorrections::clearCorrection(quint32 fid)
{
    if (!hasCorrection(fid))
        return false;
    writeRig(fid, baseOf(fid));
    return record(fid);
}

bool VenueCorrections::hasCorrection(quint32 fid) const
{
    return isActive() && venue(m_active).fixtures.contains(fid);
}

RigCalibration VenueCorrections::correction(quint32 fid) const
{
    return venue(m_active).fixtures.value(fid);
}

int VenueCorrections::correctionCount() const
{
    return isActive() ? venue(m_active).fixtures.size() : 0;
}

void VenueCorrections::reset()
{
    // MonitorProperties::reset() drops the bases with the rig itself.
    if (m_doc != nullptr && m_doc->monitorProperties() != nullptr)
        for (quint32 fid : m_doc->monitorProperties()->calibrationBaseFixtures())
            m_doc->monitorProperties()->clearCalibrationBase(fid);
    const bool had = isActive();
    m_venues.clear();
    m_active.clear();
    m_needsFocus.clear();
    m_showFile.clear();
    if (had)
        emit activeVenueChanged(QString());
    emit venuesChanged();
}
