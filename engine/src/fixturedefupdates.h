/*
  Q Light Controller Plus - qlcconsole
  fixturedefupdates.h

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

#ifndef FIXTUREDEFUPDATES_H
#define FIXTUREDEFUPDATES_H

#include <QStringList>
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QDir>

#include "qlcengine_export.h"

class QLCFixtureDef;
class Fixture;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Definitions in the user folder that are older than the current version of
 * the same fixture -- the one shipped with the app, or a newer copy in the
 * user folder -- and the way to bring them up to date: never by itself,
 * always with a backup that can be put back.
 *
 * "Older" is decided carefully, so a definition someone changed on purpose
 * is never called old just because it differs (judge()):
 *  - the same bytes: nothing to do;
 *  - another author: a customised copy, not an old one;
 *  - it has a channel or a mode the current one does not: customised;
 *  - otherwise the Creator version decides (4.12.6 < 4.14.2 <
 *    4.14.2+qlcconsole.1393), and, at the same version, between two files
 *    of the user's own, the one changed later is the current one. A shipped
 *    file's time is when the app was installed, so it never decides.
 *
 * The old QLC+ folder's copies are compared too when no link leads to them
 * (a link replaced by a file of its own leaves the synced original behind,
 * still old, for every other computer that links to it).
 *
 * The same comparison says which of a show's embedded copy and this
 * computer's is newer (ShowEmbedding::newer()), and diff()/impact() say
 * what an update would change and whether the fixtures a show patched can
 * follow it.
 */
namespace FixtureDefUpdates
{
    /** One definition file, as read for comparing. */
    struct QLC_ENGINE_EXPORT Info
    {
        QString path;           ///< as listed (a link, when it is one)
        QString realPath;       ///< where its bytes are (the link's target)
        QString linkTarget;     ///< the link's own target text, when it is a link
        bool isLink = false;
        bool linkIntoLegacy = false;  ///< a link into the old QLC+ folder (synced)
        bool inLegacyFolder = false;  ///< a file IN that folder no link here leads to
        bool shipped = false;   ///< part of the app: never written to
        QString manufacturer;
        QString model;
        QString creatorName;
        QString creatorVersion;
        QString author;
        QDateTime modified;
        QByteArray bytes;
        QString sha256;

        bool isValid() const { return !manufacturer.isEmpty() && !model.isEmpty(); }
        QString key() const { return manufacturer + QLatin1Char('|') + model; }
        /** "4.14.2+qlcconsole.1393, 19 Oct 2026" -- for a list. */
        QString versionText() const;
    };

    /** Read @p path: its Creator, make, model, time and bytes. */
    QLC_ENGINE_EXPORT Info readInfo(const QString &path, bool shipped = false);
    /** The same from bytes (a show's embedded copy). */
    QLC_ENGINE_EXPORT Info infoFromBytes(const QByteArray &bytes);

    /** -1, 0 or 1 as Creator version @p a is older, the same as or newer
     *  than @p b. @p known false (and 0) when either cannot be read. */
    QLC_ENGINE_EXPORT int compareVersions(const QString &a, const QString &b, bool *known = nullptr);
    /** Which of two definitions is newer: the version, then -- for two files
     *  of the user's, never a shipped one -- the time they were changed.
     *  0 when it cannot be told. */
    QLC_ENGINE_EXPORT int compare(const Info &a, const Info &b);

    enum Verdict
    {
        Identical,      ///< the same bytes
        OutOfDate,      ///< an older copy of the current one
        Customised,     ///< different on purpose, or cannot tell: never "old"
        Newer           ///< newer than what is called current
    };
    QLC_ENGINE_EXPORT Verdict judge(const Info &mine, const Info &current);
    QLC_ENGINE_EXPORT QString verdictText(Verdict v);

    /*********************************************************************
     * What changes
     *********************************************************************/
    struct QLC_ENGINE_EXPORT Change
    {
        enum Kind { Added, Removed, Changed };
        enum Area { General, Channel, Mode };
        Kind kind = Changed;
        Area area = General;
        QString name;           ///< the channel or mode
        QString detail;         ///< what about it, in a few words
        bool layout = false;    ///< a mode whose channels are not what they were
        /** "Channel “Pan”: coarse, not fine; default 0 → 128". */
        QString text() const;
    };
    /** What becomes different going from @p from to @p to: channels and
     *  modes added, removed or changed, modes first. */
    QLC_ENGINE_EXPORT QList<Change> diff(const QLCFixtureDef *from, const QLCFixtureDef *to);
    QLC_ENGINE_EXPORT QList<Change> diff(const QByteArray &from, const QByteArray &to);

    enum ModeFit
    {
        SameLayout,         ///< the same channels, in the same order
        ChannelsChanged,    ///< as many channels, but one does something else
        CountChanged,       ///< more or fewer channels: the patch moves
        ModeMissing         ///< no mode of that name
    };
    /** How mode @p mode of @p from fits the mode of the same name in @p to. */
    QLC_ENGINE_EXPORT ModeFit modeFit(const QLCFixtureDef *from, const QLCFixtureDef *to,
                                      const QString &mode, QString *why = nullptr);

    /** What switching the fixtures of @p doc that use @p from to @p to
     *  (by mode name) does to the show. */
    struct QLC_ENGINE_EXPORT Impact
    {
        QStringList refused;    ///< cannot be done without re-patching
        QStringList confirm;    ///< can, but a channel would do something else
        QList<Fixture *> fixtures;  ///< every fixture that would switch
        /** Nothing in the way, nothing to confirm. */
        bool clean() const { return refused.isEmpty() && confirm.isEmpty(); }
    };
    /** @p toName names @p to in what is said ("the new version" when empty;
     *  "this computer's copy" for a show's). */
    QLC_ENGINE_EXPORT Impact impact(Doc *doc, const QLCFixtureDef *from, const QLCFixtureDef *to,
                                    const QString &toName = QString());
    /** The functions that set values on, or move, @p fixtures (names). */
    QLC_ENGINE_EXPORT QStringList functionsUsing(Doc *doc, const QList<Fixture *> &fixtures);
    /** Switch @p doc's fixtures from @p from to @p to, each to its mode of the
     *  same name, @p to in the cache in place of @p from (kept, not deleted:
     *  something may still point at it). False, nothing changed, when a mode
     *  is missing. */
    QLC_ENGINE_EXPORT bool remap(Doc *doc, QLCFixtureDef *from, QLCFixtureDef *to);

    /*********************************************************************
     * The user folder
     *********************************************************************/
    struct QLC_ENGINE_EXPORT Candidate
    {
        Info mine;              ///< the user's file
        Info current;           ///< the current version of the same fixture
        Verdict verdict = OutOfDate;
        bool usedByShow = false;    ///< the open show's fixtures use this file
        QList<Change> changes;  ///< mine -> current
    };
    struct QLC_ENGINE_EXPORT Scan
    {
        QList<Candidate> candidates;
        QStringList unreachable;    ///< links whose target is not there
    };
    /** The user folder's definitions against the current ones (shipped, or
     *  newer copies), for @p doc's cache and show. Out-of-date ones, and the
     *  customised ones when @p withCustomised; never one dismissed with
     *  keepMine() or ignoreVersion(). */
    QLC_ENGINE_EXPORT Scan scan(Doc *doc, bool withCustomised = false);

    /** What updating @p c would do to the open show's fixtures (nothing,
     *  when the show does not use that file). */
    QLC_ENGINE_EXPORT Impact impactOf(Doc *doc, const Candidate &c);

    /** Don't offer an update for this file while it stays as it is. */
    QLC_ENGINE_EXPORT void keepMine(const Candidate &c);
    /** Don't offer this current version again (a newer one is offered). */
    QLC_ENGINE_EXPORT void ignoreVersion(const Candidate &c);
    QLC_ENGINE_EXPORT bool dismissed(const Candidate &c);
    /** Forget every keepMine()/ignoreVersion() (tests). */
    QLC_ENGINE_EXPORT void clearDismissals();

    enum LinkMode
    {
        UpdateLinkedOriginal,   ///< write the file the link points to
        ReplaceLink             ///< a file of its own here, the original left alone
    };

    struct QLC_ENGINE_EXPORT Result
    {
        QString backupFolder;   ///< where the old files are (undo())
        QStringList updated;    ///< one line each, saying what was written
        QStringList failed;     ///< one line each, saying why not
    };
    /** Bring each of @p candidates up to date: its old file copied to a new
     *  folder under backupRoot(), then the current version written over it
     *  -- through a link to the file it points to, or in its place, by
     *  @p linkMode. A definition the open show uses is switched in place
     *  (each fixture to its mode of the same name); one whose modes no
     *  longer fit is refused, and one whose channels would do something
     *  else needs @p confirmed. Never writes a shipped file. */
    QLC_ENGINE_EXPORT Result update(Doc *doc, const QList<Candidate> &candidates,
                                    LinkMode linkMode, bool confirmed = false);
    /** Put back what update() wrote into @p backupFolder: each file as it
     *  was (a link again, when one was replaced), unless it has been changed
     *  since. */
    QLC_ENGINE_EXPORT Result undo(Doc *doc, const QString &backupFolder);
    /** The latest update that can still be undone; empty when none. */
    QLC_ENGINE_EXPORT QString lastUndoable();
    /** The old QLC+ user folder's Fixtures (QLCFile::mergeLegacyUserData()
     *  links from it) -- on Branson's Macs a synced folder. */
    QLC_ENGINE_EXPORT QDir legacyFixturesDirectory();
    /** <user folder>/.backup -- one dated folder per update. */
    QLC_ENGINE_EXPORT QDir backupRoot();
    /** @p path is one of the app's own definitions (or the source tree's). */
    QLC_ENGINE_EXPORT bool isShippedPath(const QString &path);
}

/** @} */

#endif
