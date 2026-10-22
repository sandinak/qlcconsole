/*
  Q Light Controller
  eosimportwizard.cpp

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

#include <QDialogButtonBox>
#include <QTableWidget>
#include <QListWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QLabel>

#include "eosimportwizard.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "doc.h"

#define KColType      0
#define KColChannels  1
#define KColUse       2
#define KColStatus    3

namespace
{

/** "Other…": any definition with a mode of @a footprint channels. */
class DefinitionPicker final : public QDialog
{
public:
    DefinitionPicker(QWidget *parent, QLCFixtureDefCache *cache, int footprint, const QString &hint)
        : QDialog(parent)
        , m_cache(cache)
        , m_footprint(footprint)
    {
        setWindowTitle(tr("Choose a fixture definition"));
        resize(520, 480);
        QVBoxLayout *lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(tr("The Eos type uses %n DMX channel(s); only modes of that size keep "
                                     "the patch's addresses.", "", footprint), this));
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText(tr("Search manufacturer or model"));
        m_filter->setClearButtonEnabled(true);
        lay->addWidget(m_filter);
        m_list = new QListWidget(this);
        lay->addWidget(m_list, 1);
        m_modes = new QComboBox(this);
        lay->addWidget(m_modes);
        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        lay->addWidget(m_buttons);
        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        QStringList makers = m_cache->manufacturers();
        makers.sort(Qt::CaseInsensitive);
        for (const QString &m : makers)
        {
            QStringList models = m_cache->models(m);
            models.sort(Qt::CaseInsensitive);
            for (const QString &model : models)
            {
                QListWidgetItem *it = new QListWidgetItem(QStringLiteral("%1 — %2").arg(m, model), m_list);
                it->setData(Qt::UserRole, m);
                it->setData(Qt::UserRole + 1, model);
            }
        }
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &t) {
            const QStringList words = t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (int i = 0; i < m_list->count(); ++i)
            {
                bool show = true;
                for (const QString &w : words)
                    show = show && m_list->item(i)->text().contains(w, Qt::CaseInsensitive);
                m_list->item(i)->setHidden(!show);
            }
        });
        connect(m_list, &QListWidget::currentItemChanged, this, [this]() { fillModes(); });
        m_filter->setText(hint);
        fillModes();
    }

    EosFixtureChoice choice() const
    {
        EosFixtureChoice c;
        QListWidgetItem *it = m_list->currentItem();
        if (it == nullptr || m_modes->currentData().toString().isEmpty())
            return c;
        c.source = EosFixtureChoice::Saved;
        c.manufacturer = it->data(Qt::UserRole).toString();
        c.model = it->data(Qt::UserRole + 1).toString();
        c.mode = m_modes->currentData().toString();
        return c;
    }

private:
    void fillModes()
    {
        m_modes->clear();
        QListWidgetItem *it = m_list->currentItem();
        QLCFixtureDef *def = it == nullptr ? nullptr
                             : m_cache->fixtureDef(it->data(Qt::UserRole).toString(),
                                                   it->data(Qt::UserRole + 1).toString());
        if (def != nullptr)
            for (QLCFixtureMode *mode : def->modes())
                if (int(mode->channels().count()) == m_footprint)
                    m_modes->addItem(QStringLiteral("%1 (%2 ch)").arg(mode->name()).arg(m_footprint), mode->name());
        if (m_modes->count() == 0)
            m_modes->addItem(def == nullptr ? tr("Pick a model above")
                                            : tr("No mode of %n channel(s) in this definition", "", m_footprint),
                             QString());
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_modes->currentData().toString().isEmpty());
    }

    QLCFixtureDefCache *m_cache;
    int m_footprint;
    QLineEdit *m_filter;
    QListWidget *m_list;
    QComboBox *m_modes;
    QDialogButtonBox *m_buttons;
};

QString sourceText(const EosFixtureChoice &c)
{
    switch (c.source)
    {
        case EosFixtureChoice::Saved:     return QObject::tr("Your choice");
        case EosFixtureChoice::Exact:     return QObject::tr("Matched");
        case EosFixtureChoice::Suggested: return QObject::tr("Close match — check it");
        default:                          return QObject::tr("No match");
    }
}

bool sameChoice(const EosFixtureChoice &a, const EosFixtureChoice &b)
{
    return a.manufacturer == b.manufacturer && a.model == b.model && a.mode == b.mode;
}

} // namespace

EosImportWizard::EosImportWizard(QWidget *parent, Doc *doc, const QString &asciiPath)
    : QWizard(parent)
    , m_doc(doc)
    , m_mapper(doc->fixtureDefCache())
{
    setWindowTitle(tr("Import an Eos show"));
    setWizardStyle(QWizard::ClassicStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    resize(760, 560);
    m_mapper.loadMap(EosFixtureMapper::defaultMapPath());

    addPage(createFilesPage());
    addPage(createTypesPage());
    m_asciiEdit->setText(asciiPath);
}

EosImportWizard::~EosImportWizard()
{
}

QWizardPage *EosImportWizard::createFilesPage()
{
    QWizardPage *page = new QWizardPage(this);
    page->setTitle(tr("The show's files"));
    page->setSubTitle(tr("On the Eos: File ▸ Export ▸ USITT ASCII, with Manufacturer Data on. That one file "
                         "carries the patch, groups, palettes, presets, subs and cue lists."));
    QVBoxLayout *lay = new QVBoxLayout(page);

    QFormLayout *form = new QFormLayout();
    QHBoxLayout *asc = new QHBoxLayout();
    m_asciiEdit = new QLineEdit(page);
    m_asciiEdit->setPlaceholderText(tr("The .asc export (required)"));
    QPushButton *browse = new QPushButton(tr("Browse…"), page);
    asc->addWidget(m_asciiEdit, 1);
    asc->addWidget(browse);
    form->addRow(tr("USITT ASCII:"), asc);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString fn = QFileDialog::getOpenFileName(this, tr("Eos USITT ASCII export"),
                                                        QFileInfo(m_asciiEdit->text()).absolutePath(),
                                                        tr("USITT ASCII (*.asc *.ASC);;All files (*)"));
        if (!fn.isEmpty())
            m_asciiEdit->setText(fn);
    });

    QVBoxLayout *sheets = new QVBoxLayout();
    m_sheetList = new QListWidget(page);
    m_sheetList->setMaximumHeight(90);
    QHBoxLayout *sheetButtons = new QHBoxLayout();
    QPushButton *addSheet = new QPushButton(tr("Add…"), page);
    QPushButton *removeSheet = new QPushButton(tr("Remove"), page);
    sheetButtons->addWidget(addSheet);
    sheetButtons->addWidget(removeSheet);
    sheetButtons->addStretch(1);
    sheets->addWidget(m_sheetList);
    sheets->addLayout(sheetButtons);
    form->addRow(tr("Magic sheets:"), sheets);
    connect(addSheet, &QPushButton::clicked, this, [this]() {
        const QStringList fns = QFileDialog::getOpenFileNames(this, tr("Magic sheet XML exports"),
                                                              QFileInfo(m_asciiEdit->text()).absolutePath(),
                                                              tr("Magic sheet XML (*.xml);;All files (*)"));
        for (const QString &fn : fns)
            if (m_sheetList->findItems(fn, Qt::MatchExactly).isEmpty())
                m_sheetList->addItem(fn);
    });
    connect(removeSheet, &QPushButton::clicked, this, [this]() { delete m_sheetList->currentItem(); });

    QHBoxLayout *esf = new QHBoxLayout();
    m_esfEdit = new QLineEdit(page);
    m_esfEdit->setPlaceholderText(tr("Optional .esf3d show file, for Augment3d positions"));
    QPushButton *browseEsf = new QPushButton(tr("Browse…"), page);
    esf->addWidget(m_esfEdit, 1);
    esf->addWidget(browseEsf);
    form->addRow(tr("Augment3d:"), esf);
    connect(browseEsf, &QPushButton::clicked, this, [this]() {
        const QString fn = QFileDialog::getOpenFileName(this, tr("Eos show file"),
                                                        QFileInfo(m_asciiEdit->text()).absolutePath(),
                                                        tr("Eos show file (*.esf3d);;All files (*)"));
        if (!fn.isEmpty())
            m_esfEdit->setText(fn);
    });
    lay->addLayout(form);

    QLabel *note = new QLabel(tr("Magic sheets (Magic Sheet editor ▸ background settings ▸ export, one XML "
                                 "each) put the channels on the 2D map, a layer per sheet, in the sheet's own "
                                 "layout: move and scale the layer to the plot. Augment3d positions are not "
                                 "read yet."), page);
    note->setWordWrap(true);
    lay->addWidget(note);
    lay->addStretch(1);
    return page;
}

QWizardPage *EosImportWizard::createTypesPage()
{
    QWizardPage *page = new QWizardPage(this);
    page->setTitle(tr("Fixture types"));
    page->setSubTitle(tr("Each Eos fixture type, and the fixture definition to patch it as. A close match is "
                         "only a suggestion: check it, or choose Other…"));
    QVBoxLayout *lay = new QVBoxLayout(page);
    m_summary = new QLabel(page);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_summary);

    m_types = new QTableWidget(0, 4, page);
    m_types->setHorizontalHeaderLabels(QStringList() << tr("Eos type") << tr("Channels")
                                                     << tr("Patch as") << tr(""));
    m_types->horizontalHeader()->setSectionResizeMode(KColType, QHeaderView::ResizeToContents);
    m_types->horizontalHeader()->setSectionResizeMode(KColChannels, QHeaderView::ResizeToContents);
    m_types->horizontalHeader()->setSectionResizeMode(KColUse, QHeaderView::Stretch);
    m_types->horizontalHeader()->setSectionResizeMode(KColStatus, QHeaderView::ResizeToContents);
    m_types->verticalHeader()->setVisible(false);
    m_types->setSelectionMode(QAbstractItemView::NoSelection);
    lay->addWidget(m_types, 1);

    m_remember = new QCheckBox(tr("Remember these choices for the next Eos import"), page);
    m_remember->setChecked(true);
    lay->addWidget(m_remember);
    m_showTree = new QCheckBox(tr("Bring the cue lists into the Show Tree as tracking cue lists"), page);
    m_showTree->setChecked(true);
    lay->addWidget(m_showTree);
    return page;
}

bool EosImportWizard::readAscii(QString *error)
{
    const QString path = m_asciiEdit->text().trimmed();
    if (path.isEmpty())
    {
        if (error != nullptr)
            *error = tr("Choose the show's USITT ASCII export (.asc).");
        return false;
    }
    if (path == m_readPath)
        return true;
    EosShowData data;
    EosAsciiParser parser;
    if (!parser.parse(path, data))
    {
        if (error != nullptr)
            *error = data.warnings.join(QStringLiteral("\n"));
        return false;
    }
    if (data.patch.isEmpty() && data.cueCount() == 0)
    {
        if (error != nullptr)
            *error = tr("%1 has no patch and no cues: is it an Eos USITT ASCII export?")
                     .arg(QFileInfo(path).fileName());
        return false;
    }
    m_data = data;
    m_readPath = path;
    fillTypes();
    return true;
}

bool EosImportWizard::validateCurrentPage()
{
    if (currentId() == 0)
    {
        QString err;
        if (!readAscii(&err))
        {
            QMessageBox::warning(this, windowTitle(), err);
            return false;
        }
    }
    return QWizard::validateCurrentPage();
}

QString EosImportWizard::summary() const
{
    QStringList parts;
    parts << tr("%n channel(s)", "", int(m_data.patch.count()));
    parts << tr("%n fixture type(s)", "", int(m_data.personalities.count()));
    parts << tr("%n group(s)", "", int(m_data.groups.count()));
    parts << tr("%n palette(s)", "", int(m_data.palettes.count()));
    if (!m_data.presets.isEmpty())
        parts << tr("%n preset(s)", "", int(m_data.presets.count()));
    if (!m_data.subs.isEmpty())
        parts << tr("%n sub(s)", "", int(m_data.subs.count()));
    parts << tr("%n cue list(s) of %1 cue(s)", "", int(m_data.cueLists.count())).arg(m_data.cueCount());
    QString text = tr("<b>%1</b>: %2.").arg((m_data.title.isEmpty() ? QFileInfo(m_readPath).completeBaseName()
                                                                     : m_data.title).toHtmlEscaped(),
                                            parts.join(QStringLiteral(", ")));
    if (!m_data.skipped.isEmpty())
    {
        QStringList sk;
        for (auto it = m_data.skipped.constBegin(); it != m_data.skipped.constEnd(); ++it)
            sk << QStringLiteral("%1 × %2").arg(it.key()).arg(it.value());
        text += QStringLiteral("<br>") + tr("Not imported: %1.").arg(sk.join(QStringLiteral(", ")).toHtmlEscaped());
    }
    return text;
}

void EosImportWizard::fillTypes()
{
    m_summary->setText(summary());
    QHash<int, int> uses;
    for (const EosPatch &p : m_data.patch)
        uses[p.personality]++;

    m_types->setRowCount(0);
    m_rowKeys.clear();
    m_rowChoices.clear();
    for (const EosPersonality &p : m_data.personalities)
    {
        if (!uses.contains(p.id))
            continue;
        const int row = m_types->rowCount();
        m_types->insertRow(row);
        QTableWidgetItem *type = new QTableWidgetItem(QStringLiteral("%1 %2").arg(p.manufacturer, p.model));
        type->setToolTip(tr("Footprint %n channel(s)", "", qMax(1, p.footprint)));
        m_types->setItem(row, KColType, type);
        m_types->setItem(row, KColChannels, new QTableWidgetItem(QString::number(uses.value(p.id))));
        m_rowKeys << EosFixtureMapper::key(p);
        m_rowChoices << QList<EosFixtureChoice>();
        fillRow(row, p, m_mapper.resolve(p));
    }
}

void EosImportWizard::fillRow(int row, const EosPersonality &p, const EosFixtureChoice &select)
{
    QList<EosFixtureChoice> choices;
    choices << select;
    for (const EosFixtureChoice &c : m_mapper.candidates(p, 6))
    {
        bool dup = false;
        for (const EosFixtureChoice &o : choices)
            dup = dup || sameChoice(o, c);
        if (!dup)
            choices << c;
    }
    if (!select.isGeneric())
        choices << EosFixtureChoice();
    m_rowChoices[row] = choices;

    QComboBox *combo = new QComboBox(m_types);
    const int footprint = qMax(1, p.footprint);
    for (const EosFixtureChoice &c : choices)
        combo->addItem(c.isGeneric() ? tr("Generic dimmer, %n channel(s)", "", footprint) : c.text());
    combo->addItem(tr("Other…"));
    combo->setCurrentIndex(0);
    m_types->setCellWidget(row, KColUse, combo);
    m_types->setItem(row, KColStatus, new QTableWidgetItem(sourceText(select)));

    connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, combo, row](int index) {
        if (index == combo->count() - 1)
        {
            pickOther(row);
            return;
        }
        m_types->item(row, KColStatus)->setText(index == 0 ? sourceText(m_rowChoices.at(row).first())
                                                           : tr("Your choice"));
    });
}

void EosImportWizard::pickOther(int row)
{
    const QString key = m_rowKeys.value(row);
    const EosPersonality *pers = nullptr;
    for (const EosPersonality &p : m_data.personalities)
        if (EosFixtureMapper::key(p) == key)
            pers = &p;
    if (pers == nullptr)
        return;
    const QString hint = EosFixtureMapper::words(pers->model).value(0);
    DefinitionPicker picker(this, m_doc->fixtureDefCache(), qMax(1, pers->footprint), hint);
    if (picker.exec() == QDialog::Accepted && !picker.choice().isGeneric())
        setRowChoice(row, picker.choice());
    else if (QComboBox *combo = qobject_cast<QComboBox *>(m_types->cellWidget(row, KColUse)))
        combo->setCurrentIndex(0);
}

EosFixtureChoice EosImportWizard::rowChoice(int row) const
{
    QComboBox *combo = qobject_cast<QComboBox *>(m_types->cellWidget(row, KColUse));
    if (combo == nullptr || row >= m_rowChoices.count())
        return EosFixtureChoice();
    const int i = combo->currentIndex();
    if (i < 0 || i >= m_rowChoices.at(row).count())
        return m_rowChoices.at(row).value(0);
    return m_rowChoices.at(row).at(i);
}

void EosImportWizard::setRowChoice(int row, const EosFixtureChoice &choice)
{
    QComboBox *combo = qobject_cast<QComboBox *>(m_types->cellWidget(row, KColUse));
    if (combo == nullptr)
        return;
    QList<EosFixtureChoice> &choices = m_rowChoices[row];
    int at = -1;
    for (int i = 0; i < choices.count(); ++i)
        if (sameChoice(choices.at(i), choice))
            at = i;
    if (at < 0)
    {
        at = int(choices.count());
        choices << choice;
        combo->insertItem(at, choice.isGeneric() ? tr("Generic dimmer") : choice.text());
    }
    combo->setCurrentIndex(at);
    m_types->item(row, KColStatus)->setText(tr("Your choice"));
}

void EosImportWizard::applyChoices()
{
    for (int row = 0; row < m_rowKeys.count(); ++row)
    {
        EosFixtureChoice c = rowChoice(row);
        // What the user leaves on a close match is now theirs; a generic
        // dimmer forgets any saved choice.
        m_mapper.setChoice(m_rowKeys.at(row), c);
    }
    if (m_remember->isChecked())
        m_mapper.saveMap(EosFixtureMapper::defaultMapPath());
}

void EosImportWizard::accept()
{
    applyChoices();
    QWizard::accept();
}

EosImportBundle EosImportWizard::bundle() const
{
    EosImportBundle b;
    b.asciiPath = m_asciiEdit->text().trimmed();
    for (int i = 0; i < m_sheetList->count(); ++i)
        b.magicSheetPaths << m_sheetList->item(i)->text();
    b.augment3dEsfPath = m_esfEdit->text().trimmed();
    b.fixtureMapPath = EosFixtureMapper::defaultMapPath();
    return b;
}

bool EosImportWizard::toShowTree() const
{
    return m_showTree->isChecked();
}

void EosImportWizard::setRemember(bool on)
{
    m_remember->setChecked(on);
}
