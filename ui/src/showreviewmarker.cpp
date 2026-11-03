/*
  Q Light Controller Plus
  showreviewmarker.cpp

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

#include <QAction>
#include <QMenu>
#include <algorithm>

#include "showreviewmarker.h"
#include "showreviewpanel.h"
#include "showtreecontroller.h"
#include "contextmenus.h"
#include "doc.h"

namespace
{
QString tr(const char *text, const char *disambiguation = nullptr, int n = -1)
{
    return QCoreApplication::translate("ShowReviewMarker", text, disambiguation, n);
}

bool isBackwash(const ReviewFinding &f)
{
    return f.check == QLatin1String("PWR-12");
}

QString severityMark(ReviewFinding::Severity s)
{
    return s == ReviewFinding::Blocker ? QStringLiteral("⛔") : s == ReviewFinding::Warning
                                                               ? QStringLiteral("⚠") : QStringLiteral("·");
}

/** "cue 1.0.1 Curtain" */
QString cueName(Doc *doc, quint32 cueId)
{
    const ShowTree::Cue *cue = doc->showTree()->cue(cueId);
    if (cue == nullptr)
        return QString();
    const QString n = cue->number.isEmpty() ? cue->name : cue->number + QLatin1Char(' ') + cue->name;
    return n.trimmed();
}

/** What a submenu calls one finding: its check and the start of what it says. */
QString shortTitle(const ReviewFinding &f)
{
    QString text = f.message.section(QLatin1Char('.'), 0, 0).simplified();
    if (text.length() > 60)
        text = text.left(59) + QStringLiteral("…");
    text.remove(QChar(0x201C)).remove(QChar(0x201D));
    return QStringLiteral("%1 %2 “%3”").arg(severityMark(f.severity), f.check, text);
}
}

QList<ReviewFinding> ShowReviewMarker::openFindings(Doc *doc)
{
    QList<ReviewFinding> out;
    if (doc == nullptr || doc->showReview() == nullptr)
        return out;
    for (const ReviewFinding &f : doc->showReview()->lastResult().findings)
        if (f.isOpen() && f.severity >= ReviewFinding::Warning)
            out << f;
    std::stable_sort(out.begin(), out.end(), [](const ReviewFinding &a, const ReviewFinding &b) {
        return a.severity > b.severity;
    });
    return out;
}

QList<ReviewFinding> ShowReviewMarker::cueFindings(Doc *doc, quint32 cueId)
{
    QList<ReviewFinding> out;
    if (doc == nullptr || doc->showReview() == nullptr)
        return out;
    for (const ReviewFinding &f : doc->showReview()->lastResult().findings)
        if (f.isOpen() && ShowReview::findingCues(f).contains(cueId))
            out << f;
    return out;
}

QString ShowReviewMarker::tooltip(Doc *doc, const QList<ReviewFinding> &findings, int max)
{
    if (doc == nullptr || findings.isEmpty())
        return QString();
    QStringList parts;
    bool anyBackwash = false, anyOther = false;
    for (int i = 0; i < findings.count() && i < max; ++i)
    {
        const ReviewFinding &f = findings.at(i);
        anyBackwash = anyBackwash || isBackwash(f);
        anyOther = anyOther || !isBackwash(f);
        const ShowReview::Check *c = ShowReview::check(f.check);
        QString block = QStringLiteral("<b>%1 %2 %3</b><br>%4")
                        .arg(severityMark(f.severity), f.check,
                             c != nullptr ? c->title.toHtmlEscaped() : QString(),
                             f.message.toHtmlEscaped());
        const QList<quint32> cues = ShowReview::findingCues(f);
        if (cues.count() > 1)
        {
            QStringList names;
            for (int k = 0; k < cues.count() && k < 8; ++k)
                names << cueName(doc, cues.at(k)).toHtmlEscaped();
            if (cues.count() > 8)
                names << tr("%n more", "", cues.count() - 8);
            block += QStringLiteral("<br><i>%1</i>").arg(tr("Lands in every one of: %1")
                                                         .arg(names.join(QStringLiteral(", "))));
        }
        parts << block;
    }
    if (findings.count() > max)
        parts << tr("…and %n more", "", findings.count() - max);
    QString actions;
    if (anyBackwash && !anyOther)
        actions = tr("Right-click: I Meant It · Not Meant: Show Me the Layer · Open in Show Review");
    else if (anyBackwash)
        actions = tr("Right-click: I Meant It or Accept… · Show Me · Open in Show Review");
    else
        actions = tr("Right-click: Accept… · Show Me · Open in Show Review");
    parts << QStringLiteral("<span style=\"color:gray\">%1</span>").arg(actions.toHtmlEscaped());
    return QStringLiteral("<qt>") + parts.join(QStringLiteral("<br><br>")) + QStringLiteral("</qt>");
}

bool ShowReviewMarker::meant(Doc *doc, const ReviewFinding &finding, QWidget *parent, const QString &note)
{
    if (doc == nullptr || !finding.isOpen())
        return false;
    if (ShowReview::canMarkMeant(finding, true))
    {
        if (!ShowReview::setLightMeant(doc, finding, true))
            return false;
        doc->showReview()->run();
        return true;
    }
    QString n = note, by = ShowReview::defaultName();
    if (n.trimmed().isEmpty())
    {
        if (parent == nullptr || !ShowReviewPanel::askAcceptNote(parent, { finding }, &n, &by))
            return false;
    }
    return doc->showReview()->accept(finding, n, by);
}

bool ShowReviewMarker::notMeant(Doc *doc, const ReviewFinding &finding)
{
    if (doc == nullptr)
        return false;
    if (ShowReview::canMarkMeant(finding, false))
    {
        if (!ShowReview::setLightMeant(doc, finding, false))
            return false;
        doc->showReview()->run();
        return true;
    }
    if (finding.isOpen() || finding.checkOff)
        return false;
    return doc->showReview()->reopen(finding);
}

bool ShowReviewMarker::showMe(const ReviewFinding &finding, QWidget *parent)
{
    if (finding.jump.isEmpty())
        return false;
    return ShowReviewPanel::runJump(parent, finding.jump, finding.jumpData);
}

bool ShowReviewMarker::openInReview(const ReviewFinding &finding)
{
    ShowReviewPanel *panel = ShowReviewPanel::openShared();
    if (panel == nullptr)
        return false;
    panel->setFilter(finding.isOpen() ? ShowReviewPanel::OpenOnly : ShowReviewPanel::All);
    panel->setSearch(QString());
    return panel->selectFinding(finding.key());
}

void ShowReviewMarker::fillMenu(ContextMenu &menu, Doc *doc, const QList<ReviewFinding> &findings,
                                QWidget *parent, int max, bool offerOpen)
{
    if (doc == nullptr || findings.isEmpty())
        return;
    auto answers = [doc, parent](QMenu *into, ContextMenu *cm, const ReviewFinding &f) {
        auto add = [into, cm](const QString &text, const std::function<void()> &fn,
                             const QString &name, const QString &tip) {
            QAction *a = nullptr;
            if (cm != nullptr)
                a = cm->add(ContextMenu::Kind, text, fn, name);
            else
            {
                a = into->addAction(text);
                a->setObjectName(name);
                QObject::connect(a, &QAction::triggered, a, [fn]() { fn(); });
            }
            a->setToolTip(tip);
            a->setStatusTip(tip);
        };
        const bool backwash = isBackwash(f);
        if (f.isOpen())
        {
            if (ShowReview::canMarkMeant(f, true))
                add(tr("I Meant It"), [doc, f]() { meant(doc, f); }, QStringLiteral("reviewMeant"),
                    tr("This light on the cyc is on purpose: kept on the layer that lights it (one "
                       "undo step), so the warning goes away and stays away"));
            else
                add(tr("Accept…"), [doc, f, parent]() { meant(doc, f, parent); }, QStringLiteral("reviewAccept"),
                    tr("It's on purpose: say why. The note is saved with the show, and the finding "
                       "comes back if what it's about changes."));
        }
        else if (ShowReview::canMarkMeant(f, false) || (!f.checkOff && !f.isOpen()))
            add(backwash ? tr("Not Meant") : tr("Reopen"), [doc, f]() { notMeant(doc, f); },
                QStringLiteral("reviewNotMeant"),
                tr("Take it back: Show Review warns about it again"));
        if (!f.jump.isEmpty())
        {
            if (backwash && f.isOpen())
                add(tr("Not Meant: Show Me the Layer"), [f, parent]() { showMe(f, parent); },
                    QStringLiteral("reviewShowMe"),
                    tr("Opens the layer that lights it: aim it off, shutter or zoom it, or darken it "
                       "in this cue"));
            else
                add(tr("Show Me"), [f, parent]() { showMe(f, parent); }, QStringLiteral("reviewShowMe"),
                    tr("Where to fix it"));
        }
    };

    if (findings.count() == 1)
    {
        const ReviewFinding f = findings.first();
        if (offerOpen)
            menu.add(ContextMenu::Open, tr("Open in Show Review"), [f]() { openInReview(f); },
                     QStringLiteral("reviewOpen"));
        answers(nullptr, &menu, f);
        return;
    }
    const ReviewFinding first = findings.first();
    if (offerOpen)
        menu.add(ContextMenu::Open, tr("Open in Show Review"), [first]() { openInReview(first); },
                 QStringLiteral("reviewOpen"));
    for (int i = 0; i < findings.count() && i < max; ++i)
    {
        const ReviewFinding f = findings.at(i);
        QMenu *sub = menu.addMenu(ContextMenu::Kind, shortTitle(f), QStringLiteral("reviewFinding%1").arg(i));
        sub->setToolTipsVisible(true);
        sub->setToolTip(tooltip(doc, { f }));
        answers(sub, nullptr, f);
        if (!offerOpen)
            continue;
        QAction *open = sub->addAction(tr("Open in Show Review"));
        open->setObjectName(QStringLiteral("reviewOpen"));
        QObject::connect(open, &QAction::triggered, open, [f]() { openInReview(f); });
    }
}
