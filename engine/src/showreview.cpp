/*
  Q Light Controller Plus - qlcconsole
  showreview.cpp

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
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <algorithm>

#include "showreview.h"
#include "monitorproperties.h"
#include "scenery.h"
#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "rgbmatrix.h"
#include "efxfixture.h"
#include "fixture.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

#define KXMLAccepted    QStringLiteral("Accepted")
#define KXMLOff         QStringLiteral("Off")
#define KXMLCheck       QStringLiteral("Check")
#define KXMLTarget      QStringLiteral("Target")
#define KXMLHash        QStringLiteral("Hash")
#define KXMLBy          QStringLiteral("By")
#define KXMLWhen        QStringLiteral("When")

static const char *kNameKey = "showreview/name";

/****************************************************************************
 * ReviewFinding
 ****************************************************************************/

QString ReviewFinding::key() const
{
    QStringList t = targets;
    std::sort(t.begin(), t.end());
    return check + QLatin1Char('|') + t.join(QLatin1Char(','));
}

QString ReviewFinding::hash() const
{
    return QString::fromLatin1(QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha1)
                               .toHex().left(8));
}

/****************************************************************************
 * The catalog
 ****************************************************************************/

const QList<ShowReview::Check> &ShowReview::catalog()
{
    static const QList<Check> all = []() {
        QList<Check> c = ShowReviewChecks::showChecks();
        c += ShowReviewChecks::moveChecks();
        c += ShowReviewChecks::zoneChecks();
        c += ShowReviewChecks::treeChecks();
        c += ShowReviewChecks::vcChecks();
        c += ShowReviewChecks::venueChecks();
        return c;
    }();
    return all;
}

const ShowReview::Check *ShowReview::check(const QString &id)
{
    for (const Check &c : catalog())
        if (c.id == id)
            return &c;
    return nullptr;
}

QString ShowReview::areaName(Area area)
{
    switch (area)
    {
        case Movement: return QCoreApplication::translate("ShowReview", "Movement");
        case Palettes: return QCoreApplication::translate("ShowReview", "Palettes and looks");
        case Cues: return QCoreApplication::translate("ShowReview", "Tracking and cues");
        case Hits: return QCoreApplication::translate("ShowReview", "Hits and Virtual Console");
        case ShowControl: return QCoreApplication::translate("ShowReview", "Show control");
        case Patch: return QCoreApplication::translate("ShowReview", "Patch and outputs");
        case Fixtures: return QCoreApplication::translate("ShowReview", "Fixtures and definitions");
        case Venues: return QCoreApplication::translate("ShowReview", "Venues and focus");
        case Effects: return QCoreApplication::translate("ShowReview", "Effects");
        case Power: return QCoreApplication::translate("ShowReview", "Power and safety");
        case Files: return QCoreApplication::translate("ShowReview", "Files");
        case Naming: return QCoreApplication::translate("ShowReview", "Naming");
        case Teaching: return QCoreApplication::translate("ShowReview", "Teaching");
    }
    return QString();
}

QString ShowReview::severityName(ReviewFinding::Severity severity)
{
    switch (severity)
    {
        case ReviewFinding::Blocker: return QCoreApplication::translate("ShowReview", "blocker");
        case ReviewFinding::Warning: return QCoreApplication::translate("ShowReview", "warning");
        case ReviewFinding::Note: return QCoreApplication::translate("ShowReview", "note");
    }
    return QString();
}

QString ShowReview::footerCheckId(const QString &statusKey)
{
    // docs/design/SHOW_REVIEW.md section 2.1: D entries only. The runtime ones
    // (outputs answering, devices asleep, cloud placeholders...) stay the
    // footer's and have no id.
    static const QHash<QString, QString> ids = {
        { QStringLiteral("fixtures.unpatched"), QStringLiteral("PAT-03") },
        { QStringLiteral("fixtures.clash"), QStringLiteral("PAT-01") },
        { QStringLiteral("fixtures.duplicates"), QStringLiteral("FIX-14") },
        { QStringLiteral("fixtures.output"), QStringLiteral("FIX-07") },
        { QStringLiteral("fixtures.buried"), QStringLiteral("FIX-15") },
        { QStringLiteral("fixtures.offrow"), QStringLiteral("FIX-16") },
        { QStringLiteral("fixtures.nodefinition"), QStringLiteral("FIX-01") },
        { QStringLiteral("definitions.outofdate"), QStringLiteral("FIX-03") },
        { QStringLiteral("embedded.showonly"), QStringLiteral("FILE-03") },
        { QStringLiteral("embedded.differ"), QStringLiteral("FILE-03") },
        { QStringLiteral("embedded.problems"), QStringLiteral("FILE-03") },
        { QStringLiteral("universes.nooutput"), QStringLiteral("PAT-06") },
        { QStringLiteral("effects.failed"), QStringLiteral("FX-07") },
        { QStringLiteral("show.load"), QStringLiteral("FILE-02") },
    };
    return ids.value(statusKey);
}

QHash<quint32, QString> ShowReview::designEffectProblems(Doc *doc)
{
    return ShowReviewChecks::designEffectProblems(doc);
}

/****************************************************************************
 * ShowReview
 ****************************************************************************/

ShowReview::ShowReview(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

static bool wanted(const QStringList &patterns, const QString &id)
{
    if (patterns.isEmpty())
        return true;
    for (const QString &p : patterns)
    {
        if (p.endsWith(QLatin1Char('*')) ? id.startsWith(p.left(p.length() - 1), Qt::CaseInsensitive)
                                          : id.compare(p, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

ShowReview::Result ShowReview::run()
{
    return run(Options());
}

ShowReview::Result ShowReview::run(const Options &options)
{
    Result r;
    QElapsedTimer all;
    all.start();
    ReviewContext ctx(m_doc, options);
    for (const Check &c : catalog())
    {
        if (!wanted(options.checks, c.id))
            continue;
        QElapsedTimer t;
        t.start();
        QList<ReviewFinding> found;
        c.run(ctx, found);
        r.checkMs.insert(c.id, t.elapsed());
        for (ReviewFinding &f : found)
        {
            std::sort(f.targets.begin(), f.targets.end());
            applyAcceptance(f);
            r.findings << f;
        }
    }
    std::stable_sort(r.findings.begin(), r.findings.end(), [](const ReviewFinding &a, const ReviewFinding &b) {
        if (a.severity != b.severity)
            return a.severity > b.severity;
        return a.check < b.check;
    });
    for (const ReviewFinding &f : r.findings)
    {
        if (f.state == ReviewFinding::Accepted)
            r.accepted[f.severity]++;
        else
            r.open[f.severity]++;
        if (f.state == ReviewFinding::Changed)
            r.changed++;
    }
    r.ms = all.elapsed();
    r.when = QDateTime::currentDateTime();
    r.ran = true;
    if (!options.record)
        return r;
    m_last = r;
    emit reviewed();
    return r;
}

void ShowReview::applyAcceptance(ReviewFinding &finding) const
{
    for (const TurnedOff &off : m_off)
    {
        if (off.check == finding.check)
        {
            finding.state = ReviewFinding::Accepted;
            finding.checkOff = true;
            finding.acceptedBy = off.by;
            finding.acceptedWhen = off.when;
            finding.acceptedNote = off.note;
            return;
        }
    }
    const QString targets = QStringList(finding.targets).join(QLatin1Char(','));
    for (const Acceptance &a : m_accepted)
    {
        if (a.check != finding.check || a.targets != targets)
            continue;
        finding.state = (a.hash == finding.hash()) ? ReviewFinding::Accepted : ReviewFinding::Changed;
        finding.acceptedBy = a.by;
        finding.acceptedWhen = a.when;
        finding.acceptedNote = a.note;
        return;
    }
}

void ShowReview::changed()
{
    if (m_doc != nullptr)
        m_doc->setModified();
    // Keep the last result's states in step without a full run.
    for (ReviewFinding &f : m_last.findings)
    {
        if (f.intended)
            continue;           // a mark in the show, not an acceptance
        f.state = ReviewFinding::Open;
        f.checkOff = false;
        f.acceptedBy.clear();
        f.acceptedWhen.clear();
        f.acceptedNote.clear();
        applyAcceptance(f);
    }
    for (int i = 0; i < 3; ++i)
        m_last.open[i] = m_last.accepted[i] = 0;
    m_last.changed = 0;
    for (const ReviewFinding &f : m_last.findings)
    {
        if (f.state == ReviewFinding::Accepted)
            m_last.accepted[f.severity]++;
        else
            m_last.open[f.severity]++;
        if (f.state == ReviewFinding::Changed)
            m_last.changed++;
    }
    emit acceptancesChanged();
}

bool ShowReview::accept(const ReviewFinding &finding, const QString &note, const QString &by)
{
    if (note.trimmed().isEmpty() || finding.check.isEmpty())
        return false;
    QStringList t = finding.targets;
    std::sort(t.begin(), t.end());
    Acceptance a;
    a.check = finding.check;
    a.targets = t.join(QLatin1Char(','));
    a.hash = finding.hash();
    a.by = by.trimmed().isEmpty() ? defaultName() : by.trimmed();
    a.when = QDateTime::currentDateTime().toString(Qt::ISODate);
    a.note = note.trimmed();
    for (int i = 0; i < m_accepted.count(); ++i)
    {
        if (m_accepted.at(i).check == a.check && m_accepted.at(i).targets == a.targets)
        {
            a.unknown = m_accepted.at(i).unknown;
            m_accepted[i] = a;
            changed();
            return true;
        }
    }
    m_accepted << a;
    changed();
    return true;
}

bool ShowReview::reopen(const ReviewFinding &finding)
{
    QStringList t = finding.targets;
    std::sort(t.begin(), t.end());
    const QString targets = t.join(QLatin1Char(','));
    for (int i = 0; i < m_accepted.count(); ++i)
    {
        if (m_accepted.at(i).check == finding.check && m_accepted.at(i).targets == targets)
        {
            m_accepted.removeAt(i);
            changed();
            return true;
        }
    }
    return false;
}

bool ShowReview::turnOff(const QString &checkId, const QString &note, const QString &by)
{
    if (note.trimmed().isEmpty() || checkId.isEmpty())
        return false;
    TurnedOff off;
    off.check = checkId;
    off.by = by.trimmed().isEmpty() ? defaultName() : by.trimmed();
    off.when = QDateTime::currentDateTime().toString(Qt::ISODate);
    off.note = note.trimmed();
    for (int i = 0; i < m_off.count(); ++i)
    {
        if (m_off.at(i).check == checkId)
        {
            m_off[i] = off;
            changed();
            return true;
        }
    }
    m_off << off;
    changed();
    return true;
}

bool ShowReview::turnOn(const QString &checkId)
{
    for (int i = 0; i < m_off.count(); ++i)
    {
        if (m_off.at(i).check == checkId)
        {
            m_off.removeAt(i);
            changed();
            return true;
        }
    }
    return false;
}

bool ShowReview::isOff(const QString &checkId) const
{
    for (const TurnedOff &off : m_off)
        if (off.check == checkId)
            return true;
    return false;
}

QString ShowReview::defaultName()
{
    const QString set = QSettings().value(kNameKey).toString().trimmed();
    if (!set.isEmpty())
        return set;
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = qEnvironmentVariable("USERNAME");
    return user;
}

void ShowReview::setDefaultName(const QString &name)
{
    QSettings().setValue(kNameKey, name.trimmed());
}

void ShowReview::reset()
{
    m_accepted.clear();
    m_off.clear();
    m_unknown.clear();
    m_last = Result();
    emit reviewed();
}

void ShowReview::setVcSource(const std::function<QByteArray()> &source)
{
    m_vcSource = source;
    m_hasVcFixed = false;
}

void ShowReview::setVcXml(const QByteArray &xml)
{
    m_vcSource = [xml]() { return xml; };
    m_hasVcFixed = false;
}

void ShowReview::setVcFile(const QString &path)
{
    m_vcFixed = VcIndex::fromFile(path);
    m_hasVcFixed = true;
}

VcIndex ShowReview::vcIndex() const
{
    if (m_hasVcFixed)
        return m_vcFixed;
    if (!m_vcSource)
        return VcIndex();
    return VcIndex::fromXml(m_vcSource());
}

bool ShowReview::targetExists(const Doc *doc, const QString &key)
{
    if (doc == nullptr)
        return true;
    for (const QString &part : key.split(QLatin1Char('/')))
    {
        const int colon = part.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString kind = part.left(colon);
        bool ok = false;
        const quint32 id = part.mid(colon + 1).toUInt(&ok);
        if (!ok)
            continue;
        if (kind == QLatin1String("fx") && doc->fixture(id) == nullptr)
            return false;
        if (kind == QLatin1String("fn") && doc->function(id) == nullptr)
            return false;
        if (kind == QLatin1String("pal") && doc->palette(id) == nullptr)
            return false;
        if (kind == QLatin1String("grp") && doc->fixtureGroup(id) == nullptr)
            return false;
        if (kind == QLatin1String("list") && doc->showTree()->cueList(id) == nullptr)
            return false;
        if (kind == QLatin1String("cue") && doc->showTree()->listOfCue(id) == nullptr)
            return false;
        if (kind == QLatin1String("scenery"))
        {
            MonitorProperties *mp = const_cast<Doc *>(doc)->monitorProperties();
            if (mp != nullptr && mp->scenery(id) == nullptr)
                return false;
        }
        if (kind == QLatin1String("vc") && doc->showReview() != nullptr)
        {
            const VcIndex vc = doc->showReview()->vcIndex();
            if (vc.available && vc.indexOf(id) < 0)
                return false;
        }
    }
    return true;
}

void ShowReview::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);
    QList<const Acceptance *> keep;
    for (const Acceptance &a : m_accepted)
    {
        bool there = true;
        for (const QString &t : a.targets.split(QLatin1Char(','), Qt::SkipEmptyParts))
            there = there && targetExists(m_doc, t);
        if (there)
            keep << &a;
    }
    // Nothing accepted: nothing written, so the file is what it was.
    if (keep.isEmpty() && m_off.isEmpty() && m_unknown.isEmpty())
        return;

    doc->writeStartElement(KXMLQLCShowReview);
    for (const QXmlStreamAttribute &u : m_unknown)
        doc->writeAttribute(u.qualifiedName().toString(), u.value().toString());
    for (const Acceptance *a : keep)
    {
        doc->writeStartElement(KXMLAccepted);
        doc->writeAttribute(KXMLCheck, a->check);
        doc->writeAttribute(KXMLTarget, a->targets);
        doc->writeAttribute(KXMLHash, a->hash);
        doc->writeAttribute(KXMLBy, a->by);
        doc->writeAttribute(KXMLWhen, a->when);
        for (const QXmlStreamAttribute &u : a->unknown)
            doc->writeAttribute(u.qualifiedName().toString(), u.value().toString());
        doc->writeCharacters(a->note);
        doc->writeEndElement();
    }
    for (const TurnedOff &off : m_off)
    {
        doc->writeStartElement(KXMLOff);
        doc->writeAttribute(KXMLCheck, off.check);
        doc->writeAttribute(KXMLBy, off.by);
        doc->writeAttribute(KXMLWhen, off.when);
        doc->writeCharacters(off.note);
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

void ShowReview::loadXML(QXmlStreamReader &root)
{
    m_accepted.clear();
    m_off.clear();
    m_unknown = root.attributes();
    while (root.readNextStartElement())
    {
        const QXmlStreamAttributes attrs = root.attributes();
        if (root.name() == KXMLAccepted)
        {
            Acceptance a;
            a.check = attrs.value(KXMLCheck).toString();
            a.targets = attrs.value(KXMLTarget).toString();
            a.hash = attrs.value(KXMLHash).toString();
            a.by = attrs.value(KXMLBy).toString();
            a.when = attrs.value(KXMLWhen).toString();
            for (const QXmlStreamAttribute &at : attrs)
            {
                const QString n = at.qualifiedName().toString();
                if (n != KXMLCheck && n != KXMLTarget && n != KXMLHash && n != KXMLBy && n != KXMLWhen)
                    a.unknown.append(at);
            }
            a.note = root.readElementText(QXmlStreamReader::SkipChildElements);
            if (!a.check.isEmpty())
                m_accepted << a;
        }
        else if (root.name() == KXMLOff)
        {
            TurnedOff off;
            off.check = attrs.value(KXMLCheck).toString();
            off.by = attrs.value(KXMLBy).toString();
            off.when = attrs.value(KXMLWhen).toString();
            off.note = root.readElementText(QXmlStreamReader::SkipChildElements);
            if (!off.check.isEmpty())
                m_off << off;
        }
        else
        {
            root.skipCurrentElement();
        }
    }
    m_last = Result();
}

/****************************************************************************
 * ReviewContext
 ****************************************************************************/

ReviewContext::ReviewContext(Doc *doc, const ShowReview::Options &options)
    : m_doc(doc)
    , m_options(options)
{
}

static void addTargetFixtures(Doc *doc, const ShowTree::Target &target, QSet<quint32> &out)
{
    for (quint32 fid : doc->showTree()->targetFixtures(target))
        out.insert(fid);
}

const QSet<quint32> &ReviewContext::usedFixtures()
{
    if (m_usedBuilt)
        return m_used;
    m_usedBuilt = true;
    auto addGroup = [this](quint32 gid) {
        if (FixtureGroup *g = m_doc->fixtureGroup(gid))
            for (quint32 fid : g->fixtureList())
                m_used.insert(fid);
    };
    for (Function *f : m_doc->functions())
    {
        if (Scene *s = qobject_cast<Scene *>(f))
        {
            for (quint32 fid : s->fixtures())
                m_used.insert(fid);
            for (const SceneValue &sv : s->values())
                m_used.insert(sv.fxi);
            for (quint32 gid : s->fixtureGroups())
                addGroup(gid);
        }
        else if (EFX *e = qobject_cast<EFX *>(f))
        {
            for (EFXFixture *ef : e->fixtures())
                m_used.insert(ef->head().fxi);
        }
        else if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
        {
            addGroup(m->fixtureGroup());
        }
    }
    for (const ShowTree::Production &p : m_doc->showTree()->productions())
        for (const ShowTree::CueList &l : p.lists)
            for (const ShowTree::Cue &c : l.cues)
                for (const ShowTree::Layer &layer : c.layers)
                    addTargetFixtures(m_doc, layer.target, m_used);
    for (const ShowTree::ReusableLayer &r : m_doc->showTree()->reusableLayers())
        addTargetFixtures(m_doc, r.target, m_used);
    return m_used;
}

const VcIndex &ReviewContext::vc()
{
    if (!m_vcBuilt)
    {
        m_vcBuilt = true;
        m_vc = m_doc->showReview() != nullptr ? m_doc->showReview()->vcIndex() : VcIndex();
    }
    return m_vc;
}

bool ReviewContext::universeHasOutput(quint32 u) const
{
    if (!m_options.outputsLoaded)
        return m_options.universesWithOutput.contains(u);
    InputOutputMap *io = m_doc->inputOutputMap();
    return u < io->universesCount() && io->outputPatchesCount(u) > 0;
}

bool ReviewContext::universeHasInput(quint32 u) const
{
    if (!m_options.outputsLoaded)
        return m_options.universesWithInput.contains(u);
    InputOutputMap *io = m_doc->inputOutputMap();
    return u < io->universesCount() && (io->inputPatch(u) != nullptr || !io->inputSlot(u).isEmpty());
}

bool ReviewContext::anyOutput() const
{
    if (!m_options.outputsLoaded)
        return !m_options.universesWithOutput.isEmpty();
    InputOutputMap *io = m_doc->inputOutputMap();
    for (quint32 u = 0; u < io->universesCount(); ++u)
        if (io->outputPatchesCount(u) > 0)
            return true;
    return false;
}

const QList<ShowTree::ResolvedCue> &ReviewContext::resolved(const ShowTree::CueList &list)
{
    auto it = m_resolved.find(list.id);
    if (it == m_resolved.end())
        it = m_resolved.insert(list.id, ShowTree::resolve(list, m_doc->showTree()->lineAimTest(), nullptr,
                                                                   m_doc->showTree()->formationRows()));
    return it.value();
}

QList<quint32> ReviewContext::targetFixtures(const ShowTree::Target &target) const
{
    return m_doc->showTree()->targetFixtures(target);
}

QString ReviewContext::fixtureName(quint32 id) const
{
    Fixture *fx = m_doc->fixture(id);
    return fx != nullptr ? fx->name() : QCoreApplication::translate("ShowReview", "fixture %1").arg(id);
}

QString ReviewContext::functionName(quint32 id) const
{
    Function *f = m_doc->function(id);
    return f != nullptr ? f->name() : QCoreApplication::translate("ShowReview", "function %1").arg(id);
}

QString ReviewContext::paletteName(quint32 id) const
{
    QLCPalette *p = m_doc->palette(id);
    return p != nullptr ? p->name() : QCoreApplication::translate("ShowReview", "palette %1").arg(id);
}

QString ReviewContext::cueText(const ShowTree::CueList &list, int index) const
{
    return QCoreApplication::translate("ShowReview", "cue %1 (%2)")
            .arg(m_doc->showTree()->cueLabel(list, index), list.name);
}

ReviewFinding ReviewContext::make(const QString &check, ReviewFinding::Severity severity,
                                  const QString &message, QStringList targets,
                                  const QByteArray &fingerprint, const QString &jump,
                                  const QVariantList &jumpData)
{
    ReviewFinding f;
    f.check = check;
    f.severity = severity;
    f.message = message;
    targets.removeDuplicates();
    std::sort(targets.begin(), targets.end());
    f.targets = targets;
    f.fingerprint = fingerprint;
    f.jump = jump;
    f.jumpData = jumpData;
    return f;
}
