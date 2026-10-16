/*
  Q Light Controller Plus - qlcconsole
  showdifferencesdialog.cpp

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
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLabel>

#include "showdifferencesdialog.h"
#include "showembedding.h"
#include "doc.h"

namespace
{
    enum { KeyRole = Qt::UserRole, CompatibleRole };
    enum { ColWhat, ColNewer, ColSafe };
}

ShowDifferencesDialog::ShowDifferencesDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Differences from this computer"));
    QVBoxLayout *lay = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr("The show carries its own copies of these, and they are not "
                                  "the same as this computer's. The show's are in use; nothing "
                                  "changes until you choose."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels(QStringList() << tr("What it is") << tr("Which looks newer")
                                          << tr("Safe to switch?"));
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->header()->setSectionResizeMode(ColWhat, QHeaderView::Stretch);
    m_tree->header()->setStretchLastSection(false);
    lay->addWidget(m_tree);

    // What switching would change, for the row picked.
    m_detail = new QLabel(this);
    m_detail->setWordWrap(true);
    m_detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_detail);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_use = box->addButton(tr("Use This Computer's"), QDialogButtonBox::ActionRole);
    m_use->setToolTip(tr("The show switches to this computer's copy now, and carries it "
                         "from the next save"));
    m_useAll = box->addButton(tr("Use This Computer's for All Compatible"), QDialogButtonBox::ActionRole);
    m_keep = box->addButton(tr("Keep the Show's"), QDialogButtonBox::ActionRole);
    m_keep->setToolTip(tr("Stop warning about it (remembered in the show)"));
    m_install = box->addButton(tr("Install the Show's"), QDialogButtonBox::ActionRole);
    m_install->setToolTip(tr("Write the show's copy over this computer's; the old file goes "
                             "to the Trash"));
    connect(m_use, &QPushButton::clicked, this, &ShowDifferencesDialog::useComputers);
    connect(m_useAll, &QPushButton::clicked, this, &ShowDifferencesDialog::useComputersForAllCompatible);
    connect(m_keep, &QPushButton::clicked, this, &ShowDifferencesDialog::keepShows);
    connect(m_install, &QPushButton::clicked, this, &ShowDifferencesDialog::installShows);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowDifferencesDialog::updateButtons);
    connect(m_doc->showEmbedding(), &ShowEmbedding::changed, this, &ShowDifferencesDialog::rebuild);

    rebuild();
    resize(760, 380);
}

void ShowDifferencesDialog::rebuild()
{
    const QStringList was = selectedKeys();
    ShowEmbedding *e = m_doc->showEmbedding();
    m_tree->blockSignals(true);
    m_tree->clear();
    foreach (const ShowEmbedding::Item &it, e->differences())
    {
        const ShowEmbedding::Compatibility c = e->compatibility(it.key());
        QTreeWidgetItem *row = new QTreeWidgetItem(m_tree);
        row->setText(ColWhat, it.displayName());
        row->setText(ColNewer, ShowEmbedding::newerText(e->newer(it)));
        row->setText(ColSafe, c.compatible ? tr("Compatible") : tr("Not compatible"));
        row->setToolTip(ColWhat, QStringLiteral("%1\n%2").arg(it.showPath, it.computerPath));
        row->setToolTip(ColSafe, c.compatible
                                     ? tr("Everything the show uses works the same with this computer's")
                                     : c.problems.join(QLatin1Char('\n')));
        if (!c.compatible)
            row->setForeground(ColSafe, QColor(0xe0, 0x40, 0x30));
        row->setData(ColWhat, KeyRole, it.key());
        row->setData(ColWhat, CompatibleRole, c.compatible);
        row->setSelected(was.contains(it.key()));
    }
    m_tree->resizeColumnToContents(ColNewer);
    m_tree->resizeColumnToContents(ColSafe);
    m_tree->blockSignals(false);
    updateButtons();
    if (m_tree->topLevelItemCount() == 0 && isVisible())
        accept();                   // nothing left to choose
}

void ShowDifferencesDialog::updateButtons()
{
    const QStringList keys = selectedKeys();
    m_use->setEnabled(!keys.isEmpty());
    m_keep->setEnabled(!keys.isEmpty());
    m_install->setEnabled(!keys.isEmpty());
    int compatible = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (m_tree->topLevelItem(i)->data(ColWhat, CompatibleRole).toBool())
            ++compatible;
    m_useAll->setEnabled(compatible > 0);

    if (keys.size() == 1)
    {
        const ShowEmbedding::Compatibility c = m_doc->showEmbedding()->compatibility(keys.first());
        m_detail->setText(c.compatible
            ? tr("Switching is safe: every mode, channel and setting the show uses is the same "
                 "in this computer's copy.")
            : tr("Switching would change what the show does:\n%1").arg(c.problems.join(QLatin1Char('\n'))));
    }
    else
        m_detail->clear();
}

QStringList ShowDifferencesDialog::selectedKeys() const
{
    QStringList keys;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
        keys << it->data(ColWhat, KeyRole).toString();
    return keys;
}

QStringList ShowDifferencesDialog::keys() const
{
    QStringList keys;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        keys << m_tree->topLevelItem(i)->data(ColWhat, KeyRole).toString();
    return keys;
}

void ShowDifferencesDialog::select(const QStringList &keys)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        it->setSelected(keys.contains(it->data(ColWhat, KeyRole).toString()));
    }
}

void ShowDifferencesDialog::useComputers()
{
    ShowEmbedding *e = m_doc->showEmbedding();
    QStringList failed;
    foreach (const QString &key, selectedKeys())
    {
        const ShowEmbedding::Item it = e->item(key);
        const ShowEmbedding::Compatibility c = e->compatibility(key);
        if (!c.compatible
            && QMessageBox::warning(this, tr("Use This Computer's"),
                   tr("This computer's copy of %1 is not compatible with how the show uses it:\n\n%2"
                      "\n\nSwitch anyway?").arg(it.displayName(), c.problems.join(QLatin1Char('\n'))),
                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            continue;
        QString error;
        if (!e->useComputers(key, &error))
            failed << error;
    }
    if (!failed.isEmpty())
        QMessageBox::warning(this, tr("Use This Computer's"), failed.join(QStringLiteral("\n\n")));
}

void ShowDifferencesDialog::useComputersForAllCompatible()
{
    ShowEmbedding *e = m_doc->showEmbedding();
    QStringList failed;
    foreach (const QString &key, keys())
    {
        if (!e->compatibility(key).compatible)
            continue;
        QString error;
        if (!e->useComputers(key, &error))
            failed << error;
    }
    if (!failed.isEmpty())
        QMessageBox::warning(this, tr("Use This Computer's"), failed.join(QStringLiteral("\n\n")));
}

void ShowDifferencesDialog::keepShows()
{
    foreach (const QString &key, selectedKeys())
        m_doc->showEmbedding()->keepShows(key);
}

void ShowDifferencesDialog::installShows()
{
    const QStringList keys = selectedKeys();
    if (keys.isEmpty())
        return;
    ShowEmbedding *e = m_doc->showEmbedding();
    QStringList names;
    foreach (const QString &key, keys)
        names << e->item(key).displayName();
    if (QMessageBox::question(this, tr("Install the Show's"),
            tr("Write the show's copy over this computer's for:\n\n%1\n\nThis computer's files go "
               "to the Trash, and every show opened here uses the show's version from now on.")
                .arg(names.join(QLatin1Char('\n'))),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    QStringList failed;
    foreach (const QString &key, keys)
    {
        QString error;
        if (!e->installShows(key, &error))
            failed << error;
    }
    if (!failed.isEmpty())
        QMessageBox::warning(this, tr("Install the Show's"), failed.join(QStringLiteral("\n\n")));
}
