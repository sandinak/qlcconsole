/*
  Q Light Controller Plus - qlcconsole
  aisettingspage.cpp

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

#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

#include "aisettingspage.h"
#include "mcpserver.h"
#include "aiassist.h"

AiSettingsPage::AiSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("aiSettingsPage");
    setProperty("helpTopic", QStringLiteral("ai-assist.md"));
    const AiSettings s = AiSettings::load();

    QVBoxLayout *v = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Ask questions about the show in plain words, from the Show Tree's command "
                                  "line (? and the question). The assistant answers by asking the console "
                                  "read-only questions; it changes nothing. Off by default; never in Operate."),
                               this);
    intro->setWordWrap(true);
    v->addWidget(intro);

    m_enabled = new QCheckBox(tr("Turn on AI Assist"), this);
    m_enabled->setObjectName("aiEnabled");
    m_enabled->setChecked(s.enabled);
    v->addWidget(m_enabled);

    QFormLayout *form = new QFormLayout();
    m_provider = new QComboBox(this);
    m_provider->setObjectName("aiProvider");
    for (const QString &p : AiSettings::providers())
        m_provider->addItem(AiSettings::providerName(p), p);
    m_provider->setCurrentIndex(qMax(0, m_provider->findData(s.provider)));
    m_lastProvider = s.provider;
    form->addRow(tr("Provider"), m_provider);

    m_endpoint = new QLineEdit(s.endpoint, this);
    m_endpoint->setObjectName("aiEndpoint");
    form->addRow(tr("Endpoint"), m_endpoint);

    m_model = new QComboBox(this);
    m_model->setObjectName("aiModel");
    m_model->setEditable(true);
    m_model->setEditText(s.model);
    m_model->setToolTip(tr("Free text; Test connection lists the models the server has."));
    form->addRow(tr("Model"), m_model);

    m_key = new QLineEdit(this);
    m_key->setObjectName("aiKey");
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setPlaceholderText(AiKeychain::canStore() ? tr("Kept in the keychain, never in the settings")
                                                     : tr("Set QLC_AI_API_KEY in the environment"));
    m_key->setEnabled(AiKeychain::canStore());
    connect(m_key, &QLineEdit::textEdited, this, [this]() { m_keyEdited = true; });
    m_keyLabel = new QLabel(tr("API key"), this);
    form->addRow(m_keyLabel, m_key);

    m_context = new QComboBox(this);
    m_context->setObjectName("aiContext");
    m_context->addItem(tr("A summary of the show (fixtures, groups, counts)"), QStringLiteral("summary"));
    m_context->addItem(tr("Nothing up front (only what a question asks for)"), QStringLiteral("none"));
    m_context->setCurrentIndex(qMax(0, m_context->findData(s.sendShowContext)));
    m_context->setToolTip(tr("Never the show file itself."));
    form->addRow(tr("Send with a question"), m_context);

    m_timeout = new QSpinBox(this);
    m_timeout->setObjectName("aiTimeout");
    m_timeout->setRange(5, 600);
    m_timeout->setSuffix(tr(" s"));
    m_timeout->setValue(s.timeoutSec);
    form->addRow(tr("Give up after"), m_timeout);
    v->addLayout(form);

    QHBoxLayout *testRow = new QHBoxLayout();
    m_test = new QPushButton(tr("Test connection"), this);
    m_test->setObjectName("aiTest");
    testRow->addWidget(m_test);
    m_testResult = new QLabel(this);
    m_testResult->setObjectName("aiTestResult");
    m_testResult->setWordWrap(true);
    testRow->addWidget(m_testResult, 1);
    v->addLayout(testRow);

    m_cloudNote = new QLabel(this);
    m_cloudNote->setObjectName("aiCloudNote");
    m_cloudNote->setWordWrap(true);
    m_cloudNote->setStyleSheet(QStringLiteral("QLabel { color: #b06000; }"));
    v->addWidget(m_cloudNote);

    // The MCP server (P3): AI tools outside qlcconsole, on this computer.
    m_mcp = new QCheckBox(tr("Let AI tools on this computer use the show (MCP server)"), this);
    m_mcp->setObjectName("aiMcp");
    m_mcp->setChecked(s.mcpEnabled);
    m_mcp->setToolTip(tr("Claude Code, Claude Desktop or a local agent can ask the console the same read-only "
                         "questions AI Assist asks. A local socket: no network port, this user only; closed in "
                         "Operate."));
    v->addWidget(m_mcp);
    m_mcpChanges = new QCheckBox(tr("…and propose changes (you apply them here, one undo step)"), this);
    m_mcpChanges->setObjectName("aiMcpChanges");
    m_mcpChanges->setChecked(s.mcpChanges);
    m_mcpChanges->setToolTip(tr("A change a tool asks for waits in the Show Tree's command line, like AI "
                                "Assist's own: nothing changes until you apply it."));
    v->addWidget(m_mcpChanges);
    m_mcpNote = new QLabel(this);
    m_mcpNote->setObjectName("aiMcpNote");
    m_mcpNote->setWordWrap(true);
    m_mcpNote->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(m_mcpNote);
    v->addStretch(1);
    connect(m_mcp, &QCheckBox::toggled, this, &AiSettingsPage::updateState);
    if (McpServer *mcp = McpServer::instance())
        connect(mcp, &McpServer::stateChanged, this, &AiSettingsPage::updateMcpNote);

    connect(m_enabled, &QCheckBox::toggled, this, &AiSettingsPage::updateState);
    connect(m_provider, &QComboBox::currentIndexChanged, this, &AiSettingsPage::slotProviderChanged);
    connect(m_endpoint, &QLineEdit::textChanged, this, &AiSettingsPage::updateState);
    connect(m_test, &QPushButton::clicked, this, &AiSettingsPage::slotTest);
    if (AiAssist *ai = AiAssist::instance())
        connect(ai, &AiAssist::connectionTested, this, [this](bool ok, const QStringList &models, const QString &msg) {
            m_testResult->setText(msg);
            m_testResult->setStyleSheet(ok ? QString() : QStringLiteral("QLabel { color: red; }"));
            const QString keep = m_model->currentText();
            m_model->clear();
            m_model->addItems(models);
            m_model->setEditText(keep);
            m_test->setEnabled(true);
        });
    updateState();
}

AiSettings AiSettingsPage::settings() const
{
    AiSettings s;
    s.enabled = m_enabled->isChecked();
    s.provider = m_provider->currentData().toString();
    s.endpoint = m_endpoint->text().trimmed();
    s.model = m_model->currentText().trimmed();
    s.sendShowContext = m_context->currentData().toString();
    s.timeoutSec = m_timeout->value();
    s.mcpEnabled = m_mcp->isChecked();
    s.mcpChanges = m_mcpChanges->isChecked();
    return s;
}

void AiSettingsPage::slotProviderChanged()
{
    const QString p = m_provider->currentData().toString();
    // The endpoint follows the provider while it is the old one's default.
    if (m_endpoint->text().trimmed().isEmpty() || m_endpoint->text().trimmed() == AiSettings::defaultEndpoint(m_lastProvider))
        m_endpoint->setText(AiSettings::defaultEndpoint(p));
    m_lastProvider = p;
    m_key->clear();
    m_keyEdited = false;
    updateState();
}

void AiSettingsPage::updateState()
{
    const AiSettings s = settings();
    const bool cloudKey = s.provider != QLatin1String("ollama");
    m_key->setVisible(cloudKey);
    m_keyLabel->setVisible(cloudKey);
    for (QWidget *w : { static_cast<QWidget *>(m_provider), static_cast<QWidget *>(m_endpoint),
                        static_cast<QWidget *>(m_model), static_cast<QWidget *>(m_context),
                        static_cast<QWidget *>(m_timeout) })
        w->setEnabled(s.enabled);
    m_test->setEnabled(s.enabled);
    m_test->setToolTip(s.enabled ? tr("Ask the server which models it has.")
                                 : tr("Turn AI Assist on first: off, nothing is sent."));
    if (s.sendsOffMachine())
        m_cloudNote->setText(tr("The model runs on another computer (%1): your questions and what the console "
                                "answers them with -- fixture names, addresses, cues -- leave this one.")
                                 .arg(QUrl(s.endpoint).host()));
    else
        m_cloudNote->setText(tr("The model runs on this computer: nothing leaves it."));
    m_mcp->setEnabled(s.enabled);
    m_mcpChanges->setEnabled(s.enabled && s.mcpEnabled);
    updateMcpNote();
}

void AiSettingsPage::updateMcpNote()
{
    const AiSettings s = settings();
    if (!s.enabled || !s.mcpEnabled)
    {
        m_mcpNote->setText(tr("Off: no AI tool outside qlcconsole can reach the show."));
        return;
    }
    QString text = tr("Set the tool up with the command <code>%1</code> -- for Claude Code: "
                      "<code>claude mcp add qlcconsole -- %1</code>.")
                       .arg(McpServer::clientCommand().toHtmlEscaped());
    const McpServer *mcp = McpServer::instance();
    const AiSettings saved = AiSettings::load();
    if (saved.enabled != s.enabled || saved.mcpEnabled != s.mcpEnabled)
        text += QLatin1Char(' ') + tr("Opens when you press OK.").toHtmlEscaped();
    else if (mcp != nullptr)
        text += QLatin1Char(' ') + mcp->statusText().toHtmlEscaped();
    m_mcpNote->setText(text);
}

void AiSettingsPage::slotTest()
{
    AiAssist *ai = AiAssist::instance();
    const AiSettings s = settings();
    QString key = m_key->text();
    if (key.isEmpty() && s.provider != QLatin1String("ollama"))
        key = AiKeychain::read(s.provider);
    if (ai == nullptr || !ai->testConnection(s, key))
    {
        m_testResult->setText(tr("Can't test now (off, or in Operate)."));
        return;
    }
    m_test->setEnabled(false);
    m_testResult->setStyleSheet(QString());
    m_testResult->setText(tr("Asking %1…").arg(s.endpoint));
}

void AiSettingsPage::apply()
{
    const AiSettings s = settings();
    s.save();
    if (m_keyEdited && s.provider != QLatin1String("ollama"))
        AiKeychain::write(s.provider, m_key->text());
    m_keyEdited = false;
    if (AiAssist *ai = AiAssist::instance())
        ai->settingsChanged();
}
