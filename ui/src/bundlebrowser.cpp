/*
  Q Light Controller Plus
  bundlebrowser.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "bundlebrowser.h"
#include "bundlecache.h"
#include "bundleeditor.h"
#include "doc.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

// Item data roles
static const int BundleNameRole = Qt::UserRole;      // set on bundle items only
static const int IsFolderRole   = Qt::UserRole + 1;  // true on folder items

BundleBrowser::BundleBrowser(Doc *doc, BundleCache *cache, QWidget *parent)
    : QWidget(parent), m_doc(doc), m_cache(cache)
{
    buildUi();
    rebuildTree();
}

void BundleBrowser::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Toolbar ───────────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout;
    auto *saveBtn = new QPushButton(tr("Save as Bundle…"), this);
    saveBtn->setToolTip(tr("Capture the current look's palettes as a new shareable Bundle"));
    connect(saveBtn, &QPushButton::clicked,
            this, &BundleBrowser::slotSaveAsBundleRequested);
    toolbar->addWidget(saveBtn);
    toolbar->addStretch(1);
    root->addLayout(toolbar);

    // ── Search ────────────────────────────────────────────────────────────
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search name, description, keywords…"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &BundleBrowser::slotSearchChanged);
    root->addWidget(m_searchEdit);

    // ── Bundle folder tree ────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setAlternatingRowColors(true);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &BundleBrowser::slotSelectionChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &BundleBrowser::slotItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &BundleBrowser::slotContextMenu);
    root->addWidget(m_tree, 1);

    // ── Preview panel ─────────────────────────────────────────────────────
    auto *previewBox = new QGroupBox(tr("Preview"), this);
    auto *pvl = new QVBoxLayout(previewBox);
    m_previewLabel = new QLabel(previewBox);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_previewLabel->setTextFormat(Qt::RichText);
    pvl->addWidget(m_previewLabel);
    root->addWidget(previewBox);

    // ── Action buttons ────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    m_stampBtn  = new QPushButton(tr("Stamp to Look"), this);
    m_editBtn   = new QPushButton(tr("Edit…"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_stampBtn->setEnabled(false);
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    connect(m_stampBtn,  &QPushButton::clicked, this, &BundleBrowser::slotStamp);
    connect(m_editBtn,   &QPushButton::clicked, this, &BundleBrowser::slotEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &BundleBrowser::slotDelete);
    btnRow->addWidget(m_stampBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_deleteBtn);
    root->addLayout(btnRow);
}

QTreeWidgetItem *BundleBrowser::folderFor(const QString &category)
{
    // Find or create a top-level folder item for the given category.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->text(0) == category)
            return it;
    }
    auto *folder = new QTreeWidgetItem(m_tree, QStringList(category));
    folder->setData(0, IsFolderRole, true);
    folder->setFlags(folder->flags() & ~Qt::ItemIsSelectable);
    folder->setExpanded(true);
    // Bold folder label
    QFont f = folder->font(0);
    f.setBold(true);
    folder->setFont(0, f);
    return folder;
}

void BundleBrowser::rebuildTree()
{
    // Remember selected bundle name so we can restore it
    const QString sel = selectedBundle().name;

    m_tree->clear();
    const QString filter = m_searchEdit->text();

    for (const QLCBundle &b : m_cache->bundles())
    {
        if (!b.matches(filter)) continue;

        const QString cat = b.attributes.category.isEmpty()
                            ? tr("Uncategorised") : b.attributes.category;
        QTreeWidgetItem *folder = folderFor(cat);

        auto *item = new QTreeWidgetItem(folder, QStringList(b.name));
        item->setData(0, BundleNameRole, b.name);
        item->setData(0, IsFolderRole,   false);

        // Tooltip: tempo + mood + description
        QStringList tip;
        if (!b.attributes.tempo.isEmpty()) tip << b.attributes.tempo;
        if (!b.attributes.mood.isEmpty())  tip << b.attributes.mood;
        if (!b.description.isEmpty())      tip << b.description;
        if (!tip.isEmpty()) item->setToolTip(0, tip.join("  ·  "));
    }

    // Remove empty folders (can happen with aggressive search filtering)
    for (int i = m_tree->topLevelItemCount() - 1; i >= 0; --i)
    {
        if (m_tree->topLevelItem(i)->childCount() == 0)
            delete m_tree->takeTopLevelItem(i);
    }

    // Restore selection
    if (!sel.isEmpty())
    {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *folder = m_tree->topLevelItem(i);
            for (int j = 0; j < folder->childCount(); ++j)
            {
                QTreeWidgetItem *child = folder->child(j);
                if (child->data(0, BundleNameRole).toString() == sel)
                {
                    m_tree->setCurrentItem(child);
                    break;
                }
            }
        }
    }
}

void BundleBrowser::refresh()
{
    rebuildTree();
}

QLCBundle BundleBrowser::selectedBundle() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || item->data(0, IsFolderRole).toBool()) return QLCBundle();
    return m_cache->bundle(item->data(0, BundleNameRole).toString());
}

void BundleBrowser::updatePreview(const QLCBundle &b)
{
    if (!b.isValid())
    {
        m_previewLabel->clear();
        return;
    }

    QString html;
    if (!b.description.isEmpty())
        html += "<p>" + b.description.toHtmlEscaped() + "</p>";

    html += "<ul style='margin:0; padding-left:14px;'>";
    for (const BundleEntry &e : b.palettes)
    {
        QString swatch;
        if (e.type == "Color" && !e.color.isEmpty())
            swatch = QString(" <span style='background:%1; color:%1;'>▉</span>")
                     .arg(e.color.toHtmlEscaped());
        html += QString("<li><b>[%1]</b>%2 %3</li>")
                .arg(e.type.toHtmlEscaped(), swatch, e.name.toHtmlEscaped());
    }
    html += "</ul>";

    QStringList meta;
    if (!b.attributes.tempo.isEmpty()) meta << tr("Tempo: %1").arg(b.attributes.tempo);
    if (!b.attributes.mood.isEmpty())  meta << tr("Mood: %1").arg(b.attributes.mood);
    if (!b.keywords.isEmpty())         meta << tr("Keywords: %1").arg(b.keywords.join(", "));
    if (!b.author.isEmpty())           meta << tr("Author: %1").arg(b.author);
    if (!meta.isEmpty())
        html += "<p style='color:grey; font-size:small;'>" +
                meta.join("  ·  ").toHtmlEscaped() + "</p>";

    m_previewLabel->setText(html);
}

// ─── slots ───────────────────────────────────────────────────────────────────

void BundleBrowser::slotSearchChanged(const QString &)
{
    rebuildTree();
}

void BundleBrowser::slotSelectionChanged()
{
    const QLCBundle b = selectedBundle();
    const bool valid  = b.isValid();
    m_stampBtn->setEnabled(valid);
    m_editBtn->setEnabled(valid);
    const QString userDir = BundleCache::userBundlesDirectory().absolutePath();
    m_deleteBtn->setEnabled(valid && b.path.startsWith(userDir));
    updatePreview(b);
}

void BundleBrowser::slotItemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!item || item->data(0, IsFolderRole).toBool()) return;
    slotStamp();
}

void BundleBrowser::slotStamp()
{
    const QLCBundle b = selectedBundle();
    if (b.isValid())
        emit stampRequested(b.name);
}

void BundleBrowser::slotEdit()
{
    const QLCBundle b = selectedBundle();
    if (!b.isValid()) return;

    BundleEditor dlg(m_doc, m_cache, b, this);
    if (dlg.exec() != QDialog::Accepted) return;

    if (!m_cache->saveBundle(dlg.bundle()))
    {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Could not write the Bundle file."));
        return;
    }
    refresh();
}

void BundleBrowser::slotDelete()
{
    const QLCBundle b = selectedBundle();
    if (!b.isValid()) return;

    auto ans = QMessageBox::question(
        this, tr("Delete Bundle"),
        tr("Delete \"%1\"? This cannot be undone.").arg(b.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    if (!m_cache->deleteBundle(b.name))
    {
        QMessageBox::warning(this, tr("Delete failed"),
                             tr("Could not remove the Bundle file."));
        return;
    }
    refresh();
}

void BundleBrowser::slotDuplicate()
{
    const QLCBundle b = selectedBundle();
    if (!b.isValid()) return;

    QLCBundle copy = b;
    copy.name    = b.name + tr(" Copy");
    copy.path    = QString();
    copy.version = 1;

    BundleEditor dlg(m_doc, m_cache, copy.palettes, this);
    if (dlg.exec() != QDialog::Accepted) return;

    if (!m_cache->saveBundle(dlg.bundle()))
    {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Could not write the Bundle file."));
        return;
    }
    refresh();
}

void BundleBrowser::slotRevealInFinder()
{
    const QLCBundle b = selectedBundle();
    if (b.path.isEmpty()) return;
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(b.path).absolutePath()));
}

void BundleBrowser::slotContextMenu(const QPoint &pos)
{
    const QLCBundle b = selectedBundle();
    if (!b.isValid()) return;

    QMenu menu(this);
    menu.addAction(tr("Stamp to Look"), this, &BundleBrowser::slotStamp);
    menu.addSeparator();
    menu.addAction(tr("Edit…"),         this, &BundleBrowser::slotEdit);
    menu.addAction(tr("Duplicate"),     this, &BundleBrowser::slotDuplicate);

    const QString userDir = BundleCache::userBundlesDirectory().absolutePath();
    if (b.path.startsWith(userDir))
        menu.addAction(tr("Delete"),    this, &BundleBrowser::slotDelete);

    menu.addSeparator();
    menu.addAction(tr("Show in Finder"), this, &BundleBrowser::slotRevealInFinder);
    menu.exec(m_tree->mapToGlobal(pos));
}

void BundleBrowser::slotSaveAsBundleRequested()
{
    emit saveAsBundleRequested();
}
