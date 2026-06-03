/*
  Q Light Controller Plus
  groupselection.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>

#include <algorithm>

#include "groupselection.h"
#include "fixturegroup.h"
#include "doc.h"

GroupSelection::GroupSelection(Doc *doc, const QList<quint32> &selected,
                               QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Select fixture groups"));
    resize(360, 420);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(
        tr("Tick the fixture groups this scene's looks should apply to:"),
        this));

    m_list = new QListWidget(this);
    root->addWidget(m_list, 1);

    // All groups, sorted by name (case-insensitive), with member counts.
    QList<FixtureGroup*> groups;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        if (g != NULL)
            groups.append(g);
    std::sort(groups.begin(), groups.end(),
              [](FixtureGroup *a, FixtureGroup *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });

    foreach (FixtureGroup *g, groups)
    {
        const int n = g->fixtureList().count();
        QListWidgetItem *it = new QListWidgetItem(
            tr("%1  (%n fixture(s))", "", n).arg(g->name()), m_list);
        it->setData(Qt::UserRole, g->id());
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(selected.contains(g->id()) ? Qt::Checked
                                                      : Qt::Unchecked);
    }

    QHBoxLayout *quick = new QHBoxLayout();
    QPushButton *allBtn = new QPushButton(tr("Select all"), this);
    QPushButton *noneBtn = new QPushButton(tr("Select none"), this);
    quick->addWidget(allBtn);
    quick->addWidget(noneBtn);
    quick->addStretch();
    root->addLayout(quick);

    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(box);

    connect(allBtn, SIGNAL(clicked()), this, SLOT(slotSelectAll()));
    connect(noneBtn, SIGNAL(clicked()), this, SLOT(slotSelectNone()));
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
}

GroupSelection::~GroupSelection()
{
}

void GroupSelection::slotSelectAll()
{
    for (int i = 0; i < m_list->count(); i++)
        m_list->item(i)->setCheckState(Qt::Checked);
}

void GroupSelection::slotSelectNone()
{
    for (int i = 0; i < m_list->count(); i++)
        m_list->item(i)->setCheckState(Qt::Unchecked);
}

void GroupSelection::accept()
{
    m_selection.clear();
    for (int i = 0; i < m_list->count(); i++)
    {
        QListWidgetItem *it = m_list->item(i);
        if (it->checkState() == Qt::Checked)
            m_selection.append(it->data(Qt::UserRole).toUInt());
    }
    QDialog::accept();
}
