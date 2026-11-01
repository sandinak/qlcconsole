/* Show Review's checks over the Virtual Console (docs/design/SHOW_REVIEW.md
   section 3.4, and CUE-13): hits nobody can fire, buttons with no function,
   the same key or input on two widgets, Cue Runners on no list, and cue
   lists nothing can play. They read the VcIndex -- what the Virtual Console
   holds, with or without its UI -- and the Show Tree. A Doc with no Virtual
   Console source has no index, and these stay quiet. */

#include <QCoreApplication>
#include <QKeySequence>
#include <QMap>
#include <QSet>
#include <algorithm>

#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "controlmap.h"
#include "fixture.h"
#include "doc.h"

using namespace ShowTree;

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowReview", text);
}

static QString trn(const char *text, int n)
{
    return QCoreApplication::translate("ShowReview", text, nullptr, n);
}

static const quint32 kInvalid = 0xFFFFFFFF;

static QString vcKey(const VcWidget &w)
{
    return QStringLiteral("vc:%1").arg(w.id);
}

/** "Button 'Haze on' (in 'Busk')" */
static QString widgetText(const VcIndex &vc, int index)
{
    const VcWidget &w = vc.widgets.at(index);
    QString kind;
    if (w.type == QLatin1String("Button"))
        kind = tr("Button");
    else if (w.type == QLatin1String("Slider"))
        kind = tr("Slider");
    else if (w.type == QLatin1String("CueList"))
        kind = tr("Cue List");
    else if (w.type == QLatin1String("CueRunner"))
        kind = tr("Cue Runner");
    else if (w.type == QLatin1String("Matrix"))
        kind = tr("Matrix");
    else if (w.type == QLatin1String("Properties"))
        return w.label();
    else
        kind = tr("Widget");
    return QStringLiteral("%1 '%2' (%3)").arg(kind, w.label(), vc.place(index));
}

static QString inputText(quint32 universe, quint32 channel)
{
    return tr("universe %1, channel %2").arg(universe + 1).arg(channel + 1);
}

/** The first few names of @a names, then how many more. */
static QString nameList(const QStringList &names, int max = 3)
{
    QStringList head = names.mid(0, max);
    QString s = head.join(QStringLiteral(", "));
    if (names.count() > max)
        s = trn("%1 and %n more", names.count() - max).arg(s);
    return s;
}

/****************************************************************************
 * VC-03 Hits nobody can fire
 ****************************************************************************/

static void vc03(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const VcIndex &vc = ctx.vc();
    if (!vc.available)
        return;
    Doc *doc = ctx.doc();
    ShowTreeController *st = doc->showTree();
    const QSet<quint32> hits = vc.hitsFired();
    const QSet<quint32> functions = vc.functionsUsed();
    QSet<quint32> mapped;       // hits and functions the control map fires
    if (doc->controlMap() != nullptr)
        for (const ControlMap::Binding &b : doc->controlMap()->bindings())
            if (b.target.type == ControlMap::HitTarget || b.target.type == ControlMap::FunctionTarget)
                mapped.insert(b.target.id);
    for (const Production &p : st->productions())
    {
        const CueList *looks = p.looks();
        if (looks == nullptr)
            continue;
        for (const Cue &hit : looks->cues)
        {
            const quint32 fn = st->lookFunction(hit.id);
            if (hits.contains(hit.id) || mapped.contains(hit.id))
                continue;
            if (fn != kInvalid && (functions.contains(fn) || mapped.contains(fn)
                                   || !doc->getUsage(fn).isEmpty()))
                continue;
            const QString name = hit.name.isEmpty() ? st->cueLabel(*looks, looks->indexOf(hit.id)) : hit.name;
            out << ReviewContext::make(QStringLiteral("VC-03"), ReviewFinding::Warning,
                tr("Hit '%1' isn't on any button, key or input: nothing can fire it. Put it on a "
                   "button, or make a VC page from the hits.").arg(name),
                { QStringLiteral("list:%1/cue:%2").arg(looks->id).arg(hit.id) },
                QByteArray::number(fn), QStringLiteral("cue"), QVariantList() << hit.id);
        }
    }
}

/****************************************************************************
 * VC-04 Buttons with no function
 ****************************************************************************/

static void vc04(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const VcIndex &vc = ctx.vc();
    if (!vc.available)
        return;
    Doc *doc = ctx.doc();
    ShowTreeController *st = doc->showTree();
    auto add = [&](int i, const QString &message, const QByteArray &fp) {
        const VcWidget &w = vc.widgets.at(i);
        out << ReviewContext::make(QStringLiteral("VC-04"), ReviewFinding::Warning, message,
                                   { vcKey(w) }, fp, QStringLiteral("vc"), QVariantList() << w.id);
    };
    for (int i = 0; i < vc.widgets.count(); ++i)
    {
        const VcWidget &w = vc.widgets.at(i);
        if (w.id == kInvalid)
            continue;
        const bool button = w.type == QLatin1String("Button");
        const bool plays = button && (w.action.isEmpty() || w.action == QLatin1String("Toggle")
                                      || w.action == QLatin1String("Flash"));
        if (plays || w.type == QLatin1String("Matrix")
            || (w.type == QLatin1String("Slider") && w.mode == QLatin1String("Playback")))
        {
            bool hit = false;
            for (quint32 h : w.hits)
                hit = hit || st->isLook(h);
            if (hit || (w.function != kInvalid && doc->function(w.function) != nullptr))
                continue;
            const QByteArray fp = QByteArray::number(w.function);
            add(i, w.function != kInvalid
                    ? tr("%1 does nothing: its function was deleted.").arg(widgetText(vc, i))
                    : tr("%1 does nothing: it has no function.").arg(widgetText(vc, i)), fp);
        }
        else if (w.type == QLatin1String("Slider") && w.mode == QLatin1String("Level"))
        {
            QList<quint32> gone;
            for (const auto &ch : w.levelChannels)
                if (doc->fixture(ch.first) == nullptr && !gone.contains(ch.first))
                    gone << ch.first;
            if (gone.isEmpty())
                continue;
            std::sort(gone.begin(), gone.end());
            QByteArray fp;
            for (quint32 f : gone)
                fp += QByteArray::number(f) + ',';
            add(i, trn("%1 sets channels of %n fixture(s) that were deleted.", gone.count())
                       .arg(widgetText(vc, i)), fp);
        }
        else if (w.type == QLatin1String("CueList"))
        {
            if (w.chaser != kInvalid && doc->function(w.chaser) != nullptr)
                continue;
            add(i, w.chaser != kInvalid
                    ? tr("%1 shows a cue list that was deleted.").arg(widgetText(vc, i))
                    : tr("%1 has no cue list.").arg(widgetText(vc, i)), QByteArray::number(w.chaser));
        }
    }
}

/****************************************************************************
 * VC-05 Same key twice, VC-06 Same input twice
 ****************************************************************************/

/** A key as one spelling, so "ctrl+b" and "Ctrl+B" are one key. */
static QString normalKey(const QString &key)
{
    return QKeySequence(key, QKeySequence::PortableText).toString(QKeySequence::PortableText);
}

/** The keys Operate itself answers to, and what they do: the GO keys
 *  (App::m_cueGoAction and friends) and Space. */
static QMap<QString, QString> appShortcuts()
{
    QMap<QString, QString> m;
    auto put = [&m](const QKeySequence &k, const QString &what) {
        m.insert(k.toString(QKeySequence::PortableText), what);
    };
    put(QKeySequence(Qt::CTRL | Qt::Key_Return), tr("GO"));
    put(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return), tr("Back"));
    put(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Return), tr("Sub GO"));
    put(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Return), tr("Sub Back"));
    put(QKeySequence(Qt::Key_Space), tr("GO in a Cue Runner"));
    return m;
}

struct Holder
{
    int widget;
    QString role;
};

/** Of @a holders, those that can be live with another widget among them. */
static QList<Holder> clashing(const VcIndex &vc, const QList<Holder> &holders,
                              const std::function<bool(const Holder &, const Holder &)> &differ)
{
    QList<Holder> out;
    for (int a = 0; a < holders.count(); ++a)
    {
        bool clash = false;
        for (int b = 0; b < holders.count() && !clash; ++b)
            clash = a != b && holders.at(a).widget != holders.at(b).widget
                    && vc.canBeLiveTogether(holders.at(a).widget, holders.at(b).widget)
                    && differ(holders.at(a), holders.at(b));
        if (clash)
            out << holders.at(a);
    }
    return out;
}

static QStringList holderNames(const VcIndex &vc, const QList<Holder> &holders)
{
    QStringList names;
    for (const Holder &h : holders)
    {
        QString n = widgetText(vc, h.widget);
        if (!h.role.isEmpty())
            n = tr("%1, %2").arg(n, h.role);
        if (!names.contains(n))
            names << n;
    }
    return names;
}

static void vc05(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const VcIndex &vc = ctx.vc();
    if (!vc.available)
        return;
    QMap<QString, QList<Holder>> byKey;
    for (int i = 0; i < vc.widgets.count(); ++i)
        for (const VcBinding &b : vc.widgets.at(i).bindings)
            if (!b.key.isEmpty())
                byKey[normalKey(b.key)] << Holder { i, b.role };
    for (auto it = byKey.constBegin(); it != byKey.constEnd(); ++it)
    {
        const QString shown = QKeySequence(it.key(), QKeySequence::PortableText).toString(QKeySequence::NativeText);
        const QList<Holder> clash = clashing(vc, it.value(), [](const Holder &, const Holder &) { return true; });
        if (clash.count() >= 2)
        {
            QStringList targets;
            QByteArray fp = it.key().toUtf8();
            for (const Holder &h : clash)
            {
                targets << vcKey(vc.widgets.at(h.widget));
                fp += '|' + QByteArray::number(vc.widgets.at(h.widget).id) + h.role.toUtf8();
            }
            targets.removeDuplicates();
            const QStringList names = holderNames(vc, clash);
            out << ReviewContext::make(QStringLiteral("VC-05"), ReviewFinding::Warning,
                tr("Key %1 is on %2: one press does both.").arg(shown, names.count() == 2
                       ? tr("both %1 and %2").arg(names.at(0), names.at(1)) : nameList(names)),
                targets, fp, QStringLiteral("vc"),
                QVariantList() << vc.widgets.at(clash.first().widget).id);
        }
    }
    // A key Operate answers to itself.
    const QMap<QString, QString> app = appShortcuts();
    for (auto it = byKey.constBegin(); it != byKey.constEnd(); ++it)
    {
        auto what = app.constFind(it.key());
        if (what == app.constEnd())
            continue;
        for (const Holder &h : it.value())
        {
            const VcWidget &w = vc.widgets.at(h.widget);
            if (w.id == kInvalid)
                continue;
            out << ReviewContext::make(QStringLiteral("VC-05"), ReviewFinding::Warning,
                tr("Key %1 on %2 is also %3 in Operate: one press does both.")
                    .arg(QKeySequence(it.key(), QKeySequence::PortableText).toString(QKeySequence::NativeText),
                         holderNames(vc, { h }).first(), what.value()),
                { vcKey(w), QStringLiteral("shortcut:") + it.key() }, it.key().toUtf8() + h.role.toUtf8(),
                QStringLiteral("vc"), QVariantList() << w.id);
        }
    }
}

/** What a binding does, so two on one input that do the same aren't a clash. */
static QString jobOf(const VcWidget &w, const QString &role)
{
    return QStringLiteral("%1/%2/%3/%4/%5/%6").arg(w.type, role).arg(w.function).arg(w.chaser)
        .arg(w.follow).arg(w.group);
}

static void vc06(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const VcIndex &vc = ctx.vc();
    if (!vc.available)
        return;
    QMap<QPair<quint32, quint32>, QList<Holder>> byInput;
    for (int i = 0; i < vc.widgets.count(); ++i)
        for (const VcBinding &b : vc.widgets.at(i).bindings)
            if (b.hasInput)
                byInput[qMakePair(b.universe, b.channel)] << Holder { i, b.role };
    for (auto it = byInput.constBegin(); it != byInput.constEnd(); ++it)
    {
        const QList<Holder> clash = clashing(vc, it.value(), [&vc](const Holder &a, const Holder &b) {
            return jobOf(vc.widgets.at(a.widget), a.role) != jobOf(vc.widgets.at(b.widget), b.role);
        });
        if (clash.count() < 2)
            continue;
        QStringList targets;
        QByteArray fp = QByteArray::number(it.key().first) + '/' + QByteArray::number(it.key().second);
        for (const Holder &h : clash)
        {
            targets << vcKey(vc.widgets.at(h.widget));
            fp += '|' + QByteArray::number(vc.widgets.at(h.widget).id) + h.role.toUtf8();
        }
        targets.removeDuplicates();
        const QStringList names = holderNames(vc, clash);
        out << ReviewContext::make(QStringLiteral("VC-06"), ReviewFinding::Warning,
            tr("Input %1 fires %2: one control, two jobs.").arg(inputText(it.key().first, it.key().second),
                names.count() == 2 ? tr("both %1 and %2").arg(names.at(0), names.at(1)) : nameList(names)),
            targets, fp, QStringLiteral("vc"), QVariantList() << vc.widgets.at(clash.first().widget).id);
    }
}

/****************************************************************************
 * VC-08 Cue Runner on no list
 ****************************************************************************/

static void vc08(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const VcIndex &vc = ctx.vc();
    if (!vc.available)
        return;
    ShowTreeController *st = ctx.doc()->showTree();
    bool anyList = false;
    for (const Production &p : st->productions())
        anyList = anyList || !st->playableLists(p.id).isEmpty();
    for (int i = 0; i < vc.widgets.count(); ++i)
    {
        const VcWidget &w = vc.widgets.at(i);
        if (w.type != QLatin1String("CueRunner") || w.id == kInvalid)
            continue;
        QString message;
        if (w.follows)
        {
            const CueList *l = st->cueList(w.follow);
            if (l == nullptr)
                message = tr("%1 follows a cue list that was deleted.");
            else if (l->looks || l->isSubCues())
                message = tr("%1 follows a list that isn't one GO can play.");
        }
        else if (!anyList)
            message = tr("%1 follows the current production, and the show has no cue list.");
        if (message.isEmpty())
            continue;
        out << ReviewContext::make(QStringLiteral("VC-08"), ReviewFinding::Warning,
            message.arg(widgetText(vc, i)), { vcKey(w) },
            QByteArray::number(w.follow) + (w.follows ? "f" : "c"), QStringLiteral("vc"),
            QVariantList() << w.id);
    }
}

/****************************************************************************
 * CUE-13 A list nothing can play
 ****************************************************************************/

static void cue13(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const VcIndex &vc = ctx.vc();
    if (!vc.available)
        return;
    Doc *doc = ctx.doc();
    ShowTreeController *st = doc->showTree();
    const QSet<quint32> functions = vc.functionsUsed();
    QSet<quint32> followed;
    for (const VcWidget &w : vc.widgets)
        if (w.follows)
            followed.insert(w.follow);
    QSet<quint32> listsMapped, functionsMapped;
    if (doc->controlMap() != nullptr)
        for (const ControlMap::Binding &b : doc->controlMap()->bindings())
        {
            if (b.target.type == ControlMap::CueListTarget)
                listsMapped.insert(b.target.id);
            else if (b.target.type == ControlMap::CueTarget)
            {
                if (const CueList *l = st->listOfCue(b.target.id))
                    listsMapped.insert(l->id);
            }
            else if (b.target.type == ControlMap::FunctionTarget)
                functionsMapped.insert(b.target.id);
        }
    for (const Production &p : st->productions())
    {
        const quint32 main = st->mainList(p.id);
        for (const CueList &list : p.lists)
        {
            if (list.looks || list.isSubCues() || list.id == main || list.followTimecode
                || followed.contains(list.id) || listsMapped.contains(list.id))
                continue;
            if (list.chaser != kInvalid && (functions.contains(list.chaser)
                                            || functionsMapped.contains(list.chaser)
                                            || !doc->getUsage(list.chaser).isEmpty()))
                continue;
            out << ReviewContext::make(QStringLiteral("CUE-13"), ReviewFinding::Warning,
                tr("List '%1' can't be played: it isn't the main list and nothing on the Virtual "
                   "Console runs it.").arg(list.name),
                { QStringLiteral("list:%1").arg(list.id) }, QByteArray::number(list.chaser),
                QStringLiteral("list"), QVariantList() << list.id);
        }
    }
}

/****************************************************************************
 * The list
 ****************************************************************************/

QList<ShowReview::Check> ShowReviewChecks::vcChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("CUE-13"), tr("A list nothing can play"), S::Cues,
          tr("A cue list that isn't the main list, that no Cue Runner follows, no VC Cue List or "
             "button runs and no timecode or control route reaches can't be GOne in Operate."), cue13 },
        { QStringLiteral("VC-03"), tr("Hits nobody can fire"), S::Hits,
          tr("A hit is fired from a VC button, a key or an input. One on none of them is "
             "a look nobody can call."), vc03 },
        { QStringLiteral("VC-04"), tr("Buttons with no function"), S::Hits,
          tr("A button, matrix or playback slider with no function (or a deleted one), a level "
             "slider on deleted fixtures, a Cue List on a deleted list: the widget is there and "
             "does nothing."), vc04 },
        { QStringLiteral("VC-05"), tr("Same key twice"), S::Hits,
          tr("Two widgets on one key (on the same page, or always live), or a key Operate "
             "already answers to, like the GO keys: one press does both."), vc05 },
        { QStringLiteral("VC-06"), tr("Same input twice"), S::Hits,
          tr("Two widgets with different jobs on one MIDI or input control: one fader or pad "
             "moves both."), vc06 },
        { QStringLiteral("VC-08"), tr("Cue Runner on no list"), S::Hits,
          tr("A Cue Runner that follows a cue list that was deleted, or follows the current "
             "production in a show with no cue list, shows nothing and GOes nothing."), vc08 },
    };
}
