/*
  Q Light Controller Plus - qlcconsole
  showembedding.h

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

#ifndef SHOWEMBEDDING_H
#define SHOWEMBEDDING_H

#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QDir>

#include <memory>

class QXmlStreamReader;
class QXmlStreamWriter;
class QTemporaryDir;
class QLCFixtureDef;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLShowEmbedded QStringLiteral("Embedded")

/**
 * A show that carries its own definitions and scripts
 * (SHOW_EMBEDDING_DESIGN.md).
 *
 * Saving a .qlcc embeds, byte for byte, every fixture definition, effect
 * script and RGB script the show uses that is not part of the app. Opening
 * one compares each with this computer's copy:
 *
 *  - only the show has it: the show's is used (and can be installed);
 *  - both, identical: nothing to say;
 *  - both, different: the SHOW's is used until the operator chooses --
 *    nothing changes under a patched rig by itself.
 *
 * The show's copies are written to a working folder of their own for as
 * long as the show is open, and put in the caches in place of this
 * computer's; closing the show (Doc::clearContents()) puts this computer's
 * back. Saving embeds the version in use -- never quietly this computer's.
 */
class ShowEmbedding final : public QObject
{
    Q_OBJECT

public:
    enum Kind { FixtureDefinition, EffectScript, RGBScript };
    enum State
    {
        ShowOnly,       ///< this computer has none
        Identical,      ///< this computer's is the same, byte for byte
        Different       ///< this computer's is not the show's
    };
    enum Newer
    {
        NewerUnknown,   ///< can't tell
        ComputerNewer,  ///< this computer's file changed after it was embedded
        ShowNewer       ///< this computer has an older copy
    };

    struct Item
    {
        Kind kind = FixtureDefinition;
        QString manufacturer;   ///< a fixture definition's
        QString model;          ///< a fixture definition's
        QString name;           ///< an RGB script's (algo.name)
        QString file;           ///< its file name
        QByteArray bytes;       ///< the file, unchanged
        QString sha256;         ///< of @a bytes, hex
        QString from;           ///< user, legacy, showcache, showfolder
        QDateTime fromModified; ///< the source file's time when embedded
        QDateTime embedded;     ///< when it was embedded
        QString keptAgainst;    ///< "Keep the show's": this computer's SHA-256 then

        /* While the show is open -- not saved. */
        State state = ShowOnly;
        bool fromShowFolder = false; ///< found in the show's folder, not embedded
        QString showPath;       ///< the show's copy, on disk
        QString computerPath;   ///< this computer's file, when it has one
        QString computerSha;    ///< of that file
        bool usingShow = false; ///< the show's copy is the one in use

        /** "fixture:Make|Model", "effect:file.js", "rgb:file.js". */
        QString key() const;
        /** "PHS Chorus Step Row 64 Heads (fixture definition)". */
        QString displayName() const;
        /** A difference the operator has not dealt with. */
        bool isUnreviewedDifference() const
        { return state == Different && usingShow && keptAgainst != computerSha; }
    };

    struct Compatibility
    {
        bool compatible = true;
        QStringList problems;   ///< one line each, naming the fixtures or looks
    };

    explicit ShowEmbedding(Doc *doc);
    ~ShowEmbedding() override;

    /*********************************************************************
     * The file
     *********************************************************************/
    /** Read the <Embedded> section of workspace @p fileName (it comes
     *  before <Engine>, so only the start of the file is read). False when
     *  the file cannot be read; true, with no items, when it has none. */
    bool readWorkspace(const QString &fileName);
    /** Read an <Embedded> element (the reader on its start). */
    bool loadXML(QXmlStreamReader &xml);
    /** Write the items as an <Embedded> element; nothing when there are none. */
    void saveXML(QXmlStreamWriter *xml) const;

    /*********************************************************************
     * Opening
     *********************************************************************/
    /** Before the show's contents load: each item compared with this
     *  computer's, and the show's put in use where this computer has none
     *  or a different one. Then @p showFolder's Fixtures/, EffectScripts/
     *  and RGBScripts/ for what neither has. */
    void apply(const QString &showFolder = QString());
    /** This computer's copies back, the show's working folder gone. */
    void restoreComputer();

    /*********************************************************************
     * Saving
     *********************************************************************/
    /** The items become what the show uses now that is not part of the
     *  app, each as it is in use: the show's copy where that is in use. */
    void collect();

    QList<Item> items() const { return m_items; }
    bool hasItem(const QString &key) const;
    Item item(const QString &key) const;
    /** What went wrong reading the show's copies (one line each): an
     *  altered copy, one that does not load. Cleared with the show. */
    QStringList problems() const { return m_problems; }
    /** The differences not dealt with (not kept, show's in use). */
    QList<Item> differences() const;
    /** What only the show has, in use and not installed. */
    QList<Item> showOnly() const;

    /** Which side looks newer, from the times. */
    Newer newer(const Item &item) const;
    static QString newerText(Newer n);
    /** Whether switching @p key to this computer's copy keeps everything the
     *  show uses working: every mode its fixtures use, with the same channels
     *  in the same order; every parameter its looks set, of the same kind. */
    Compatibility compatibility(const QString &key) const;

    /*********************************************************************
     * The operator's choices
     *********************************************************************/
    /** Switch to this computer's copy now (embedded at the next save).
     *  False, with @p error, when it cannot be done without re-patching. */
    bool useComputers(const QString &key, QString *error = nullptr);
    /** Stop warning about @p key (remembered in the show). */
    void keepShows(const QString &key);
    /** Write the show's copy to this computer: over its file (that one to
     *  the Trash), or into the user folder when it has none or ships it. */
    bool installShows(const QString &key, QString *error = nullptr);

    /** Tests: delete replaced files rather than move them to the Trash. */
    static void setDeleteInsteadOfTrash(bool on);

    /*********************************************************************
     * Helpers
     *********************************************************************/
    /** Whether a show saved as @p fileName carries its definitions and
     *  scripts: a .qlcc does; a .qxw is left as QLC+ can read it. */
    static bool embedsIn(const QString &fileName);
    static QString sha256(const QByteArray &bytes);
    /** Whether @p path is one of the app's own (shipped) scripts. */
    static bool isShippedScript(const QString &path, bool rgb);
    /** The show's working folder, empty when it has none. */
    QString workingDirectory() const;

signals:
    void changed();

private:
    void ensureWorkingDirectory();
    QString materialize(Item &item);
    QString classify(const QString &path) const;
    bool isShowCopy(const QString &path) const;
    void applyFixture(Item &item);
    void applyScript(Item &item);
    void applyShowFolder(const QString &showFolder);
    void refreshScripts();
    void repointScripts(const QString &fromPath, const QString &toPath, const Item &item);
    Item *find(const QString &key);
    bool discard(const QString &path) const;

private:
    Doc *m_doc;
    QList<Item> m_items;
    QStringList m_problems;
    std::unique_ptr<QTemporaryDir> m_work;
    QString m_showFolder;
    bool m_scriptDirsSet = false;                 ///< the script cache searches ours
    QHash<QString, QLCFixtureDef *> m_showDefs;   ///< key -> the show's, in the cache
    QHash<QString, QLCFixtureDef *> m_displaced;  ///< key -> this computer's, set aside
    QList<QLCFixtureDef *> m_retired;             ///< out of the cache, kept for stale pointers
};

/** @} */

#endif
