/*
  Q Light Controller Plus
  controlmappanel.cpp

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

#include <QAbstractItemView>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>

#include "controlmappanel.h"
#include "controlmap.h"
#include "doc.h"

namespace
{
enum Column { ControlCol = 0, TargetCol, ModeCol };
const int TargetRole = Qt::UserRole;
const int TypeRole = Qt::UserRole + 1;
}

ControlMapPanel::ControlMapPanel(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("controlMapPanel"));
    setProperty("helpTopic", QStringLiteral("other-improvements.md#control-map-midi-and-osc-triggers"));

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_empty = new QLabel(tr("No triggers yet. Right-click a scene or effect in Build, or a cue list, "
                            "cue or hit in the Show Tree, and choose Learn trigger…; then press the "
                            "key, pad or button that should fire it."), this);
    m_empty->setObjectName(QStringLiteral("controlMapEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_empty);

    m_table = new QTableWidget(0, 3, this);
    m_table->setObjectName(QStringLiteral("controlMapTable"));
    m_table->setHorizontalHeaderLabels({ tr("Control"), tr("Fires"), tr("When pressed") });
    m_table->horizontalHeader()->setSectionResizeMode(ControlCol, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(TargetCol, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ModeCol, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(140);
    lay->addWidget(m_table);

    QHBoxLayout *row = new QHBoxLayout();
    m_remove = new QPushButton(tr("Remove"), this);
    m_remove->setObjectName(QStringLiteral("controlMapRemove"));
    m_remove->setToolTip(tr("The selected triggers: their controls fire nothing (their lights go off)"));
    m_removeAll = new QPushButton(tr("Remove all…"), this);
    m_removeAll->setObjectName(QStringLiteral("controlMapRemoveAll"));
    m_last = new QLabel(this);
    m_last->setObjectName(QStringLiteral("controlMapLast"));
    m_last->setStyleSheet(QStringLiteral("color: gray;"));
    row->addWidget(m_remove);
    row->addWidget(m_removeAll);
    row->addWidget(m_last, 1);
    lay->addLayout(row);

    connect(m_remove, &QPushButton::clicked, this, &ControlMapPanel::slotRemove);
    connect(m_removeAll, &QPushButton::clicked, this, &ControlMapPanel::slotRemoveAll);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_remove->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty());
    });
    ControlMap *cm = m_doc->controlMap();
    connect(cm, &ControlMap::bindingsChanged, this, &ControlMapPanel::refresh);
    connect(cm, &ControlMap::fired, this, &ControlMapPanel::slotFired);
    // Names follow renames.
    connect(m_doc, &Doc::functionChanged, this, [this]() { if (isVisible()) refresh(); });
    connect(m_doc, &Doc::functionNameChanged, this, [this]() { if (isVisible()) refresh(); });
    refresh();
}

QString ControlMapPanel::summary() const
{
    const int n = m_doc->controlMap()->count();
    if (n == 0)
        return tr("None");
    return n == 1 ? tr("1 trigger") : tr("%1 triggers").arg(n);
}

void ControlMapPanel::refresh()
{
    ControlMap *cm = m_doc->controlMap();
    const QList<ControlMap::Binding> bindings = cm->bindings();
    m_table->setRowCount(0);
    m_table->setRowCount(bindings.count());
    for (int r = 0; r < bindings.count(); ++r)
    {
        const ControlMap::Binding &b = bindings.at(r);
        QTableWidgetItem *c = new QTableWidgetItem(cm->controlName(b.universe, b.channel));
        c->setData(TargetRole, b.target.id);
        c->setData(TypeRole, int(b.target.type));
        m_table->setItem(r, ControlCol, c);
        QTableWidgetItem *t = new QTableWidgetItem(cm->targetName(b.target));
        if (!cm->targetExists(b.target))
            t->setForeground(Qt::gray);
        m_table->setItem(r, TargetCol, t);

        QComboBox *mode = new QComboBox(m_table);
        mode->setObjectName(QStringLiteral("controlMapMode"));
        const bool list = b.target.type == ControlMap::CueListTarget || b.target.type == ControlMap::CueTarget;
        const QList<ControlMap::Mode> modes = list
            ? QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StopOnly }
            : QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StartOnly,
                                       ControlMap::StopOnly };
        for (ControlMap::Mode m : modes)
            mode->addItem(ControlMap::modeName(m, b.target.type), int(m));
        int at = mode->findData(int(b.mode));
        mode->setCurrentIndex(at < 0 ? 0 : at);
        const ControlMap::Target target = b.target;
        connect(mode, QOverload<int>::of(&QComboBox::activated), this, [this, mode, target](int) {
            m_doc->controlMap()->setMode(target, ControlMap::Mode(mode->currentData().toInt()));
        });
        m_table->setCellWidget(r, ModeCol, mode);
    }
    const bool any = !bindings.isEmpty();
    m_empty->setVisible(!any);
    m_table->setVisible(any);
    m_removeAll->setEnabled(any);
    m_remove->setEnabled(any && !m_table->selectionModel()->selectedRows().isEmpty());
    emit summaryChanged();
}

void ControlMapPanel::showEvent(QShowEvent *ev)
{
    refresh();      // names of things renamed since
    QWidget::showEvent(ev);
}

void ControlMapPanel::slotRemove()
{
    QList<ControlMap::Target> targets;
    for (const QModelIndex &i : m_table->selectionModel()->selectedRows())
    {
        QTableWidgetItem *it = m_table->item(i.row(), ControlCol);
        if (it != nullptr)
            targets << ControlMap::Target{ ControlMap::TargetType(it->data(TypeRole).toInt()),
                                           it->data(TargetRole).toUInt() };
    }
    for (const ControlMap::Target &t : targets)
        m_doc->controlMap()->removeBinding(t);
}

void ControlMapPanel::slotRemoveAll()
{
    const int n = m_doc->controlMap()->count();
    if (n == 0)
        return;
    if (property("noConfirm").toBool()
        || QMessageBox::question(this, tr("Remove all triggers"),
                                 n == 1 ? tr("Remove the trigger? Its control will fire nothing.")
                                       : tr("Remove all %1 triggers? Their controls will fire nothing.").arg(n))
               == QMessageBox::Yes)
        m_doc->controlMap()->removeAll();
}

void ControlMapPanel::slotFired(const QString &what)
{
    m_last->setText(tr("Last: %1").arg(what));
}
