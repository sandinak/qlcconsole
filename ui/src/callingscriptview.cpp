/*
  Q Light Controller Plus
  callingscriptview.cpp

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

#include <climits>

#include <QSyntaxHighlighter>
#include <QContextMenuEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QPainter>
#include <QCheckBox>
#include <QComboBox>
#include <QMimeData>
#include <QSet>
#include <QPointer>
#include <QFileInfo>
#include <QDataStream>
#include <QTimer>
#include <QLabel>
#include <QMenu>
#include <QFile>

#include "callingscriptview.h"
#include "showtreecontroller.h"
#include "showtreeview.h"
#include "showcontrol.h"
#include "cuerunner.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

/****************************************************************************
 * The text, with the margin the cues are called in
 ****************************************************************************/

namespace {

class HeadingLight : public QSyntaxHighlighter
{
public:
    HeadingLight(QTextDocument *doc, CallingScriptView *view)
        : QSyntaxHighlighter(doc)
        , m_view(view)
    {
    }

protected:
    void highlightBlock(const QString &) override
    {
        const QList<CallingScriptSheet::Line> &lines = m_view->sheet().lines();
        const int n = currentBlock().blockNumber();
        if (n < 0 || n >= lines.count() || !lines.at(n).heading)
            return;
        QTextCharFormat f;
        f.setFontWeight(QFont::Bold);
        setFormat(0, currentBlock().length(), f);
    }

private:
    QPointer<CallingScriptView> m_view;
};

} // namespace

class CallingScriptEdit : public QPlainTextEdit
{
public:
    explicit CallingScriptEdit(CallingScriptView *view);

    int marginWidth() const { return m_marginWidth; }
    void marksChanged();
    void paintMargin(QPaintEvent *event);
    void scrollToLine(int line);
    QStringList marginText(int line) const;
    int firstLine() const { return firstVisibleBlock().blockNumber(); }
    bool lineVisible(int line) const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    class Margin : public QWidget
    {
    public:
        explicit Margin(CallingScriptEdit *edit) : QWidget(edit), m_edit(edit) {}
    protected:
        void paintEvent(QPaintEvent *event) override { m_edit->paintMargin(event); }
    private:
        CallingScriptEdit *m_edit;
    };

    static bool hasCue(const QMimeData *mime);
    static QList<quint32> cuesOf(const QMimeData *mime);
    int wordStartAt(const QPoint &viewportPos) const;
    /** The sheet's line for block @a n, only while the text is what it was laid out from. */
    const CallingScriptSheet::Line *lineOf(int n) const;

    CallingScriptView *m_view;
    int m_marginWidth;
    Margin *m_margin;
    HeadingLight *m_light;
};

CallingScriptEdit::CallingScriptEdit(CallingScriptView *view)
    : QPlainTextEdit(view)
    , m_view(view)
    , m_marginWidth(fontMetrics().averageCharWidth() * 38)
    , m_margin(new Margin(this))
    , m_light(new HeadingLight(document(), view))
{
    setObjectName(QStringLiteral("callingScriptText"));
    setReadOnly(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setViewportMargins(m_marginWidth, 0, 0, 0);
    connect(this, &QPlainTextEdit::updateRequest, m_margin, [this](const QRect &r, int dy) {
        if (dy != 0)
            m_margin->scroll(0, dy);
        else
            m_margin->update(0, r.y(), m_margin->width(), r.height());
    });
}

void CallingScriptEdit::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_margin->setGeometry(QRect(cr.left(), cr.top(), m_marginWidth, cr.height()));
}

const CallingScriptSheet::Line *CallingScriptEdit::lineOf(int n) const
{
    const QList<CallingScriptSheet::Line> &lines = m_view->sheet().lines();
    if (n < 0 || n >= lines.count())
        return nullptr;
    const QTextBlock b = document()->findBlockByNumber(n);
    if (!b.isValid() || b.text() != lines.at(n).text)
        return nullptr;                 // being edited: the margin waits for Done
    return &lines.at(n);
}

QStringList CallingScriptEdit::marginText(int line) const
{
    QStringList out;
    const CallingScriptSheet::Line *l = lineOf(line);
    if (l == nullptr)
        return out;
    for (const CallingScriptSheet::Marker &m : l->markers)
        out << QStringLiteral("%1 %2 %3").arg(m.go ? tr("GO") : tr("STBY"), m.number, m.name).trimmed();
    return out;
}

void CallingScriptEdit::paintMargin(QPaintEvent *event)
{
    QPainter p(m_margin);
    p.fillRect(event->rect(), palette().alternateBase());
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    QFont bold = font();
    bold.setBold(true);
    QFont standbyFont = font();
    if (font().pointSizeF() > 0)
        standbyFont.setPointSizeF(qMax(7.0, font().pointSizeF() * 0.85));
    const QColor green(50, 150, 70);
    const QColor amber(210, 140, 20);
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            const int n = block.blockNumber();
            const CallingScriptSheet::Line *l = lineOf(n);
            int x = 4;
            if (l != nullptr)
                for (const CallingScriptSheet::Marker &m : l->markers)
                {
                    const bool now = m.go && m.cueId == m_view->nowCue();
                    const bool next = m.cueId == m_view->standbyCue() && (!m.go || m_view->standbyLine() == n);
                    QColor c = palette().color(QPalette::Text);
                    if (now)
                        c = green;
                    else if (next)
                        c = amber;
                    p.setPen(c);
                    p.setFont(m.go ? bold : standbyFont);
                    const QString t = QStringLiteral("%1 %2 %3").arg(m.go ? tr("GO") : tr("STBY"), m.number, m.name);
                    const QFontMetrics fm(p.font());
                    const int room = m_margin->width() - x - 4;
                    if (room <= 0)
                        break;
                    const QString shown = fm.elidedText(t, Qt::ElideRight, room);
                    p.drawText(x, top, room, bottom - top, Qt::AlignVCenter | Qt::AlignLeft, shown);
                    x += fm.horizontalAdvance(shown) + 10;
                }
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}

void CallingScriptEdit::marksChanged()
{
    QList<QTextEdit::ExtraSelection> sel;
    auto lineSelection = [&](int line, const QColor &c) {
        const QTextBlock b = document()->findBlockByNumber(line);
        if (line < 0 || !b.isValid())
            return;
        QTextEdit::ExtraSelection s;
        s.format.setBackground(c);
        s.format.setProperty(QTextFormat::FullWidthSelection, true);
        s.cursor = QTextCursor(b);
        sel << s;
    };
    if (m_view->standbyLine() >= 0)
        lineSelection(m_view->standbyLine(), QColor(255, 190, 40, 70));
    if (m_view->nowLine() >= 0)
        lineSelection(m_view->nowLine(), QColor(60, 190, 90, 80));
    // The words the cues are called on.
    for (const CallingScriptSheet::Line &l : m_view->sheet().lines())
        for (const CallingScriptSheet::Marker &m : l.markers)
        {
            QTextEdit::ExtraSelection s;
            s.cursor = QTextCursor(document());
            s.cursor.setPosition(qMin(m.offset, document()->characterCount() - 1));
            s.cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
            if (!s.cursor.hasSelection())
                continue;
            s.format.setFontUnderline(true);
            s.format.setUnderlineColor(m.go ? QColor(50, 150, 70) : QColor(210, 140, 20));
            sel << s;
        }
    setExtraSelections(sel);
    m_light->rehighlight();
    m_margin->update();
}

bool CallingScriptEdit::lineVisible(int line) const
{
    const QTextBlock b = document()->findBlockByNumber(line);
    if (!b.isValid())
        return false;
    const QRectF r = blockBoundingGeometry(b).translated(contentOffset());
    return r.top() >= 0 && r.bottom() <= viewport()->height();
}

void CallingScriptEdit::scrollToLine(int line)
{
    const QTextBlock b = document()->findBlockByNumber(line);
    if (!b.isValid())
        return;
    // The scroll bar counts layout lines; put the block a third of the way
    // down the view, so what is coming is below it. (Not by moving the
    // cursor: putting the user's selection back would scroll back to it.)
    int above = 0;
    for (QTextBlock p = document()->begin(); p.isValid() && p != b; p = p.next())
        above += qMax(1, p.lineCount());
    const int lineHeight = qMax(1, fontMetrics().lineSpacing());
    const int inView = qMax(1, viewport()->height() / lineHeight);
    verticalScrollBar()->setValue(qMax(0, above - inView / 3));
}

bool CallingScriptEdit::hasCue(const QMimeData *mime)
{
    return mime != nullptr && mime->hasFormat(QLatin1String(ShowTreeView::cueMimeType()));
}

QList<quint32> CallingScriptEdit::cuesOf(const QMimeData *mime)
{
    QList<quint32> ids;
    if (!hasCue(mime))
        return ids;
    QByteArray data = mime->data(QLatin1String(ShowTreeView::cueMimeType()));
    QDataStream stream(&data, QIODevice::ReadOnly);
    while (!stream.atEnd())
    {
        quint32 id;
        stream >> id;
        if (stream.status() != QDataStream::Ok)
            break;
        ids << id;
    }
    return ids;
}

int CallingScriptEdit::wordStartAt(const QPoint &viewportPos) const
{
    QTextCursor c = cursorForPosition(viewportPos);
    const int at = c.position();
    c.select(QTextCursor::WordUnderCursor);
    return c.hasSelection() && c.selectionStart() <= at ? c.selectionStart() : at;
}

void CallingScriptEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasCue(event->mimeData()))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QPlainTextEdit::dragEnterEvent(event);
}

void CallingScriptEdit::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasCue(event->mimeData()))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QPlainTextEdit::dragMoveEvent(event);
}

void CallingScriptEdit::dropEvent(QDropEvent *event)
{
    if (!hasCue(event->mimeData()))
    {
        QPlainTextEdit::dropEvent(event);
        return;
    }
    const QList<quint32> cues = cuesOf(event->mimeData());
    const int offset = wordStartAt(event->position().toPoint());
    // Several cues dropped on a word: each on the word, in order (the later ones standby-first
    // is the stage manager's to sort out); a plain drop is a GO, Alt a standby.
    const bool standby = event->modifiers() & Qt::AltModifier;
    for (quint32 id : cues)
        m_view->dropCue(id, offset, !standby);
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void CallingScriptEdit::contextMenuEvent(QContextMenuEvent *event)
{
    if (!textCursor().hasSelection())
        setTextCursor(cursorForPosition(event->pos()));
    QMenu *menu = createStandardContextMenu();
    menu->addSeparator();
    const quint32 chosen = m_view->chosenCue();
    if (chosen != invalidId())
    {
        QAction *here = menu->addAction(tr("Cue Here (GO)"), m_view, [this, chosen]() { m_view->cueHere(chosen, true); });
        here->setObjectName(QStringLiteral("scriptCueHereAction"));
        menu->addAction(tr("Standby Here"), m_view, [this, chosen]() { m_view->cueHere(chosen, false); });
    }
    const int line = cursorForPosition(event->pos()).blockNumber();
    if (const CallingScriptSheet::Line *l = lineOf(line))
    {
        QSet<quint32> seen;
        for (const CallingScriptSheet::Marker &m : l->markers)
        {
            if (seen.contains(m.cueId))
                continue;
            seen.insert(m.cueId);
            const quint32 id = m.cueId;
            menu->addAction(tr("Take Cue %1 Off the Script").arg(m.number), m_view, [this, id]() { m_view->removeCue(id); });
        }
    }
    menu->exec(event->globalPos());
    delete menu;
}

/****************************************************************************
 * The view
 ****************************************************************************/

CallingScriptView::CallingScriptView(Doc *doc, quint32 productionId, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_doc(doc)
    , m_production(productionId)
    , m_text(nullptr)
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("callingScriptView"));
    setWindowTitle(tr("Calling Script"));
    setProperty("helpTopic", QStringLiteral("show-tree.md#the-calling-script"));
    resize(900, 640);

    QVBoxLayout *root = new QVBoxLayout(this);
    QHBoxLayout *bar = new QHBoxLayout;
    m_import = new QPushButton(tr("Import…"), this);
    m_import->setObjectName(QStringLiteral("scriptImport"));
    m_import->setToolTip(tr("Use a plain text or Markdown file as the script or lyrics"));
    m_edit = new QPushButton(tr("Edit Text"), this);
    m_edit->setObjectName(QStringLiteral("scriptEdit"));
    m_edit->setCheckable(true);
    m_edit->setToolTip(tr("Type or paste the script. Cues keep to their words as the text changes"));
    m_cue = new QComboBox(this);
    m_cue->setObjectName(QStringLiteral("scriptCue"));
    m_cue->setToolTip(tr("The cue Cue Here places. Drag a cue from the Show Tree onto a word instead"));
    m_cue->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_here = new QPushButton(tr("Cue Here"), this);
    m_here->setObjectName(QStringLiteral("scriptCueHere"));
    m_here->setToolTip(tr("Call this cue's GO at the selected text (or the cursor)"));
    m_standby = new QPushButton(tr("Standby Here"), this);
    m_standby->setObjectName(QStringLiteral("scriptStandbyHere"));
    m_standby->setToolTip(tr("Call this cue's standby at the selected text (or the cursor)"));
    m_follow = new QCheckBox(tr("Follow"), this);
    m_follow->setObjectName(QStringLiteral("scriptFollow"));
    m_follow->setChecked(true);
    m_follow->setToolTip(tr("Scroll to the line being called as the show runs"));
    m_pdf = new QPushButton(tr("PDF…"), this);
    m_pdf->setObjectName(QStringLiteral("scriptPdf"));
    m_pdf->setToolTip(tr("Print the script with its cues in the margin"));
    m_go = new QPushButton(tr("GO"), this);
    m_go->setObjectName(QStringLiteral("scriptGo"));
    m_go->setToolTip(tr("GO the main cue list, as the Run tab's GO does"));
    QFont gf = m_go->font();
    gf.setBold(true);
    m_go->setFont(gf);
    for (QWidget *w : QList<QWidget *>{m_import, m_edit, m_cue, m_here, m_standby})
        bar->addWidget(w);
    bar->addStretch(1);
    for (QWidget *w : QList<QWidget *>{m_follow, m_pdf, m_go})
        bar->addWidget(w);
    root->addLayout(bar);

    m_text = new CallingScriptEdit(this);
    m_text->setPlaceholderText(tr("Paste the script or lyrics here (Edit Text), or Import… a .txt or .md file. "
                                  "Then select a word and press Cue Here, or drag a cue from the Show Tree onto it."));
    root->addWidget(m_text, 1);
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("scriptSummary"));
    root->addWidget(m_summary);

    connect(m_import, &QPushButton::clicked, this, &CallingScriptView::slotImport);
    connect(m_edit, &QPushButton::toggled, this, &CallingScriptView::slotEditToggled);
    connect(m_here, &QPushButton::clicked, this, &CallingScriptView::slotCueHere);
    connect(m_standby, &QPushButton::clicked, this, &CallingScriptView::slotStandbyHere);
    connect(m_pdf, &QPushButton::clicked, this, &CallingScriptView::slotPdf);
    connect(m_go, &QPushButton::clicked, this, [this]() { go(); });
    connect(m_follow, &QCheckBox::toggled, this, [this](bool on) {
        m_forceScroll = on;
        updateMarks(true);
        m_forceScroll = false;
    });
    connect(m_doc->showTree(), &ShowTreeController::changed, this, &CallingScriptView::refresh);
    connect(m_doc->showTree(), &ShowTreeController::hitArmingChanged, this, &CallingScriptView::slotTick);
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, &CallingScriptView::slotTick);

    refresh();
    if (m_sheet.isEmpty())
        m_edit->setChecked(true);
}

QPlainTextEdit *CallingScriptView::textEdit() const
{
    return m_text;
}

Chaser *CallingScriptView::mainChaser() const
{
    const CueList *list = m_doc->showTree()->cueList(mainList());
    return list == nullptr || list->chaser == invalidId() ? nullptr
                                                          : qobject_cast<Chaser *>(m_doc->function(list->chaser));
}

quint32 CallingScriptView::mainList() const
{
    return m_doc->showTree()->mainList(m_production);
}

void CallingScriptView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_cueSource)
        chooseCue(m_cueSource());
    refresh();
    m_timer->start();
}

void CallingScriptView::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}

void CallingScriptView::closeEvent(QCloseEvent *event)
{
    commitEdit();
    QWidget::closeEvent(event);
}

void CallingScriptView::refresh()
{
    if (m_rebuilding)
        return;
    rebuild();
    updateMarks(true);
}

void CallingScriptView::rebuild()
{
    m_rebuilding = true;
    ShowTreeController *st = m_doc->showTree();
    if (st->production(m_production) == nullptr)
    {
        m_rebuilding = false;
        close();
        return;
    }
    m_sheet = CallingScriptSheet::build(m_doc, m_production);
    const QString text = st->callingScript(m_production).text;
    if (!isEditing() && m_text->toPlainText() != text)
        m_text->setPlainText(text);
    fillCues();
    const int placed = m_sheet.markerCount();
    m_summary->setText(m_sheet.isEmpty() ? tr("No script yet")
                                         : tr("%1 — %n cue(s) placed", "", placed)
                                               .arg(m_sheet.title().isEmpty() ? tr("Script") : m_sheet.title()));
    updateButtons();
    m_rebuilding = false;
}

void CallingScriptView::fillCues()
{
    ShowTreeController *st = m_doc->showTree();
    const Production *prod = st->production(m_production);
    const quint32 keep = chosenCue();
    QList<QPair<QString, quint32>> items;
    if (prod != nullptr)
        for (const CueList &l : prod->lists)
        {
            if (l.looks)
                continue;
            for (int i = 0; i < l.cues.count(); ++i)
                items << qMakePair(QStringLiteral("%1  %2").arg(st->cueNumber(l, i), l.cues.at(i).name).trimmed(),
                                   l.cues.at(i).id);
        }
    bool same = items.count() == m_cue->count();
    for (int i = 0; same && i < items.count(); ++i)
        same = m_cue->itemData(i).toUInt() == items.at(i).second && m_cue->itemText(i) == items.at(i).first;
    if (same)
        return;
    QSignalBlocker block(m_cue);
    m_cue->clear();
    for (const auto &it : items)
        m_cue->addItem(it.first, it.second);
    chooseCue(keep);
}

void CallingScriptView::updateButtons()
{
    const bool editing = isEditing();
    const bool can = m_cue->count() > 0 && !m_text->toPlainText().isEmpty();
    m_here->setEnabled(can && !editing);
    m_standby->setEnabled(can && !editing);
    m_pdf->setEnabled(!m_sheet.isEmpty());
    m_go->setEnabled(mainChaser() != nullptr);
}

void CallingScriptView::updateMarks(bool scroll)
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = mainList();
    Chaser *ch = mainChaser();
    const quint32 oldNow = m_nowCue;
    m_nowCue = invalidId();
    m_standbyCue = invalidId();
    if (ch != nullptr)
    {
        m_nowCue = st->runningCue(listId);
        ShowControl *sc = m_doc->showControl();
        const int step = sc->standbyStep(ch);
        if (step >= 0)
            m_standbyCue = st->cueOfStep(listId, step, -1);
    }
    const int oldNowLine = m_nowLine;
    const int oldStandbyLine = m_standbyLine;
    m_nowLine = m_nowCue == invalidId() ? -1 : m_sheet.lineOfCue(m_nowCue, true);
    m_standbyLine = -1;
    if (m_standbyCue != invalidId())
    {
        m_standbyLine = m_sheet.lineOfCue(m_standbyCue, false);
        if (m_standbyLine < 0)
            m_standbyLine = m_sheet.lineOfCue(m_standbyCue, true);
    }
    if (m_text == nullptr)
        return;
    m_text->marksChanged();
    // Follow the show: when the line being called changes (or the standby
    // moves on while nothing runs), bring it into view.
    if (scroll && m_follow->isChecked() && !isEditing()
        && (m_forceScroll || m_nowCue != oldNow || m_nowLine != oldNowLine || m_standbyLine != oldStandbyLine))
    {
        const int line = m_nowLine >= 0 ? m_nowLine : m_standbyLine;
        if (line >= 0 && !isLineVisible(line))
            m_text->scrollToLine(line);
    }
}

void CallingScriptView::slotTick()
{
    if (m_rebuilding)
        return;
    updateMarks(true);
}

int CallingScriptView::firstVisibleLine() const
{
    return m_text->firstLine();
}

bool CallingScriptView::isLineVisible(int line) const
{
    return m_text->lineVisible(line);
}

int CallingScriptView::marginWidth() const
{
    return m_text->marginWidth();
}

QStringList CallingScriptView::marginText(int line) const
{
    return m_text->marginText(line);
}

bool CallingScriptView::follows() const
{
    return m_follow->isChecked();
}

void CallingScriptView::setFollows(bool follow)
{
    m_follow->setChecked(follow);
}

bool CallingScriptView::isEditing() const
{
    return m_edit->isChecked();
}

void CallingScriptView::setEditing(bool editing)
{
    m_edit->setChecked(editing);
}

void CallingScriptView::slotEditToggled(bool on)
{
    if (m_rebuilding)
        return;
    if (on)
    {
        m_text->setReadOnly(false);
    }
    else
    {
        m_text->setReadOnly(true);
        m_doc->showTree()->setScriptText(m_production, m_text->toPlainText());
        emit scriptChanged();
    }
    rebuild();
    updateMarks(false);
}

void CallingScriptView::commitEdit()
{
    if (isEditing())
        m_edit->setChecked(false);
}

quint32 CallingScriptView::chosenCue() const
{
    return m_cue->currentIndex() < 0 ? invalidId() : m_cue->currentData().toUInt();
}

void CallingScriptView::chooseCue(quint32 cueId)
{
    const int i = cueId == invalidId() ? -1 : m_cue->findData(cueId);
    if (i >= 0)
        m_cue->setCurrentIndex(i);
}

int CallingScriptView::placeOffset() const
{
    const QTextCursor c = m_text->textCursor();
    return c.hasSelection() ? c.selectionStart() : c.position();
}

bool CallingScriptView::cueHere(quint32 cueId, bool go)
{
    if (isEditing())
        commitEdit();
    const bool ok = m_doc->showTree()->anchorCue(m_production, cueId, placeOffset(),
                                                 go ? ShowTreeController::ScriptMark::Go
                                                    : ShowTreeController::ScriptMark::Standby);
    if (ok)
    {
        // Calling a script through, the next cue is the one wanted next.
        if (go && m_cue->currentData().toUInt() == cueId && m_cue->currentIndex() + 1 < m_cue->count())
            m_cue->setCurrentIndex(m_cue->currentIndex() + 1);
        emit scriptChanged();
    }
    return ok;
}

bool CallingScriptView::dropCue(quint32 cueId, int offset, bool go)
{
    if (isEditing())
        commitEdit();
    const bool ok = m_doc->showTree()->anchorCue(m_production, cueId, offset,
                                                 go ? ShowTreeController::ScriptMark::Go
                                                    : ShowTreeController::ScriptMark::Standby);
    if (ok)
        emit scriptChanged();
    return ok;
}

bool CallingScriptView::removeCue(quint32 cueId)
{
    const bool ok = m_doc->showTree()->unanchorCue(m_production, cueId, ShowTreeController::ScriptMark::Go, true);
    if (ok)
        emit scriptChanged();
    return ok;
}

void CallingScriptView::slotCueHere()
{
    cueHere(chosenCue(), true);
}

void CallingScriptView::slotStandbyHere()
{
    cueHere(chosenCue(), false);
}

bool CallingScriptView::importFile(const QString &path, QString *error)
{
    commitEdit();
    const bool ok = m_doc->showTree()->importScript(m_production, path, error);
    if (ok)
    {
        setEditing(false);
        refresh();
        emit scriptChanged();
    }
    return ok;
}

void CallingScriptView::slotImport()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import the script or lyrics"), QString(),
                                                      tr("Text (*.txt *.md *.markdown);;All files (*)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!importFile(path, &error))
        QMessageBox::warning(this, tr("Calling Script"), error);
}

bool CallingScriptView::savePdf(const QString &path, QString *error) const
{
    const ShowTree::Production *p = m_doc->showTree()->production(m_production);
    const QString heading = tr("Calling script — %1").arg(p == nullptr ? QString() : p->name)
                            + (m_sheet.title().isEmpty() ? QString() : QStringLiteral(" — ") + m_sheet.title());
    return m_sheet.writePdf(path, heading, error);
}

void CallingScriptView::slotPdf()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the calling script as a PDF"),
                                                      tr("Calling script.pdf"), tr("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!savePdf(path, &error))
        QMessageBox::warning(this, tr("Calling Script"), error);
}

bool CallingScriptView::go()
{
    Chaser *ch = mainChaser();
    if (ch == nullptr)
        return false;
    ShowControl::Request r;
    r.command = ShowControl::Go;
    r.chaser = ch->id();
    const bool ok = CueRunner::runCommand(m_doc, r, ShowControl::RouteRunTab);
    updateMarks(true);
    return ok;
}
