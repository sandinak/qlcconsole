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
                }
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
    apply();
    save();
    emit activeVenueChanged(m_active);
    emit correctionsChanged();
    return true;
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
    props->setFixtureRigProps(fid, rp);
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
    const Venue v = venue(m_active);
    for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
    {
        // A fixture removed from the show since: nothing to correct.
        if (m_doc->fixture(it.key()) == nullptr)
            continue;
        ensureBase(it.key());
        writeRig(it.key(), it.value());
    }
}

void VenueCorrections::unapply()
{
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr)
        return;
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
        }
        else
        {
            v.fixtures.insert(fid, now);
            if (const Fixture *fx = m_doc->fixture(fid))
                v.names.insert(fid, fx->name());
        }
        v.modified = QDateTime::currentDateTime();
    }
    emit correctionsChanged();
    return save(why);
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
    m_showFile.clear();
    if (had)
        emit activeVenueChanged(QString());
    emit venuesChanged();
}
