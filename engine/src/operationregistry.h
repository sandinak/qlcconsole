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
 * line it runs goes through here), AI Assist (the model's tool calls; in
 * P1 only the read-only operations) and, later, an MCP server. No operation
 * logic lives in a front end.
 *
 * Shaped like Show Review's registry (a static catalog of plain structs
 * with std::function bodies, headless, owned by nobody): the catalog is
 * shared; an instance only binds it to a Doc and, for the command line,
 * the view's ShowTreeCommand.
 *
 * The Show Tree grammar is the `command` operation (a whole line, one undo
 * step, exactly as before). Its words become operations of their own when
 * P2 gives write operations schemas, previews and confirm.
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
        /** The `command` operation: what ShowTreeCommand::run() said. */
        ShowTreeCommand::Result command;
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
    /** The read-only operations: what AI Assist P1 offers the model. */
    static QList<const Operation *> readOnlyOperations();

    OperationRegistry(Doc *doc, ShowTreeCommand *commander = nullptr);

    /** Validate and run operation @a name with @a args. An unknown
     *  operation, or arguments that fail validate(), run nothing. */
    Result run(const QString &name, const QJsonObject &args,
               const ShowTreeCommand::Context &selection = ShowTreeCommand::Context());

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

    /** Is function @a id used somewhere the engine can't see? An extra
     *  to the Virtual Console (ShowReview::vcIndex(), which the registry
     *  reads itself); unset: nothing more is. */
    static void setFunctionUseProbe(const std::function<bool(quint32)> &probe);

private:
    bool nameTaken(const QString &word) const;

    Doc *m_doc;
    ShowTreeCommand *m_commander;
};

/** @} */

#endif
