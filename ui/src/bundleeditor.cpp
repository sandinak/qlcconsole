/*
  Q Light Controller Plus
  bundleeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "bundleeditor.h"
#include "bundlecache.h"
#include "qlcpalette.h"
#include "doc.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDate>
#include <QMessageBox>
#include <QDebug>

// ─── helpers ─────────────────────────────────────────────────────────────────

static QString paletteTypeLabel(QLCPalette *p)
{
    switch (p->type())
    {
        case QLCPalette::Color:   return "Color";
        case QLCPalette::Dimmer:  return "Dimmer";
        case QLCPalette::Effect:  return "Effect";
        case QLCPalette::PanTilt: return "PanTilt";
        case QLCPalette::Beam:    return "Beam";
        default:                  return "Other";
    }
}

static BundleEntry entryFromPalette(QLCPalette *p)
{
    BundleEntry e;
    e.name = p->name();

    switch (p->type())
    {
        case QLCPalette::Color:
        {
            e.type  = "Color";
            e.color = p->rgbValue().name();
            QColor wauv = p->wauvValue();
            if (wauv.isValid() && wauv != Qt::black)
                e.wauv = wauv.name();
            break;
        }
        case QLCPalette::Dimmer:
            e.type         = "Dimmer";
            e.dimmerValue  = p->value().toInt();
            break;
        case QLCPalette::Effect:
            e.type         = "Effect";
            e.script       = p->scriptPath();
            e.effectPreset = p->effectPreset();
            e.params       = p->effectParamValues();
            break;
        case QLCPalette::PanTilt:
        {
            e.type = "PanTilt";
            QVariantList vals = p->values();
            e.pan  = vals.value(0).toDouble();
            e.tilt = vals.value(1).toDouble();
            break;
        }
        case QLCPalette::Beam:
        {
            e.type  = "Beam";
            QVariantList vals = p->values();
            e.focus = vals.value(0).toInt();
            e.frost = vals.value(1).toInt();
            e.iris  = vals.value(2).toInt();
            break;
        }
        default:
            e.type = "Other";
            break;
    }
    return e;
}

// ─── BundleEditor ────────────────────────────────────────────────────────────

BundleEditor::BundleEditor(Doc *doc, BundleCache *cache,
                           const QList<quint32> &sourcePalettes,
                           QWidget *parent)
    : QDialog(parent), m_doc(doc), m_cache(cache), m_editMode(false)
{
    setWindowTitle(tr("Save as Bundle…"));
    buildUi(false);
    populatePaletteList(sourcePalettes);
    updateContainsFromEntries();
}

BundleEditor::BundleEditor(Doc *doc, BundleCache *cache,
                           const QLCBundle &existing,
                           QWidget *parent)
    : QDialog(parent), m_doc(doc), m_cache(cache),
      m_bundle(existing), m_editMode(true)
{
    setWindowTitle(tr("Edit Bundle — %1").arg(existing.name));
    buildUi(true);

    m_nameEdit->setText(existing.name);
    m_descEdit->setPlainText(existing.description);
    m_authorEdit->setText(existing.author);
    m_keywordsEdit->setText(existing.keywords.join(", "));

    int catIdx = m_categoryCombo->findText(existing.attributes.category);
    if (catIdx >= 0) m_categoryCombo->setCurrentIndex(catIdx);

    int tempoIdx = m_tempoCombo->findText(existing.attributes.tempo,
                                          Qt::MatchFixedString);
    if (tempoIdx >= 0) m_tempoCombo->setCurrentIndex(tempoIdx);

    m_moodEdit->setText(existing.attributes.mood);

    populatePaletteList(existing.palettes);
}

BundleEditor::BundleEditor(Doc *doc, BundleCache *cache,
                           const QList<BundleEntry> &entries,
                           QWidget *parent)
    : QDialog(parent), m_doc(doc), m_cache(cache), m_editMode(false)
{
    setWindowTitle(tr("Save as Bundle…"));
    buildUi(false);
    m_bundle.palettes = entries;
    populatePaletteList(entries);
    updateContainsFromEntries();
}

void BundleEditor::buildUi(bool editMode)
{
    setMinimumWidth(480);
    auto *root = new QVBoxLayout(this);

    // Metadata form
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Required"));
    form->addRow(tr("Name:"), m_nameEdit);

    m_descEdit = new QTextEdit(this);
    m_descEdit->setFixedHeight(60);
    m_descEdit->setPlaceholderText(tr("One-paragraph description (optional)"));
    form->addRow(tr("Description:"), m_descEdit);

    m_authorEdit = new QLineEdit(this);
    form->addRow(tr("Author:"), m_authorEdit);

    m_keywordsEdit = new QLineEdit(this);
    m_keywordsEdit->setPlaceholderText(tr("sunset, warm, outdoor  (comma-separated)"));
    form->addRow(tr("Keywords:"), m_keywordsEdit);

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItems({ tr("Color"), tr("Dimmer"), tr("Position"),
                                tr("Beam"), tr("Mixed") });
    form->addRow(tr("Category:"), m_categoryCombo);

    m_tempoCombo = new QComboBox(this);
    m_tempoCombo->addItem(tr("—"),      QString());
    m_tempoCombo->addItem(tr("Slow"),   "slow");
    m_tempoCombo->addItem(tr("Medium"), "medium");
    m_tempoCombo->addItem(tr("Fast"),   "fast");
    form->addRow(tr("Tempo:"), m_tempoCombo);

    m_moodEdit = new QLineEdit(this);
    m_moodEdit->setPlaceholderText(tr("warm, dramatic, subtle… (optional)"));
    form->addRow(tr("Mood:"), m_moodEdit);

    root->addLayout(form);

    // Palette snapshot list
    auto *grp = new QGroupBox(tr("Palette snapshot"), this);
    auto *gl  = new QVBoxLayout(grp);
    m_paletteList = new QListWidget(grp);
    m_paletteList->setSelectionMode(QAbstractItemView::NoSelection);
    if (editMode)
    {
        m_paletteList->setEnabled(false);
        gl->addWidget(new QLabel(tr("(snapshot is fixed — re-save to update)"), grp));
    }
    gl->addWidget(m_paletteList);
    root->addWidget(grp);

    // Buttons
    auto *bbox = new QDialogButtonBox(this);
    m_okBtn = bbox->addButton(QDialogButtonBox::Ok);
    bbox->addButton(QDialogButtonBox::Cancel);
    m_okBtn->setEnabled(false);
    root->addWidget(bbox);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &BundleEditor::slotNameChanged);
    connect(bbox, &QDialogButtonBox::accepted, this, &BundleEditor::slotAccept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void BundleEditor::populatePaletteList(const QList<quint32> &paletteIds)
{
    m_bundle.palettes.clear();
    m_paletteList->clear();
    for (quint32 pid : paletteIds)
    {
        QLCPalette *p = m_doc->palette(pid);
        if (!p) continue;
        BundleEntry e = entryFromPalette(p);
        if (!e.isValid() || e.type == "Other") continue;
        m_bundle.palettes << e;
        m_paletteList->addItem(
            QString("[%1]  %2").arg(e.type, e.name));
    }
}

void BundleEditor::populatePaletteList(const QList<BundleEntry> &entries)
{
    m_paletteList->clear();
    for (const BundleEntry &e : entries)
        m_paletteList->addItem(QString("[%1]  %2").arg(e.type, e.name));
}

void BundleEditor::updateContainsFromEntries()
{
    QStringList contains;
    for (const BundleEntry &e : m_bundle.palettes)
    {
        QString t = e.type.toLower();
        if (t == "pantilt") t = "position";
        if (!contains.contains(t)) contains << t;
    }
    m_bundle.attributes.contains = contains;

    // Auto-pick category from dominant type
    if (contains.size() == 1)
        m_categoryCombo->setCurrentText(m_bundle.palettes.first().type);
    else if (!contains.isEmpty())
        m_categoryCombo->setCurrentText("Mixed");
}

void BundleEditor::updateOkButton()
{
    m_okBtn->setEnabled(!m_nameEdit->text().trimmed().isEmpty()
                        && !m_bundle.palettes.isEmpty());
}

void BundleEditor::slotNameChanged(const QString &)
{
    updateOkButton();
}

void BundleEditor::slotAccept()
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) return;

    // Duplicate check (allow overwrite of same name when editing)
    if (!m_editMode && m_cache->contains(name))
    {
        auto ans = QMessageBox::question(
            this, tr("Bundle already exists"),
            tr("A bundle named \"%1\" already exists. Overwrite it?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes) return;
    }

    m_bundle.name        = name;
    m_bundle.description = m_descEdit->toPlainText().trimmed();
    m_bundle.author      = m_authorEdit->text().trimmed();
    m_bundle.version    += (m_editMode ? 1 : 0);

    // Parse keywords
    m_bundle.keywords.clear();
    for (const QString &kw : m_keywordsEdit->text().split(','))
    {
        const QString t = kw.trimmed();
        if (!t.isEmpty()) m_bundle.keywords << t;
    }

    m_bundle.attributes.category = m_categoryCombo->currentText();
    m_bundle.attributes.tempo    = m_tempoCombo->currentData().toString();
    m_bundle.attributes.mood     = m_moodEdit->text().trimmed();

    const QString today = todayIso();
    if (m_bundle.created.isEmpty()) m_bundle.created = today;
    m_bundle.modified = today;

    accept();
}

QString BundleEditor::todayIso()
{
    return QDate::currentDate().toString(Qt::ISODate);
}
