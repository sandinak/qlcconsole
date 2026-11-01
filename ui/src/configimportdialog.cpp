/*
  Q Light Controller Plus - qlcconsole
  configimportdialog.cpp

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

#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QJsonArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QSpinBox>
#include <QLabel>

#include "configimportdialog.h"
#include "inputoutputmap.h"
#include "doc.h"

ConfigImportDialog::ConfigImportDialog(QWidget *parent, Doc *doc, const QString &file)
    : QDialog(parent)
    , m_doc(doc)
    , m_file(file)
{
    setObjectName(QStringLiteral("configImportDialog"));
    setWindowTitle(tr("Import universes and I/O setup"));
    resize(720, 640);
    m_source = ConfigImport::readSource(file);

    QVBoxLayout *main = new QVBoxLayout(this);
    QLabel *head = new QLabel(m_source.ok
        ? tr("<b>%1</b>: %n universe(s). Takes the universes' names and their output, input (with its input "
             "profile) and feedback patches. Fixtures, groups and palettes come in with File ▸ Import ▸ "
             "Workspace. Nothing changes until Import; it is one undo step.", "", m_source.universes.count())
              .arg(QFileInfo(file).fileName().toHtmlEscaped())
        : m_source.error, this);
    head->setWordWrap(true);
    main->addWidget(head);

    // What.
    QHBoxLayout *what = new QHBoxLayout;
    const struct { const char *key; const char *text; } names[] = {
        { "universes", QT_TR_NOOP("Universe names") }, { "outputs", QT_TR_NOOP("Outputs") },
        { "inputs", QT_TR_NOOP("Inputs and profiles") }, { "feedback", QT_TR_NOOP("Feedback") } };
    for (const auto &n : names)
    {
        QCheckBox *c = new QCheckBox(tr(n.text), this);
        c->setObjectName(QStringLiteral("configSection_") + QLatin1String(n.key));
        c->setChecked(true);
        c->setProperty("section", QLatin1String(n.key));
        connect(c, &QCheckBox::toggled, this, &ConfigImportDialog::refresh);
        m_sections << c;
        what->addWidget(c);
    }
    what->addStretch();
    main->addLayout(what);

    // Which universes, and where they go.
    m_table = new QTableWidget(m_source.universes.count(), 3, this);
    m_table->setObjectName(QStringLiteral("configUniverses"));
    m_table->setHorizontalHeaderLabels(QStringList { tr("Import"), tr("Universe in the file"), tr("Goes on universe here") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setMaximumHeight(150);
    for (int i = 0; i < m_source.universes.count(); ++i)
    {
        const ConfigImport::SourceUniverse &u = m_source.universes.at(i);
        QCheckBox *use = new QCheckBox(m_table);
        use->setChecked(true);
        connect(use, &QCheckBox::toggled, this, &ConfigImportDialog::refresh);
        m_table->setCellWidget(i, 0, use);
        m_table->setItem(i, 1, new QTableWidgetItem(tr("%1  %2").arg(u.number).arg(u.name)));
        QSpinBox *to = new QSpinBox(m_table);
        to->setRange(1, int(InputOutputMap::maxUniverses()));
        to->setValue(u.number);
        connect(to, QOverload<int>::of(&QSpinBox::valueChanged), this, &ConfigImportDialog::refresh);
        m_table->setCellWidget(i, 2, to);
    }
    main->addWidget(m_table);

    // Mapping and choices.
    QFormLayout *form = new QFormLayout;
    m_addresses = new QPlainTextEdit(this);
    m_addresses->setObjectName(QStringLiteral("configAddresses"));
    m_addresses->setPlaceholderText(tr("2.0.0.50 = 2.0.0.60   (one per line: network address in the file = address here)"));
    m_addresses->setMaximumHeight(52);
    connect(m_addresses, &QPlainTextEdit::textChanged, this, &ConfigImportDialog::refresh);
    form->addRow(tr("Network addresses"), m_addresses);
    m_plugins = new QPlainTextEdit(this);
    m_plugins->setObjectName(QStringLiteral("configPlugins"));
    m_plugins->setPlaceholderText(tr("Old Plugin = Plugin here   (one per line, for a plugin that isn't installed)"));
    m_plugins->setMaximumHeight(40);
    connect(m_plugins, &QPlainTextEdit::textChanged, this, &ConfigImportDialog::refresh);
    form->addRow(tr("Plugins"), m_plugins);
    m_collision = new QComboBox(this);
    m_collision->setObjectName(QStringLiteral("configCollision"));
    m_collision->addItem(tr("Choose…"), QString());
    m_collision->addItem(tr("Skip it (keep the one here)"), QStringLiteral("skip"));
    m_collision->addItem(tr("Replace it"), QStringLiteral("replace"));
    m_collision->addItem(tr("Renumber (put the file's on a new universe)"), QStringLiteral("renumber"));
    connect(m_collision, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigImportDialog::refresh);
    form->addRow(tr("A universe here already has a setup"), m_collision);
    m_missing = new QComboBox(this);
    m_missing->setObjectName(QStringLiteral("configMissing"));
    m_missing->addItem(tr("Choose…"), QString());
    m_missing->addItem(tr("Skip those patches"), QStringLiteral("skip"));
    m_missing->addItem(tr("Keep them, offline"), QStringLiteral("keep"));
    connect(m_missing, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigImportDialog::refresh);
    form->addRow(tr("A plugin isn't available here"), m_missing);
    main->addLayout(form);

    m_preview = new QPlainTextEdit(this);
    m_preview->setObjectName(QStringLiteral("configPreview"));
    m_preview->setReadOnly(true);
    main->addWidget(m_preview, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_import = box->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    m_import->setObjectName(QStringLiteral("configImport"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(box);

    refresh();
}

QMap<QString, QString> ConfigImportDialog::parseLines(const QString &text)
{
    QMap<QString, QString> out;
    for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString from = line.left(eq).trimmed();
        const QString to = line.mid(eq + 1).trimmed();
        if (!from.isEmpty() && !to.isEmpty())
            out.insert(from, to);
    }
    return out;
}

QJsonObject ConfigImportDialog::arguments() const
{
    ConfigImport::Options o;
    o.file = m_file;
    QStringList sections;
    for (QCheckBox *c : m_sections)
        if (c->isChecked())
            sections << c->property("section").toString();
    if (sections.count() != m_sections.count())
        o.sections = sections;
    bool all = true;
    for (int i = 0; i < m_source.universes.count(); ++i)
    {
        QCheckBox *use = qobject_cast<QCheckBox *>(m_table->cellWidget(i, 0));
        QSpinBox *to = qobject_cast<QSpinBox *>(m_table->cellWidget(i, 2));
        const int number = m_source.universes.at(i).number;
        if (use != nullptr && !use->isChecked())
        {
            all = false;
            continue;
        }
        o.universes << number;
        if (to != nullptr && to->value() != number)
            o.universeMap.insert(number, to->value());
    }
    if (all)
        o.universes.clear();
    o.addressMap = parseLines(m_addresses->toPlainText());
    o.pluginMap = parseLines(m_plugins->toPlainText());
    o.onCollision = m_collision->currentData().toString();
    o.onMissingPlugin = m_missing->currentData().toString();
    return ConfigImport::toArgs(o);
}

void ConfigImportDialog::refresh()
{
    if (m_preview == nullptr || m_import == nullptr)
        return;
    if (!m_source.ok)
    {
        m_preview->setPlainText(m_source.error);
        m_import->setEnabled(false);
        return;
    }
    // Nothing picked at all is not "everything".
    bool any = false;
    for (int i = 0; i < m_source.universes.count(); ++i)
        if (QCheckBox *use = qobject_cast<QCheckBox *>(m_table->cellWidget(i, 0)))
            any = any || use->isChecked();
    bool sec = false;
    for (QCheckBox *c : m_sections)
        sec = sec || c->isChecked();
    if (!any || !sec)
    {
        m_plan = ConfigImport::Plan();
        m_preview->setPlainText(tr("Pick at least one universe and one section."));
        m_import->setEnabled(false);
        return;
    }
    ConfigImport::Options o;
    QString why;
    if (!ConfigImport::optionsFrom(arguments(), &o, &why))
    {
        m_preview->setPlainText(why);
        m_import->setEnabled(false);
        return;
    }
    m_plan = ConfigImport::plan(m_doc, o);
    m_preview->setPlainText(m_plan.text());
    m_import->setEnabled(m_plan.ok());
}
