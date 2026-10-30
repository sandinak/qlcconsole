/*
  Q Light Controller Plus
  focusreport.cpp

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

#include <QLocale>
#include <QHash>
#include <QSet>

#include <algorithm>

#include "csvfield.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "focussession.h"
#include "focusreport.h"
#include "teachingsettings.h"
#include "showoperate.h"
#include "rigowner.h"
#include "stagetarget.h"
#include "focuscalls.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

namespace
{
QString signedDeg(float v)
{
    // A real minus sign, as the panels have.
    return (v < 0.0f ? QStringLiteral("−") : QStringLiteral("+"))
           + QString::number(double(qAbs(v)), 'f', 1) + QStringLiteral("°");
}

bool differs(float a, float b)
{
    return qAbs(a - b) >= 0.05f;
}

QString when(const QDateTime &t)
{
    return t.isValid() ? QLocale().toString(t, QLocale::ShortFormat) : QString();
}
}

QList<FocusReport::Row> FocusReport::rows(Doc *doc, const QString &venueName)
{
    QList<Row> out;
    if (doc == nullptr || doc->programmer() == nullptr || doc->monitorProperties() == nullptr)
        return out;
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    MonitorProperties *props = doc->monitorProperties();
    const VenueCorrections::Venue v = venues->venue(venueName);
    const bool practice = v.practice.on;

    auto fill = [&](Row &r, quint32 fid) {
        const Fixture *fx = doc->fixture(fid);
        r.fid = fid;
        r.fixture = fx != nullptr ? fx->name() : v.names.value(fid, tr("Fixture %1").arg(fid));
        if (fx != nullptr)
        {
            if (const QLCFixtureDef *def = fx->fixtureDef())
                r.type = QStringLiteral("%1 %2").arg(def->manufacturer(), def->model()).trimmed();
            if (const QLCFixtureMode *m = fx->fixtureMode())
                r.mode = m->name();
            r.address = QStringLiteral("%1.%2").arg(fx->universe() + 1).arg(fx->address() + 1);
            // Whose light it is: the tour's, the house's, a rental (Q137).
            r.owner = RigOwner::displayName(RigOwner::effective(doc, fid));
            const FixtureRigProps rp = props->fixtureRigProps(fid);
            if (const Truss *t = props->truss(rp.trussId))
                r.hung = t->name();
        }
        // The correction: this venue's calibration against the show's own.
        const RigCalibration base = venues->baseOf(fid);
        const bool corrected = v.fixtures.contains(fid);
        const RigCalibration c = corrected ? v.fixtures.value(fid) : base;
        QStringList parts;
        if (c.fixedAim)
        {
            if (differs(c.panZeroDir, base.panZeroDir))
                parts << tr("aim %1 pan").arg(signedDeg(c.panZeroDir - base.panZeroDir));
            if (differs(c.aimDown, base.aimDown))
                parts << tr("aim %1 down").arg(signedDeg(c.aimDown - base.aimDown));
        }
        else
        {
            if (differs(c.panOffsetDeg, base.panOffsetDeg))
                parts << tr("%1 pan").arg(signedDeg(c.panOffsetDeg - base.panOffsetDeg));
            if (differs(c.tiltOffsetDeg, base.tiltOffsetDeg))
                parts << tr("%1 tilt").arg(signedDeg(c.tiltOffsetDeg - base.tiltOffsetDeg));
        }
        r.correction = parts.join(QStringLiteral(", "));
        if (c.lensDeg > 0.0f)
            r.lens = QString::number(double(c.lensDeg), 'f', 1) + QStringLiteral("°");
        const VenueCorrections::Check check = v.checks.value(fid);
        r.checked = check.when.isValid();
        r.checkedAt = check.when;
        r.checkedBy = check.by;
        r.dead = check.dead;
        r.swapped = venues->isSwapped(venueName, fid);
        r.needsFocus = r.swapped || (!r.checked && !r.dead);
        if (practice)
            r.practiceError = venues->practiceError(venueName, fid);
        QStringList notes;
        if (r.swapped)
            notes << tr("Focused here as %1 %2, %3").arg(v.makers.value(fid), v.models.value(fid),
                                                          v.modes.value(fid));
        if (fx == nullptr)
            notes << tr("No longer in the show");
        r.notes = notes.join(QStringLiteral("; "));
    };

    auto targetOf = [&](Row &r, const FocusSession::Reference &ref) {
        if (ref.kind == FocusSession::Reference::None)
        {
            r.target = tr("where it lands");
            return;
        }
        r.target = FocusCalls::targetName(doc, ref);
        if (ref.kind != FocusSession::Reference::Target)
            return;
        const StageTarget *t = props->stageTarget(ref.id);
        if (t == nullptr)
            return;
        // Where it stands in THIS venue: its own position there, else the show's.
        const QVector3D at = v.targets.contains(ref.id) ? v.targets.value(ref.id) : t->basePosition();
        r.position = props->stagePositionText(at);
    };

    QSet<quint32> listed;
    const QList<FocusCall> calls = doc->programmer()->focusCalls()->calls();
    for (int i = 0; i < calls.size(); ++i)
    {
        const FocusCall &call = calls.at(i);
        const QList<quint32> fids = FocusSession::stageOrder(doc, FocusCalls::fixturesOf(doc, call));
        for (quint32 fid : fids)
        {
            Row r;
            r.call = i;
            r.callName = FocusCalls::displayName(doc, call);
            fill(r, fid);
            targetOf(r, call.target);
            if (!call.note.isEmpty())
                r.notes = r.notes.isEmpty() ? call.note : call.note + QStringLiteral("; ") + r.notes;
            out << r;
            listed.insert(fid);
        }
    }
    // Lights the venue corrects, checks or scrambled that no call has.
    QList<quint32> others;
    for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
        others << it.key();
    for (auto it = v.checks.constBegin(); it != v.checks.constEnd(); ++it)
        others << it.key();
    for (auto it = v.practice.truth.constBegin(); it != v.practice.truth.constEnd(); ++it)
        others << it.key();
    std::sort(others.begin(), others.end());
    for (quint32 fid : others)
    {
        if (listed.contains(fid))
            continue;
        listed.insert(fid);
        Row r;
        fill(r, fid);
        r.target = QString();
        out << r;
    }
    return out;
}

QString FocusReport::flags(const Row &row)
{
    QStringList f;
    if (row.dead)
        f << tr("dead");
    if (row.swapped)
        f << tr("swapped");
    if (row.needsFocus)
        f << tr("needs focus");
    return f.join(QStringLiteral(", "));
}

QString FocusReport::html(Doc *doc, const QString &venueName, const QString &showName)
{
    const QList<Row> list = rows(doc, venueName);
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    const VenueCorrections::Venue v = venues->venue(venueName);
    auto esc = [](const QString &t) { return t.toHtmlEscaped(); };

    int checked = 0, dead = 0, needs = 0;
    QStringList who;
    QSet<quint32> seen;
    for (const Row &r : list)
    {
        if (seen.contains(r.fid))
            continue;
        seen.insert(r.fid);
        checked += r.checked ? 1 : 0;
        dead += r.dead ? 1 : 0;
        needs += r.needsFocus ? 1 : 0;
        if (!r.checkedBy.isEmpty() && !who.contains(r.checkedBy))
            who << r.checkedBy;
    }

    QString h;
    h += QStringLiteral("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">");
    h += QStringLiteral("<title>%1</title>").arg(esc(tr("Focus report: %1").arg(venueName)));
    h += QStringLiteral("<style>"
                        "body{font-family:Helvetica,Arial,sans-serif;font-size:10pt;color:#000;background:#fff}"
                        "h1{font-size:16pt;margin:0 0 4pt 0}"
                        "p.meta{margin:0 0 8pt 0;color:#333}"
                        "table{border-collapse:collapse;width:100%}"
                        "th,td{border:1px solid #888;padding:3pt 4pt;vertical-align:top;text-align:left}"
                        "th{background:#e8e8e8}"
                        "tr.call td{background:#f4f4f4;font-weight:bold}"
                        "td.flag{color:#a00;font-weight:bold}"
                        "</style></head><body>\n");
    h += QStringLiteral("<h1>%1</h1>\n").arg(esc(tr("Focus report: %1").arg(venueName)));
    QStringList meta;
    if (!showName.isEmpty())
        meta << tr("Show: %1").arg(esc(showName));
    meta << tr("Printed %1").arg(esc(when(QDateTime::currentDateTime())));
    if (v.modified.isValid())
        meta << tr("Venue last changed %1").arg(esc(when(v.modified)));
    h += QStringLiteral("<p class=\"meta\">%1</p>\n").arg(meta.join(QStringLiteral(" &middot; ")));
    h += QStringLiteral("<p class=\"meta\">%1</p>\n")
         .arg(esc(tr("%1 lights: %2 checked, %3 dead, %4 need focus.")
                  .arg(seen.size()).arg(checked).arg(dead).arg(needs)));
    // A hand-in from a Teaching show says so: which lesson, and whether the
    // teacher's PIN was on it (so a grader knows how far to trust the score).
    if (doc->teachingSettings()->enabled())
    {
        const TeachingSettings *t = doc->teachingSettings();
        QStringList teach;
        teach << (t->lesson().isEmpty() ? tr("Teaching show") : tr("Teaching show, lesson %1").arg(t->lesson()));
        teach << (doc->operateSettings()->hasPin() ? tr("PIN set: yes") : tr("PIN set: no"));
        if (!t->handedOut().isEmpty())
            teach << tr("handed out %1").arg(when(QDateTime::fromString(t->handedOut(), Qt::ISODate)));
        h += QStringLiteral("<p class=\"meta\">%1</p>\n").arg(esc(teach.join(QStringLiteral(" \u00b7 "))));
    }
    if (!who.isEmpty())
        h += QStringLiteral("<p class=\"meta\">%1</p>\n").arg(esc(tr("Focused by: %1").arg(who.join(QStringLiteral(", ")))));

    if (v.practice.on)
    {
        const VenueCorrections::Score sc = venues->practiceScore(venueName);
        h += QStringLiteral("<h2>%1</h2>\n").arg(esc(tr("Practice score")));
        h += QStringLiteral("<p>%1</p>\n").arg(esc(
                 tr("Scrambled from %1. %2 of %3 lights within 1°; average %4° off, worst %5°. "
                    "%6 saves in %7.")
                 .arg(v.practice.from.isEmpty() ? tr("the show's own calibration") : v.practice.from)
                 .arg(sc.withinOne).arg(sc.lights)
                 .arg(sc.meanError, 0, 'f', 1).arg(sc.worstError, 0, 'f', 1)
                 .arg(sc.saves)
                 .arg(sc.seconds >= 60.0 ? tr("%1 min %2 s").arg(int(sc.seconds) / 60).arg(int(sc.seconds) % 60)
                                         : tr("%1 s").arg(int(sc.seconds)))));
    }

    h += QStringLiteral("<table>\n<tr>");
    QStringList heads = { tr("Fixture"), tr("Type"), tr("Owner"), tr("Hung"), tr("Focus to"), tr("Where"),
                          tr("Correction"), tr("Lens"), tr("Checked"), tr("Flags"), tr("Notes") };
    if (v.practice.on)
        heads.insert(7, tr("Off by"));
    for (const QString &c : heads)
        h += QStringLiteral("<th>%1</th>").arg(esc(c));
    h += QStringLiteral("</tr>\n");
    int lastCall = -2;
    for (const Row &r : list)
    {
        if (r.call != lastCall)
        {
            lastCall = r.call;
            const QString title = r.call >= 0 ? tr("Call %1: %2").arg(r.call + 1).arg(r.callName)
                                              : tr("Outside the calls");
            h += QStringLiteral("<tr class=\"call\"><td colspan=\"%1\">%2</td></tr>\n")
                 .arg(heads.size()).arg(esc(title));
        }
        QStringList cells = { r.fixture, r.type, r.owner, r.hung, r.target, r.position,
                              r.correction.isEmpty() ? QStringLiteral("—") : r.correction, r.lens,
                              r.checked ? (r.checkedBy.isEmpty() ? when(r.checkedAt)
                                                                 : tr("%1, %2").arg(r.checkedBy, when(r.checkedAt)))
                                        : QString() };
        if (v.practice.on)
            cells.insert(7, r.practiceError >= 0.0 ? QStringLiteral("%1°").arg(r.practiceError, 0, 'f', 1)
                                                    : QString());
        h += QStringLiteral("<tr>");
        for (const QString &c : cells)
            h += QStringLiteral("<td>%1</td>").arg(esc(c));
        h += QStringLiteral("<td class=\"flag\">%1</td><td>%2</td></tr>\n").arg(esc(flags(r)), esc(r.notes));
    }
    h += QStringLiteral("</table>\n");

    // The room: trims and targets moved for this venue (Q128).
    MonitorProperties *props = doc->monitorProperties();
    if (!v.trims.isEmpty() || !v.targets.isEmpty())
    {
        h += QStringLiteral("<h2>%1</h2>\n<ul>\n").arg(esc(tr("The room")));
        for (auto it = v.trims.constBegin(); it != v.trims.constEnd(); ++it)
        {
            const Truss *t = props->truss(it.key());
            h += QStringLiteral("<li>%1</li>\n").arg(esc(tr("Trim %1: %2")
                     .arg(t != nullptr ? t->name() : QString::number(it.key()),
                          MonitorProperties::formatStageDistance(it.value(), props->gridUnits()))));
        }
        for (auto it = v.targets.constBegin(); it != v.targets.constEnd(); ++it)
        {
            const StageTarget *t = props->stageTarget(it.key());
            h += QStringLiteral("<li>%1</li>\n").arg(esc(tr("Target %1: %2")
                     .arg(t != nullptr ? t->name() : QString::number(it.key()),
                          props->stagePositionText(it.value()))));
        }
        h += QStringLiteral("</ul>\n");
    }
    h += QStringLiteral("</body></html>\n");
    return h;
}

QStringList FocusReport::csvColumns()
{
    // Lightwright's own names where it has one, so an import maps itself.
    return { QStringLiteral("Purpose"), QStringLiteral("Position"), QStringLiteral("Unit#"),
             QStringLiteral("Instrument Type"), QStringLiteral("Address"), QStringLiteral("Focus"),
             QStringLiteral("Focus Position"), QStringLiteral("Correction"), QStringLiteral("Lens"),
             QStringLiteral("Checked By"), QStringLiteral("Checked At"), QStringLiteral("Flags"),
             QStringLiteral("Notes"), QStringLiteral("Fixture"), QStringLiteral("Fixture ID"),
             QStringLiteral("Owner") };
}

QString FocusReport::csvField(const QString &text)
{
    return CsvField::escape(text);
}

QString FocusReport::csv(Doc *doc, const QString &venueName)
{
    const QList<Row> list = rows(doc, venueName);
    QStringList lines;
    lines << csvColumns().join(QLatin1Char(','));
    // The unit number: the light's place on its position, counted as listed.
    QHash<QString, int> units;
    for (const Row &r : list)
    {
        const QString unit = r.hung.isEmpty() ? QString() : QString::number(++units[r.hung]);
        QString flagText = flags(r);
        QString notes = r.notes;
        if (r.practiceError >= 0.0)
            notes = (notes.isEmpty() ? QString() : notes + QStringLiteral("; "))
                    + tr("practice: %1° off").arg(r.practiceError, 0, 'f', 1);
        const QStringList fields = {
            r.call >= 0 ? tr("Call %1: %2").arg(r.call + 1).arg(r.callName) : QString(),
            r.hung, unit, r.type, r.address, r.target, r.position, r.correction, r.lens,
            r.checkedBy, r.checkedAt.isValid() ? r.checkedAt.toString(Qt::ISODate) : QString(),
            flagText, notes, r.fixture, QString::number(r.fid), r.owner };
        QStringList out;
        for (const QString &f : fields)
            out << csvField(f);
        lines << out.join(QLatin1Char(','));
    }
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}
