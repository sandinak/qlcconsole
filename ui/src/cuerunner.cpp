/*
  Q Light Controller Plus
  cuerunner.cpp

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

#include <QTreeWidgetItem>
#include <QProgressBar>
#include <QTreeWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QComboBox>
#include <QShortcut>
#include <QToolTip>
#include <QTextLayout>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QAction>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QSet>

#include <functional>

#include "showtreecontroller.h"
#include "timecodesource.h"
#include "timecodecues.h"
#include "chaserrunner.h"
#include "focuscallsdialog.h"
#include "venuecorrections.h"
#include "focuscalls.h"
#include "focusmenu.h"
#include "programmercontroller.h"
#include "focuscallcue.h"
#include "cuerunner.h"
#include "vcbutton.h"
#include "usedby.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
enum { RoleStep = Qt::UserRole, RoleSub = Qt::UserRole + 1, RoleCue = Qt::UserRole + 2,
       RoleColour = Qt::UserRole + 3, RoleSection = Qt::UserRole + 4, RoleSectionKey = Qt::UserRole + 5 };
enum { ColState = 0, ColCue, ColName, ColTiming, ColNote };

QString seconds(uint ms)
{
    return QStringLiteral("%1s").arg(ms / 1000.0);
}
} // namespace

static QPointer<QAction> s_upgradeAction;

void CueRunner::setUpgradeAction(QAction *action)
{
    s_upgradeAction = action;
}

bool CueRunner::runCommand(Doc *doc, const ShowControl::Request &request, ShowControl::Route route)
{
    return doc != nullptr && doc->showControl()->run(request, route);
}

QAction *CueRunner::upgradeAction()
{
    return s_upgradeAction.data();
}

bool CueRunner::isEmptyStateShown() const
{
    return m_empty != nullptr && !m_empty->isHidden();
}

CueRunner::CueRunner(Doc *doc, Size size, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_size(size)
    , m_follow(invalidId())
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("cueRunner"));
    setProperty("helpTopic", QStringLiteral("show-tree.md#the-cue-runner"));
    buildLayout();

    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &CueRunner::refresh);
    ShowTreeController *st = m_doc->showTree();
    connect(st, &ShowTreeController::changed, this, [this]() {
        m_treeDirty = true;
        fillFollowBox();     // a production or list added, renamed or removed
    });
    connect(st, &ShowTreeController::currentProductionChanged, this, [this]() {
        m_treeDirty = true;
        fillFollowBox();
        refresh();
    });
    connect(m_doc->showControl(), &ShowControl::mainCueListChanged, this, [this]() { m_treeDirty = true; });
    connect(m_doc->showControl(), &ShowControl::commandHandled, this, [this]() { refresh(); });
    refresh();
}

CueRunner::~CueRunner()
{
}

QString CueRunner::sizeToString(Size size)
{
    switch (size)
    {
        case Full: return QStringLiteral("Full");
        case List: return QStringLiteral("List");
        default: return QStringLiteral("Compact");
    }
}

CueRunner::Size CueRunner::stringToSize(const QString &str)
{
    if (str == QLatin1String("Full"))
        return Full;
    if (str == QLatin1String("List"))
        return List;
    return Compact;
}

/****************************************************************************
 * Layout
 ****************************************************************************/

/* A label that wraps to at most maxLines() lines and elides the rest (the
   whole text in its tooltip) instead of growing the runner. */
class CueRunnerLabel final : public QLabel
{
public:
    explicit CueRunnerLabel(QWidget *parent) : QLabel(parent) {}

    int maxLines() const { return m_maxLines; }
    void setMaxLines(int lines)
    {
        if (lines == m_maxLines)
            return;
        m_maxLines = qMax(1, lines);
        updateGeometry();
        update();
    }
    /** How many lines the text takes at @a width (up to @a limit). */
    int linesAt(int width, int limit) const
    {
        if (width <= 0 || text().isEmpty())
            return 1;
        QTextLayout tl(text(), font());
        tl.beginLayout();
        int n = 0;
        for (QTextLine line = tl.createLine(); line.isValid() && n < limit; line = tl.createLine())
        {
            line.setLineWidth(width);
            ++n;
        }
        tl.endLayout();
        return qMax(1, n);
    }

    QSize sizeHint() const override
    {
        const int w = contentsRect().width() > 0 ? contentsRect().width() : 80;
        const int lines = m_maxLines > 1 ? linesAt(w, m_maxLines) : 1;
        return QSize(80, lines * fontMetrics().lineSpacing() + contentsMargins().top() + contentsMargins().bottom());
    }
    QSize minimumSizeHint() const override
    {
        return QSize(fontMetrics().horizontalAdvance(QStringLiteral("…")), sizeHint().height());
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, foregroundRole()));
        p.setFont(font());
        const QRect r = contentsRect();
        const QFontMetrics fm = fontMetrics();
        if (m_maxLines <= 1)
        {
            p.drawText(r, int(alignment()) | Qt::TextSingleLine, fm.elidedText(text(), Qt::ElideRight, r.width()));
            return;
        }
        // Wrap; the last line that fits takes the rest, elided.
        QTextLayout tl(text(), font());
        tl.beginLayout();
        int y = r.top();
        int n = 0;
        for (QTextLine line = tl.createLine(); line.isValid(); line = tl.createLine())
        {
            line.setLineWidth(r.width());
            if (++n == m_maxLines || y + 2 * fm.lineSpacing() > r.bottom() + 1)
            {
                const QString rest = text().mid(line.textStart());
                p.drawText(QRect(r.left(), y, r.width(), fm.lineSpacing()), Qt::AlignLeft | Qt::TextSingleLine,
                           fm.elidedText(rest, Qt::ElideRight, r.width()));
                break;
            }
            p.drawText(QRect(r.left(), y, r.width(), fm.lineSpacing()), Qt::AlignLeft | Qt::TextSingleLine,
                       text().mid(line.textStart(), line.textLength()));
            y += fm.lineSpacing();
        }
        tl.endLayout();
    }
    bool event(QEvent *ev) override
    {
        if (ev->type() == QEvent::ToolTip
            && fontMetrics().horizontalAdvance(text()) > contentsRect().width() * m_maxLines)
        {
            QToolTip::showText(static_cast<QHelpEvent *>(ev)->globalPos(), text(), this);
            return true;
        }
        return QLabel::event(ev);
    }

private:
    int m_maxLines = 1;
};

/* A handle inside the runner: a bar between the info and the buttons
   (dragged up and down) or between GO and Sub GO (left and right). */
class CueRunnerHandle final : public QWidget
{
public:
    CueRunnerHandle(Qt::Orientation drag, QWidget *parent)
        : QWidget(parent)
        , m_drag(drag)
    {
        if (drag == Qt::Vertical)
        {
            setFixedHeight(kThickness);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setCursor(Qt::SplitVCursor);
        }
        else
        {
            setFixedWidth(kThickness);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            setCursor(Qt::SplitHCursor);
        }
    }

    static const int kThickness = 8;
    /** Called with how far it moved since the press, in pixels. */
    std::function<void()> pressed;
    std::function<void(int)> dragged;

protected:
    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton)
            return QWidget::mousePressEvent(ev);
        m_from = ev->globalPosition().toPoint();
        m_dragging = true;
        if (pressed)
            pressed();
        ev->accept();
    }
    void mouseMoveEvent(QMouseEvent *ev) override
    {
        if (!m_dragging)
            return QWidget::mouseMoveEvent(ev);
        const QPoint d = ev->globalPosition().toPoint() - m_from;
        if (dragged)
            dragged(m_drag == Qt::Vertical ? d.y() : d.x());
        ev->accept();
    }
    void mouseReleaseEvent(QMouseEvent *ev) override
    {
        m_dragging = false;
        ev->accept();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = palette().color(underMouse() || m_dragging ? QPalette::Highlight : QPalette::Mid);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        // A short grip in the middle: three dots.
        const QPointF mid = QRectF(rect()).center();
        for (int i = -1; i <= 1; ++i)
        {
            const QPointF at = m_drag == Qt::Vertical ? mid + QPointF(i * 7, 0) : mid + QPointF(0, i * 7);
            p.drawEllipse(at, 1.6, 1.6);
        }
        QColor line = c;
        line.setAlpha(90);
        p.setPen(QPen(line, 1, Qt::DashLine));
        if (m_drag == Qt::Vertical)
            p.drawLine(QPointF(0, mid.y()), QPointF(width(), mid.y()));
        else
            p.drawLine(QPointF(mid.x(), 0), QPointF(mid.x(), height()));
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }

private:
    Qt::Orientation m_drag;
    QPoint m_from;
    bool m_dragging = false;
};

namespace
{
QString chipStyle(const QString &background, bool small)
{
    return small ? QStringLiteral("QLabel { color: white; background: %1; padding: 0px 4px; border-radius: 3px; }")
                   .arg(background)
                 : QStringLiteral("QLabel { color: white; background: %1; padding: 1px 6px; }").arg(background);
}

int pixelSize(const QFont &f)
{
    return QFontInfo(f).pixelSize();
}

void setPixelSize(QWidget *w, int px, bool bold)
{
    QFont f = w->font();
    if (f.pixelSize() == px && f.bold() == bold)
        return;
    f.setPixelSize(px);
    f.setBold(bold);
    w->setFont(f);
}
} // namespace

void CueRunner::buildLayout()
{
    if (m_size == Full)
        buildFullLayout();
    else
        buildPanelLayout();
    fillFollowBox();
}

QToolButton *CueRunner::makeButton(const QString &text, const QString &tip, const char *name)
{
    QToolButton *b = new QToolButton(this);
    b->setText(text);
    b->setToolTip(tip);
    b->setObjectName(QLatin1String(name));
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return b;
}

void CueRunner::makeButtons()
{
    m_go = makeButton(tr("GO"), tr("GO (⌘↩, Space in the Run tab): the loaded cue, else the next"), "crGo");
    m_go->setStyleSheet(QStringLiteral("QToolButton { background: #2e7d32; color: white; border-radius: 4px; }"
                                       "QToolButton:disabled { background: #5d7f5f; color: #c8d6c9; }"));
    m_subGo = makeButton(tr("Sub GO"), tr("Sub GO (⌥⌘↩): the next sub-cue of the cue playing"), "crSubGo");
    m_subBack = makeButton(tr("Sub Back"), tr("Sub Back (⌥⌘⇧↩): the previous sub-cue"), "crSubBack");
    m_back = makeButton(tr("Back"), tr("Back (⌘⇧↩): the previous main cue (its sub-cues dropped)"), "crBack");
    m_pause = makeButton(tr("Pause"), tr("Pause / Resume: holds the fade and an auto-follow's wait"), "crPause");
    m_stop = makeButton(tr("Stop"), tr("Stop this list (⌘⇧Esc stops everything)"), "crStop");
    connect(m_go, &QToolButton::clicked, this, &CueRunner::go);
    connect(m_subGo, &QToolButton::clicked, this, &CueRunner::subGo);
    connect(m_subBack, &QToolButton::clicked, this, &CueRunner::subBack);
    connect(m_back, &QToolButton::clicked, this, &CueRunner::back);
    connect(m_pause, &QToolButton::clicked, this, &CueRunner::pauseResume);
    connect(m_stop, &QToolButton::clicked, this, &CueRunner::stop);
}

void CueRunner::makeFollowBox()
{
    m_followBox = new QComboBox(this);
    m_followBox->setObjectName(QStringLiteral("crFollow"));
    m_followBox->setToolTip(tr("Which cue list this runner plays: the current production's main list (what "
                               "the footer GO and show control play), or one list of any production"));
    connect(m_followBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setFollow(m_followBox->itemData(index).toUInt());
    });
}

void CueRunner::makeTree()
{
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("crCues"));
    m_tree->setHeaderLabels({ QString(), tr("Cue"), tr("Name"), tr("Timing"), tr("Note") });
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_tree->setToolTip(tr("Click a cue: LOAD it (it becomes STANDBY, what GO plays). Double-click: jump to "
                          "it now (a sub-cue: its cue first)."));
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        load(rowCue(item));
        pickRowCue(rowCue(item));
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        jump(rowCue(item));
    });
}

/* The state column fits its longest word, bold, on a sub-cue's row: a
   lesson has students read "▷ STANDBY", so it mustn't end in "…" (Q115). */
void CueRunner::fitStateColumn()
{
    if (m_tree == nullptr)
        return;
    QFont bold = m_tree->font();
    bold.setBold(true);
    const QFontMetrics fm(bold);
    int text = 0;
    for (const QString &s : { tr("▷ STANDBY"), tr("SUB NEXT"), tr("NEXT"), tr("▶ %1%").arg(100) })
        text = qMax(text, fm.horizontalAdvance(s));
    const int margin = m_tree->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, m_tree) + 1;
    const int width = 2 * m_tree->indentation() + text + 2 * margin + fm.averageCharWidth();
    if (m_tree->header()->sectionSize(ColState) < width)
        m_tree->header()->resizeSection(ColState, width);
}

/* The Run tab: big targets, the list, every button. */
void CueRunner::buildFullLayout()
{
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(6);

    QFont big = font();
    big.setPointSizeF(big.pointSizeF() * 1.6);
    QFont huge = font();
    huge.setPointSizeF(huge.pointSizeF() * 2.6);
    huge.setBold(true);
    QFont hf = big;
    hf.setBold(true);

    // Header: production · list, the timecode, LIVE / DESIGN.
    QHBoxLayout *head = new QHBoxLayout();
    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("crHeader"));
    m_header->setFont(hf);
    head->addWidget(m_header, 1);
    makeFollowBox();
    head->addWidget(m_followBox);
    m_timecode = new QLabel(this);
    m_timecode->setObjectName(QStringLiteral("crTimecode"));
    m_timecode->setFont(big);
    m_timecode->setToolTip(tr("The timecode the current production follows (its source, else the show's)"));
    head->addWidget(m_timecode);
    /* Pre-show: Venue focus -- the show's focus calls on the controller,
       corrections saved for this venue (docs/design/FOCUS_REFINE_DESIGN.md). Greyed,
       with why, while there is nothing to focus (board Q146). */
    m_focusCallsBtn = new QToolButton(this);
    m_focusCallsBtn->setObjectName(QStringLiteral("crFocusCalls"));
    m_focusCallsBtn->setText(tr("Venue focus\342\200\246"));
    connect(m_focusCallsBtn, &QToolButton::clicked, this, [this]() {
        FocusCallsDialog::startVenueFocus(m_doc, window());
    });
    head->addWidget(m_focusCallsBtn);
    connect(m_doc->programmer()->focusCalls(), &FocusCalls::changed, this, &CueRunner::refreshFocusCalls);
    connect(m_doc->programmer()->venueCorrections(), &VenueCorrections::activeVenueChanged,
            this, &CueRunner::refreshFocusCalls);
    refreshFocusCalls();
    m_mode = new QLabel(this);
    m_mode->setObjectName(QStringLiteral("crMode"));
    m_mode->setFont(hf);
    m_mode->setToolTip(tr("LIVE (Operate): the cue lists own the stage. DESIGN: the Show Tree previews what "
                          "is selected. ⌘F12 switches."));
    head->addWidget(m_mode);
    v->addLayout(head);

    makeTree();
    m_tree->setFont(big);
    v->addWidget(m_tree, 1);

    // Q62: no main cue list (a 4.x show): what that means, and the way on.
    m_empty = new QWidget(this);
    m_empty->setObjectName(QStringLiteral("crEmpty"));
    {
        QVBoxLayout *ev = new QVBoxLayout(m_empty);
        ev->addStretch(1);
        QLabel *why = new QLabel(tr("This show has no cue list yet. Upgrade turns its chasers into one."),
                                 m_empty);
        why->setObjectName(QStringLiteral("crEmptyText"));
        why->setFont(big);
        why->setAlignment(Qt::AlignCenter);
        why->setWordWrap(true);
        ev->addWidget(why);
        m_upgrade = new QPushButton(tr("Upgrade This Show…"), m_empty);
        m_upgrade->setObjectName(QStringLiteral("crUpgrade"));
        m_upgrade->setFont(big);
        m_upgrade->setToolTip(tr("File ▸ Upgrade This Show…: the show's cue-list chasers become Show Tree "
                                 "cue lists, checked channel for channel. One undo step."));
        connect(m_upgrade, &QPushButton::clicked, this, []() {
            if (QAction *a = CueRunner::upgradeAction())
                a->trigger();
        });
        ev->addWidget(m_upgrade, 0, Qt::AlignHCenter);
        ev->addStretch(2);
    }
    m_empty->hide();
    v->addWidget(m_empty, 1);

    m_now = new QLabel(this);
    m_now->setObjectName(QStringLiteral("crNow"));
    m_now->setFont(hf);
    m_now->setWordWrap(true);
    v->addWidget(m_now);
    /* A focus-call cue armed (docs/design/FOCUS_REFINE_DESIGN.md section 7): waiting for
       the controller's Start; this button, or Cmd+Return, does the same. */
    {
        m_focusArmed = new QWidget(this);
        m_focusArmed->setObjectName(QStringLiteral("crFocusArmed"));
        QHBoxLayout *fh = new QHBoxLayout(m_focusArmed);
        fh->setContentsMargins(0, 0, 0, 0);
        m_focusArmedLabel = new QLabel(m_focusArmed);
        m_focusArmedLabel->setObjectName(QStringLiteral("crFocusArmedText"));
        m_focusArmedLabel->setFont(hf);
        m_focusArmedLabel->setStyleSheet(QStringLiteral("color: #d08a1c;"));
        fh->addWidget(m_focusArmedLabel, 1);
        m_focusStart = new QToolButton(m_focusArmed);
        m_focusStart->setObjectName(QStringLiteral("crFocusStart"));
        m_focusStart->setText(tr("Start venue focus"));
        m_focusStart->setToolTip(tr("Begin venue focus (Start on the controller, or \u2318\u21a9)"));
        connect(m_focusStart, &QToolButton::clicked, this, [this]() {
            m_doc->programmer()->focusCallCue()->start();
        });
        fh->addWidget(m_focusStart);
        v->addWidget(m_focusArmed);
        connect(m_doc->programmer()->focusCallCue(), &FocusCallCue::stateChanged,
                this, &CueRunner::refreshFocusArmed);
        refreshFocusArmed();
    }
    m_hitsNow = new QLabel(this);
    m_hitsNow->setObjectName(QStringLiteral("crHitsNow"));
    m_hitsNow->setWordWrap(true);
    m_hitsNow->setToolTip(tr("VC buttons whose hit for this moment fires instead of a wider one"));
    m_hitsNow->hide();
    v->addWidget(m_hitsNow);
    connect(m_doc->showTree(), &ShowTreeController::hitArmingChanged, this, &CueRunner::refreshHitsNow);
    connect(m_doc->showTree(), &ShowTreeController::changed, this, &CueRunner::refreshHitsNow);
    m_next = new QLabel(this);
    m_next->setObjectName(QStringLiteral("crNext"));
    m_next->setFont(big);
    m_next->setWordWrap(true);
    m_next->setToolTip(tr("What GO plays, and what changes on stage: only the attributes that differ from "
                          "now, from the tracking"));
    v->addWidget(m_next);
    m_subNext = new QLabel(this);
    m_subNext->setObjectName(QStringLiteral("crSubNext"));
    m_subNext->setFont(big);
    m_subNext->setWordWrap(true);
    v->addWidget(m_subNext);
    m_tcNext = new QLabel(this);
    m_tcNext->setObjectName(QStringLiteral("crTimecodeNext"));
    m_tcNext->setFont(big);
    m_tcNext->hide();
    v->addWidget(m_tcNext);

    makeButtons();
    // Green only when GO has something to play (Q62).
    m_go->setStyleSheet(QStringLiteral("QToolButton { background: #2e7d32; color: white; }"
                                       "QToolButton:disabled { background: palette(mid); "
                                       "color: palette(midlight); }"));
    for (QToolButton *b : { m_go, m_subGo })
    {
        b->setFont(huge);
        b->setMinimumHeight(96);
    }
    for (QToolButton *b : { m_subBack, m_back, m_pause, m_stop })
    {
        b->setFont(big);
        b->setMinimumHeight(64);
    }
    QHBoxLayout *bar = new QHBoxLayout();
    bar->addWidget(m_back, 1);
    bar->addWidget(m_subBack, 1);
    bar->addWidget(m_go, 3);
    bar->addWidget(m_subGo, 2);
    bar->addWidget(m_pause, 1);
    bar->addWidget(m_stop, 1);
    v->addLayout(bar, 0);

    // Keyboard GO in the Run tab, as QLab's: Space.
    QShortcut *space = new QShortcut(QKeySequence(Qt::Key_Space), this, nullptr, nullptr,
                                     Qt::WidgetWithChildrenShortcut);
    connect(space, &QShortcut::activated, this, &CueRunner::go);
}

/* The VC sizes: the info area over the buttons, a handle between them
   and one between GO and Sub GO. The info takes the room; layoutPanel()
   sizes the rest. */
void CueRunner::buildPanelLayout()
{
    const bool compact = m_size == Compact;
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(4, 3, 4, 4);
    v->setSpacing(0);

    // The info area.
    m_info = new QWidget(this);
    m_info->setObjectName(QStringLiteral("crInfo"));
    QVBoxLayout *iv = new QVBoxLayout(m_info);
    iv->setContentsMargins(2, 0, 2, 3);
    iv->setSpacing(1);

    QHBoxLayout *head = new QHBoxLayout();
    head->setSpacing(4);
    m_header = new CueRunnerLabel(m_info);
    m_header->setObjectName(QStringLiteral("crHeader"));
    head->addWidget(m_header, 1);
    if (!compact)
    {
        makeFollowBox();
        m_followBox->setParent(m_info);
        m_followBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_followBox->setMinimumContentsLength(8);
        head->addWidget(m_followBox);
    }
    m_timecode = new QLabel(m_info);
    m_timecode->setObjectName(QStringLiteral("crTimecode"));
    m_timecode->setToolTip(tr("The timecode the current production follows (its source, else the show's)"));
    m_timecode->setStyleSheet(QStringLiteral("QLabel { border: 1px solid palette(mid); padding: 0px 4px; "
                                             "border-radius: 3px; }"));
    head->addWidget(m_timecode, 0, Qt::AlignVCenter);
    m_mode = new QLabel(m_info);
    m_mode->setObjectName(QStringLiteral("crMode"));
    m_mode->setToolTip(tr("LIVE (Operate): the cue lists own the stage. DESIGN: the Show Tree previews what "
                          "is selected. ⌘F12 switches."));
    head->addWidget(m_mode, 0, Qt::AlignVCenter);
    iv->addLayout(head);

    if (!compact)
    {
        makeTree();
        m_tree->setParent(m_info);
        iv->addWidget(m_tree, 1);
    }

    m_now = new CueRunnerLabel(m_info);
    m_now->setObjectName(QStringLiteral("crNow"));
    iv->addWidget(m_now);
    m_fade = new QProgressBar(m_info);
    m_fade->setObjectName(QStringLiteral("crFade"));
    m_fade->setRange(0, 100);
    m_fade->setTextVisible(false);
    m_fade->setFixedHeight(3);
    m_fade->setToolTip(tr("The fade of what plays"));
    m_fade->hide();
    iv->addWidget(m_fade);
    m_next = new CueRunnerLabel(m_info);
    m_next->setObjectName(QStringLiteral("crNext"));
    m_next->setToolTip(tr("What GO plays, and what changes on stage: only the attributes that differ from "
                          "now, from the tracking"));
    iv->addWidget(m_next);
    m_subNext = new CueRunnerLabel(m_info);
    m_subNext->setObjectName(QStringLiteral("crSubNext"));
    iv->addWidget(m_subNext);
    m_tcNext = new QLabel(m_info);
    m_tcNext->setObjectName(QStringLiteral("crTimecodeNext"));
    m_tcNext->hide();
    iv->addWidget(m_tcNext);
    if (compact)
    {
        QPalette dim = m_info->palette();
        dim.setColor(QPalette::WindowText, dim.color(QPalette::Disabled, QPalette::WindowText));
        for (int i = 0; i < kMaxUpcoming; ++i)
        {
            QLabel *l = new CueRunnerLabel(m_info);
            l->setObjectName(QStringLiteral("crUpcoming%1").arg(i));
            l->setPalette(dim);
            l->hide();
            iv->addWidget(l);
            m_upcoming << l;
        }
        iv->addStretch(1);
    }
    v->addWidget(m_info, 1);

    m_infoHandle = new CueRunnerHandle(Qt::Vertical, this);
    m_infoHandle->setObjectName(QStringLiteral("crInfoHandle"));
    m_infoHandle->setToolTip(tr("Drag: the buttons' height (Design mode)"));
    m_infoHandle->pressed = [this]() { m_dragFrom = m_go->height(); };
    m_infoHandle->dragged = [this](int dy) {
        setButtonHeight(m_dragFrom - dy);
        emit layoutEdited();
    };
    v->addWidget(m_infoHandle);

    // The buttons: GO | Sub GO, the secondary ones under them.
    makeButtons();
    m_buttons = new QWidget(this);
    m_buttons->setObjectName(QStringLiteral("crButtons"));
    QVBoxLayout *bv = new QVBoxLayout(m_buttons);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(3);
    QHBoxLayout *row = new QHBoxLayout();
    row->setSpacing(0);
    m_go->setParent(m_buttons);
    m_subGo->setParent(m_buttons);
    m_goHandle = new CueRunnerHandle(Qt::Horizontal, m_buttons);
    m_goHandle->setObjectName(QStringLiteral("crGoHandle"));
    m_goHandle->setToolTip(tr("Drag: how much of the row GO takes (Design mode)"));
    m_goHandle->pressed = [this]() { m_dragFrom = m_goShare; };
    m_goHandle->dragged = [this](int dx) {
        const int w = qMax(1, m_go->width() + m_subGo->width());
        setGoShare(m_dragFrom + qRound(100.0 * dx / w));
        emit layoutEdited();
    };
    row->addWidget(m_go, m_goShare);
    row->addWidget(m_goHandle);
    row->addWidget(m_subGo, 100 - m_goShare);
    bv->addLayout(row, 1);

    m_secondary = new QWidget(m_buttons);
    m_secondary->setObjectName(QStringLiteral("crSecondary"));
    QHBoxLayout *sec = new QHBoxLayout(m_secondary);
    sec->setContentsMargins(0, 0, 0, 0);
    sec->setSpacing(3);
    QList<QToolButton *> small { m_back, m_stop };
    if (!compact)
        small = { m_back, m_subBack, m_pause, m_stop };
    for (QToolButton *b : { m_subBack, m_back, m_pause, m_stop })
    {
        b->setParent(m_secondary);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setVisible(small.contains(b));
        if (small.contains(b))
            sec->addWidget(b);
    }
    bv->addWidget(m_secondary);
    v->addWidget(m_buttons, 0);

    setMinimumSize(compact ? QSize(160, 96) : QSize(220, 160));
    applyVisibility();
    applyInteractive();
}

void CueRunner::layoutPanel()
{
    if (m_info == nullptr || m_go == nullptr)
        return;
    const bool compact = m_size == Compact;
    const QMargins mg = layout()->contentsMargins();
    const int avail = height() - mg.top() - mg.bottom();
    const int handle = m_handlesLocked ? 0 : CueRunnerHandle::kThickness;
    const int base = pixelSize(font());
    QVBoxLayout *iv = static_cast<QVBoxLayout *>(m_info->layout());
    const int infoMargins = iv->contentsMargins().top() + iv->contentsMargins().bottom();

    // Header and chips: small, always.
    setPixelSize(m_header, base, true);
    const int chip = qMax(9, int(base * 0.8));
    setPixelSize(m_timecode, chip, false);
    setPixelSize(m_mode, chip, true);
    const int headH = qMax(m_header->sizeHint().height(), m_mode->sizeHint().height());

    // What the info needs at the least: the header, the cue on now, NEXT
    // (list: and some of the list).
    QFont nowMin = font();
    nowMin.setPixelSize(base + 2);
    nowMin.setBold(true);
    const int infoMin = compact ? infoMargins + headH + QFontMetrics(nowMin).lineSpacing()
                                  + QFontMetrics(font()).lineSpacing() + 2 * iv->spacing()
                                : 90;

    // The buttons: the height set for them, less only when there's no room
    // (first the secondary row goes, then GO shrinks to its least).
    const int secondaryH = m_secondary->sizeHint().height() + m_buttons->layout()->spacing();
    int goRow = qBound(int(kMinButtonHeight), m_buttonHeight, int(kMaxButtonHeight));
    bool secondary = m_showSecondary;
    if (secondary && avail - handle - infoMin - secondaryH < kMinButtonHeight)
        secondary = false;
    m_secondary->setVisible(secondary);
    const int secondarySpace = secondary ? secondaryH : 0;
    goRow = qMin(goRow, qMax(24, avail - handle - secondarySpace - infoMin));
    m_buttons->setFixedHeight(goRow + secondarySpace);
    QHBoxLayout *row = static_cast<QHBoxLayout *>(m_buttons->layout()->itemAt(0)->layout());
    row->setStretch(0, m_showSubGo ? m_goShare : 1);
    row->setStretch(2, m_showSubGo ? 100 - m_goShare : 0);
    setPixelSize(m_go, qBound(12, int(goRow * 0.34), 34), true);
    setPixelSize(m_subGo, qBound(11, int(goRow * 0.24), 24), true);

    CueRunnerLabel *next = static_cast<CueRunnerLabel *>(m_next);
    if (!compact)
    {
        setPixelSize(m_now, base + 1, true);
        setPixelSize(m_next, base, false);
        setPixelSize(m_subNext, base, false);
        next->setMaxLines(1);
        return;
    }

    // The info: fonts to its height, NEXT on up to three lines, then as many
    // upcoming cues as fit.
    const int infoH = avail - handle - goRow - secondarySpace;
    const int nowPx = qBound(base + 2, int(infoH * 0.2), qMin(44, qMax(base + 2, width() / 14)));
    const int nextPx = qBound(base, int(nowPx * 0.55), 22);
    setPixelSize(m_now, nowPx, true);
    setPixelSize(m_next, nextPx, false);
    setPixelSize(m_subNext, nextPx, false);
    for (QLabel *l : m_upcoming)
        setPixelSize(l, qMax(base - 1, nextPx - 2), false);

    int used = infoMargins + headH + iv->spacing() + m_now->sizeHint().height();
    if (m_fade->isVisibleTo(this))
        used += iv->spacing() + m_fade->height();
    if (m_subNext->isVisibleTo(this))
        used += iv->spacing() + m_subNext->sizeHint().height();
    const int nextLine = QFontMetrics(m_next->font()).lineSpacing();
    const int textWidth = m_info->width() - iv->contentsMargins().left() - iv->contentsMargins().right();
    const int room = qMax(1, (infoH - used - iv->spacing()) / nextLine);
    next->setMaxLines(qMin(qMin(3, room), next->linesAt(textWidth, 3)));
    used += iv->spacing() + next->maxLines() * nextLine;

    const int line = m_upcoming.first()->sizeHint().height() + iv->spacing();
    const int fit = qMax(0, (infoH - used) / line);
    const int shown = qMin(qMin(m_upcomingCount, int(m_upcomingTexts.count())), fit);
    for (int i = 0; i < m_upcoming.count(); ++i)
    {
        QLabel *l = m_upcoming.at(i);
        if (i < shown && l->text() != m_upcomingTexts.at(i))
            l->setText(m_upcomingTexts.at(i));
        l->setVisible(i < shown);
    }
}

void CueRunner::applyVisibility()
{
    if (m_info == nullptr)
        return;
    m_subGo->setVisible(m_showSubGo);
    m_goHandle->setVisible(m_showSubGo && !m_handlesLocked);
    m_infoHandle->setVisible(!m_handlesLocked);
    m_goHandle->setEnabled(!m_handlesLocked);
    m_infoHandle->setEnabled(!m_handlesLocked);
    m_secondary->setVisible(m_showSecondary);
    layoutPanel();
}

void CueRunner::applyInteractive()
{
    for (QWidget *w : { static_cast<QWidget *>(m_go), static_cast<QWidget *>(m_subGo),
                        static_cast<QWidget *>(m_subBack), static_cast<QWidget *>(m_back),
                        static_cast<QWidget *>(m_pause), static_cast<QWidget *>(m_stop),
                        static_cast<QWidget *>(m_tree), static_cast<QWidget *>(m_followBox) })
        if (w != nullptr)
            w->setAttribute(Qt::WA_TransparentForMouseEvents, !m_interactive);
}

void CueRunner::setUpcomingCount(int count)
{
    m_upcomingCount = qBound(0, count, int(kMaxUpcoming));
    refresh();
    layoutPanel();
}

void CueRunner::setShowTimecode(bool show)
{
    m_showTimecode = show;
    refresh();
}

void CueRunner::setShowSecondary(bool show)
{
    m_showSecondary = show;
    applyVisibility();
}

void CueRunner::setShowSubGo(bool show)
{
    m_showSubGo = show;
    applyVisibility();
}

void CueRunner::setButtonHeight(int px)
{
    m_buttonHeight = qBound(int(kMinButtonHeight), px, int(kMaxButtonHeight));
    layoutPanel();
}

void CueRunner::setGoShare(int percent)
{
    m_goShare = qBound(int(kMinGoShare), percent, int(kMaxGoShare));
    layoutPanel();
}

void CueRunner::setHandlesLocked(bool locked)
{
    m_handlesLocked = locked;
    applyVisibility();
}

void CueRunner::setControlsInteractive(bool interactive)
{
    m_interactive = interactive;
    applyInteractive();
}

QWidget *CueRunner::infoHandle() const
{
    return m_infoHandle;
}

QWidget *CueRunner::goHandle() const
{
    return m_goHandle;
}

QStringList CueRunner::upcomingTexts() const
{
    QStringList out;
    for (QLabel *l : m_upcoming)
        if (l->isVisibleTo(this))
            out << l->text();
    return out;
}

void CueRunner::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    layoutPanel();
}

void CueRunner::setRunnerSize(Size size)
{
    if (size == m_size)
        return;
    m_size = size;
    qDeleteAll(findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
    for (QShortcut *s : findChildren<QShortcut *>(QString(), Qt::FindDirectChildrenOnly))
        delete s;
    delete layout();
    m_header = nullptr;
    m_followBox = nullptr;
    m_tree = nullptr;
    m_info = nullptr;
    m_buttons = nullptr;
    m_secondary = nullptr;
    m_fade = nullptr;
    m_infoHandle = nullptr;
    m_goHandle = nullptr;
    m_upcoming.clear();
    setMinimumSize(0, 0);
    buildLayout();
    m_treeDirty = true;
    refresh();
    layoutPanel();
}

void CueRunner::setFollow(quint32 listId)
{
    if (listId == m_follow)
        return;
    m_follow = listId;
    m_treeDirty = true;
    fillFollowBox();
    refresh();
    emit followChanged();
}

void CueRunner::fillFollowBox()
{
    if (m_followBox == nullptr)
        return;
    ShowTreeController *st = m_doc->showTree();
    const Production *cur = st->production(st->currentProduction());
    QList<QPair<QString, quint32>> items;
    items << qMakePair(cur == nullptr ? tr("The main cue list") : tr("Current production (%1)").arg(cur->name),
                       invalidId());
    for (const Production &p : st->productions())
        for (quint32 lid : st->playableLists(p.id))
            items << qMakePair(QStringLiteral("%1 · %2").arg(p.name, st->cueList(lid)->name), lid);
    // Called on every tree change: rebuilt only when the lists differ, so an
    // open drop-down isn't pulled from under the mouse.
    bool same = m_followBox->count() == items.count();
    for (int i = 0; same && i < items.count(); ++i)
        same = m_followBox->itemText(i) == items.at(i).first
               && m_followBox->itemData(i).toUInt() == items.at(i).second;
    if (!same)
    {
        QSignalBlocker block(m_followBox);
        m_followBox->clear();
        for (const auto &item : items)
            m_followBox->addItem(item.first, item.second);
    }
    {
        QSignalBlocker block(m_followBox);
        m_followBox->setCurrentIndex(qMax(0, m_followBox->findData(m_follow)));
    }
    // One list to play and it is the one played: nothing to choose (Q75).
    m_followBox->setHidden(m_followBox->count() <= 2 && m_follow == invalidId());
}

Chaser *CueRunner::chaser() const
{
    if (m_follow != invalidId())
    {
        const CueList *l = m_doc->showTree()->cueList(m_follow);
        return l == nullptr ? nullptr : qobject_cast<Chaser *>(m_doc->function(l->chaser));
    }
    return m_doc->showControl()->cueList(QString());
}

quint32 CueRunner::listId() const
{
    const Chaser *ch = chaser();
    quint32 lid = invalidId();
    if (ch != nullptr)
        m_doc->showTree()->productionOfChaser(ch->id(), &lid);
    return lid;
}

bool CueRunner::execute(ShowControl::Command command, const QString &cue)
{
    Chaser *ch = chaser();
    if (ch == nullptr)
        return false;
    ShowControl::Request r;
    r.command = command;
    r.cue = cue;
    r.chaser = ch->id();
    // The Run tab speaks for the console (a notice, the Operate offer); a
    // VC Cue Runner is the VC's: the previews hold, quietly.
    const bool ok = runCommand(m_doc, r, m_size == Full ? ShowControl::RouteRunTab
                                                        : ShowControl::RouteVcRunner);
    refresh();
    return ok;
}

void CueRunner::refreshFocusCalls()
{
    if (m_focusCallsBtn == nullptr)
        return;
    FocusMenu::setAvailable(m_focusCallsBtn, FocusMenu::venueFocusBlocked(m_doc),
                            tr("Pre-show: focus the rig for this venue, call by call, on the controller "
                               "(Design mode)"));
}

void CueRunner::refreshFocusArmed()
{
    if (m_focusArmed == nullptr)
        return;
    const FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    switch (fcc->state())
    {
        case FocusCallCue::Armed:
            m_focusArmedLabel->setText((PadNames::isPlayStation(m_doc->programmer()->padFamily())
                                       ? m_doc->programmer()->padText(tr("Venue focus: waiting for the controller \u2014 press {Menu}"))
                                       : tr("Venue focus: waiting for the controller \u2014 press Start")));
            m_focusStart->show();
            break;
        case FocusCallCue::Running:
            m_focusArmedLabel->setText(m_doc->programmer()->padText(tr("Venue focus running \u2014 {Menu} ends it and goes on")));
            m_focusStart->hide();
            break;
        case FocusCallCue::NamingVenue:
            m_focusArmedLabel->setText(tr("Venue focus: name the venue on the laptop"));
            m_focusStart->hide();
            break;
        case FocusCallCue::Idle:
            break;
    }
    m_focusArmed->setVisible(fcc->state() != FocusCallCue::Idle);
}

QString CueRunner::focusArmedText() const
{
    return m_focusArmed != nullptr && !m_focusArmed->isHidden() ? m_focusArmedLabel->text() : QString();
}

void CueRunner::go()
{
    execute(ShowControl::Go);
}

void CueRunner::back()
{
    execute(ShowControl::Back);
}

void CueRunner::subGo()
{
    execute(ShowControl::SubGo);
}

void CueRunner::subBack()
{
    execute(ShowControl::SubBack);
}

void CueRunner::pauseResume()
{
    Chaser *ch = chaser();
    if (ch != nullptr)
        execute(ch->isPaused() ? ShowControl::Resume : ShowControl::Pause);
}

void CueRunner::stop()
{
    execute(ShowControl::Stop);
}

void CueRunner::load(const QString &cue)
{
    if (!cue.isEmpty())
        execute(ShowControl::Load, cue);
}

void CueRunner::jump(const QString &cue)
{
    if (!cue.isEmpty())
        execute(ShowControl::Go, cue);
}

QString CueRunner::stepCue(int step) const
{
    Chaser *ch = chaser();
    if (ch == nullptr || step < 0 || step >= ch->stepsCount())
        return QString();
    ShowTreeController *st = m_doc->showTree();
    if (const CueList *list = st->cueList(listId()))
        return step < list->cues.count() ? st->cueNumber(*list, step) : QString();
    return ch->stepCue(step);
}

void CueRunner::loadNext()
{
    Chaser *ch = chaser();
    if (ch == nullptr || !canLoadNext())
        return;
    load(stepCue(m_doc->showControl()->standbyStep(ch) + 1));
}

void CueRunner::loadPrevious()
{
    Chaser *ch = chaser();
    if (ch == nullptr || !canLoadPrevious())
        return;
    const int standby = m_doc->showControl()->standbyStep(ch);
    load(stepCue(standby < 0 ? ch->stepsCount() - 1 : standby - 1));
}

QString CueRunner::rowCue(const QTreeWidgetItem *item) const
{
    return item == nullptr ? QString() : item->data(ColCue, RoleCue).toString();
}

void CueRunner::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    m_timer->start();
    refresh();
    refreshFocusCalls();    // lights placed on the plot since
}

void CueRunner::pickRowCue(const QString &cue)
{
    // The Run tab, in Design: what you click is what the stage shows; GO
    // then plays it for real (it is on standby). In Operate a click only
    // loads -- only the run is live.
    if (m_size != Full || cue.isEmpty() || m_doc->mode() != Doc::Design)
        return;
    ShowTreeController *st = m_doc->showTree();
    const CueList *list = st != nullptr ? st->cueList(listId()) : nullptr;
    if (list == nullptr)
        return;
    for (int i = 0; i < list->cues.count(); ++i)
        if (st->cueNumber(*list, i) == cue)
        {
            m_pickedCue = list->cues.at(i).id;
            st->pickCue(m_pickedCue);
            return;
        }
}

void CueRunner::hideEvent(QHideEvent *ev)
{
    // Leaving the Run: its pick doesn't stay on the rig (as leaving the
    // Show Tree stops its preview).
    if (m_pickedCue != ShowTree::invalidId() && m_doc->showTree() != nullptr
        && m_doc->showTree()->previewedCue() == m_pickedCue)
        m_doc->showTree()->stopPreview();
    m_pickedCue = ShowTree::invalidId();
    m_timer->stop();
    QWidget::hideEvent(ev);
}

/****************************************************************************
 * What it shows
 ****************************************************************************/

void CueRunner::rebuildTree()
{
    if (m_tree == nullptr)
        return;
    ShowTreeController *st = m_doc->showTree();
    Chaser *ch = chaser();
    const quint32 lid = listId();
    // Q190: the song sections, as header rows the cues fold under.
    const QList<Section> sections = st->sections(lid);
    QStringList sectionSig;
    for (const Section &sec : sections)
        sectionSig << QStringLiteral("%1:%2").arg(sec.first).arg(sectionTitle(sec));
    // Tonight's running order: rows come in the order the list plays.
    const QList<int> playOrder = st->playOrderOf(lid);
    QStringList orderSig;
    for (int at : playOrder)
        orderSig << QString::number(at);
    const QString key = QStringLiteral("%1|%2|%3|%4|%5").arg(ch == nullptr ? 0u : ch->id()).arg(lid)
                        .arg(ch == nullptr ? 0 : ch->stepsCount()).arg(sectionSig.join(QLatin1Char(';')),
                                                                       orderSig.join(QLatin1Char(',')));
    if (!m_treeDirty && key == m_builtFor)
        return;
    m_treeDirty = false;
    m_builtFor = key;
    // Folded sections stay folded across a rebuild.
    QSet<QString> folded;
    for (int t = 0; t < m_tree->topLevelItemCount(); ++t)
        if (QTreeWidgetItem *h = m_tree->topLevelItem(t))
            if (h->data(ColCue, RoleSection).toBool() && !h->isExpanded())
                folded.insert(h->data(ColCue, RoleSectionKey).toString());
    m_tree->clear();
    fitStateColumn();
    if (ch == nullptr)
        return;

    auto row = [](QTreeWidgetItem *item, int step, int sub, const QString &cue, const QString &name,
                  const QString &timing, const QString &note) {
        item->setData(ColCue, RoleStep, step);
        item->setData(ColCue, RoleSub, sub);
        item->setData(ColCue, RoleCue, cue);
        item->setText(ColCue, cue);
        item->setText(ColName, name);
        item->setText(ColTiming, timing);
        item->setText(ColNote, note);
    };

    const CueList *list = st->cueList(lid);
    if (list != nullptr)
    {
        QTreeWidgetItem *sectionItem = nullptr;
        QList<QTreeWidgetItem *> sectionItems;
        int lastSection = -2;
        for (int p = 0; p < playOrder.count(); ++p)
        {
            const int i = playOrder.at(p);
            const int si = sectionOfIndex(sections, i);
            if (si >= 0 && si != lastSection)
            {
                sectionItem = new QTreeWidgetItem(m_tree);
                sectionItem->setData(ColCue, RoleStep, -1);
                sectionItem->setData(ColCue, RoleSub, -1);
                sectionItem->setData(ColCue, RoleSection, true);
                // Its identity, so a fold outlives a rename: the key, or its first cue.
                sectionItem->setData(ColCue, RoleSectionKey, sections.at(si).key.isEmpty()
                                     ? QStringLiteral("#%1").arg(sections.at(si).headCue) : sections.at(si).key);
                sectionItem->setFirstColumnSpanned(true);
                sectionItem->setText(ColState, sectionTitle(sections.at(si)));
                QFont f = m_tree->font();
                f.setBold(true);
                sectionItem->setFont(ColState, f);
                sectionItem->setFlags(Qt::ItemIsEnabled);
                sectionItem->setToolTip(ColState, tr("A song section: click ▸ to fold it"));
                sectionItems << sectionItem;
            }
            else if (si < 0)
                sectionItem = nullptr;
            lastSection = si;
            const Cue &c = list->cues.at(i);
            const int tcFps = m_doc->timecodeSource() != nullptr ? m_doc->timecodeSource()->fps() : 30;
            QStringList timing;
            if (c.fadeIn > 0)
                timing << tr("fade %1").arg(seconds(c.fadeIn));
            if (int(c.hold) >= 0)
                timing << tr("follow %1").arg(seconds(c.hold));
            if (list->followTimecode && c.timecode >= 0 && !c.skip)
                timing << tr("at %1").arg(TimecodeCues::format(c.timecode, tcFps));
            QTreeWidgetItem *item = sectionItem != nullptr ? new QTreeWidgetItem(sectionItem)
                                                           : new QTreeWidgetItem(m_tree);
            row(item, p, -1, st->cueNumber(*list, i), c.name, timing.join(QStringLiteral(" · ")), c.note);
            item->setData(ColCue, RoleColour, st->cueColour(c.id));
            if (const CueList *subs = st->cueList(st->subCueList(c.id)))
            {
                for (int k = 0; k < subs->cues.count(); ++k)
                {
                    const Cue &s = subs->cues.at(k);
                    QStringList t;
                    t << (k == 0 ? tr("with GO") : s.follow ? tr("auto %1").arg(seconds(s.wait)) : tr("Sub GO"));
                    if (s.fadeIn > 0)
                        t << tr("fade %1").arg(seconds(s.fadeIn));
                    QTreeWidgetItem *si = new QTreeWidgetItem(item);
                    row(si, p, k, st->cueNumber(*subs, k), s.name, t.join(QStringLiteral(" · ")), s.note);
                    si->setData(ColCue, RoleColour, st->cueColour(s.id));
                }
                item->setExpanded(true);
            }
        }
        for (QTreeWidgetItem *h : sectionItems)
            h->setExpanded(!folded.contains(h->data(ColCue, RoleSectionKey).toString()));
        return;
    }

    // A plain chaser (the main cue list isn't a tree list): its steps.
    const QList<ChaserStep> steps = ch->steps();
    for (int i = 0; i < steps.count(); ++i)
    {
        const Function *f = m_doc->function(steps.at(i).fid);
        QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
        row(item, i, -1, ch->stepCue(i), f == nullptr ? QString() : f->name(),
            steps.at(i).fadeIn > 0 ? tr("fade %1").arg(seconds(steps.at(i).fadeIn)) : QString(), steps.at(i).note);
        for (int k = 0; k < ch->subCueCount(i); ++k)
        {
            QTreeWidgetItem *si = new QTreeWidgetItem(item);
            row(si, i, k, ch->subCue(i, k), QString(), QString(), QString());
        }
        item->setExpanded(ch->isSubGoStep(i));
    }
}

/* A cue's label as the runner says it: "2.1 Hit", or the step's cue and
   function for a plain chaser. */
static QString cueText(Doc *doc, Chaser *ch, quint32 listId, int step, int sub)
{
    ShowTreeController *st = doc->showTree();
    const quint32 cueId = st->cueOfStep(listId, step, sub);
    if (const CueList *l = st->listOfCue(cueId))
        return st->cueLabel(*l, l->indexOf(cueId));
    if (sub >= 0)
        return ch->subCue(step, sub);
    const QList<ChaserStep> steps = ch->steps();
    const Function *f = step >= 0 && step < steps.count() ? doc->function(steps.at(step).fid) : nullptr;
    return f == nullptr ? ch->stepCue(step) : QStringLiteral("%1 %2").arg(ch->stepCue(step), f->name());
}

void CueRunner::updateStates()
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    Chaser *ch = chaser();
    const quint32 lid = listId();

    // Header.
    {
        QString head;
        if (ch == nullptr)
            head = m_follow == invalidId() ? tr("No main cue list") : tr("No such cue list");
        else
        {
            quint32 pid = st->productionOfChaser(ch->id());
            const Production *p = st->production(pid);
            const CueList *l = st->cueList(lid);
            head = p != nullptr && l != nullptr ? QStringLiteral("%1 · %2").arg(p->name, l->name) : ch->name();
        }
        m_header->setText(head);
        // The Run tab with no main cue list: why, and Upgrade.
        if (m_empty != nullptr)
        {
            const bool empty = ch == nullptr && m_follow == invalidId();
            if (empty != !m_empty->isHidden())
            {
                m_empty->setVisible(empty);
                m_tree->setVisible(!empty);
            }
            m_upgrade->setVisible(upgradeAction() != nullptr);
        }
    }
    // LIVE / DESIGN, the timecode.
    {
        const bool live = m_doc->mode() == Doc::Operate;
        const QString modeText = live ? tr("LIVE") : tr("DESIGN");
        if (m_mode->text() != modeText)
        {
            m_mode->setText(modeText);
            // DESIGN in the theme's own highlight, as the Design | Operate
            // switch: blue is Blind's alone (docs/design/LIVE_BLIND.md).
            QString style = chipStyle(live ? QStringLiteral("#c62828") : QStringLiteral("palette(highlight)"),
                                      m_size != Full);
            if (!live)
                style.replace(QStringLiteral("color: white"), QStringLiteral("color: palette(highlighted-text)"));
            m_mode->setStyleSheet(style);
        }
        const QString tc = timecodeText();
        m_timecode->setText(tc);
        // The timecode only when there is one, as the footer's MTC chip (Q98);
        // the VC sizes only when asked as well.
        TimecodeSource *src = m_doc->timecodeSource();
        const bool hasSource = src != nullptr && src->health() != TimecodeSource::NoSource;
        m_timecode->setHidden(!hasSource || (m_size != Full && !m_showTimecode));

        // The next cue that goes on at a timecode, and how long to wait.
        QString nextText;
        QString tcCue;
        qint32 tcMs = 0;
        if (ch != nullptr && src != nullptr && m_doc->timecodeCues()->next(ch->id(), &tcCue, &tcMs))
        {
            const int fps = src->fps();
            const qint64 left = qint64(tcMs) - qint64(src->positionMs());
            const qint64 s = (left + 999) / 1000;
            const QString wait = !src->isRunning() ? tr("timecode holding")
                               : s >= 60 ? tr("in %1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'))
                                         : tr("in %1 s").arg(s);
            nextText = tr("Timecode: cue %1 at %2, %3").arg(tcCue, TimecodeCues::format(tcMs, fps), wait);
        }
        if (m_tcNext->text() != nextText)
            m_tcNext->setText(nextText);
        m_tcNext->setVisible(!nextText.isEmpty());
    }

    const bool running = ch != nullptr && ch->isRunning();
    const int cur = running ? ch->currentStepIndex() : -1;
    const int curSub = running ? ch->currentSubCueIndex() : -1;
    const int standby = ch == nullptr ? -1 : sc->standbyStep(ch);
    const bool loaded = ch != nullptr && sc->loadedStep(ch->id()) >= 0;
    const int loadedSub = loaded ? sc->loadedSubCue(ch->id()) : -1;
    const int subNext = running && ch->isSubGoStep(cur) ? ch->nextSubCueIndex() : -1;

    // The fade of what plays: the sub-cue's own when it has sub-cues.
    int progress = -1;
    if (running)
    {
        ChaserRunnerStep step(ch->currentRunningStep());
        if (Chaser *inner = qobject_cast<Chaser *>(step.m_function))
            if (inner->isRunning())
                step = inner->currentRunningStep();
        if (step.m_function != nullptr && step.m_fadeIn != Function::defaultSpeed() && step.m_fadeIn > 0
            && step.m_elapsed < step.m_fadeIn)
            progress = int(100.0 * step.m_elapsed / step.m_fadeIn);
    }

    // Rows: done, ▶ running, ▷ STANDBY, NEXT, SUB NEXT.
    if (m_tree != nullptr)
    {
        const QBrush grey = palette().brush(QPalette::Disabled, QPalette::Text);
        const QBrush normal = palette().brush(QPalette::Text);
        const QColor background = m_tree->palette().color(QPalette::Base);
        const QColor alternate = m_tree->palette().color(QPalette::AlternateBase);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        {
            QTreeWidgetItem *item = *it;
            if (item->data(ColCue, RoleSection).toBool())
                continue;
            const int step = item->data(ColCue, RoleStep).toInt();
            const int sub = item->data(ColCue, RoleSub).toInt();
            QString state;
            bool done = false;
            if (sub < 0)
            {
                if (step == cur)
                    state = progress >= 0 && curSub < 0 ? tr("▶ %1%").arg(progress) : tr("▶");
                else if (step == standby)
                    state = loaded && loadedSub < 0 ? tr("▷ STANDBY") : tr("NEXT");
                else
                    done = running && step < cur;
            }
            else if (step == cur)
            {
                if (sub == curSub)
                    state = progress >= 0 ? tr("▶ %1%").arg(progress) : tr("▶");
                else if (sub == subNext)
                    state = tr("SUB NEXT");
                done = sub < curSub;
            }
            if (sub >= 0 && step == standby && loaded && sub == loadedSub)
                state = tr("▷ STANDBY");
            if (item->text(ColState) != state)
                item->setText(ColState, state);
            QFont f = item->font(ColName);
            const bool bold = !state.isEmpty();
            if (f.bold() != bold)
            {
                f.setBold(bold);
                for (int c = 0; c <= ColNote; ++c)
                    item->setFont(c, f);
            }
            // Done: grey. Else the cue's text colour (its own, else its
            // list's), made readable on the theme.
            const QString colour = item->data(ColCue, RoleColour).toString();
            const QBrush fg = done ? grey : colour.isEmpty() ? normal
                                                             : QBrush(readableOn(readableOn(QColor(colour), background),
                                                                                 alternate));
            for (int c = ColCue; c <= ColNote; ++c)
                if (item->foreground(c) != fg)
                    item->setForeground(c, fg);
            if (item->foreground(ColState) != (done ? grey : normal))
                item->setForeground(ColState, done ? grey : normal);
        }
    }

    // Now / next / sub next, each with what changes.
    const QString nextBefore = m_next->text();
    const quint32 nowCue = running ? st->cueOfStep(lid, cur, curSub) : invalidId();
    if (ch == nullptr)
        m_now->setText(QString());
    else if (!running)
        m_now->setText(tr("Stopped"));
    else
    {
        QString t = tr("▶ %1").arg(cueText(m_doc, ch, lid, cur, -1));
        if (curSub >= 0)
            t += QStringLiteral(" · ") + cueText(m_doc, ch, lid, cur, curSub);
        if (ch->isPaused())
            t += tr("  (paused)");
        else if (progress >= 0)
            t += tr("  %1%").arg(progress);
        m_now->setText(t);
    }
    tint(m_now, nowCue, false);

    quint32 nextCue = invalidId();
    if (ch == nullptr || standby < 0)
        m_next->setText(ch == nullptr ? QString() : tr("NEXT: end of the list"));
    else
    {
        // GO into a cue with sub-cues plays its first.
        int toSub = loaded ? loadedSub : -1;
        if (toSub < 0 && ch->isSubGoStep(standby))
            toSub = 0;
        const quint32 toCue = st->cueOfStep(lid, standby, toSub);
        QString t = (loaded ? tr("▷ STANDBY %1") : tr("NEXT %1")).arg(cueText(m_doc, ch, lid, standby, toSub < 0 || !loaded ? -1 : toSub));
        if (toCue != invalidId())
        {
            const QString what = st->changeLine(nowCue, toCue);
            t += QStringLiteral(": ") + (what.isEmpty() ? tr("no change") : what);
        }
        m_next->setText(t);
        nextCue = toCue;
    }
    tint(m_next, nextCue, false);

    quint32 subNextCue = invalidId();
    if (subNext >= 0)
    {
        QString t = tr("SUB NEXT %1").arg(cueText(m_doc, ch, lid, cur, subNext));
        const quint32 toCue = st->cueOfStep(lid, cur, subNext);
        if (toCue != invalidId())
        {
            const QString what = st->changeLine(nowCue, toCue);
            t += QStringLiteral(": ") + (what.isEmpty() ? tr("no change") : what);
        }
        m_subNext->setText(t);
        subNextCue = toCue;
    }
    else if (running && ch->isSubGoStep(cur))
        m_subNext->setText(tr("SUB: end of %1").arg(ch->stepCue(cur)));
    else
        m_subNext->setText(QString());
    tint(m_subNext, subNextCue, false);
    const bool subNextShown = !m_subNext->text().isEmpty();
    const bool relayout = m_subNext->isVisibleTo(this) != subNextShown || m_next->text() != nextBefore
                          || (m_fade != nullptr && m_fade->isVisibleTo(this) != (running && progress >= 0));
    m_subNext->setVisible(subNextShown);
    if (m_fade != nullptr)
    {
        m_fade->setVisible(running && progress >= 0);
        m_fade->setValue(qMax(0, progress));
    }

    // The cues after NEXT (compact).
    if (!m_upcoming.isEmpty())
    {
        QStringList up;
        const int count = ch == nullptr ? 0 : ch->stepsCount();
        for (int i = standby + 1; standby >= 0 && i < count && up.count() < m_upcomingCount; ++i)
        {
            tint(m_upcoming.value(up.count()), st->cueOfStep(lid, i, -1), true);
            up << tr("then %1").arg(cueText(m_doc, ch, lid, i, -1));
        }
        if (up != m_upcomingTexts || relayout)
        {
            m_upcomingTexts = up;
            layoutPanel();
        }
    }
    else if (relayout)
        layoutPanel();

    // Buttons.
    m_go->setEnabled(ch != nullptr && standby >= 0);
    m_subGo->setEnabled(subNext >= 0);
    m_subBack->setEnabled(running && ch->isSubGoStep(cur) && ch->previousSubCueIndex() >= 0);
    m_back->setEnabled(running);
    m_pause->setEnabled(running);
    m_pause->setText(running && ch->isPaused() ? tr("Resume") : tr("Pause"));
    m_stop->setEnabled(running);

    int flags = 0;
    if (m_go->isEnabled())
        flags |= FlagGo;
    if (m_subGo->isEnabled())
        flags |= FlagSubGo;
    if (m_back->isEnabled())
        flags |= FlagBack;
    if (m_subBack->isEnabled())
        flags |= FlagSubBack;
    if (running && ch->isPaused())
        flags |= FlagPaused;
    if (running)
        flags |= FlagStop;
    const int steps = ch == nullptr ? 0 : ch->stepsCount();
    if (standby >= 0 && standby + 1 < steps)
        flags |= FlagLoadNext;
    if (steps > 0 && standby != 0)
        flags |= FlagLoadPrevious;
    if (flags != m_flags)
    {
        m_flags = flags;
        emit statesChanged();
    }
}

void CueRunner::tint(QLabel *label, quint32 cueId, bool dim)
{
    if (label == nullptr)
        return;
    // The theme's (dimmed for the cues after NEXT), else the cue's text
    // colour, made readable on what the label sits on.
    const QWidget *under = label->parentWidget() != nullptr ? label->parentWidget() : this;
    const QPalette base = under->palette();
    QColor c = base.color(dim ? QPalette::Disabled : QPalette::Active, QPalette::WindowText);
    const QString hex = cueId == invalidId() ? QString() : m_doc->showTree()->cueColour(cueId);
    if (!hex.isEmpty())
        c = readableOn(QColor(hex), base.color(QPalette::Window));
    QPalette p = label->palette();
    if (p.color(QPalette::Active, QPalette::WindowText) == c && p.color(QPalette::Inactive, QPalette::WindowText) == c)
        return;
    p.setColor(QPalette::Active, QPalette::WindowText, c);
    p.setColor(QPalette::Inactive, QPalette::WindowText, c);
    label->setPalette(p);
}

QColor CueRunner::textColour(const QString &part) const
{
    const QLabel *l = part == QLatin1String("now") ? m_now
                      : part == QLatin1String("next") ? m_next
                      : part == QLatin1String("subnext") ? m_subNext
                      : part.startsWith(QLatin1String("upcoming")) ? m_upcoming.value(part.mid(8).toInt())
                      : nullptr;
    return l == nullptr ? QColor() : l->palette().color(QPalette::Active, QPalette::WindowText);
}

void CueRunner::refreshHitsNow()
{
    if (m_hitsNow == nullptr)
        return;
    const ShowTreeController *st = m_doc->showTree();
    QStringList lines;
    for (const VCButton *b : UsedBy::allVcButtons())
    {
        const QList<quint32> hits = b->hits();
        if (hits.count() < 2)
            continue;
        const quint32 armed = st->armedHit(hits);
        if (armed == ShowTree::invalidId())
            continue;
        QStringList over;
        for (quint32 h : hits)
            if (h != armed && st->isHitArmed(h) && st->hitOverrides(armed, h) && st->cue(h) != nullptr)
                over << st->cue(h)->name;
        if (!over.isEmpty() && st->cue(armed) != nullptr)
            lines << tr("%1: %2 \u21b3 overrides %3").arg(b->caption().isEmpty() ? tr("button") : b->caption(),
                                                         st->cue(armed)->name, over.join(QStringLiteral(", ")));
    }
    m_hitsNow->setText(lines.join(QStringLiteral("\n")));
    m_hitsNow->setVisible(!lines.isEmpty());
}

void CueRunner::refresh()
{
    rebuildTree();
    updateStates();
}

QString CueRunner::headerText() const
{
    return m_header->text();
}

QString CueRunner::nowText() const
{
    return m_now->text();
}

QString CueRunner::nextText() const
{
    return m_next->text();
}

QString CueRunner::subNextText() const
{
    return m_subNext->text();
}

QString CueRunner::modeText() const
{
    return m_mode->text();
}

QString CueRunner::timecodeText() const
{
    TimecodeSource *tc = m_doc->timecodeSource();
    if (tc == nullptr || tc->health() == TimecodeSource::NoSource)
        return tr("TC —");
    const quint32 ms = tc->positionMs();
    const int fps = qMax(1, tc->fps());
    const QString t = QStringLiteral("%1:%2:%3:%4")
                      .arg(ms / 3600000, 2, 10, QLatin1Char('0'))
                      .arg((ms / 60000) % 60, 2, 10, QLatin1Char('0'))
                      .arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'))
                      .arg(int((ms % 1000) * fps / 1000), 2, 10, QLatin1Char('0'));
    return tc->isRunning() ? tr("TC %1").arg(t) : tr("TC %1 hold").arg(t);
}
