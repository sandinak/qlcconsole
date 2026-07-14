/*
  Q Light Controller Plus
  palettemanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>

#include "palettemanager.h"
#include "paletteeditdialog.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

namespace {
/** One-line value description for the palette list. */
QString paletteValueText(QLCPalette *p)
{
    if (p == nullptr)
        return QString();
    switch (p->type())
    {
    case QLCPalette::Color:   return p->rgbValue().name();
    case QLCPalette::PanTilt: return QString("P%1 / T%2")
                                  .arg(p->intValue1()).arg(p->intValue2());
    case QLCPalette::Aim:     return QString("Aim");
    default:                  return QString::number(p->intValue1());
    }
}
}

PaletteManager::PaletteManager(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Palettes"));
    resize(560, 380);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(
        tr("Named, reusable palettes (Color / Dimmer / Pan-Tilt / Gobo / "
           "Shutter). Attach them to scenes + fixture groups to build "
           "looks that follow group membership."), this));

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels(QStringList()
        << tr("Name") << tr("Type") << tr("Value") << tr("Used by"));
    m_tree->setRootIsDecorated(false);
    m_tree->header()->setStretchLastSection(false);
    m_tree->setColumnWidth(0, 220);
    root->addWidget(m_tree, 1);

    QHBoxLayout *btns = new QHBoxLayout();
    m_newButton = new QPushButton(tr("New…"), this);
    m_editButton = new QPushButton(tr("Edit…"), this);
    m_renameButton = new QPushButton(tr("Rename…"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    btns->addWidget(m_newButton);
    btns->addWidget(m_editButton);
    btns->addWidget(m_renameButton);
    btns->addWidget(m_deleteButton);
    btns->addStretch();
    root->addLayout(btns);

    QPushButton *close = new QPushButton(tr("Close"), this);
    btns->addWidget(close);

    connect(m_newButton, SIGNAL(clicked()), this, SLOT(slotNew()));
    connect(m_editButton, SIGNAL(clicked()), this, SLOT(slotEdit()));
    connect(m_renameButton, SIGNAL(clicked()), this, SLOT(slotRename()));
    connect(m_deleteButton, SIGNAL(clicked()), this, SLOT(slotDelete()));
    connect(close, SIGNAL(clicked()), this, SLOT(accept()));
    connect(m_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotSelectionChanged()));
    connect(m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotEdit()));

    reload();
    slotSelectionChanged();
}

PaletteManager::~PaletteManager()
{
}

void PaletteManager::reload()
{
    const quint32 keep = selectedPaletteId();
    m_tree->clear();
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p == nullptr)
            continue;
        QTreeWidgetItem *it = new QTreeWidgetItem(m_tree);
        it->setText(0, p->name());
        it->setText(1, QLCPalette::typeToString(p->type()));
        it->setText(2, paletteValueText(p));
        const int refs = referenceCount(p->id());
        it->setText(3, refs > 0 ? tr("%n scene(s)", "", refs) : tr("—"));
        it->setData(0, Qt::UserRole, p->id());
        if (p->id() == keep)
            it->setSelected(true);
    }
}

quint32 PaletteManager::selectedPaletteId() const
{
    const QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
    if (sel.isEmpty())
        return QLCPalette::invalidId();   // NOT 0 — 0 is a legitimate palette id
    return sel.first()->data(0, Qt::UserRole).toUInt();
}

int PaletteManager::referenceCount(quint32 paletteId) const
{
    int n = 0;
    foreach (Function *f, m_doc->functions())
    {
        Scene *s = qobject_cast<Scene*>(f);
        if (s != nullptr && s->palettes().contains(paletteId))
            ++n;
    }
    return n;
}

void PaletteManager::slotSelectionChanged()
{
    // Doc::createPaletteId() starts at 0, so the FIRST palette in a workspace
    // legitimately has id 0 — testing against 0 left Edit/Rename/Delete greyed
    // out for it. QLCPalette::invalidId() is the sentinel everywhere else.
    const bool has = (selectedPaletteId() != QLCPalette::invalidId());
    m_editButton->setEnabled(has);
    m_renameButton->setEnabled(has);
    m_deleteButton->setEnabled(has);
}

void PaletteManager::slotNew()
{
    PaletteEditDialog dlg(this, m_doc);
    if (dlg.exec() != QDialog::Accepted || dlg.result() == nullptr)
        return;
    QLCPalette *p = dlg.result();
    if (!m_doc->addPalette(p))
    {
        delete p;
        return;
    }
    m_doc->setModified();
    reload();
}

void PaletteManager::slotEdit()
{
    const quint32 id = selectedPaletteId();
    QLCPalette *p = m_doc->palette(id);
    if (p == nullptr)
        return;
    PaletteEditDialog dlg(p, this, m_doc);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_doc->setModified();
    reload();
}

void PaletteManager::slotRename()
{
    const quint32 id = selectedPaletteId();
    QLCPalette *p = m_doc->palette(id);
    if (p == nullptr)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename palette"), tr("Name:"),
        QLineEdit::Normal, p->name(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    p->setName(name.trimmed());
    m_doc->setModified();
    reload();
}

void PaletteManager::slotDelete()
{
    const quint32 id = selectedPaletteId();
    QLCPalette *p = m_doc->palette(id);
    if (p == nullptr)
        return;
    const int refs = referenceCount(id);
    QString msg = tr("Delete palette \"%1\"?").arg(p->name());
    if (refs > 0)
        msg += tr("\n\nIt is used by %n scene(s); they will lose this look.",
                  "", refs);
    if (QMessageBox::question(this, tr("Delete palette"), msg,
                              QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes)
        return;

    // Detach from any scenes first so no dangling references remain.
    foreach (Function *f, m_doc->functions())
    {
        Scene *s = qobject_cast<Scene*>(f);
        if (s != nullptr && s->palettes().contains(id))
            s->removePalette(id);
    }
    m_doc->deletePalette(id);
    m_doc->setModified();
    reload();
    slotSelectionChanged();
}
