/*
  Q Light Controller Plus - qlcconsole
  configimport.h

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

#ifndef CONFIGIMPORT_H
#define CONFIGIMPORT_H

#include <QCoreApplication>
#include <QJsonObject>
#include <QStringList>
#include <QString>
#include <QList>
#include <QMap>
#include <functional>

#include "qlcengine_export.h"
#include "universe.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Configuration import (docs/design/AI_ASSIST.md section 5b): take the
 * universes and the I/O setup (outputs, inputs with their input profile,
 * feedback, each with its plugin, line and plugin parameters) from another
 * show file, with a mapping. Deterministic and headless: it is what the
 * `import_config` operation (File ▸ Import ▸ Configuration, the command
 * line, AI Assist) runs, and it works with AI off.
 *
 * The file is only read as XML; nothing in it is opened or patched until
 * apply(). Fixtures, groups and palettes are not part of it: File ▸ Import ▸
 * Workspace brings those in (and the summary says how many the file has).
 *
 * plan() checks everything and changes nothing. A problem stops the whole
 * import (nothing is applied); a warning does not. Route checks are
 * warnings, never problems: designing off the show network is normal.
 * Patches keep their route requirement (the line's network, the target
 * address) exactly as the file has it, so an off-network import succeeds
 * and the patch waits, kept, until the network is there (the same
 * "pending" patch a show opened off its network gets).
 */
class QLC_ENGINE_EXPORT ConfigImport
{
    Q_DECLARE_TR_FUNCTIONS(ConfigImport)

public:
    /** What a section name means. */
    static QStringList allSections();      //!< universes, outputs, inputs, feedback

    struct SourceUniverse
    {
        int number = 0;             //!< 1-based position in the file
        QString name;
        bool passthrough = false;
        bool hasSlot = false;       //!< a controller slot (not imported)
        QList<Universe::OfflinePatch> patches;  //!< Input / Output / Feedback, in file order
    };

    struct Source
    {
        bool ok = false;
        QString error;
        QList<SourceUniverse> universes;
        int fixtures = 0;
        int groups = 0;
        int palettes = 0;
    };

    /** Read @a path (a .qxw: plain XML). */
    static Source readSource(const QString &path);
    /** A readable summary of @a source: universes and their patches, and
     *  what File ▸ Import ▸ Workspace would bring. */
    static QString sourceText(const Source &source);

    /** The choices an import is made with. */
    struct Options
    {
        QString file;
        QStringList sections;               //!< empty: all of allSections()
        QList<int> universes;               //!< source universe numbers; empty: all
        QMap<int, int> universeMap;         //!< source number -> target number (1-based)
        QMap<QString, QString> addressMap;  //!< "2.0.0.50" -> "2.0.0.60"
        QMap<QString, QString> pluginMap;   //!< source plugin -> plugin here
        QString onCollision;                //!< "skip" | "replace" | "renumber"; empty: ask
        QString onMissingPlugin;            //!< "skip" | "keep"; empty: ask
    };

    /** The operation's JSON arguments -> Options; false (and @a why) when
     *  a value isn't in the right form. */
    static bool optionsFrom(const QJsonObject &args, Options *options, QString *why);
    /** The other way (the command-line text and previews). */
    static QJsonObject toArgs(const Options &options);

    struct Step
    {
        int sourceNumber = 0;
        QString sourceName;
        int target = 0;             //!< 0-based index in this show
        bool isNew = false;         //!< a universe made for it
        bool skip = false;          //!< left alone (collision: skip)
        bool collision = false;
        bool setName = false;
        QString name;
        bool passthrough = false;
        bool setOutputs = false;
        bool setInput = false;
        bool setFeedback = false;
        QList<Universe::OfflinePatch> outputs;
        QList<Universe::OfflinePatch> inputs;
        QList<Universe::OfflinePatch> feedback;
        QStringList skipped;        //!< patches left out (plugin missing here, skip)
    };

    struct Plan
    {
        Options options;
        Source source;
        QList<Step> steps;
        QStringList problems;       //!< any: nothing is applied
        QStringList warnings;       //!< route, plugin kept offline, profile missing...
        QStringList diff;           //!< current -> resulting, a line each
        bool ok() const { return problems.isEmpty(); }
        /** Problems, warnings and the diff as text for a preview. */
        QString text() const;
    };

    /** Check @a options against @a doc: reads the file, changes nothing. */
    static Plan plan(Doc *doc, const Options &options);

    struct Applied
    {
        bool ok = false;
        QString message;            //!< one line
        QStringList report;         //!< imported, skipped, remapped, warnings
        /** Puts the universes back (true) or forward (false): attach it to
         *  the undo step the import is part of. */
        std::function<void(bool undone)> undo;
    };

    /** Apply @a options (plan() first: a problem applies nothing). Design
     *  mode only. The caller makes it one undo step. */
    static Applied apply(Doc *doc, const Options &options);
};

/** @} */

#endif
