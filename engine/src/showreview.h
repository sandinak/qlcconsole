/*
  Q Light Controller Plus - qlcconsole
  showreview.h

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

#ifndef SHOWREVIEW_H
#define SHOWREVIEW_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QXmlStreamAttributes>
#include <functional>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class ReviewContext;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCShowReview QStringLiteral("ShowReview")

/**
 * One thing Show Review found (docs/design/SHOW_REVIEW.md section 5.1): which
 * check, how bad, what it says in crew words, what it is about (stable ids,
 * the acceptance key), what the check read to decide (the fingerprint an
 * acceptance is tied to) and where Fix goes.
 */
struct QLC_ENGINE_EXPORT ReviewFinding
{
    enum Severity { Note = 0, Warning = 1, Blocker = 2 };
    enum State { Open, Accepted, Changed };

    QString check;              //!< "PAT-01"
    Severity severity = Warning;
    QString message;            //!< plain crew language, translated
    QStringList targets;        //!< stable keys, sorted: "fx:7", "list:3/cue:118"
    QByteArray fingerprint;     //!< what the check read (hashed for storage)
    /** Where Fix goes: a kind the UI maps ("cue", "function", "fixtures",
     *  "palette", "plot", "power", "status:<footer key>") and its ids. */
    QString jump;
    QVariantList jumpData;

    State state = Open;
    QString acceptedBy;         //!< Accepted / Changed: who, when and why
    QString acceptedWhen;
    QString acceptedNote;
    bool checkOff = false;      //!< accepted because its check is off for the show

    /** The acceptance key: the check and its sorted targets. */
    QString key() const;
    /** The fingerprint as saved: a short hex hash. */
    QString hash() const;
    bool isOpen() const { return state != Accepted; }
};

/**
 * Show Review: the show-file audit (docs/design/SHOW_REVIEW.md). A registry
 * of checks, each a pure read of the Doc; run them, and the findings come
 * back with what was accepted applied. Acceptances (who, when, the note) and
 * checks turned off for the show are saved with it, under the Engine as
 * <ShowReview>, written only when there is something in it. An acceptance
 * reopens (Changed) when what its check read changes.
 *
 * Owned by Doc (a thin forwarder for the XML); headless: the CLI
 * (qlcconsole --review, ShowReviewCli) and the panel run the same code.
 */
class QLC_ENGINE_EXPORT ShowReview final : public QObject
{
    Q_OBJECT

public:
    enum Area { Movement, Palettes, Cues, Hits, ShowControl, Patch, Fixtures, Venues,
                Effects, Power, Files, Naming };

    typedef std::function<void(ReviewContext &, QList<ReviewFinding> &)> Runner;

    struct Check
    {
        QString id;             //!< "PAT-01", stable forever
        QString title;          //!< "Two fixtures on the same channels"
        Area area = Patch;
        QString why;            //!< Why this matters: a paragraph
        Runner run;
    };

    /** Every check built, in catalog order. */
    static const QList<Check> &catalog();
    static const Check *check(const QString &id);
    static QString areaName(Area area);
    static QString severityName(ReviewFinding::Severity severity);

    /** The footer readiness entries that are design problems, folded in
     *  under their catalog id (section 2.1, 3.13): "fixtures.clash" ->
     *  "PAT-01". Empty for a runtime entry (it stays the footer's). */
    static QString footerCheckId(const QString &statusKey);

    /** FX-07's half of the footer's "effects.failed": effect looks in
     *  scenes with no script chosen, or a script not on this computer
     *  (palette id -> why). The footer adds what failed while running. */
    static QHash<quint32, QString> designEffectProblems(Doc *doc);

    /** What a run looks at. */
    struct Options
    {
        /** Check ids or prefixes ending in '*' ("PAT-*"); empty: all. */
        QStringList checks;
        /** Whether the Doc's own output patches are loaded. The CLI loads
         *  no I/O (no plugins); it says which universes the file patches
         *  an output on instead. */
        bool outputsLoaded = true;
        QSet<quint32> universesWithOutput;
        /** Keep the result as lastResult() and emit reviewed(). False: a
         *  question asked of the checks (the command line, AI Assist) that
         *  leaves the panel's and the footer's result as it was. */
        bool record = true;
    };

    struct Result
    {
        QList<ReviewFinding> findings;      //!< worst first, then by check
        int open[3] = { 0, 0, 0 };          //!< per Severity, not accepted
        int accepted[3] = { 0, 0, 0 };
        int changed = 0;                    //!< accepted, but changed since
        qint64 ms = 0;
        QMap<QString, qint64> checkMs;
        QDateTime when;
        bool ran = false;
    };

    explicit ShowReview(Doc *doc);

    /** Run the checks; the result is also kept (lastResult()) and
     *  reviewed() emitted. */
    Result run(const Options &options);
    Result run();
    const Result &lastResult() const { return m_last; }

    /************************************************************************
     * Acceptance
     ************************************************************************/
    struct Acceptance
    {
        QString check;
        QString targets;        //!< sorted, comma separated
        QString hash;
        QString by;
        QString when;           //!< ISO date time
        QString note;
        QXmlStreamAttributes unknown;
    };
    struct TurnedOff
    {
        QString check;
        QString by;
        QString when;
        QString note;
    };

    /** Accept @a finding with @a note (required) by @a by: saved with the
     *  show, the show marked modified. A note-less accept is refused. */
    bool accept(const ReviewFinding &finding, const QString &note, const QString &by);
    /** Drop the acceptance of @a finding: it is open again. */
    bool reopen(const ReviewFinding &finding);
    /** Turn check @a checkId off for this show (an acceptance of every
     *  finding it makes), with a note. */
    bool turnOff(const QString &checkId, const QString &note, const QString &by);
    bool turnOn(const QString &checkId);
    bool isOff(const QString &checkId) const;

    QList<Acceptance> acceptances() const { return m_accepted; }
    QList<TurnedOff> turnedOff() const { return m_off; }

    /** Who an acceptance is by: this computer's "Your name" (QSettings),
     *  else the account's name. */
    static QString defaultName();
    static void setDefaultName(const QString &name);

    /** A new show: nothing accepted, nothing off, no result. */
    void reset();

    /** Write <ShowReview> (nothing when empty). Acceptances whose subject
     *  is gone from the show are dropped. */
    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);

    /** Whether target key @a key ("fx:7") still names something in @a doc. */
    static bool targetExists(const Doc *doc, const QString &key);

signals:
    /** A run finished (lastResult() is new). */
    void reviewed();
    /** An acceptance or a check turned off/on. */
    void acceptancesChanged();

private:
    void applyAcceptance(ReviewFinding &finding) const;
    void changed();

private:
    Doc *m_doc;
    QList<Acceptance> m_accepted;
    QList<TurnedOff> m_off;
    QXmlStreamAttributes m_unknown;
    Result m_last;
};

/** @} */

#endif
