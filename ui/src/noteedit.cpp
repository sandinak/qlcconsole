/*
  Q Light Controller Plus
  noteedit.cpp

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
#include <QAbstractTextDocumentLayout>
#include <QHelpEvent>
#include <QScrollBar>
#include <QKeyEvent>
#include <QPainter>
#include <QToolTip>
#include <QtMath>
#include <QVariant>

#include "noteedit.h"

NoteEdit::NoteEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptRichText(false);
    setTabChangesFocus(true);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    document()->setDocumentMargin(2);
    connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        m_finished = false;
        fit();
    });
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &NoteEdit::fit);
    setExpanded(false);
}

QString NoteEdit::text() const
{
    return toPlainText();
}

void NoteEdit::setText(const QString &text)
{
    if (!m_expanded)
        m_original = text;
    if (text != toPlainText())
    {
        const bool finished = m_finished;
        setPlainText(text);
        m_finished = finished;
        if (!m_expanded)
            verticalScrollBar()->setValue(0);
    }
    updateTip();
    viewport()->update();
}

void NoteEdit::setHelpTip(const QString &tip)
{
    m_help = tip;
    updateTip();
}

void NoteEdit::setMaxLines(int lines)
{
    m_maxLines = qMax(1, lines);
    fit();
}

void NoteEdit::setAnchorRect(const QRect &cell)
{
    m_floating = true;
    m_anchor = cell;
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    fit();
}

QString NoteEdit::flattened(const QString &text)
{
    QString t = text;
    t.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    t.replace(QChar(0x2028), QLatin1Char('\n'));
    return t.split(QLatin1Char('\n')).join(QStringLiteral(" ⏎ "));
}

QString NoteEdit::tipFor(const QString &note, const QString &help)
{
    if (note.trimmed().isEmpty())
        return help;
    QString tip = QStringLiteral("<p style='white-space:pre-wrap'>%1</p>").arg(note.toHtmlEscaped());
    if (!help.isEmpty())
        tip += QStringLiteral("<p><i>%1</i></p>").arg(help.toHtmlEscaped());
    return tip;
}

int NoteEdit::heightFor(int lines) const
{
    const QMargins m = contentsMargins();
    return fontMetrics().lineSpacing() * lines + qCeil(document()->documentMargin() * 2)
           + m.top() + m.bottom() + 2;
}

QSize NoteEdit::sizeHint() const
{
    return QSize(QTextEdit::sizeHint().width(), m_expanded ? height() : heightFor(1));
}

QSize NoteEdit::minimumSizeHint() const
{
    return QSize(fontMetrics().averageCharWidth() * 6, heightFor(1));
}

void NoteEdit::fit()
{
    if (m_fitting)
        return;
    m_fitting = true;
    int h = heightFor(1);
    if (m_expanded)
    {
        const QMargins m = contentsMargins();
        const int doc = qCeil(document()->size().height()) + m.top() + m.bottom() + 2;
        h = qBound(heightFor(1), doc, heightFor(m_maxLines));
    }
    if (m_floating)
    {
        // Over the rows below the cell; up, if the view ends first.
        int y = m_anchor.top();
        if (QWidget *p = parentWidget())
            if (y + h > p->height())
                y = qMax(0, p->height() - h);
        setGeometry(m_anchor.left(), y, m_anchor.width(), qMax(h, m_anchor.height()));
    }
    else if (height() != h || maximumHeight() != h)
    {
        setFixedHeight(h);
    }
    if (m_expanded)
        ensureCursorVisible();
    m_fitting = false;
}

void NoteEdit::setExpanded(bool on)
{
    m_expanded = on;
    setVerticalScrollBarPolicy(on ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (!on)
    {
        QTextCursor c = textCursor();
        c.movePosition(QTextCursor::Start);
        setTextCursor(c);
        verticalScrollBar()->setValue(0);
    }
    fit();
    updateGeometry();
    viewport()->update();
}

void NoteEdit::updateTip()
{
    setToolTip(tipFor(text(), m_help));
}

void NoteEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))
        {
            textCursor().insertText(QStringLiteral("\n"));
        }
        else
        {
            m_finished = true;
            m_original = text();
            updateTip();
            emit editingFinished();
            if (!m_floating)
                clearFocus();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && !m_floating)
    {
        // Put the note back as it was, and close up.
        const QString was = m_original;
        setPlainText(was);
        m_finished = true;
        clearFocus();
        event->accept();
        return;
    }
    QTextEdit::keyPressEvent(event);
}

void NoteEdit::focusInEvent(QFocusEvent *event)
{
    if (event->reason() != Qt::PopupFocusReason)
    {
        m_original = text();
        m_finished = false;
        setExpanded(true);
    }
    QTextEdit::focusInEvent(event);
}

void NoteEdit::focusOutEvent(QFocusEvent *event)
{
    QTextEdit::focusOutEvent(event);
    if (event->reason() == Qt::PopupFocusReason)
        return;     // its context menu
    // As a QLineEdit: leaving the field commits it. An item view's editor
    // is committed by its delegate instead.
    if (!m_finished && !m_floating)
    {
        m_finished = true;
        m_original = text();
        emit editingFinished();
    }
    m_finished = true;
    updateTip();
    if (!m_floating && event->reason() != Qt::ActiveWindowFocusReason)
        setExpanded(false);
}

void NoteEdit::paintEvent(QPaintEvent *event)
{
    if (m_expanded)
    {
        QTextEdit::paintEvent(event);
        return;
    }
    // Closed up: the note on one line, elided.
    QPainter p(viewport());
    const int margin = qCeil(document()->documentMargin());
    const QRect r = viewport()->rect().adjusted(margin + 1, 0, -margin, 0);
    QString t = flattened(text());
    if (t.isEmpty())
    {
        if (placeholderText().isEmpty())
            return;
        p.setPen(palette().color(QPalette::PlaceholderText));
        t = placeholderText();
    }
    else
    {
        p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::Text));
    }
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(t, Qt::ElideRight, r.width()));
}

void NoteEdit::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    fit();
}

/****************************************************************************
 * NoteDelegate
 ****************************************************************************/

NoteDelegate::NoteDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *NoteDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    // On the view's window, not its viewport: a short step list (the
    // walker's) must not clip a note opened up over the rows below it.
    NoteEdit *e = new NoteEdit(parent != nullptr ? parent->window() : nullptr);
    e->setObjectName(QStringLiteral("noteCellEditor"));
    e->setProperty("noteViewport", QVariant::fromValue<QObject *>(parent));
    e->setAutoFillBackground(true);
    e->raise();
    connect(e, &NoteEdit::editingFinished, this, [this, e]() {
        NoteDelegate *self = const_cast<NoteDelegate *>(this);
        emit self->commitData(e);
        emit self->closeEditor(e, QAbstractItemDelegate::NoHint);
    });
    return e;
}

void NoteDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    NoteEdit *e = qobject_cast<NoteEdit *>(editor);
    if (e == nullptr)
        return QStyledItemDelegate::setEditorData(editor, index);
    if (!e->isExpanded())   // never under what is being typed
        e->setText(index.data(Qt::EditRole).toString());
}

void NoteDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    NoteEdit *e = qobject_cast<NoteEdit *>(editor);
    if (e == nullptr)
        return QStyledItemDelegate::setModelData(editor, model, index);
    model->setData(index, e->text(), Qt::EditRole);
}

void NoteDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    NoteEdit *e = qobject_cast<NoteEdit *>(editor);
    QWidget *vp = e != nullptr ? qobject_cast<QWidget *>(e->property("noteViewport").value<QObject *>()) : nullptr;
    QWidget *host = editor->parentWidget();
    if (e == nullptr || vp == nullptr || host == nullptr)
        return QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    // Wider than the cell where the view has room: some 40 characters.
    const QRect cell = option.rect;
    const int want = qMax(cell.width(), option.fontMetrics.averageCharWidth() * 40);
    const int w = qMin(want, qMax(cell.width(), vp->width()));
    const int x = qMax(0, qMin(cell.left(), vp->width() - w));
    const QPoint at = vp->mapTo(host, QPoint(x, cell.top()));
    e->setAnchorRect(QRect(at, QSize(w, cell.height())));
    e->raise();
}

void NoteDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->text = NoteEdit::flattened(option->text);
    option->textElideMode = Qt::ElideRight;
    option->features &= ~QStyleOptionViewItem::WrapText;
}

QSize NoteDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // One line, and no wider than some 30 characters: the rest is elided.
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(qMin(s.width(), option.fontMetrics.averageCharWidth() * 30));
    return s;
}

bool NoteDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                             const QModelIndex &index)
{
    if (event != nullptr && view != nullptr && event->type() == QEvent::ToolTip && index.isValid())
    {
        const QString note = index.data(Qt::DisplayRole).toString();
        if (!note.trimmed().isEmpty())
        {
            QToolTip::showText(event->globalPos(),
                               NoteEdit::tipFor(note, index.data(Qt::ToolTipRole).toString()),
                               view->viewport(), option.rect);
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}
