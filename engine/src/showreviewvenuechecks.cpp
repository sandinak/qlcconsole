/* Show Review's checks over venues and focus (docs/design/SHOW_REVIEW.md
   section 3.8): focus calls with no target, lights a focus call can't aim,
   swapped fixtures that need focus again, a venue file that is missing or
   unreadable, a practice venue left applied. They read the focus calls and
   the venue corrections the programmer keeps (with the venue sidecar
   loaded, as the app and the command line both do). */

#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QFile>
#include <QMap>
#include <algorithm>

#include "showreviewcontext.h"
#include "programmercontroller.h"
#include "venuecorrections.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "focussession.h"
#include "focuscalls.h"
#include "stagetarget.h"
#include "scene.h"
#include "fixture.h"
#include "doc.h"

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowReview", text);
}

static QString trn(const char *text, int n)
{
    return QCoreApplication::translate("ShowReview", text, nullptr, n);
}

static VenueCorrections *venues(ReviewContext &ctx)
{
    return ctx.doc()->programmer() != nullptr ? ctx.doc()->programmer()->venueCorrections() : nullptr;
}

static QString venueKey(const QString &venue)
{
    QString v = venue;
    v.replace(QLatin1Char(','), QLatin1Char(' ')).replace(QLatin1Char('/'), QLatin1Char(' '));
    return QStringLiteral("venue:") + v;
}

static QString names(ReviewContext &ctx, const QList<quint32> &ids, int max = 4)
{
    QStringList n;
    for (int i = 0; i < ids.count() && i < max; ++i)
        n << ctx.fixtureName(ids.at(i));
    QString s = n.join(QStringLiteral(", "));
    if (ids.count() > max)
        s = trn("%1 and %n more", ids.count() - max).arg(s);
    return s;
}

static QByteArray idBytes(QList<quint32> ids)
{
    std::sort(ids.begin(), ids.end());
    QByteArray b;
    for (quint32 id : ids)
        b += QByteArray::number(id) + ',';
    return b;
}

static QString callKey(int index)
{
    return QStringLiteral("focuscall:%1").arg(index);
}

/****************************************************************************
 * VEN-01 Focus calls with no target, VEN-02 Unaimable lights in a focus call
 ****************************************************************************/

static void ven01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    if (doc->programmer() == nullptr || doc->programmer()->focusCalls() == nullptr)
        return;
    const QList<FocusCall> calls = doc->programmer()->focusCalls()->calls();
    for (int i = 0; i < calls.count(); ++i)
    {
        const FocusCall &call = calls.at(i);
        QStringList problems;
        if (!call.target.isValid())
            problems << tr("has no target");
        else if (call.target.kind == FocusSession::Reference::Target
                 && doc->monitorProperties()->stageTarget(call.target.id) == nullptr)
            problems << tr("aims at a stage target that was deleted");
        else if (call.target.kind == FocusSession::Reference::SceneAim
                 && qobject_cast<Scene *>(doc->function(call.target.id)) == nullptr)
            problems << tr("aims at a scene that was deleted");
        if (call.groups.isEmpty() && call.fixtures.isEmpty())
            problems << tr("has no groups or lights");
        else if (FocusCalls::fixturesOf(doc, call).isEmpty())
            problems << tr("has only groups and lights that are gone");
        if (problems.isEmpty())
            continue;
        out << ReviewContext::make(QStringLiteral("VEN-01"), ReviewFinding::Warning,
            tr("Focus call '%1' %2: Venue focus has nothing to do for it.")
                .arg(FocusCalls::displayName(doc, call), problems.join(tr(" and "))),
            { callKey(i) }, problems.join(QLatin1Char('|')).toUtf8(), QStringLiteral("fixtures"));
    }
}

static void ven02(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    if (doc->programmer() == nullptr || doc->programmer()->focusCalls() == nullptr)
        return;
    MonitorProperties *mp = doc->monitorProperties();
    // A light off the plot is FIX-05's, when the show uses the plot at all.
    const bool plotUsed = mp != nullptr && !mp->fixtureItemsID().isEmpty();
    const QList<FocusCall> calls = doc->programmer()->focusCalls()->calls();
    for (int i = 0; i < calls.count(); ++i)
    {
        const QList<quint32> lights = FocusCalls::fixturesOf(doc, calls.at(i));
        const QList<quint32> ok = FocusSession::focusable(doc, lights);
        QList<quint32> left;
        for (quint32 fid : lights)
            if (!ok.contains(fid) && !(plotUsed && !mp->containsFixture(fid)))
                left << fid;
        if (left.isEmpty())
            continue;
        QStringList keys { callKey(i) };
        for (quint32 fid : left)
            keys << QStringLiteral("fx:%1").arg(fid);
        out << ReviewContext::make(QStringLiteral("VEN-02"), ReviewFinding::Warning,
            trn("Focus call '%1' has %n light(s) that can't be aimed (%2): they'll be left out.", left.count())
                .arg(FocusCalls::displayName(doc, calls.at(i)), names(ctx, left)),
            keys, idBytes(left), QStringLiteral("fixtures"), QVariantList() << left.first());
    }
}

/****************************************************************************
 * VEN-05 Swapped fixtures need focus
 ****************************************************************************/

static void ven05(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    VenueCorrections *vc = venues(ctx);
    if (vc == nullptr || vc->activeVenue().isEmpty())
        return;
    const QString name = vc->activeVenue();
    const VenueCorrections::Venue v = vc->venue(name);
    for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
    {
        const quint32 fid = it.key();
        if (!vc->isSwapped(name, fid))
            continue;
        const Fixture *fx = ctx.doc()->fixture(fid);
        QString now;
        if (fx != nullptr && fx->fixtureDef() != nullptr)
            now = QStringLiteral("%1 %2").arg(fx->fixtureDef()->manufacturer(), fx->fixtureDef()->model()).trimmed();
        const QString was = QStringLiteral("%1 %2").arg(v.makers.value(fid), v.models.value(fid)).trimmed();
        out << ReviewContext::make(QStringLiteral("VEN-05"), ReviewFinding::Warning,
            tr("%1 was a %2 when %3 was focused; it's a %4 now. Refocus it: the old correction is left off.")
                .arg(ctx.fixtureName(fid), was, name, now),
            { QStringLiteral("fx:%1").arg(fid), venueKey(name) },
            QByteArray::number(fid) + '/' + was.toUtf8() + '/' + now.toUtf8(),
            QStringLiteral("fixtures"), QVariantList() << fid);
    }
}

/****************************************************************************
 * VEN-09 Venue sidecar missing, VEN-10 Practice venue left on
 ****************************************************************************/

static void ven09(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    VenueCorrections *vc = venues(ctx);
    if (vc == nullptr || vc->filePath().isEmpty())
        return;     // not saved yet: nowhere for the file to be
    const QString path = vc->filePath();
    const QString file = QFileInfo(path).fileName();
    if (QFileInfo::exists(path))
    {
        QString why;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            why = f.errorString();
        else
        {
            QXmlStreamReader r(&f);
            while (!r.atEnd())
                r.readNext();
            if (r.hasError())
                why = r.errorString();
        }
        if (why.isEmpty())
            return;
        out << ReviewContext::make(QStringLiteral("VEN-09"), ReviewFinding::Warning,
            tr("The venue file '%1' beside the show can't be read (%2): the venues' corrections aren't applied.")
                .arg(file, why),
            { QStringLiteral("file:") + file }, "unreadable", QStringLiteral("fixtures"));
        return;
    }
    // Missing: matters when the show has venues in memory, or lays its
    // universes out for a house rig.
    QStringList uses;
    if (!vc->venueNames().isEmpty())
        uses << trn("%n venue(s)", vc->venueNames().count());
    InputOutputMap *io = ctx.doc()->inputOutputMap();
    int houseSlots = 0;
    for (int u = 0; io != nullptr && u < int(io->universesCount()); ++u)
        if (io->universeSlotOwner(u) == RigOwner::House)
            ++houseSlots;
    if (houseSlots > 0)
        uses << trn("%n house universe(s)", houseSlots);
    if (uses.isEmpty())
        return;
    const QString applied = vc->activeVenue();
    out << ReviewContext::make(QStringLiteral("VEN-09"), ReviewFinding::Warning,
        tr("The show uses %1, but its venue file '%2' isn't beside it.")
            .arg(applied.isEmpty() ? uses.join(tr(" and ")) : tr("venue '%1'").arg(applied), file),
        { QStringLiteral("file:") + file }, "missing:" + uses.join(QLatin1Char(',')).toUtf8(),
        QStringLiteral("fixtures"));
}

static void ven10(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    VenueCorrections *vc = venues(ctx);
    if (vc == nullptr)
        return;
    const QString active = vc->activeVenue();
    QStringList practices;
    bool real = false;
    for (const QString &name : vc->venueNames())
    {
        if (VenueCorrections::isPractice(vc->venue(name)))
            practices << name;
        else
            real = true;
    }
    if (!active.isEmpty() && vc->isPracticeActive())
    {
        out << ReviewContext::make(QStringLiteral("VEN-10"), ReviewFinding::Warning,
            tr("The practice venue '%1' is still applied: the lights are scrambled on purpose. "
               "End the practice before the show.").arg(active),
            { venueKey(active) }, "applied:" + active.toUtf8(), QStringLiteral("fixtures"));
        return;
    }
    if (!practices.isEmpty() && !real)
        out << ReviewContext::make(QStringLiteral("VEN-10"), ReviewFinding::Note,
            trn("The show has %n practice venue(s) and no real venue.", practices.count()),
            { venueKey(practices.first()) }, practices.join(QLatin1Char('|')).toUtf8(),
            QStringLiteral("fixtures"));
}

/****************************************************************************
 * The list
 ****************************************************************************/

QList<ShowReview::Check> ShowReviewChecks::venueChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("VEN-01"), tr("Focus calls with no target"), S::Venues,
          tr("A focus call needs a point to focus to and lights to focus. One with no target, a "
             "deleted target, or no lights left gives Venue focus nothing to do."), ven01 },
        { QStringLiteral("VEN-02"), tr("Lights a focus call can't aim"), S::Venues,
          tr("Venue focus aims moving heads and fixed lights it can place on the plot. Other "
             "lights in a call are left out when it runs; this says so before."), ven02 },
        { QStringLiteral("VEN-05"), tr("Swapped fixtures need focus"), S::Venues,
          tr("A venue remembers what each light was when it was focused. A light that is another "
             "fixture now (a lamp swapped for a mover, a new mode) would be aimed wrong by the old "
             "correction, so it's left out until it's focused again."), ven05 },
        { QStringLiteral("VEN-09"), tr("Venue file missing"), S::Venues,
          tr("A venue's corrections, house rig and patch live in a file beside the show. If it's "
             "missing or can't be read, the show opens without them."), ven09 },
        { QStringLiteral("VEN-10"), tr("Practice venue left on"), S::Venues,
          tr("A scramble-practice venue puts the lights off on purpose. Left applied, or left as "
             "the only venue, the real show would be focused wrong."), ven10 },
    };
}
