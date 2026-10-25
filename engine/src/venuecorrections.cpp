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
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QFileInfo>
#include <QTimer>
#include <QFile>
#include <QDir>

#include <cmath>

#include "venuecorrections.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "stagetarget.h"
#include "inputoutputmap.h"
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

/* A practice venue's right calibration, as a <Truth> (Q140 practice). */
void writeTruth(QXmlStreamWriter &w, quint32 fid, const RigCalibration &c)
{
    w.writeStartElement(QStringLiteral("Truth"));
    w.writeAttribute(QStringLiteral("ID"), QString::number(fid));
    w.writeAttribute(QStringLiteral("PanOfs"), num(c.panOffsetDeg, 2));
    w.writeAttribute(QStringLiteral("TiltOfs"), num(c.tiltOffsetDeg, 2));
    w.writeAttribute(QStringLiteral("PanZero"), num(c.panZeroDir, 1));
    if (c.lensDeg > 0.0f)
        w.writeAttribute(QStringLiteral("Lens"), num(c.lensDeg, 1));
    w.writeEndElement();
}

RigCalibration readTruth(const QXmlStreamAttributes &a)
{
    RigCalibration c;
    c.panOffsetDeg = a.value(QStringLiteral("PanOfs")).toFloat();
    c.tiltOffsetDeg = a.value(QStringLiteral("TiltOfs")).toFloat();
    c.panZeroDir = a.value(QStringLiteral("PanZero")).toFloat();
    c.lensDeg = a.value(QStringLiteral("Lens")).toFloat();
    return c;
}

/* How far @p saved is from @p truth: pan and tilt together, degrees. */
double offBy(const RigCalibration &saved, const RigCalibration &truth)
{
    const double dp = double(saved.panOffsetDeg - truth.panOffsetDeg);
    const double dt = double(saved.tiltOffsetDeg - truth.tiltOffsetDeg);
    return std::sqrt(dp * dp + dt * dt);
}
}

VenueCorrections::VenueCorrections(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    m_rig = new VenueRig(doc, this);
    /* The house rig changed while a venue is applied: into its file. A
       burst (a house fixture dragged on the plot) is written once it ends. */
    m_rigTimer = new QTimer(this);
    m_rigTimer->setSingleShot(true);
    m_rigTimer->setInterval(400);
    connect(m_rigTimer, &QTimer::timeout, this, [this]() { recordRig(); });
    connect(m_rig, &VenueRig::rigChanged, this, [this](bool soon) {
        if (m_writing)
            return;
        if (soon)
            m_rigTimer->start();
        else
            recordRig();
    });
    // A patch changed while a venue is applied is that room's (Q138).
    connect(m_rig, &VenueRig::patchEdited, this, [this]() {
        if (!m_writing)
            keepPatch();
    });

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
    // A house fixture's calibration is part of its house-rig entry.
    if (m_rig->isVenueFixture(fid))
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
        QString wanted = m_active;
        // A practice left applied: the show opens on the venue it came from.
        if (hasVenue(wanted) && venue(wanted).practice.on)
            wanted = venue(wanted).practice.from;
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
    // The show saved: what the house rig is now goes into its venue too.
    if (isActive() && activeVenueData() != nullptr)
    {
        m_rigTimer->stop();
        recordRig();
        if (m_rig->patchChanged(activeVenueData()->rig))
            keepPatch();
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

bool VenueCorrections::writeSidecarFor(const QString &showFile, QString *why) const
{
    const QString path = sidecarPath(showFile);
    if (path.isEmpty() || m_venues.isEmpty())
        return false;
    QByteArray xml = toXml();
    if (!m_showFile.isEmpty())
        xml.replace(QStringLiteral("Show=\"%1\"").arg(QFileInfo(m_showFile).fileName().toHtmlEscaped()).toUtf8(),
                    QStringLiteral("Show=\"%1\"").arg(QFileInfo(showFile).fileName().toHtmlEscaped()).toUtf8());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(xml) < 0 || !f.commit())
    {
        if (why != nullptr)
            *why = tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
        return false;
    }
    return true;
}

QString VenueCorrections::showStem(const QString &showFile)
{
    QString stem = QFileInfo(showFile).completeBaseName();
    if (stem.endsWith(QStringLiteral(".venues")))
        stem.chop(7);
    static const QRegularExpression tails[] = {
        QRegularExpression(QStringLiteral("\\s*\\(\\d+\\)$")),          // "Show (2)"
        QRegularExpression(QStringLiteral("\\s+copy(\\s+\\d+)?$"),
                           QRegularExpression::CaseInsensitiveOption),    // "Show copy 2"
        QRegularExpression(QStringLiteral("[-_ ]v?\\d+(\\.\\d+)*$"),
                           QRegularExpression::CaseInsensitiveOption),    // "Show-0.6", "Show v3"
    };
    for (bool again = true; again;)
    {
        again = false;
        for (const QRegularExpression &re : tails)
        {
            const QString cut = QString(stem).remove(re);
            if (cut != stem && !cut.isEmpty())
            {
                stem = cut;
                again = true;
            }
        }
    }
    return stem.trimmed();
}

QStringList VenueCorrections::importCandidates(const QString &showFile)
{
    QStringList out;
    if (showFile.isEmpty() || QFile::exists(sidecarPath(showFile)))
        return out;
    const QFileInfo fi(showFile);
    const QString stem = showStem(showFile);
    const QFileInfoList files = fi.dir().entryInfoList({ QStringLiteral("*.venues.xml") }, QDir::Files,
                                                       QDir::Time);
    for (const QFileInfo &f : files)
    {
        QFile file(f.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        QXmlStreamReader r(&file);
        if (!r.readNextStartElement() || r.name() != kRoot)
            continue;
        const QString show = r.attributes().value(QStringLiteral("Show")).toString();
        if (!show.isEmpty() && showStem(show) == stem)
            out << f.absoluteFilePath();
    }
    return out;
}

bool VenueCorrections::importFrom(const QString &sidecar, QString *why)
{
    QFile f(sidecar);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (why != nullptr)
            *why = tr("Cannot read %1: %2").arg(QDir::toNativeSeparators(sidecar), f.errorString());
        return false;
    }
    unapply();
    if (!fromXml(f.readAll(), why))
        return false;
    const QString wanted = m_active;
    m_active.clear();
    m_confirmed = false;
    if (!wanted.isEmpty() && hasVenue(wanted))
    {
        m_active = wanted;
        apply();
    }
    save(why);
    emit activeVenueChanged(m_active);
    emit venuesChanged();
    emit correctionsChanged();
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
    /* Version 2 only when a venue owns more than corrections (VENUES.md
       section 7): a file without is written as it always was. */
    bool owns = false;
    for (const Venue &v : m_venues)
        owns = owns || !v.rig.isEmpty();
    w.writeAttribute(QStringLiteral("Version"), QString::number(owns ? 2 : kVersion));
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
        VenueRig::writeXml(w, v.rig);
        // Who checked what, and the dead lights (Q134, Q140).
        for (auto it = v.checks.constBegin(); it != v.checks.constEnd(); ++it)
        {
            w.writeStartElement(QStringLiteral("Check"));
            w.writeAttribute(QStringLiteral("ID"), QString::number(it.key()));
            if (it.value().when.isValid())
                w.writeAttribute(QStringLiteral("When"), it.value().when.toString(Qt::ISODate));
            if (!it.value().by.isEmpty())
                w.writeAttribute(QStringLiteral("By"), it.value().by);
            if (it.value().dead)
                w.writeAttribute(QStringLiteral("Dead"), QStringLiteral("1"));
            w.writeEndElement();
        }
        if (v.practice.on)
        {
            const Practice &p = v.practice;
            w.writeStartElement(QStringLiteral("Practice"));
            if (!p.from.isEmpty())
                w.writeAttribute(QStringLiteral("From"), p.from);
            if (p.started.isValid())
                w.writeAttribute(QStringLiteral("Started"), p.started.toString(Qt::ISODateWithMs));
            if (p.lastSave.isValid())
                w.writeAttribute(QStringLiteral("LastSave"), p.lastSave.toString(Qt::ISODateWithMs));
            w.writeAttribute(QStringLiteral("Saves"), QString::number(p.saves));
            for (auto it = p.truth.constBegin(); it != p.truth.constEnd(); ++it)
                writeTruth(w, it.key(), it.value());
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
            // The house rig, the patch, what is not ours (read whole).
            if (VenueRig::readXml(r, v.rig))
                continue;
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
            else if (r.name() == QLatin1String("Check"))
            {
                bool ok = false;
                const QXmlStreamAttributes a = r.attributes();
                const quint32 id = a.value(QStringLiteral("ID")).toUInt(&ok);
                if (ok)
                {
                    Check c;
                    c.when = QDateTime::fromString(a.value(QStringLiteral("When")).toString(), Qt::ISODate);
                    c.by = a.value(QStringLiteral("By")).toString();
                    c.dead = a.value(QStringLiteral("Dead")) == QLatin1String("1");
                    v.checks.insert(id, c);
                }
            }
            else if (r.name() == QLatin1String("Practice"))
            {
                const QXmlStreamAttributes a = r.attributes();
                v.practice.on = true;
                v.practice.from = a.value(QStringLiteral("From")).toString();
                v.practice.started = QDateTime::fromString(a.value(QStringLiteral("Started")).toString(),
                                                           Qt::ISODateWithMs);
                v.practice.lastSave = QDateTime::fromString(a.value(QStringLiteral("LastSave")).toString(),
                                                            Qt::ISODateWithMs);
                v.practice.saves = a.value(QStringLiteral("Saves")).toInt();
                while (r.readNextStartElement())
                {
                    if (r.name() == QLatin1String("Truth"))
                    {
                        bool ok = false;
                        const quint32 id = r.attributes().value(QStringLiteral("ID")).toUInt(&ok);
                        if (ok)
                            v.practice.truth.insert(id, readTruth(r.attributes()));
                    }
                    r.skipCurrentElement();
                }
                continue;   // its end element is read
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
    updateReserved();
    return true;
}

void VenueCorrections::updateReserved()
{
    QSet<quint32> ids;
    for (const Venue &v : m_venues)
        ids += v.rig.fixtureIds();
    m_rig->setReservedIds(ids);
}

VenueCorrections::Venue *VenueCorrections::activeVenueData()
{
    for (Venue &v : m_venues)
        if (v.name == m_active)
            return &v;
    return nullptr;
}

bool VenueCorrections::isNotOurs(quint32 universe, quint32 address) const
{
    if (m_doc == nullptr || !isActive())
        return false;
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    return universe < quint32(unis.size()) && unis.at(int(universe))->isNotOurs(int(address));
}

bool VenueCorrections::recordRig(QString *why)
{
    Venue *v = activeVenueData();
    if (v == nullptr || !m_rig->isApplied())
        return true;
    bool changed = m_rig->capture(v->rig);
    /* A fixture now the venue's: its calibration is in its house-rig entry,
       not a correction. One now the show's: its calibration as it is, the
       show's own. */
    for (quint32 fid : m_rig->venueFixtures())
        if (v->fixtures.remove(fid) > 0)
        {
            v->names.remove(fid);
            v->makers.remove(fid);
            v->models.remove(fid);
            v->modes.remove(fid);
            changed = true;
        }
    for (const Fixture *fx : m_doc->fixtures())
        if (!m_rig->isVenueFixture(fx->id()))
            ensureBase(fx->id());
    if (!changed)
        return true;
    v->modified = QDateTime::currentDateTime();
    updateReserved();
    emit houseRigChanged();
    return save(why);
}

int VenueCorrections::moveShowHouseIn()
{
    const int n = m_rig->moveShowHouseIn();
    if (n > 0)
        recordRig();
    return n;
}

bool VenueCorrections::keepPatch(QString *why)
{
    Venue *v = activeVenueData();
    if (v == nullptr)
    {
        if (why != nullptr)
            *why = tr("No venue is applied.");
        return false;
    }
    v->rig.patch = m_rig->currentPatch();
    v->modified = QDateTime::currentDateTime();
    emit houseRigChanged();
    return save(why);
}

bool VenueCorrections::forgetPatch(QString *why)
{
    Venue *v = activeVenueData();
    if (v == nullptr)
    {
        if (why != nullptr)
            *why = tr("No venue is applied.");
        return false;
    }
    v->rig.patch.clear();
    const bool was = m_writing;
    m_writing = true;
    m_rig->restoreShowPatch();
    m_writing = was;
    v->modified = QDateTime::currentDateTime();
    emit houseRigChanged();
    return save(why);
}

bool VenueCorrections::setNotOurs(const QString &name, const QList<VenueRig::NotOurs> &notOurs, QString *why)
{
    bool found = false;
    for (Venue &v : m_venues)
        if (v.name == name)
        {
            v.rig.notOurs = notOurs;
            v.modified = QDateTime::currentDateTime();
            found = true;
        }
    if (!found)
    {
        if (why != nullptr)
            *why = tr("There is no venue called %1.").arg(name);
        return false;
    }
    if (name == m_active && isActive())
        m_rig->applyNotOurs(notOurs);
    emit houseRigChanged();
    return save(why);
}

QList<VenueRig::Clash> VenueCorrections::clashes(const QString &name) const
{
    if (!hasVenue(name))
        return QList<VenueRig::Clash>();
    const bool applied = isActive() && name == m_active;
    return m_rig->clashes(venue(name).rig, applied);
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
    if (props == nullptr || !isActive() || props->hasCalibrationBase(fid) || m_rig->isVenueFixture(fid))
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
    /* What the room owns first (VENUES.md): its house fixtures, their
       places and heads in the groups, its patch, what is not ours. */
    if (Venue *own = activeVenueData())
    {
        const bool was = m_writing;
        m_writing = true;
        m_rig->apply(own->rig);
        m_writing = was;
        // Renumbered on the way in (a show fixture took an id): written so.
        if (m_rig->capture(own->rig))
        {
            updateReserved();
            save();
        }
        QStringList lines;
        for (const VenueRig::Clash &c : m_rig->clashes(own->rig, true))
            lines << c.text;
        if (!lines.isEmpty())
            emit note(tr("%1 clashes with the show:\n%2").arg(m_active, lines.join(QLatin1Char('\n'))));
    }
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
        // A house fixture's calibration is part of its house-rig entry.
        if (m_doc->fixture(it.key()) == nullptr || m_rig->isVenueFixture(it.key()))
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

bool VenueCorrections::isSwapped(const QString &name, quint32 fid) const
{
    const Venue v = venue(name);
    if (!v.models.contains(fid) || m_doc->fixture(fid) == nullptr)
        return false;
    QString maker, model, mode;
    identify(fid, maker, model, mode);
    return v.makers.value(fid) != maker || v.models.value(fid) != model || v.modes.value(fid) != mode;
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
    // The house rig as it is now goes with its venue before it comes off.
    if (m_rig->isApplied())
    {
        m_rigTimer->stop();
        recordRig();
    }
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
    const bool was = m_writing;
    m_writing = true;
    m_rig->unapply();
    m_writing = was;
}

bool VenueCorrections::record(quint32 fid, QString *why)
{
    if (!isActive())
    {
        if (why != nullptr)
            *why = tr("No venue is applied.");
        return false;
    }
    // A house fixture's calibration is part of its house-rig entry.
    if (m_rig->isVenueFixture(fid))
        return recordRig(why);
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
    if (m_doc->fixture(fid) == nullptr || needsFocus(fid))
    {
        // Not on the rig (orphaned, or skipped): only the venue holds it.
        for (Venue &v : m_venues)
            if (v.name == m_active)
            {
                v.fixtures.remove(fid);
                v.names.remove(fid);
                v.makers.remove(fid);
                v.models.remove(fid);
                v.modes.remove(fid);
                v.modified = QDateTime::currentDateTime();
            }
        m_needsFocus.removeAll(fid);
        emit correctionsChanged();
        return save();
    }
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
    m_rigTimer->stop();
    m_rig->forget();
    m_rig->setReservedIds(QSet<quint32>());
    m_venues.clear();
    m_active.clear();
    m_needsFocus.clear();
    m_showFile.clear();
    if (had)
        emit activeVenueChanged(QString());
    emit venuesChanged();
}

/*****************************************************************************
 * A new venue from the last one (board Q140)
 *****************************************************************************/

QString VenueCorrections::addVenueFrom(const QString &name, const QString &from)
{
    if (!hasVenue(from))
        return QString();
    if (!name.trimmed().isEmpty() && hasVenue(name.trimmed()))
        return name.trimmed();
    const Venue src = venue(from);
    const QString n = addVenue(name);
    for (Venue &v : m_venues)
    {
        if (v.name != n)
            continue;
        // What the rig was there; nothing has been checked here yet.
        v.fixtures = src.fixtures;
        v.names = src.names;
        v.makers = src.makers;
        v.models = src.models;
        v.modes = src.modes;
        v.trims = src.trims;
        v.targets = src.targets;
    }
    save();
    emit venuesChanged();
    return n;
}

QString VenueCorrections::lastVenue() const
{
    QString out;
    QDateTime newest;
    for (const Venue &v : m_venues)
    {
        if (v.practice.on)
            continue;
        const QDateTime when = v.modified.isValid() ? v.modified : v.created;
        if (out.isEmpty() || (when.isValid() && (!newest.isValid() || when > newest)))
        {
            out = v.name;
            newest = when;
        }
    }
    return out;
}

/*****************************************************************************
 * Checks and dead lights (board Q134, Q140)
 *****************************************************************************/

bool VenueCorrections::markChecked(const QList<quint32> &fids)
{
    if (!isActive() || fids.isEmpty())
        return false;
    const QDateTime now = QDateTime::currentDateTime();
    for (Venue &v : m_venues)
    {
        if (v.name != m_active)
            continue;
        for (quint32 fid : fids)
        {
            Check &c = v.checks[fid];
            c.when = now;
            c.by = m_checkedBy;
        }
        if (v.practice.on)
        {
            ++v.practice.saves;
            v.practice.lastSave = now;
        }
        v.modified = now;
    }
    emit correctionsChanged();
    return save();
}

bool VenueCorrections::isChecked(quint32 fid) const
{
    return isActive() && venue(m_active).checks.value(fid).when.isValid();
}

VenueCorrections::Check VenueCorrections::check(quint32 fid) const
{
    return isActive() ? venue(m_active).checks.value(fid) : Check();
}

bool VenueCorrections::setDead(quint32 fid, bool dead)
{
    if (!isActive())
        return false;
    for (Venue &v : m_venues)
    {
        if (v.name != m_active)
            continue;
        Check c = v.checks.value(fid);
        if (c.dead == dead)
            return true;
        c.dead = dead;
        if (!c.dead && !c.when.isValid())
            v.checks.remove(fid);
        else
            v.checks.insert(fid, c);
        v.modified = QDateTime::currentDateTime();
    }
    emit correctionsChanged();
    return save();
}

bool VenueCorrections::isDead(quint32 fid) const
{
    return isActive() && venue(m_active).checks.value(fid).dead;
}

QList<quint32> VenueCorrections::deadLights() const
{
    QList<quint32> out;
    if (!isActive())
        return out;
    const Venue v = venue(m_active);
    for (auto it = v.checks.constBegin(); it != v.checks.constEnd(); ++it)
        if (it.value().dead)
            out << it.key();
    return out;
}

/*****************************************************************************
 * Scramble practice
 *****************************************************************************/

QString VenueCorrections::startPractice(const QList<quint32> &fids, quint32 seed, float minDeg,
                                        float maxDeg, QString *why)
{
    if (filePath().isEmpty())
    {
        if (why != nullptr)
            *why = tr("Save the show first: a practice venue is kept beside it, like any venue.");
        return QString();
    }
    if (isPracticeActive())
    {
        if (why != nullptr)
            *why = tr("A practice is applied already: end it first.");
        return QString();
    }
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> lights;
    for (quint32 fid : fids)
        if (m_doc->fixture(fid) != nullptr && !lights.contains(fid))
            lights << fid;
    if (lights.isEmpty())
    {
        if (why != nullptr)
            *why = tr("None of these lights can be scrambled: practice moves the moving heads.");
        return QString();
    }

    QString name = tr("Practice %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    {
        const QString stem = name;
        for (int i = 2; hasVenue(name); ++i)
            name = QStringLiteral("%1 (%2)").arg(stem).arg(i);
    }
    Venue v;
    v.name = name;
    v.created = v.modified = QDateTime::currentDateTime();
    v.practice.on = true;
    v.practice.from = m_active;
    v.practice.started = v.created;
    QRandomGenerator gen(seed);
    const float lo = qMax(0.0f, qMin(minDeg, maxDeg));
    const float hi = qMax(minDeg, maxDeg);
    auto off = [&gen, lo, hi]() {
        const float mag = lo + float(gen.generateDouble()) * (hi - lo);
        const float rounded = float(qRound(double(mag) * 10.0) / 10.0);
        return gen.bounded(2) == 0 ? -rounded : rounded;
    };
    for (quint32 fid : lights)
    {
        // Right is where it is right now: this room's, else the show's.
        const RigCalibration truth = RigCalibration::of(props->fixtureRigProps(fid));
        RigCalibration scrambled = truth;
        scrambled.panOffsetDeg = truth.panOffsetDeg + off();
        scrambled.tiltOffsetDeg = truth.tiltOffsetDeg + off();
        v.practice.truth.insert(fid, truth);
        v.fixtures.insert(fid, scrambled);
        if (const Fixture *fx = m_doc->fixture(fid))
            v.names.insert(fid, fx->name());
        QString maker, model, mode;
        identify(fid, maker, model, mode);
        v.makers.insert(fid, maker);
        v.models.insert(fid, model);
        v.modes.insert(fid, mode);
    }
    // The targets and trims as the room has them now.
    if (isActive())
    {
        const Venue cur = venue(m_active);
        v.trims = cur.trims;
        v.targets = cur.targets;
    }
    m_venues << v;
    emit venuesChanged();
    if (!setActiveVenue(name))
        return QString();
    return name;
}

bool VenueCorrections::isPracticeActive() const
{
    return isActive() && venue(m_active).practice.on;
}

bool VenueCorrections::practiceTruth(quint32 fid, RigCalibration &truth) const
{
    if (!isPracticeActive())
        return false;
    const Venue v = venue(m_active);
    if (!v.practice.truth.contains(fid))
        return false;
    truth = v.practice.truth.value(fid);
    return true;
}

double VenueCorrections::practiceError(const QString &name, quint32 fid) const
{
    const Venue v = venue(name);
    if (!v.practice.on || !v.practice.truth.contains(fid))
        return -1.0;
    // A light focused exactly back to the show's own has no correction left.
    const RigCalibration saved = v.fixtures.contains(fid) ? v.fixtures.value(fid) : baseOf(fid);
    return offBy(saved, v.practice.truth.value(fid));
}

VenueCorrections::Score VenueCorrections::practiceScore(const QString &name) const
{
    Score sc;
    const Venue v = venue(name);
    if (!v.practice.on)
        return sc;
    double sum = 0.0;
    for (auto it = v.practice.truth.constBegin(); it != v.practice.truth.constEnd(); ++it)
    {
        if (m_doc->fixture(it.key()) == nullptr)
            continue;
        const double e = practiceError(name, it.key());
        sc.errors.insert(it.key(), e);
        ++sc.lights;
        sum += e;
        sc.worstError = qMax(sc.worstError, e);
        if (e <= 1.0 + 1e-6)
            ++sc.withinOne;
        if (v.checks.value(it.key()).when.isValid())
            ++sc.checked;
    }
    sc.meanError = sc.lights > 0 ? sum / sc.lights : 0.0;
    sc.saves = v.practice.saves;
    if (v.practice.started.isValid() && v.practice.lastSave.isValid())
        sc.seconds = qMax<qint64>(0, v.practice.started.msecsTo(v.practice.lastSave)) / 1000.0;
    return sc;
}

bool VenueCorrections::endPractice()
{
    if (!isPracticeActive())
        return false;
    const QString from = venue(m_active).practice.from;
    return setActiveVenue(hasVenue(from) ? from : QString());
}
