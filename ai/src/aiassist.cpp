/*
  Q Light Controller Plus - qlcconsole
  aiassist.cpp

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

#include <QNetworkAccessManager>
#include <QCoreApplication>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QHash>
#include <QTimer>
#include <QDebug>

#include "operationregistry.h"
#include "aiassist.h"

namespace
{
AiAssist *s_instance = nullptr;
int s_violations = 0;
#ifdef QT_DEBUG
bool s_assertOnViolation = true;
#else
bool s_assertOnViolation = false;
#endif

struct Tracked
{
    QPointer<QObject> obj;
    QString label;
};
QList<Tracked> &tracked()
{
    static QList<Tracked> list;
    return list;
}

/** The context sent with "summary": capped, never the show file. */
const int kMaxContext = 6000;
}

/****************************************************************************
 * AiResources
 ****************************************************************************/

void AiResources::track(QObject *obj, const QString &label)
{
    QList<Tracked> &list = tracked();
    for (int i = list.count() - 1; i >= 0; --i)
        if (list.at(i).obj.isNull())
            list.removeAt(i);
    list.append({ QPointer<QObject>(obj), label });
}

QStringList AiResources::live()
{
    QStringList out;
    for (const Tracked &t : tracked())
        if (!t.obj.isNull())
            out << t.label;
    return out;
}

void AiResources::destroyAll()
{
    const QList<Tracked> list = tracked();
    tracked().clear();
    for (const Tracked &t : list)
        if (!t.obj.isNull())
            delete t.obj.data();
}

/****************************************************************************
 * The unload on entering Operate (section 5a steps 3-4): teardown machinery,
 * not AI Assist -- it lives only until Ollama has answered.
 ****************************************************************************/

namespace
{
class AiUnloader : public QObject
{
public:
    typedef std::function<void(bool resident, const QString &problem)> Done;

    AiUnloader(const AiSettings &settings, const Done &done)
        : m_settings(settings)
        , m_done(done)
    {
        m_nam = new QNetworkAccessManager(this);
        AiOpenAiProvider p(settings, QString());
        QNetworkRequest req(p.url(QStringLiteral("/api/generate")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setTransferTimeout(5000);
        const QByteArray body = QJsonDocument(QJsonObject { { QStringLiteral("model"), settings.model },
                                                            { QStringLiteral("keep_alive"), 0 } })
                                    .toJson(QJsonDocument::Compact);
        QNetworkReply *r = m_nam->post(req, body);
        connect(r, &QNetworkReply::finished, this, [this, r]() {
            r->deleteLater();
            QString problem;
            if (r->error() != QNetworkReply::NoError)
                problem = AiAssist::tr("Ollama didn't confirm the unload (%1)").arg(r->errorString());
            check(problem);
        });
    }

private:
    void check(const QString &problem)
    {
        AiOpenAiProvider p(m_settings, QString());
        QNetworkRequest req(p.url(QStringLiteral("/api/ps")));
        req.setTransferTimeout(5000);
        QNetworkReply *r = m_nam->get(req);
        connect(r, &QNetworkReply::finished, this, [this, r, problem]() {
            r->deleteLater();
            bool resident = false;
            QString why = problem;
            if (r->error() == QNetworkReply::ConnectionRefusedError)
            {
                why.clear();     // Ollama isn't running: nothing is loaded
            }
            else if (r->error() != QNetworkReply::NoError)
            {
                if (why.isEmpty())
                    why = AiAssist::tr("couldn't ask Ollama what is loaded (%1)").arg(r->errorString());
            }
            else
            {
                const QJsonArray models = QJsonDocument::fromJson(r->readAll()).object()
                                              .value(QStringLiteral("models")).toArray();
                for (const QJsonValue &v : models)
                {
                    const QJsonObject m = v.toObject();
                    if (m.value(QStringLiteral("name")).toString() == m_settings.model
                        || m.value(QStringLiteral("model")).toString() == m_settings.model)
                        resident = true;
                }
                why.clear();     // /api/ps is the word on what is loaded
            }
            const Done done = m_done;
            deleteLater();
            done(resident, why);
        });
    }

    AiSettings m_settings;
    Done m_done;
    QNetworkAccessManager *m_nam;
};
}

/****************************************************************************
 * AiAssist
 ****************************************************************************/

AiAssist::AiAssist(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    qRegisterMetaType<AiAssist::Answer>();
    qRegisterMetaType<AiAssist::Step>();
    m_settings = AiSettings::load();
    m_operate = doc->mode() == Doc::Operate;
    connect(doc, &Doc::modeChanged, this, &AiAssist::slotModeChanged);
    s_instance = this;
}

AiAssist::~AiAssist()
{
    abortAll();
    if (s_instance == this)
        s_instance = nullptr;
}

AiAssist *AiAssist::instance()
{
    return s_instance;
}

bool AiAssist::isEnabled()
{
    return AiSettings::load().enabled;
}

bool AiAssist::isAvailable() const
{
    return m_settings.enabled && !m_operate;
}

bool AiAssist::isAssistantLine(const QString &line)
{
    const QString t = line.trimmed();
    if (!t.startsWith(QLatin1Char('?')) || !isEnabled())
        return false;
    const QStringList words = t.mid(1).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty())
        return false;                       // "?": the grammar's help
    if (words.count() == 1 && ShowTreeCommand::word(words.first()) != nullptr)
        return false;                       // "? spread": that word's help
    return true;
}

void AiAssist::settingsChanged()
{
    const AiSettings now = AiSettings::load();
    const bool wasEnabled = m_settings.enabled;
    m_settings = now;
    m_unreachable.clear();
    if (wasEnabled && !now.enabled)
    {
        // Off: nothing kept, nothing sent.
        abortAll();
    }
    emit availabilityChanged();
}

int AiAssist::guardViolations()
{
    return s_violations;
}

void AiAssist::setAssertOnViolation(bool on)
{
    s_assertOnViolation = on;
}

bool AiAssist::checkGuard(const char *where) const
{
    QString why;
    if (m_doc->mode() == Doc::Operate || m_operate)
        why = QStringLiteral("in Operate mode");
    else if (QCoreApplication::instance() != nullptr
             && QThread::currentThread() != QCoreApplication::instance()->thread())
        why = QStringLiteral("off the GUI thread");
    if (why.isEmpty())
        return true;
    ++s_violations;
    qCritical().noquote() << QStringLiteral("AI Assist: %1 ran %2").arg(QLatin1String(where), why);
    if (s_assertOnViolation)
        Q_ASSERT_X(false, where, "AI Assist code ran in Operate mode or off the GUI thread");
    return false;
}

QNetworkAccessManager *AiAssist::network()
{
    if (m_nam == nullptr)
    {
        m_nam = new QNetworkAccessManager(this);
        AiResources::track(m_nam, QStringLiteral("network manager"));
    }
    return m_nam;
}

QList<AiTool> AiAssist::tools()
{
    QList<AiTool> out;
    for (const OperationRegistry::Operation *op : OperationRegistry::readOnlyOperations())
        out << AiTool { op->name, op->description, op->schema };
    for (const OperationRegistry::Operation *op : OperationRegistry::changeOperations())
        out << AiTool { op->name, op->description, op->schema };
    return out;
}

QString AiAssist::systemPrompt(const ShowTreeCommand::Context &selection) const
{
    QString s = tr(
        "You are AI Assist inside qlcconsole, a lighting console. You answer the operator's questions "
        "about the show that is open by calling the read-only tools (list_*, find_*, describe_*, why_on, "
        "show_summary). To change the show, call the change tools (make_cue, set_look, set_intensity, "
        "record_cue...): they are NOT run; they are shown to the operator as a proposal, who applies "
        "them all as one undo step or discards them. Put every change of one request in ONE reply, in "
        "the order they should run: each acts on the Show Tree's selected cue, and make_cue, add_cue "
        "and make_cue_list change which cue or list later steps act on. Use only names the show has "
        "(ask the read-only tools first when unsure). Answer briefly, in plain lighting-crew language, "
        "from what the tools return; don't invent fixtures, groups, palettes or cues.");
    if (m_settings.sendShowContext != QLatin1String("summary"))
        return s;
    OperationRegistry reg(m_doc);
    QString context;
    for (const char *name : { "show_summary", "list_groups", "list_fixtures" })
    {
        const OperationRegistry::Result r = reg.run(QLatin1String(name), QJsonObject(), selection);
        if (r.ok)
            context += QStringLiteral("\n\n[%1]\n%2").arg(QLatin1String(name), r.text);
    }
    if (context.size() > kMaxContext)
    {
        context.truncate(kMaxContext);
        context += QStringLiteral("\n(cut; call the tools for the rest)");
    }
    return s + QStringLiteral("\n\nThe show now:") + context;
}

bool AiAssist::ask(const QString &question, const ShowTreeCommand::Context &selection, QString *error)
{
    auto refuse = [error](const QString &why) {
        if (error != nullptr)
            *error = why;
        return false;
    };
    m_settings = AiSettings::load();
    if (!m_settings.enabled)
        return refuse(tr("AI Assist is off (Settings ▸ AI Assist)"));
    if (m_operate || m_doc->mode() == Doc::Operate)
        return refuse(tr("AI Assist is unavailable in Operate mode"));
    if (m_reply != nullptr)
        return refuse(tr("Still answering the last question (Esc cancels it)"));
    if (question.trimmed().isEmpty())
        return refuse(tr("Ask a question after the ?"));
    if (!checkGuard("ask"))
        return refuse(tr("AI Assist is unavailable in Operate mode"));

    QString key;
    if (m_settings.provider == QLatin1String("anthropic") || m_settings.provider == QLatin1String("openai-compatible"))
        key = AiKeychain::read(m_settings.provider);
    if (m_settings.provider == QLatin1String("anthropic") && key.isEmpty())
        return refuse(tr("No API key for Anthropic (Settings ▸ AI Assist)"));
    m_provider = AiProvider::create(m_settings, key);
    m_selection = selection;
    m_system = systemPrompt(selection);
    m_messages.clear();
    AiMessage q;
    q.text = question.trimmed();
    m_messages << q;
    m_round = 0;
    m_answer = Answer();
    m_answer.question = q.text;
    discard();                          // a new question: the old proposal goes
    sendRound();
    emit busyChanged(true);
    return true;
}

void AiAssist::sendRound()
{
    if (!checkGuard("sendRound"))
        return;
    m_usedModel = true;
    QNetworkReply *r = network()->post(m_provider->chatRequest(),
                                       m_provider->chatBody(m_system, m_messages, tools()));
    AiResources::track(r, QStringLiteral("request"));
    m_reply = r;
    connect(r, &QNetworkReply::finished, this, &AiAssist::handleReply);
}

void AiAssist::handleReply()
{
    QNetworkReply *r = qobject_cast<QNetworkReply *>(sender());
    if (r == nullptr || r != m_reply)
        return;
    m_reply = nullptr;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError)
    {
        const QByteArray body = r->readAll();
        QString why = r->errorString();
        const AiTurn t = m_provider->parseChat(body);
        if (!t.error.isEmpty())
            why = t.error;
        if (r->error() == QNetworkReply::OperationCanceledError)
            why = tr("No answer from %1 in %2 s").arg(m_settings.endpoint).arg(m_settings.timeoutSec);
        // No HTTP answer at all (refused, no such host, timed out, closed):
        // the provider is down, not merely unhappy with the question.
        if (!r->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid())
        {
            m_unreachable = tr("Can't reach %1: %2").arg(m_settings.endpoint, why);
            emit availabilityChanged();
        }
        finish(false, why);
        return;
    }
    if (!m_unreachable.isEmpty())
    {
        m_unreachable.clear();
        emit availabilityChanged();
    }
    const AiTurn turn = m_provider->parseChat(r->readAll());
    if (!turn.ok)
    {
        finish(false, turn.error);
        return;
    }
    if (turn.calls.isEmpty())
    {
        finishWithProposal(turn.text);
        return;
    }

    // Every call must be an operation of the registry the model is offered
    // (read-only or a change; never the raw `command`); one that isn't is
    // refused and the reply is shown as plain text.
    QList<AiToolCall> changes;
    for (const AiToolCall &c : turn.calls)
    {
        const OperationRegistry::Operation *op = OperationRegistry::operation(c.name);
        if (op == nullptr || op->name == QLatin1String("command"))
            m_answer.rejected << QStringLiteral("%1 %2").arg(c.name,
                                     QString::fromUtf8(QJsonDocument(c.args).toJson(QJsonDocument::Compact)));
        else if (!op->readOnly)
            changes << c;
    }
    if (!m_answer.rejected.isEmpty())
    {
        finish(false, turn.text.isEmpty() ? tr("The assistant asked for something AI Assist can't do here.")
                                          : turn.text);
        return;
    }
    if (++m_round > MaxRounds)
    {
        if (!m_answer.proposal.isEmpty())
            finishWithProposal(tr("Stopped after %1 rounds; this is what was proposed so far.").arg(MaxRounds));
        else
            finish(false, tr("Stopped after %1 rounds of questions to the console.").arg(MaxRounds));
        return;
    }

    AiMessage said;
    said.role = AiMessage::Assistant;
    said.text = turn.text;
    said.calls = turn.calls;
    OperationRegistry reg(m_doc);

    if (!changes.isEmpty())
    {
        // Changes are proposals: checked against their schemas (and what
        // validate() can tell) now, run only when the operator applies
        // them. The model hears each was proposed, not run, and goes on
        // until it answers in text (a model that sends one step a reply
        // builds the plan over several). One that doesn't fit is refused
        // with why -- with the rest of its reply -- and runs nothing.
        QHash<QString, QString> errors;
        for (const AiToolCall &c : changes)
        {
            const QString why = reg.check(c.name, c.args);
            if (!why.isEmpty())
                errors.insert(c.id, why);
        }
        if (errors.isEmpty())
            for (const AiToolCall &c : changes)
            {
                const OperationRegistry::Operation *op = OperationRegistry::operation(c.name);
                m_answer.proposal << Step { c.name, c.args, op->cliSyntax(c.args), op->describe(c.args) };
            }
        m_messages << said;
        for (const AiToolCall &c : turn.calls)
        {
            const OperationRegistry::Operation *op = OperationRegistry::operation(c.name);
            AiMessage result;
            result.role = AiMessage::Tool;
            result.toolCallId = c.id;
            result.toolName = c.name;
            if (!errors.isEmpty())
                result.text = errors.contains(c.id)
                                  ? tr("error: %1. Nothing in this reply was taken; send it again, corrected.").arg(errors.value(c.id))
                                  : tr("not taken: another call in this reply was refused.");
            else if (op->readOnly)
                result.text = tr("not run: ask questions before proposing changes.");
            else
                result.text = tr("proposed as step %1 (not run yet: the operator applies every step together, as "
                                 "one undo step). Send any further steps, or answer in text when the change is "
                                 "complete.").arg(m_answer.proposal.count());
            m_messages << result;
        }
        sendRound();
        return;
    }

    m_messages << said;
    for (const AiToolCall &c : turn.calls)
    {
        const OperationRegistry::Operation *op = OperationRegistry::operation(c.name);
        const OperationRegistry::Result res = reg.run(c.name, c.args, m_selection);
        AiMessage result;
        result.role = AiMessage::Tool;
        result.toolCallId = c.id;
        result.toolName = c.name;
        if (res.ok)
        {
            result.text = res.message + QLatin1Char('\n') + res.text;
            const QString cmd = op->cliSyntax ? op->cliSyntax(c.args) : c.name;
            if (!m_answer.commands.contains(cmd))
                m_answer.commands << cmd;
        }
        else
            result.text = tr("error: %1").arg(res.message);
        m_messages << result;
    }
    sendRound();
}

void AiAssist::finishWithProposal(const QString &text)
{
    if (m_answer.proposal.isEmpty())
    {
        finish(true, text);
        return;
    }
    m_proposal = m_answer.proposal;
    m_proposalSelection = m_selection;
    m_proposalQuestion = m_answer.question;
    finish(true, text.trimmed().isEmpty() ? tr("Proposed %n change(s).", "", m_proposal.count()) : text.trimmed());
    emit proposalChanged();
}

void AiAssist::finish(bool ok, const QString &text)
{
    m_answer.ok = ok;
    m_answer.text = text;
    const Answer a = m_answer;
    m_provider.reset();
    m_messages.clear();
    emit busyChanged(false);
    emit answered(a);
}

void AiAssist::cancel()
{
    if (m_reply == nullptr)
        return;
    QNetworkReply *r = m_reply;
    m_reply = nullptr;
    r->disconnect(this);
    r->abort();
    r->deleteLater();
    finish(false, tr("Cancelled"));
}

bool AiAssist::apply(const QList<int> &included, OperationRegistry *registry, ShowTreeCommand::Result *result)
{
    ShowTreeCommand::Result out;
    out.context = m_proposalSelection;
    auto refuse = [&out, result](const QString &why) {
        out.ok = false;
        out.message = why;
        if (result != nullptr)
            *result = out;
        return false;
    };
    if (m_proposal.isEmpty())
        return refuse(tr("Nothing proposed"));
    // Never in Operate: refused before any AI path runs (and the guard
    // would say so if one did).
    if (m_operate || m_doc->mode() == Doc::Operate)
        return refuse(tr("AI Assist is unavailable in Operate mode"));
    if (!m_settings.enabled)
        return refuse(tr("AI Assist is off (Settings ▸ AI Assist)"));
    if (!checkGuard("apply"))
        return refuse(tr("AI Assist is unavailable in Operate mode"));

    QList<OperationRegistry::Call> calls;
    for (int i = 0; i < m_proposal.count(); ++i)
        if (included.isEmpty() || included.contains(i))
            calls << OperationRegistry::Call { m_proposal.at(i).name, m_proposal.at(i).args };
    if (calls.isEmpty())
        return refuse(tr("No step is included"));

    std::unique_ptr<OperationRegistry> own;
    if (registry == nullptr)
    {
        if (!m_commander && m_doc->showTree() != nullptr)
            m_commander.reset(new ShowTreeCommand(m_doc, m_doc->showTree()));
        own.reset(new OperationRegistry(m_doc, m_commander.get()));
        registry = own.get();
    }
    QString label = m_proposalQuestion.simplified();
    if (label.size() > 40)
        label = label.left(39) + QChar(0x2026);
    const OperationRegistry::Result r = registry->runPlan(calls, m_proposalSelection, tr("AI Assist: %1").arg(label));
    if (!r.ok)
        return refuse(r.message);
    out = r.command;
    out.ok = true;
    out.message = r.message;
    if (result != nullptr)
        *result = out;
    m_proposal.clear();
    emit proposalChanged();
    return true;
}

void AiAssist::discard()
{
    if (m_proposal.isEmpty())
        return;
    m_proposal.clear();
    emit proposalChanged();
}

bool AiAssist::testConnection(const AiSettings &settings, const QString &apiKey)
{
    if (!settings.enabled || m_operate || m_doc->mode() == Doc::Operate)
        return false;
    if (!checkGuard("testConnection"))
        return false;
    if (m_testReply != nullptr)
    {
        m_testReply->disconnect(this);
        m_testReply->abort();
        m_testReply->deleteLater();
    }
    std::shared_ptr<AiProvider> p(AiProvider::create(settings, apiKey).release());
    QNetworkReply *r = network()->get(p->modelsRequest());
    AiResources::track(r, QStringLiteral("connection test"));
    m_testReply = r;
    connect(r, &QNetworkReply::finished, this, [this, r, p, settings]() {
        if (r != m_testReply)
            return;
        m_testReply = nullptr;
        r->deleteLater();
        QString error;
        QStringList models;
        if (r->error() != QNetworkReply::NoError)
        {
            const QByteArray body = r->readAll();
            p->parseModels(body, &error);
            if (error.isEmpty() || error == AiSettings::tr("The server's answer isn't JSON"))
                error = r->errorString();
        }
        else
            models = p->parseModels(r->readAll(), &error);
        const bool ok = error.isEmpty();
        if (settings.endpoint == m_settings.endpoint && settings.provider == m_settings.provider)
        {
            const QString was = m_unreachable;
            m_unreachable = ok ? QString() : tr("Can't reach %1: %2").arg(settings.endpoint, error);
            if (was != m_unreachable)
                emit availabilityChanged();
        }
        QString msg;
        if (!ok)
            msg = tr("Can't reach %1: %2").arg(settings.endpoint, error);
        else if (!settings.model.isEmpty() && !models.contains(settings.model))
            msg = tr("Connected (%n model(s)); \"%1\" isn't one of them", "", models.count()).arg(settings.model);
        else
            msg = tr("Connected: %n model(s)", "", models.count());
        emit connectionTested(ok, models, msg);
    });
    return true;
}

/****************************************************************************
 * Design / Operate (section 5a)
 ****************************************************************************/

void AiAssist::abortAll()
{
    const bool wasBusy = m_reply != nullptr;
    if (m_reply != nullptr)
    {
        QNetworkReply *r = m_reply;
        m_reply = nullptr;
        r->disconnect(this);
        r->abort();
    }
    if (m_testReply != nullptr)
    {
        QNetworkReply *r = m_testReply;
        m_testReply = nullptr;
        r->disconnect(this);
        r->abort();
    }
    // The replies are the manager's children: they go with it, now.
    delete m_nam;
    m_nam = nullptr;
    m_provider.reset();
    m_messages.clear();
    // A proposal never outlives Design (nor AI Assist being on).
    discard();
    m_commander.reset();
    if (wasBusy)
    {
        m_answer.ok = false;
        m_answer.text = tr("Cancelled: the console went to Operate");
        emit busyChanged(false);
        emit answered(m_answer);
    }
}

void AiAssist::slotModeChanged(Doc::Mode mode)
{
    if (mode == Doc::Operate)
    {
        if (m_operate)
            return;
        m_operate = true;
        // 1-2: the entry points go, everything in flight stops, the
        // network manager is destroyed -- synchronous and quick.
        emit availabilityChanged();
        abortAll();
        // 3-4 after the switch has returned: never delays it.
        QTimer::singleShot(0, this, &AiAssist::teardown);
        return;
    }
    if (!m_operate)
        return;
    m_operate = false;
    m_usedModel = false;
    m_settings = AiSettings::load();
    if (!m_resourceError.isEmpty())
    {
        m_resourceError.clear();
        emit resourceErrorCleared();
    }
    emit availabilityChanged();
}

void AiAssist::teardown()
{
    if (!m_operate)
        return;
    // 3: a local Ollama this session talked to unloads its model. On
    // another machine nothing here holds it: skip, audit what is ours.
    if (m_usedModel && m_settings.isLocalOllama())
    {
        new AiUnloader(m_settings, [this](bool resident, const QString &problem) {
            audit(resident, problem);
        });
        return;
    }
    audit(false, QString());
}

void AiAssist::audit(bool modelResident, const QString &extra)
{
    m_usedModel = false;
    QStringList issues;
    const QStringList live = AiResources::live();
    if (!live.isEmpty())
    {
        issues << tr("still alive: %1").arg(live.join(QStringLiteral(", ")));
        AiResources::destroyAll();
        const QStringList after = AiResources::live();
        if (!after.isEmpty())
            issues << tr("couldn't stop: %1").arg(after.join(QStringLiteral(", ")));
    }
    if (modelResident)
        issues << tr("the model %1 is still loaded in Ollama").arg(m_settings.model);
    if (!extra.isEmpty())
        issues << extra;
    if (issues.isEmpty())
    {
        emit teardownFinished(true);
        return;
    }
    m_resourceError = tr("AI Assist resources still active in Operate mode: %1").arg(issues.join(QStringLiteral("; ")));
    qWarning().noquote() << m_resourceError;
    emit resourceError(m_resourceError);
    emit teardownFinished(false);
}
