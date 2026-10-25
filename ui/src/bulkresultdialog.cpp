/*
  Q Light Controller Plus - qlcconsole
  bulkresultdialog.cpp

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
#include <QTreeWidgetItem>
#include <QGuiApplication>
#include <QApplication>
#include <QTextStream>
#include <QPushButton>
#include <QFileDialog>
#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QClipboard>
#include <QShowEvent>
#include <QSplitter>
#include <QScreen>
#include <QLabel>
#include <QFile>

#include "bulkresultdialog.h"

BulkResultDialog::BulkResultDialog(const QString &title, const QString &summary, QWidget *parent)
    : QDialog(parent)
    , m_summary(summary)
{
    setObjectName(QStringLiteral("bulkResultDialog"));
    setWindowTitle(title);
    setSizeGripEnabled(true);
    QVBoxLayout *vl = new QVBoxLayout(this);
    QLabel *head = new QLabel(summary, this);
    head->setObjectName(QStringLiteral("bulkResultSummary"));
    head->setWordWrap(true);
    QFont f = head->font();
    f.setBold(true);
    head->setFont(f);
    vl->addWidget(head);
    m_text << summary;

    m_sections = new QVBoxLayout;
    vl->addLayout(m_sections, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_buttons->setObjectName(QStringLiteral("bulkResultButtons"));
    QPushButton *copy = m_buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("bulkResultCopy"));
    copy->setToolTip(tr("Copy all of it to the clipboard"));
    QPushButton *save = m_buttons->addButton(tr("Save as Text…"), QDialogButtonBox::ActionRole);
    save->setObjectName(QStringLiteral("bulkResultSave"));
    connect(copy, &QPushButton::clicked, this, &BulkResultDialog::copyText);
    connect(save, &QPushButton::clicked, this, &BulkResultDialog::saveText);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(m_buttons);
}

QList<BulkResultDialog::Entry> BulkResultDialog::entries(const QStringList &lines)
{
    QList<Entry> out;
    foreach (const QString &l, lines)
    {
        Entry e;
        e.text = l;
        out << e;
    }
    return out;
}

void BulkResultDialog::addSection(const QString &heading, const QStringList &lines, const QString &name)
{
    addSection(heading, entries(lines), name);
}

void BulkResultDialog::addSection(const QString &heading, const QList<Entry> &list, const QString &name)
{
    if (list.isEmpty())
        return;
    QLabel *h = new QLabel(QStringLiteral("%1 (%2)").arg(heading).arg(list.count()), this);
    QFont f = h->font();
    f.setBold(true);
    h->setFont(f);
    m_sections->addWidget(h);
    QTreeWidget *tree = new QTreeWidget(this);
    tree->setObjectName(name.isEmpty() ? QStringLiteral("bulkResultSection") : name);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setWordWrap(true);
    tree->setUniformRowHeights(false);
    tree->setRootIsDecorated(false);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tree->header()->setSectionResizeMode(QHeaderView::Stretch);
    m_text << QString() << heading + QLatin1Char(':');
    foreach (const Entry &e, list)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(tree, QStringList() << e.text);
        it->setToolTip(0, e.text);
        m_text << QStringLiteral("- ") + e.text;
        foreach (const QString &d, e.details)
        {
            QTreeWidgetItem *c = new QTreeWidgetItem(it, QStringList() << d);
            c->setToolTip(0, d);
            m_text << QStringLiteral("    ") + d;
        }
        if (e.details.isEmpty() == false)
            tree->setRootIsDecorated(true);
    }
    tree->setMinimumHeight(80);
    m_sections->addWidget(tree, 1);
}

void BulkResultDialog::addNote(const QString &text)
{
    QLabel *l = new QLabel(text, this);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sections->addWidget(l);
    m_text << QString() << text;
}

void BulkResultDialog::setQuestion(const QString &question)
{
    if (m_question == nullptr)
    {
        m_question = new QLabel(this);
        m_question->setWordWrap(true);
        m_sections->addWidget(m_question);
    }
    m_question->setText(question);
    m_buttons->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    m_text << QString() << question;
}

QString BulkResultDialog::plainText() const
{
    return m_text.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void BulkResultDialog::copyText()
{
    QApplication::clipboard()->setText(plainText());
}

void BulkResultDialog::saveText()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save as Text"), QString(),
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text) == false)
    {
        QMessageBox::warning(this, tr("Save as Text"), tr("Could not write %1.").arg(path));
        return;
    }
    QTextStream(&file) << plainText();
}

void BulkResultDialog::fitToScreen()
{
    QScreen *screen = nullptr;
    if (parentWidget() != nullptr)
        screen = parentWidget()->screen();
    if (screen == nullptr)
        screen = QGuiApplication::primaryScreen();
    if (screen == nullptr)
        return;
    const QRect avail = screen->availableGeometry();
    const QSize most(avail.width() * 9 / 10, avail.height() * 9 / 10);
    setMaximumSize(avail.size());
    const QSize want = sizeHint().expandedTo(QSize(560, 360));
    resize(want.boundedTo(most));
    // The frame too, centred, inside the screen.
    QRect r(QPoint(0, 0), size());
    r.moveCenter(avail.center());
    move(r.topLeft());
}

void BulkResultDialog::showEvent(QShowEvent *ev)
{
    if (m_fitted == false)
    {
        m_fitted = true;
        fitToScreen();
    }
    QDialog::showEvent(ev);
}

void BulkResultDialog::tell(QWidget *parent, const QString &title, const QString &summary,
                            const QList<QPair<QString, QList<Entry> > > &sections,
                            const QStringList &notes)
{
    BulkResultDialog dlg(title, summary, parent);
    for (const QPair<QString, QList<Entry> > &s : sections)
        dlg.addSection(s.first, s.second);
    foreach (const QString &n, notes)
        dlg.addNote(n);
    dlg.exec();
}

bool BulkResultDialog::confirm(QWidget *parent, const QString &title, const QString &intro,
                               const QString &heading, const QStringList &items,
                               const QString &question)
{
    BulkResultDialog dlg(title, intro, parent);
    dlg.addSection(heading, items, QStringLiteral("bulkConfirmItems"));
    dlg.setQuestion(question);
    return dlg.exec() == QDialog::Accepted;
}
