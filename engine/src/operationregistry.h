/*
  Q Light Controller Plus - qlcconsole
  operationregistry.h

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

#ifndef OPERATIONREGISTRY_H
#define OPERATIONREGISTRY_H

#include <QCoreApplication>
#include <QJsonObject>
#include <QStringList>
#include <QString>
#include <QList>
#include <functional>

#include "qlcengine_export.h"
#include "showtreecommand.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Operation Registry (docs/design/AI_ASSIST.md section 4): every thing
 * the console can be asked to do by a front end that isn't the mouse, once.
 * Each operation has a stable name, a JSON schema for its arguments,
 * validate(), execute(), the command-line text that does the same
 * (cliSyntax()) and a sentence for a preview (describe()).
 *
 * The front ends are thin: the Show Tree command line (runLine(): every
 * line it runs goes through here), AI Assist (the model's tool calls: the
 * read-only operations run as asked, the change operations only once the
 * operator has confirmed them, as one plan -- runPlan()) and, later, an MCP
 * server. No operation logic lives in a front end.
 *
 * Shaped like Show Review's registry (a static catalog of plain structs
 * with std::function bodies, headless, owned by nobody): the catalog is
 * shared; an instance only binds it to a Doc and, for the command line,
 * the view's ShowTreeCommand.
 *
 * The Show Tree grammar is the `command` operation (a whole line, one undo
 * step, exactly as before): what a typed line that isn't a question runs.
 * Its change words are also operations of their own (P2: `make_cue`,
 * `set_look`, `record_cue`...), each with a schema, so a model can call
 * them as tools: each renders the one grammar line that does it
 * (cliSyntax()) and runs that line, so the grammar stays the only place
 * the Show Tree is edited from text.
 */
class QLC_ENGINE_EXPORT OperationRegistry final
{
    Q_DECLARE_TR_FUNCTIONS(OperationRegistry)

public:
    /** What an operation is run with. */
    struct Context
    {
        Doc *doc = nullptr;
        /** The command line's grammar (the `command` operation); null:
         *  that operation is unavailable (AI Assist, the tests). */
        ShowTreeCommand *commander = nullptr;
        /** Where the command line's selection is. */
        ShowTreeCommand::Context selection;
    };

    struct Result
    {
        bool ok = false;
        /** A line for the command line's message. */
        QString message;
        /** The answer, plain text (a read-only operation's list, the
         *  findings): what the command line's help pane shows and what the
         *  model is given back. Empty for the `command` operation. */
        QString text;
        /** The `command` operation and the change operations: what
         *  ShowTreeCommand::run() said (runPlan(): the last step's, with
         *  where the plan leaves the selection). */
        ShowTreeCommand::Result command;
    };

    /** One step of a plan: an operation and its arguments. */
    struct Call
    {
        QString name;
        QJsonObject args;
    };

    typedef std::function<QString(Doc *, const QJsonObject &)> Validator;
    typedef std::function<Result(Context &, const QJsonObject &)> Executor;
    typedef std::function<QString(const QJsonObject &)> Renderer;
    /** The command-line words after the operation's own word -> its
     *  arguments; false (and @a why) when they don't fit. */
    typedef std::function<bool(const QStringList &, QJsonObject *, QString *)> Parser;

    struct Operation
    {
        QString name;           //!< "find_conflicts", stable forever
        QString title;          //!< "Find address conflicts"
        QString description;    //!< for the model and the help: what it answers
        bool readOnly = true;   //!< a pure read of the Doc
        QJsonObject schema;     //!< JSON schema of the arguments (an "object")
        /** Its word in the command line ("conflicts"); empty: none. */
        QString cliWord;
        QString cliHelp;        //!< "conflicts" / "describe <function>"
        Validator validate;     //!< "" when the arguments are good
        Executor execute;
        Renderer cliSyntax;     //!< arguments -> the command-line text
        Parser parse;           //!< the command-line words -> arguments
        Renderer describe;      //!< arguments -> a sentence for a preview
    };

    /** Every operation, in catalog order. */
    static const QList<Operation> &catalog();
    static const Operation *operation(const QString &name);
    /** The read-only operations: what AI Assist runs when the model asks. */
    static QList<const Operation *> readOnlyOperations();
    /** The change operations (not `command`): what AI Assist P2 may
     *  propose, run only after the operator confirms. */
    static QList<const Operation *> changeOperations();

    /** @a args against JSON schema @a schema (the subset the catalog uses:
     *  type, properties, required, additionalProperties: false, enum,
     *  minimum/maximum, minLength, pattern, items, minItems/maxItems).
     *  "" when they fit, else why not ("level: 140 is above 100"). A null
     *  value counts as left out. */
    static QString checkArgs(const QJsonObject &schema, const QJsonObject &args);

    OperationRegistry(Doc *doc, ShowTreeCommand *commander = nullptr);

    /** Validate and run operation @a name with @a args. An unknown
     *  operation, or arguments that fail the schema (checkArgs()) or
     *  validate(), run nothing. */
    Result run(const QString &name, const QJsonObject &args,
               const ShowTreeCommand::Context &selection = ShowTreeCommand::Context());

    /** What run() would refuse @a name with @a args for, without running
     *  it: "" when it would run. */
    QString check(const QString &name, const QJsonObject &args) const;

    /** A confirmed plan of change operations (docs/design/AI_ASSIST.md
     *  section 7): every step is checked first, then they run in order --
     *  each where the one before left the selection -- as ONE undo step
     *  named @a label on the Show Tree's history. Any step that is refused
     *  (checked or run) puts everything back: nothing partial. Refused
     *  outright in Operate, for a read-only step, or without the command
     *  line's grammar. */
    Result runPlan(const QList<Call> &calls, const ShowTreeCommand::Context &selection,
                   const QString &label);

    /** The command line: a registry word ("conflicts", "why Spot 1") runs
     *  its operation, unless a group, fixture or function of the show is
     *  called that (the show's names win, as before); anything else is the
     *  `command` operation (the Show Tree grammar), unchanged. `help` gets
     *  the registry's words added. */
    ShowTreeCommand::Result runLine(const QString &line, const ShowTreeCommand::Context &selection);

    /** Which operation @a line would run, and its arguments ("command" and
     *  {line} for the grammar). */
    QString resolveLine(const QString &line, QJsonObject *args, QString *why = nullptr) const;

    /** The command line's help section for the registry's words (rich
     *  text, examples as "ex:" links like ShowTreeCommand::helpText()). */
    static QString helpText();

    /** Words of a command line, split as ShowTreeCommand reads them:
     *  "quoted names" stay one word, without their quotes. */
    static QStringList splitWords(const QString &line);
    /** A name as it goes in a command line: quoted when it has a space. */
    static QString quoted(const QString &name);

    /** Answers longer than this many characters are cut (with a line that
     *  says so): what a model is sent stays bounded. */
    static const int MaxText = 8000;

    /** Is function @a id used somewhere the engine can't see (a Virtual
     *  Console widget)? The UI sets it; unset: nothing is. */
    static void setFunctionUseProbe(const std::function<bool(quint32)> &probe);

private:
    bool nameTaken(const QString &word) const;

    Doc *m_doc;
    ShowTreeCommand *m_commander;
};

/** @} */

#endif
