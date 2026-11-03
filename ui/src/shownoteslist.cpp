/*
  Q Light Controller Plus
  shownoteslist.cpp

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
#include <QTreeWidgetItem>
#include <QPrintDialog>
#include <QTextDocument>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPdfWriter>
#include <QFileInfo>
#include <QComboBox>
#include <QLineEdit>
#include <QPrinter>
#include <QPainter>
#include <QImage>
#include <QLabel>
#include <QFile>
#include <QDateTime>
#include <QLocale>
#include <QUrl>

#include "shownoteslist.h"
#include "shownotespane.h"
#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const int NoteIdRole = Qt::UserRole + 1;
enum Col { ItemCol = 0, WhenCol, ByCol, NoteCol, StateCol };
}

ShowNotesList::ShowNotesList(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("stNotesList"));
    setWindowTitle(tr("Show notes"));
    setModal(false);
    setProperty("helpTopic", QStringLiteral("show-tree.md#notes"));
    QVBoxLayout *v = new QVBoxLayout(this);

    QHBoxLayout *filters = new QHBoxLayout();
    m_state = new QComboBox(this);
    m_state->setObjectName(QStringLiteral("stNotesFilterState"));
    m_state->addItem(tr("All notes"), int(AllNotes));
    m_state->addItem(tr("Open"), int(OpenNotes));
    m_state->addItem(tr("Done"), int(DoneNotes));
    filters->addWidget(m_state);
    m_item = new QComboBox(this);
    m_item->setObjectName(QStringLiteral("stNotesFilterItem"));
    m_item->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_item->setMinimumContentsLength(14);
    filters->addWidget(m_item, 1);
    m_text = new QLineEdit(this);
    m_text->setObjectName(QStringLiteral("stNotesFilterText"));
    m_text->setPlaceholderText(tr("Filter: words, a cue number"));
    m_text->setClearButtonEnabled(true);
    filters->addWidget(m_text, 1);
    v->addLayout(filters);

    m_table = new QTreeWidget(this);
    m_table->setObjectName(QStringLiteral("stNotesTable"));
    m_table->setRootIsDecorated(false);
    m_table->setUniformRowHeights(false);
    m_table->setWordWrap(true);
    m_table->setHeaderLabels({ tr("Item"), tr("When"), tr("By"), tr("Note"), tr("") });
    m_table->header()->setStretchLastSection(false);
    m_table->header()->setSectionResizeMode(ItemCol, QHeaderView::Interactive);
    m_table->header()->setSectionResizeMode(NoteCol, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(StateCol, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(ItemCol, 200);
    m_table->setColumnWidth(WhenCol, 110);
    m_table->setColumnWidth(ByCol, 90);
    v->addWidget(m_table, 1);
    connect(m_table, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        jumpTo(m_table->indexOfTopLevelItem(item));
    });

    QHBoxLayout *bottom = new QHBoxLayout();
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("stNotesCount"));
    bottom->addWidget(m_count, 1);
    QPushButton *go = new QPushButton(tr("Go to"), this);
    go->setObjectName(QStringLiteral("stNotesGoTo"));
    go->setToolTip(tr("Select the note's item in the Show Tree (or double-click the row)"));
    connect(go, &QPushButton::clicked, this, [this]() {
        if (QTreeWidgetItem *item = m_table->currentItem())
            jumpTo(m_table->indexOfTopLevelItem(item));
    });
    bottom->addWidget(go);
    QPushButton *print = new QPushButton(tr("Print…"), this);
    print->setObjectName(QStringLiteral("stNotesPrint"));
    connect(print, &QPushButton::clicked, this, &ShowNotesList::slotPrint);
    bottom->addWidget(print);
    QPushButton *exp = new QPushButton(tr("Export…"), this);
    exp->setObjectName(QStringLiteral("stNotesExport"));
    exp->setToolTip(tr("The notes listed, as Markdown (.md) or PDF"));
    connect(exp, &QPushButton::clicked, this, &ShowNotesList::slotExport);
    bottom->addWidget(exp);
    QPushButton *close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    bottom->addWidget(close);
    v->addLayout(bottom);

    connect(m_state, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShowNotesList::refresh);
    connect(m_item, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_filling)
            refresh();
    });
    connect(m_text, &QLineEdit::textChanged, this, &ShowNotesList::refresh);
    if (ShowTreeController *st = controller())
    {
        connect(st, &ShowTreeController::notesChanged, this, &ShowNotesList::refresh);
        connect(st, &ShowTreeController::changed, this, &ShowNotesList::refresh);
    }
    resize(820, 480);
    refresh();
}

ShowTreeController *ShowNotesList::controller() const
{
    return m_doc != nullptr ? m_doc->showTree() : nullptr;
}

void ShowNotesList::setStateFilter(StateFilter filter)
{
    m_state->setCurrentIndex(m_state->findData(int(filter)));
}

void ShowNotesList::setItemFilter(NoteKind kind, quint32 target)
{
    if (target == 0xFFFFFFFF)
    {
        m_item->setCurrentIndex(0);
        return;
    }
    const int at = m_item->findData(QStringLiteral("%1:%2").arg(noteKindToString(kind)).arg(target));
    m_item->setCurrentIndex(at < 0 ? 0 : at);
}

void ShowNotesList::setTextFilter(const QString &text)
{
    m_text->setText(text);
}

QList<Note> ShowNotesList::filtered() const
{
    QList<Note> out;
    ShowTreeController *st = controller();
    if (st == nullptr)
        return out;
    const StateFilter state = StateFilter(m_state->currentData().toInt());
    const QString item = m_item->currentData().toString();
    const QString words = m_text->text().trimmed();
    for (const Note &n : st->allNotes())
    {
        if ((state == OpenNotes && n.done) || (state == DoneNotes && !n.done))
            continue;
        if (!item.isEmpty() && item != QStringLiteral("%1:%2").arg(noteKindToString(n.kind)).arg(n.target))
            continue;
        if (!words.isEmpty() && !n.text.contains(words, Qt::CaseInsensitive)
            && !st->noteTargetLabel(n).contains(words, Qt::CaseInsensitive)
            && !n.author.contains(words, Qt::CaseInsensitive))
            continue;
        out << n;
    }
    return out;
}

void ShowNotesList::refresh()
{
    ShowTreeController *st = controller();
    if (st == nullptr)
        return;
    // The items that have notes, in the order of their newest note.
    {
        m_filling = true;
        const QString was = m_item->currentData().toString();
        m_item->clear();
        m_item->addItem(tr("Every item"), QString());
        QStringList seen;
        for (const Note &n : st->allNotes())
        {
            const QString key = QStringLiteral("%1:%2").arg(noteKindToString(n.kind)).arg(n.target);
            if (seen.contains(key))
                continue;
            seen << key;
            m_item->addItem(st->noteTargetLabel(n), key);
        }
        const int at = m_item->findData(was);
        m_item->setCurrentIndex(at < 0 ? 0 : at);
        m_filling = false;
    }

    m_table->clear();
    const QList<Note> list = filtered();
    int open = 0;
    for (const Note &n : list)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_table);
        item->setText(ItemCol, st->noteTargetLabel(n));
        item->setText(WhenCol, ShowNotesPane::whenText(n.when));
        item->setText(ByCol, n.author);
        QString text = n.text;
        if (n.view.valid)
            text += (text.isEmpty() ? QString() : QStringLiteral(" ")) + tr("[snapshot%1]")
                    .arg(n.view.cueLabel.isEmpty() ? QString() : tr(": %1 on stage").arg(n.view.cueLabel));
        item->setText(NoteCol, text);
        item->setToolTip(NoteCol, n.text);
        item->setText(StateCol, n.done ? tr("done") : tr("open"));
        item->setData(0, NoteIdRole, n.id);
        if (n.done)
            for (int c = 0; c < m_table->columnCount(); ++c)
                item->setForeground(c, palette().color(QPalette::Disabled, QPalette::Text));
        else
            ++open;
    }
    m_count->setText((list.count() == 1 ? tr("1 note") : tr("%1 notes").arg(list.count()))
                     + QStringLiteral(" · ") + tr("%1 open").arg(open));
}

QList<quint32> ShowNotesList::listedNotes() const
{
    QList<quint32> out;
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
        out << m_table->topLevelItem(i)->data(0, NoteIdRole).toUInt();
    return out;
}

bool ShowNotesList::jumpTo(int row)
{
    ShowTreeController *st = controller();
    QTreeWidgetItem *item = m_table->topLevelItem(row);
    if (st == nullptr || item == nullptr)
        return false;
    const Note *n = st->note(item->data(0, NoteIdRole).toUInt());
    if (n == nullptr || !jump)
        return false;
    jump(*n);
    return true;
}

QString ShowNotesList::toMarkdown() const
{
    ShowTreeController *st = controller();
    QStringList out;
    out << QStringLiteral("# %1").arg(tr("Show notes"));
    out << QString();
    out << tr("%1 · %2").arg(m_doc != nullptr && !m_doc->workspacePath().isEmpty()
                             ? QFileInfo(m_doc->workspacePath()).fileName() : tr("Untitled show"),
                             QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    out << QString();
    QString lastItem;
    for (const Note &n : filtered())
    {
        const QString item = st->noteTargetLabel(n);
        if (item != lastItem)
        {
            out << QString() << QStringLiteral("## %1").arg(item) << QString();
            lastItem = item;
        }
        QString line = QStringLiteral("- [%1] ").arg(n.done ? QStringLiteral("x") : QStringLiteral(" "));
        QStringList meta { ShowNotesPane::whenText(n.when) };
        if (!n.author.isEmpty())
            meta << n.author;
        line += QStringLiteral("*%1* — ").arg(meta.join(QStringLiteral(", ")));
        line += QString(n.text).replace(QLatin1Char('\n'), QStringLiteral("  \n  "));
        if (n.view.valid)
            line += QStringLiteral(" (%1)").arg(n.view.cueLabel.isEmpty()
                                                ? tr("snapshot of the view")
                                                : tr("snapshot of the view, %1 on stage").arg(n.view.cueLabel));
        out << line;
    }
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString ShowNotesList::toHtml() const
{
    ShowTreeController *st = controller();
    QString html = QStringLiteral("<html><body style=\"font-family: sans-serif;\"><h2>%1</h2><p>%2</p>")
                   .arg(tr("Show notes").toHtmlEscaped(),
                        (m_doc != nullptr && !m_doc->workspacePath().isEmpty()
                         ? QFileInfo(m_doc->workspacePath()).fileName() : tr("Untitled show")).toHtmlEscaped()
                        + QStringLiteral(" · ")
                        + QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat).toHtmlEscaped());
    html += QStringLiteral("<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" width=\"100%\">"
                           "<tr><th>&#x2713;</th><th>%1</th><th>%2</th><th>%3</th><th>%4</th></tr>")
            .arg(tr("Item").toHtmlEscaped(), tr("When").toHtmlEscaped(), tr("By").toHtmlEscaped(),
                 tr("Note").toHtmlEscaped());
    for (const Note &n : filtered())
    {
        QString text = n.text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        if (n.view.valid)
        {
            text += QStringLiteral("<br><i>%1</i>").arg((n.view.cueLabel.isEmpty()
                                                        ? tr("Snapshot of the view")
                                                        : tr("Snapshot of the view, %1 on stage")
                                                          .arg(n.view.cueLabel)).toHtmlEscaped());
            if (!n.view.image.isEmpty())
                text += QStringLiteral("<br><img src=\"note:%1\" width=\"200\">").arg(n.view.image);
        }
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                .arg(n.done ? QStringLiteral("&#x2713;") : QString(), st->noteTargetLabel(n).toHtmlEscaped(),
                     ShowNotesPane::whenText(n.when).toHtmlEscaped(), n.author.toHtmlEscaped(), text);
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

/* A text document of the list, with the snapshot pictures it names. */
static void fillDocument(QTextDocument &doc, const QString &html, ShowTreeController *st,
                         const QList<Note> &notes)
{
    if (st != nullptr)
        for (const Note &n : notes)
            if (!n.view.image.isEmpty())
            {
                QImage img;
                if (img.loadFromData(st->noteImage(n.view.image)))
                    doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("note:") + n.view.image), img);
            }
    doc.setHtml(html);
}

bool ShowNotesList::exportTo(const QString &path, QString *error)
{
    if (path.endsWith(QLatin1String(".md"), Qt::CaseInsensitive)
        || path.endsWith(QLatin1String(".markdown"), Qt::CaseInsensitive)
        || path.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive))
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if (error != nullptr)
                *error = f.errorString();
            return false;
        }
        f.write(toMarkdown().toUtf8());
        return true;
    }
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(tr("Show notes"));
    QTextDocument doc;
    fillDocument(doc, toHtml(), controller(), filtered());
    doc.print(&writer);
    if (!QFileInfo::exists(path))
    {
        if (error != nullptr)
            *error = tr("Could not write %1").arg(path);
        return false;
    }
    return true;
}

void ShowNotesList::slotExport()
{
    QString selected;
    const QString path = QFileDialog::getSaveFileName(this, tr("Export show notes"), tr("Show notes.pdf"),
                                                      tr("PDF (*.pdf);;Markdown (*.md)"), &selected);
    if (path.isEmpty())
        return;
    QString file = path;
    if (QFileInfo(file).suffix().isEmpty())
        file += selected.contains(QLatin1String("*.md")) ? QStringLiteral(".md") : QStringLiteral(".pdf");
    QString why;
    if (!exportTo(file, &why))
        QMessageBox::warning(this, tr("Export show notes"), why);
}

void ShowNotesList::slotPrint()
{
    QPrinter printer;
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("Print show notes"));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QTextDocument doc;
    fillDocument(doc, toHtml(), controller(), filtered());
    doc.print(&printer);
}
