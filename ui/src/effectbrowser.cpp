/*
  Q Light Controller Plus
  effectbrowser.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <algorithm>
#include <QCollator>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QPainter>
#include <QPolygonF>
#include <QRadialGradient>
#include <QTabBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QtMath>
#include <cmath>

#include "naturalcompare.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectbrowser.h"
#include "effectorigin.h"
#include "effectscript.h"
#include "doc.h"

#define SETTINGS_RECENT "lookeditor/recentEffects"

namespace
{
// The look editor's effect kinds (lookeditor.cpp): header, script, preset, WLED.
enum { KindHeader = 0, KindScript = 1, KindPreset = 2, KindWled = 3, KindShow = 4 };
const int KindRole = Qt::UserRole + 1;

// List item data.
const int EntryRole = Qt::UserRole + 10;    ///< index into m_entries
const int PathRole  = Qt::UserRole + 11;    ///< preview script path

const int kLights = 16;

/** A script's number, or 0: it may say undefined, or worse. */
double num(const QJSValue &v)
{
    const double d = v.toNumber();
    return std::isfinite(d) ? d : 0.0;
}
const int kStripW = 16 * 6;

/* A matrix effect is previewed on a pixel box, not a strip: fifteen by five,
   rows of RGB pixels and of white ones (RGB, W, RGB, W, RGB) as a Warmdance
   XL-450 is built -- so what its white setting does shows too. */
const int kBoxCols = 15;
const int kBoxRows = 5;
bool boxRowIsWhite(int row) { return row % 2 == 1; }
}

struct EffectBrowser::Run
{
    EffectScript script;
    QJSValue state;
};

/** A row: the strip of lights running the effect, its name, what it does. */
class EffectRowDelegate final : public QStyledItemDelegate
{
public:
    EffectRowDelegate(EffectBrowser *browser) : QStyledItemDelegate(browser), m_browser(browser) {}

    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &) const override
    {
        return QSize(200, opt.fontMetrics.height() * 2 + 10);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, index);
        o.text.clear();
        o.icon = QIcon();
        o.widget->style()->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);

        const int entry = index.data(EntryRole).toInt();
        if (entry < 0 || entry >= m_browser->m_entries.size())
            return;
        const EffectBrowser::Entry &e = m_browser->m_entries.at(entry);
        p->save();
        const int indent = e.isPreset ? 18 : 0;
        const QRect r = opt.rect.adjusted(6 + indent, 4, -6, -4);

        // What it does, running now: a strip of lights; where heads point,
        // with their trails; or a row of beams.
        const QString path = index.data(PathRole).toString();
        const EffectBrowser::Preview pv = m_browser->m_frames.value(path);
        const int cell = kStripW / kLights;
        const QRect strip(r.left(), r.center().y() - cell / 2 - 1, kStripW, cell + 2);
        if (pv.kind == EffectBrowser::Preview::Matrix)
            paintMatrix(p, QRect(r.left(), r.top() + 1, kStripW, r.height() - 2), pv);
        else if (pv.kind == EffectBrowser::Preview::Position)
            paintPosition(p, QRect(r.left(), r.top() + 1, kStripW, r.height() - 2), pv,
                          m_browser->m_paths.value(path));
        else if (pv.kind == EffectBrowser::Preview::Gobo || pv.kind == EffectBrowser::Preview::Beam)
            paintBeams(p, QRect(r.left(), r.top() + 1, kStripW, r.height() - 2), pv);
        else
        {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(20, 20, 24));
            p->drawRoundedRect(strip.adjusted(-2, -2, 2, 2), 3, 3);
            for (int i = 0; i < kLights; ++i)
            {
                const QColor c = pv.lights.isEmpty() ? QColor(50, 50, 56) : pv.lights.value(i);
                p->setBrush(c);
                p->drawEllipse(QRectF(strip.left() + i * cell + 0.5, strip.top() + 1.5, cell - 1, cell - 1));
            }
        }

        // Its name, and what it does.
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor text = selected ? opt.palette.highlightedText().color() : opt.palette.text().color();
        const int tx = strip.right() + 12;
        QFont bold = opt.font;
        bold.setBold(!e.isPreset);
        p->setFont(bold);
        p->setPen(text);
        const QRect nameRect(tx, r.top(), r.right() - tx, r.height() / 2);
        QString name = e.label;
        if (m_browser->m_search->text().trimmed().length() > 0 && !e.category.isEmpty())
            name += QStringLiteral("   ·  ") + e.category;
        /* Where it came from: nothing for a built-in, "mine" for one saved
           on this computer, the palette mark for the show's own. */
        int badgeW = 0;
        if (e.origin == EffectOrigin::Mine)
            badgeW = QFontMetrics(opt.font).horizontalAdvance(EffectOrigin::tr("mine")) + 10;
        else if (e.origin == EffectOrigin::ThisShow)
            badgeW = 16;
        const QFontMetrics bfm(bold);
        const QString shown = bfm.elidedText(name, Qt::ElideRight, qMax(20, nameRect.width() - badgeW - 6));
        p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, shown);
        if (badgeW > 0)
        {
            const int bx = nameRect.left() + bfm.horizontalAdvance(shown) + 6;
            if (e.origin == EffectOrigin::Mine)
            {
                QFont tiny = opt.font;
                tiny.setPointSizeF(qMax(7.0, tiny.pointSizeF() - 2.0));
                p->setFont(tiny);
                const QRect pill(bx, nameRect.center().y() - QFontMetrics(tiny).height() / 2 - 1,
                                 badgeW - 2, QFontMetrics(tiny).height() + 2);
                QColor fill = opt.palette.highlight().color();
                fill.setAlpha(selected ? 90 : 60);
                p->setPen(Qt::NoPen);
                p->setBrush(fill);
                p->drawRoundedRect(pill, 4, 4);
                p->setPen(text);
                p->drawText(pill, Qt::AlignCenter, EffectOrigin::tr("mine"));
            }
            else
            {
                const QRect mark(bx, nameRect.center().y() - 6, 13, 13);
                QIcon(EffectOrigin::paletteMark()).paint(p, mark);
            }
        }
        QFont small = opt.font;
        small.setPointSizeF(qMax(8.0, small.pointSizeF() - 1.0));
        p->setFont(small);
        QColor dim = text;
        dim.setAlpha(150);
        p->setPen(dim);
        const QRect descRect(tx, r.top() + r.height() / 2, r.right() - tx, r.height() / 2);
        // Made from another effect: the grey line says which (hover: how).
        const QString line = e.basedOn.isEmpty() ? e.description : e.basedOn;
        p->drawText(descRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(small).elidedText(line, Qt::ElideRight, descRect.width()));
        p->restore();
    }

private:
    /* A matrix effect's box: each pixel a square, the white rows' unlit
       pixels a shade warmer than the RGB rows', so the rows read. */
    static void paintMatrix(QPainter *p, const QRect &box, const EffectBrowser::Preview &pv)
    {
        if (pv.cols <= 0 || pv.rows <= 0)
            return;
        const int cell = qMax(2, qMin(box.width() / pv.cols, box.height() / pv.rows));
        const QRect grid(box.left() + (box.width() - cell * pv.cols) / 2,
                         box.top() + (box.height() - cell * pv.rows) / 2,
                         cell * pv.cols, cell * pv.rows);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(20, 20, 24));
        p->drawRoundedRect(grid.adjusted(-2, -2, 2, 2), 3, 3);
        for (int y = 0; y < pv.rows; ++y)
            for (int x = 0; x < pv.cols; ++x)
            {
                QColor c = pv.lights.value(y * pv.cols + x, QColor(30, 30, 34));
                if (c == QColor(30, 30, 34) && boxRowIsWhite(y))
                    c = QColor(38, 36, 34);
                p->setBrush(c);
                p->drawRect(QRectF(grid.left() + x * cell + 0.5, grid.top() + y * cell + 0.5,
                                   cell - 1, cell - 1));
            }
    }

    /* The shape the effect draws (four heads' paths over a few seconds,
       faint), every head where it is now, and those four bright on top --
       a circle reads as a circle, and the spread as heads along it. */
    static void paintPosition(QPainter *p, const QRect &box, const EffectBrowser::Preview &pv,
                              const QVector<QVector<QPointF> > &paths)
    {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(20, 20, 24));
        p->drawRoundedRect(box, 3, 3);
        p->setPen(QPen(QColor(44, 44, 52), 1, Qt::DotLine));
        p->drawLine(box.center().x(), box.top() + 2, box.center().x(), box.bottom() - 2);
        p->drawLine(box.left() + 2, box.center().y(), box.right() - 2, box.center().y());
        p->setRenderHint(QPainter::Antialiasing, true);
        const QRectF in = QRectF(box).adjusted(5, 4, -5, -4);
        static const QColor tints[4] = { QColor(255, 210, 90), QColor(110, 200, 255),
                                         QColor(255, 120, 170), QColor(140, 240, 140) };
        static const int tracked[4] = { 0, 5, 10, 15 };
        const int n = pv.spots.size();

        /* Zoomed to the movement -- a small orbit in 540 degrees of pan would
           be a speck -- one scale both ways, so a circle stays a circle, and
           never closer than a twentieth of the range. */
        QRectF seen;
        auto take = [&seen](const QPointF &q) {
            const QRectF pt(q, QSizeF(1e-6, 1e-6));
            seen = seen.isNull() ? pt : seen.united(pt);
        };
        if (!paths.isEmpty())
        {
            foreach (const QPointF &q, paths.at(0))
                take(q);
        }
        foreach (const QPointF &q, pv.spots)
            take(q);
        const double span = qMax(0.05, qMax(seen.width() / in.width() * in.height(), seen.height()));
        const double scale = in.height() / span;
        const QPointF mid = seen.isNull() ? QPointF(0.5, 0.5) : seen.center();
        auto at = [&](const QPointF &q) {
            return QPointF(in.center().x() + (q.x() - mid.x()) * scale,
                           in.center().y() + (q.y() - mid.y()) * scale);
        };

        // One head's path is the shape (the others run it at their own
        // moment): four of them drawn over each other only scribble.
        if (!paths.isEmpty())
        {
            p->setPen(QPen(QColor(200, 200, 215, 110), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPolygonF line;
            foreach (const QPointF &q, paths.at(0))
                line << at(q);
            p->drawPolyline(line);
        }
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(170, 170, 185, 120));
        for (int h = 0; h < n; ++h)
            p->drawEllipse(at(pv.spots.at(h)), 1.3, 1.3);
        for (int k = 0; k < 4; ++k)
        {
            if (tracked[k] >= n)
                continue;
            p->setBrush(tints[k]);
            p->drawEllipse(at(pv.spots.at(tracked[k])), 2.6, 2.6);
        }
        p->setRenderHint(QPainter::Antialiasing, false);
    }

    /* Eight beams seen end-on: their size (iris, zoom), or which gobo slot
       (its pattern). */
    static void paintBeams(QPainter *p, const QRect &box, const EffectBrowser::Preview &pv)
    {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(20, 20, 24));
        p->drawRoundedRect(box, 3, 3);
        p->setRenderHint(QPainter::Antialiasing, true);
        const int n = qMin(8, pv.beams.size());
        if (n == 0)
            return;
        const double w = double(box.width() - 4) / n;
        const double rMax = qMin(w, double(box.height())) / 2.0 - 1.5;
        for (int i = 0; i < n; ++i)
        {
            const int src = pv.beams.size() <= 8 ? i : i * pv.beams.size() / n;
            const QPointF c(box.left() + 2 + w * (i + 0.5), box.center().y() + 0.5);
            if (pv.kind == EffectBrowser::Preview::Beam)
            {
                const double rr = 1.5 + qBound(0.0, pv.beams.at(src), 1.0) * (rMax - 1.5);
                QRadialGradient g(c, rr);
                g.setColorAt(0.0, QColor(255, 250, 235));
                g.setColorAt(1.0, QColor(255, 240, 210, 60));
                p->setBrush(g);
                p->drawEllipse(c, rr, rr);
                continue;
            }
            // A gobo: the slot's pattern, so a chase reads as patterns moving.
            p->setBrush(QColor(255, 248, 230));
            p->drawEllipse(c, rMax, rMax);
            p->setBrush(QColor(20, 20, 24));
            const int slot = qMax(0, int(std::lround(pv.beams.at(src))));
            const int spokes = 2 + slot % 5;
            for (int k = 0; k < spokes; ++k)
            {
                const double a = (2.0 * M_PI * k) / spokes + slot * 0.4;
                const QPointF d(std::cos(a) * rMax * 0.55, std::sin(a) * rMax * 0.55);
                p->drawEllipse(c + d, rMax * 0.28, rMax * 0.28);
            }
        }
        p->setRenderHint(QPainter::Antialiasing, false);
    }

    EffectBrowser *m_browser;
};

QString EffectBrowser::Entry::key() const
{
    return QString::number(kind) + QLatin1Char(':') + name;
}

EffectBrowser::EffectBrowser(Doc *doc, QComboBox *source, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_source(source)
{
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search effects…"));
    m_search->setClearButtonEnabled(true);
    v->addWidget(m_search);

    /* Where effects came from: the ones that come with qlcconsole, the ones
       saved on this computer, the show's own. */
    QHBoxLayout *origins = new QHBoxLayout;
    origins->setContentsMargins(0, 0, 0, 0);
    origins->setSpacing(2);
    const QStringList names = { tr("Built-in"), tr("Mine"), tr("This show") };
    const QStringList tips = {
        tr("The effects that come with qlcconsole"),
        tr("Effects saved on this computer with Save to My effects…: in any show"),
        tr("This show's effect palettes (Save in this show…): picking one copies it into the look") };
    const char *objs[] = { "effectOriginBuiltIn", "effectOriginMine", "effectOriginShow" };
    for (int i = 0; i < 3; ++i)
    {
        QToolButton *b = new QToolButton(this);
        b->setObjectName(QLatin1String(objs[i]));
        b->setText(names.at(i));
        b->setToolTip(tips.at(i));
        b->setCheckable(true);
        b->setAutoRaise(true);
        connect(b, &QToolButton::toggled, this, [this]() { refill(); });
        m_origin[i] = b;
        origins->addWidget(b);
    }
    origins->addStretch(1);
    v->addLayout(origins);

    m_tabs = new QTabBar(this);
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    // Whole names ("Position", not "Positi…"): scroll arrows if they don't fit.
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->setUsesScrollButtons(true);
    v->addWidget(m_tabs);

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new EffectRowDelegate(this));
    m_list->setUniformItemSizes(false);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMinimumHeight(220);
    v->addWidget(m_list, 1);

    m_previewColours << QColor(255, 40, 40) << QColor(40, 80, 255) << QColor(255, 255, 255);
    m_clock.start();
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(80);
    connect(m_previewTimer, &QTimer::timeout, this, &EffectBrowser::tickPreviews);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { refill(); });
    connect(m_tabs, &QTabBar::currentChanged, this, [this]() {
        if (!m_filling)
            refill();
    });
    connect(m_list, &QListWidget::itemClicked, this, &EffectBrowser::pick);
    connect(m_list, &QListWidget::itemActivated, this, &EffectBrowser::pick);
    // Moving the current row picks it (arrow keys). A refill moves it too,
    // but pick() ignores those (m_filling); re-picking what is already
    // picked changes nothing.
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *now, QListWidgetItem *) {
        if (m_pickWhileBrowsing)
            pick(now);
    });
    // The combo changed by other means (the Browse dialog, a look loading).
    connect(m_source, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_filling)
            return;
        for (int i = 0; i < m_list->count(); ++i)
        {
            const int e = m_list->item(i)->data(EntryRole).toInt();
            if (e >= 0 && e < m_entries.size() && m_entries.at(e).comboIndex == m_source->currentIndex())
            {
                m_list->setCurrentRow(i);
                return;
            }
        }
        m_list->clearSelection();
    });

    reload();
}

EffectBrowser::~EffectBrowser()
{
}

QStringList EffectBrowser::recent()
{
    return QSettings().value(SETTINGS_RECENT).toStringList();
}

void EffectBrowser::setPreviewColours(const QList<QColor> &colours)
{
    m_previewColours = colours;
    if (m_previewColours.isEmpty())
        m_previewColours << QColor(255, 40, 40) << QColor(40, 80, 255) << QColor(255, 255, 255);
}

QStringList EffectBrowser::tabNames() const
{
    QStringList out;
    for (int i = 0; i < m_tabs->count(); ++i)
        out << m_tabs->tabText(i);
    return out;
}

void EffectBrowser::reload()
{
    m_entries.clear();
    EffectScriptCache *cache = (m_doc != nullptr && m_doc->effectScriptRunner() != nullptr)
                                   ? m_doc->effectScriptRunner()->cache() : nullptr;
    QString category;
    QString lastScriptPath;
    QString lastScriptLabel;
    QStringList categories;
    for (int i = 0; i < m_source->count(); ++i)
    {
        const int kind = m_source->itemData(i, KindRole).toInt();
        const QString name = m_source->itemData(i).toString();
        if (kind == KindHeader)
        {
            category = m_source->itemText(i).trimmed();
            categories << category;
            continue;
        }
        if (name.isEmpty())
            continue;                       // "(none)"
        Entry e;
        e.comboIndex = i;
        e.kind = kind;
        e.name = name;
        e.label = name;
        e.category = category;
        e.description = m_source->itemData(i, Qt::ToolTipRole).toString();
        e.isPreset = (kind == KindPreset || kind == KindShow);
        e.origin = m_source->itemData(i, EffectOrigin::OriginRole).toInt();
        e.basedOn = m_source->itemData(i, EffectOrigin::BasedOnRole).toString();
        e.diffTip = m_source->itemData(i, EffectOrigin::DiffTipRole).toString();
        if (kind == KindShow)
            e.label = m_source->itemText(i).remove(QStringLiteral("◆")).trimmed();
        if (cache != nullptr)
        {
            if (kind == KindScript)
                e.scriptPath = lastScriptPath = cache->scriptPath(name);
            else if (kind == KindPreset || kind == KindShow)
                e.scriptPath = lastScriptPath;      // its generator, the row above
            else if (kind == KindWled)
            {
                e.scriptPath = cache->scriptPathForWled(name);
                if (e.scriptPath.isEmpty())
                    e.description = tr("Runs on WLED fixtures (no preview yet)");
                else if (e.description.isEmpty())
                    e.description = tr("Runs on WLED fixtures; others run its port");
            }
        }
        /* A WLED effect with no port runs on a WLED device and nowhere else:
           listed only when the look has one to run on ("not show the effects
           that won't work if a wled instance isn't in the targets") -- or when
           it is what the look already runs, so the list never loses it. */
        if (kind == KindWled && e.scriptPath.isEmpty() && !m_wledTargets
            && i != m_source->currentIndex())
            continue;
        if (kind == KindScript && e.description.isEmpty() && cache != nullptr)
            e.description = cache->scriptMeta(name).description;
        if (kind == KindScript)
            lastScriptLabel = e.label;
        e.sortName = e.isPreset ? lastScriptLabel + QChar(1) + e.label : e.label;
        m_entries << e;
    }

    m_filling = true;
    const QString was = m_tabs->currentIndex() >= 0 ? m_tabs->tabText(m_tabs->currentIndex()) : QString();
    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);
    m_tabs->addTab(tr("Recent"));
    foreach (const QString &c, categories)
    {
        bool any = false;                   // a tab with nothing left to pick goes
        for (const Entry &e : m_entries)
            any = any || e.category == c;
        if (any)
            m_tabs->addTab(c);
        else
            categories.removeAll(c);
    }

    // Open on the tab of what the look has now; else where it was; else the first.
    int open = -1;
    for (const Entry &e : m_entries)
        if (e.comboIndex == m_source->currentIndex())
            open = categories.indexOf(e.category) + 1;
    if (open <= 0)
        open = qMax(0, tabNames().indexOf(was));
    if (open == 0 && recent().isEmpty() && m_tabs->count() > 1)
        open = 1;
    m_tabs->setCurrentIndex(open);
    m_filling = false;
    refill();
}

void EffectBrowser::refill()
{
    m_filling = true;
    m_list->clear();
    const QString q = m_search->text().trimmed();
    const QString tab = m_tabs->currentIndex() >= 0 ? m_tabs->tabText(m_tabs->currentIndex()) : QString();
    const bool recentTab = (m_tabs->currentIndex() == 0);

    // Where it came from: none checked is all of them.
    bool originOn[3] = { false, false, false };
    bool byOrigin = false;
    for (int k = 0; k < 3; ++k)
    {
        originOn[k] = m_origin[k] != nullptr && m_origin[k]->isChecked();
        byOrigin = byOrigin || originOn[k];
    }
    auto originOk = [&](const Entry &e) { return !byOrigin || originOn[qBound(0, e.origin, 2)]; };

    QList<int> shown;
    if (!q.isEmpty() || byOrigin)
    {
        // Searching (or a where-from filter) looks everywhere, whatever tab is open.
        for (int i = 0; i < m_entries.size(); ++i)
        {
            const Entry &e = m_entries.at(i);
            if (!originOk(e))
                continue;
            if (q.isEmpty() || e.label.contains(q, Qt::CaseInsensitive)
                || e.description.contains(q, Qt::CaseInsensitive)
                || e.category.contains(q, Qt::CaseInsensitive) || e.basedOn.contains(q, Qt::CaseInsensitive))
                shown << i;
        }
    }
    else if (recentTab)
    {
        foreach (const QString &key, recent())
            for (int i = 0; i < m_entries.size(); ++i)
                if (m_entries.at(i).key() == key)
                {
                    shown << i;
                    break;
                }
    }
    else
    {
        for (int i = 0; i < m_entries.size(); ++i)
            if (m_entries.at(i).category == tab)
                shown << i;
    }

    /* A to Z ("can we put the sets in alpha order?"), numbers as numbers --
       Pulse 2 before Pulse 10 -- and a preset right under its effect. Recent
       stays in the order it was used. */
    if (!recentTab || !q.isEmpty() || byOrigin)
    {
        std::stable_sort(shown.begin(), shown.end(), [this](int a, int b) {
            return naturalCompare(m_entries.at(a).sortName, m_entries.at(b).sortName) < 0;
        });
    }

    foreach (int i, shown)
    {
        const Entry &e = m_entries.at(i);
        QListWidgetItem *item = new QListWidgetItem(e.label, m_list);
        item->setData(EntryRole, i);
        item->setData(PathRole, e.scriptPath);
        item->setToolTip(e.diffTip.isEmpty() ? e.description
                                             : e.description.isEmpty() ? e.diffTip
                                                                       : e.description + QStringLiteral("\n\n") + e.diffTip);
        if (e.comboIndex == m_source->currentIndex())
            m_list->setCurrentItem(item);
    }
    m_filling = false;
    tickPreviews();
}

void EffectBrowser::pick(QListWidgetItem *item)
{
    if (item == nullptr || m_filling)
        return;
    const int i = item->data(EntryRole).toInt();
    if (i < 0 || i >= m_entries.size())
        return;
    const Entry &e = m_entries.at(i);

    QStringList r = recent();
    r.removeAll(e.key());
    r.prepend(e.key());
    while (r.size() > 12)
        r.removeLast();
    QSettings().setValue(SETTINGS_RECENT, r);

    if (m_source->currentIndex() != e.comboIndex)
        m_source->setCurrentIndex(e.comboIndex);
}

void EffectBrowser::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    m_previewTimer->start();
}

void EffectBrowser::hideEvent(QHideEvent *e)
{
    m_previewTimer->stop();
    QWidget::hideEvent(e);
}

void EffectBrowser::tickPreviews()
{
    // Only what can be seen: a scrolled-off preview costs a script run for
    // nothing.
    const QRect view = m_list->viewport()->rect();
    QSet<QString> done;
    for (int i = 0; i < m_list->count(); ++i)
    {
        QListWidgetItem *item = m_list->item(i);
        if (!m_list->visualItemRect(item).intersects(view))
            continue;
        const QString path = item->data(PathRole).toString();
        if (path.isEmpty() || done.contains(path))
            continue;
        done.insert(path);
        const Preview pv = previewFrame(path);
        m_frames.insert(path, pv);
        if (pv.kind == Preview::Position && !m_paths.contains(path))
            m_paths.insert(path, samplePaths(path));       // once: the shape it draws
    }
    // A cache of runs, not of every script ever scrolled past.
    if (m_runs.size() > 48)
    {
        for (auto it = m_runs.begin(); it != m_runs.end(); )
            it = done.contains(it.key()) ? std::next(it) : m_runs.erase(it);
    }
    m_list->viewport()->update();
}

EffectBrowser::Preview EffectBrowser::previewFrame(const QString &path)
{
    QSharedPointer<Run> run = m_runs.value(path);
    if (!m_runs.contains(path))
    {
        run.reset(new Run);
        if (run->script.load(path))
            run->state = run->script.newState();
        else
            run.clear();
        m_runs.insert(path, run);       // a broken one is not retried every frame
    }
    if (run.isNull())
        return Preview();
    Preview pv = previewWith(run.data(), m_clock.elapsed() / 1000.0, m_steered.contains(path));
    if (pv.kind == Preview::Lights && pv.lights.isEmpty())
    {
        m_runs.insert(path, QSharedPointer<Run>());     // it gave nothing usable
        return pv;
    }
    /* Nothing at all with the joystick held still, and it takes one: a
       follow-spot, waiting to be steered. Steer it. */
    bool dark = pv.kind == Preview::Lights;
    foreach (const QColor &c, pv.lights)
        dark = dark && c == QColor(30, 30, 34);
    if (dark && !m_steered.contains(path))
    {
        bool joystick = false;
        for (const EffectScript::InputDef &d : run->script.inputDefs())
            joystick = joystick || d.name == QLatin1String("x");
        if (joystick)
        {
            m_steered.insert(path);
            pv = previewWith(run.data(), m_clock.elapsed() / 1000.0, true);
        }
    }
    return pv;
}

QVector<QVector<QPointF> > EffectBrowser::samplePaths(const QString &path)
{
    /* Where four heads go over the next eight seconds, from a run of its
       own (the live one keeps its state): the shape the effect draws. */
    static const int tracked[4] = { 0, 5, 10, 15 };
    auto sample = [&](bool steer) {
        QVector<QVector<QPointF> > paths(4);
        Run run;
        if (!run.script.load(path))
            return QVector<QVector<QPointF> >();
        run.state = run.script.newState();
        for (int k = 0; k <= 160; ++k)
        {
            const Preview pv = previewWith(&run, k * 0.05, steer);
            if (pv.kind != Preview::Position)
                return QVector<QVector<QPointF> >();
            for (int h = 0; h < 4; ++h)
                if (tracked[h] < pv.spots.size())
                    paths[h] << pv.spots.at(tracked[h]);
        }
        return paths;
    };
    // The joystick held still: the effect's own shape. Nothing moving then?
    // It is the joystick's (a follow-spot): steer it, there and live.
    QVector<QVector<QPointF> > paths = sample(m_steered.contains(path));
    QRectF seen;
    if (!paths.isEmpty())
    {
        foreach (const QPointF &q, paths.at(0))
            seen = seen.isNull() ? QRectF(q, QSizeF(1e-9, 1e-9)) : seen.united(QRectF(q, QSizeF(1e-9, 1e-9)));
    }
    if (!paths.isEmpty() && seen.width() < 1e-3 && seen.height() < 1e-3)
    {
        m_steered.insert(path);
        paths = sample(true);
    }
    return paths;
}

EffectBrowser::Preview EffectBrowser::previewWith(Run *run, double t, bool steer)
{
    Preview pv;
    QJSEngine *eng = run->script.engine();
    if (run->script.fixtureTypes().contains(QStringLiteral("matrix")))
        return previewMatrix(run, t);
    QJSValue fixtures = eng->newArray(kLights);
    for (int i = 0; i < kLights; ++i)
    {
        QJSValue o = eng->newObject();
        o.setProperty(QStringLiteral("id"), i);
        o.setProperty(QStringLiteral("head"), 0);
        o.setProperty(QStringLiteral("hasRGB"), true);
        o.setProperty(QStringLiteral("hasDimmer"), true);
        o.setProperty(QStringLiteral("hasPanTilt"), true);
        o.setProperty(QStringLiteral("panRange"), 540);
        o.setProperty(QStringLiteral("tiltRange"), 270);
        QJSValue pos = eng->newObject();
        pos.setProperty(QStringLiteral("x"), (i - 7.5) * 0.5);
        pos.setProperty(QStringLiteral("y"), 0.0);
        pos.setProperty(QStringLiteral("z"), 3.0);
        o.setProperty(QStringLiteral("pos"), pos);
        QJSValue grid = eng->newObject();
        grid.setProperty(QStringLiteral("col"), i);
        grid.setProperty(QStringLiteral("row"), 0);
        grid.setProperty(QStringLiteral("cols"), kLights);
        grid.setProperty(QStringLiteral("rows"), 1);
        o.setProperty(QStringLiteral("grid"), grid);
        o.setProperty(QStringLiteral("aimAt"), eng->newObject());
        // An Aim look's target, downstage of the rig: Stage Shape builds on it.
        QJSValue target = eng->newObject();
        target.setProperty(QStringLiteral("x"), 0.0);
        target.setProperty(QStringLiteral("y"), 3.0);
        target.setProperty(QStringLiteral("z"), 0.0);
        o.setProperty(QStringLiteral("sceneTarget"), target);
        fixtures.setProperty(quint32(i), o);
    }
    QJSValue inputs = eng->newObject();
    for (const EffectScript::InputDef &d : run->script.inputDefs())
    {
        // A joystick's X/Y: held in the middle, or -- for an effect that
        // does nothing else (a follow-spot) -- someone steering it slowly.
        if (steer && d.name == QLatin1String("x"))
            inputs.setProperty(d.name, 0.5 + 0.3 * std::sin(t * 0.9 + 1.0));
        else if (steer && d.name == QLatin1String("y"))
            inputs.setProperty(d.name, 0.5 + 0.25 * std::sin(t * 1.4));
        else
            inputs.setProperty(d.name, double(d.defaultValue));
    }
    inputs.setProperty(QStringLiteral("_time"), t);
    inputs.setProperty(QStringLiteral("_bpm"), 120.0);
    inputs.setProperty(QStringLiteral("_beat"), std::fmod(t * 2.0, 1.0));
    inputs.setProperty(QStringLiteral("_beatCount"), int(t * 2.0));

    auto colourJS = [eng](const QColor &c) {
        QJSValue o = eng->newObject();
        o.setProperty(QStringLiteral("r"), c.red());
        o.setProperty(QStringLiteral("g"), c.green());
        o.setProperty(QStringLiteral("b"), c.blue());
        return o;
    };
    QJSValue palettes = eng->newObject();
    QJSValue colours = eng->newArray(quint32(m_previewColours.size()));
    for (int k = 0; k < m_previewColours.size(); ++k)
        colours.setProperty(quint32(k), colourJS(m_previewColours.at(k)));
    QJSValue look = eng->newObject();
    look.setProperty(QStringLiteral("colors"), colours);
    look.setProperty(QStringLiteral("dimmer"), 1.0);
    palettes.setProperty(QStringLiteral("look"), look);
    int nextColour = 0;
    for (const EffectScript::PaletteDef &d : run->script.paletteDefs())
        if (d.type.compare(QLatin1String("color"), Qt::CaseInsensitive) == 0)
            palettes.setProperty(d.name, colourJS(m_previewColours.value(nextColour++ % qMax(1, m_previewColours.size()))));

    QJSValue params = eng->newObject();
    for (const EffectScript::ParamDef &d : run->script.paramDefs())
        if (d.type != QLatin1String("path") && d.type != QLatin1String("image"))
            params.setProperty(d.name, double(d.defaultValue));

    const QJSValue result = run->script.callTick(fixtures, inputs, palettes, params, run->state,
                                                 eng->newObject());
    if (!result.isArray())
        return pv;

    /* What kind of effect it is, from what it sends: pan/tilt or an aim
       point is a position effect, a gobo slot a gobo one, an iris or zoom a
       beam one; anything else lights. */
    const QJSValue first = result.property(0);
    const auto has = [&first](const char *k) { return first.isObject() && first.hasProperty(QLatin1String(k)); };
    if (has("pan") || has("tilt") || has("aimX"))
    {
        pv.kind = Preview::Position;
        for (int i = 0; i < kLights; ++i)
        {
            const QJSValue o = result.property(quint32(i));
            QPointF n(0.5, 0.5);
            if (o.isObject() && o.hasProperty(QStringLiteral("aimX")))
                // The heads hang along x, 3 m up: a few metres either way.
                n = QPointF(0.5 + num(o.property(QStringLiteral("aimX"))) / 12.0,
                            0.5 + num(o.property(QStringLiteral("aimY"))) / 12.0);
            else if (o.isObject())
            {
                if (o.hasProperty(QStringLiteral("pan")))
                    n.setX(num(o.property(QStringLiteral("pan"))) / 540.0);
                if (o.hasProperty(QStringLiteral("tilt")))
                    n.setY(num(o.property(QStringLiteral("tilt"))) / 270.0);
            }
            pv.spots << n;
        }
        return pv;
    }
    if (has("gobo") || has("iris") || has("zoom"))
    {
        pv.kind = has("gobo") ? Preview::Gobo : Preview::Beam;
        const char *key = has("gobo") ? "gobo" : has("iris") ? "iris" : "zoom";
        for (int i = 0; i < kLights; ++i)
        {
            const QJSValue o = result.property(quint32(i));
            pv.beams << (o.isObject() ? num(o.property(QLatin1String(key))) : 0.0);
        }
        return pv;
    }

    QVector<QColor> &out = pv.lights;
    out.resize(kLights);
    for (int i = 0; i < kLights; ++i)
    {
        const QJSValue o = result.property(quint32(i));
        if (!o.isObject())
        {
            out[i] = QColor(30, 30, 34);
            continue;
        }
        const bool hasRgb = o.hasProperty(QStringLiteral("r")) || o.hasProperty(QStringLiteral("g"))
                         || o.hasProperty(QStringLiteral("b"));
        const double dim = o.hasProperty(QStringLiteral("dimmer"))
                               ? qBound(0.0, num(o.property(QStringLiteral("dimmer"))), 1.0) : -1.0;
        if (hasRgb)
        {
            const double m = dim < 0.0 ? 1.0 : dim;
            out[i] = QColor(qBound(0, qRound(qBound(-1.0, num(o.property(QStringLiteral("r"))), 1e6) * m), 255),
                            qBound(0, qRound(qBound(-1.0, num(o.property(QStringLiteral("g"))), 1e6) * m), 255),
                            qBound(0, qRound(qBound(-1.0, num(o.property(QStringLiteral("b"))), 1e6) * m), 255));
        }
        else if (dim >= 0.0)
            out[i] = QColor(qRound(255 * dim), qRound(235 * dim), qRound(200 * dim));
        else if (o.hasProperty(QStringLiteral("a")) || o.hasProperty(QStringLiteral("uv"))
                 || o.hasProperty(QStringLiteral("w")))
        {
            // Amber, UV or white emitters, in their own colours.
            const double a = qBound(0.0, num(o.property(QStringLiteral("a"))) / 255.0, 1.0);
            const double uv = qBound(0.0, num(o.property(QStringLiteral("uv"))) / 255.0, 1.0);
            const double w = qBound(0.0, num(o.property(QStringLiteral("w"))) / 255.0, 1.0);
            out[i] = QColor(qBound(0, qRound(255 * a + 120 * uv + 255 * w), 255),
                            qBound(0, qRound(150 * a + 255 * w), 255),
                            qBound(0, qRound(255 * uv + 255 * w), 255));
        }
        else if (o.hasProperty(QStringLiteral("shutterOpen")))
        {
            // A shutter: open is lit, closed dark.
            const double open = qBound(0.0, num(o.property(QStringLiteral("shutterOpen"))), 1.0);
            out[i] = QColor(qRound(255 * open), qRound(250 * open), qRound(235 * open));
        }
        else
            out[i] = QColor(30, 30, 34);
    }
    return pv;
}

EffectBrowser::Preview EffectBrowser::previewMatrix(Run *run, double t)
{
    Preview pv;
    QJSEngine *eng = run->script.engine();
    const int n = kBoxCols * kBoxRows;
    /* The box's heads as the host hands them over: its 45 RGB heads, then
       its 30 white ones, each where it is in the box. */
    QJSValue fixtures = eng->newArray(quint32(n));
    QVector<QPoint> at;
    for (int row = 0; row < kBoxRows; ++row)
        if (!boxRowIsWhite(row))
            for (int col = 0; col < kBoxCols; ++col)
                at << QPoint(col, row);
    for (int row = 0; row < kBoxRows; ++row)
        if (boxRowIsWhite(row))
            for (int col = 0; col < kBoxCols; ++col)
                at << QPoint(col, row);
    for (int i = 0; i < n; ++i)
    {
        const bool white = boxRowIsWhite(at.at(i).y());
        QJSValue o = eng->newObject();
        o.setProperty(QStringLiteral("id"), 1);
        o.setProperty(QStringLiteral("head"), i);
        o.setProperty(QStringLiteral("heads"), n);
        o.setProperty(QStringLiteral("hasRGB"), true);
        o.setProperty(QStringLiteral("hasDimmer"), true);
        o.setProperty(QStringLiteral("headRGB"), !white);
        o.setProperty(QStringLiteral("headWhite"), white);
        o.setProperty(QStringLiteral("phase"), 0.0);
        QJSValue grid = eng->newObject();
        grid.setProperty(QStringLiteral("col"), at.at(i).x());
        grid.setProperty(QStringLiteral("row"), at.at(i).y());
        grid.setProperty(QStringLiteral("cols"), kBoxCols);
        grid.setProperty(QStringLiteral("rows"), kBoxRows);
        o.setProperty(QStringLiteral("grid"), grid);
        fixtures.setProperty(quint32(i), o);
    }
    QJSValue inputs = eng->newObject();
    inputs.setProperty(QStringLiteral("_time"), t);
    inputs.setProperty(QStringLiteral("_bpm"), 120.0);
    inputs.setProperty(QStringLiteral("_beat"), std::fmod(t * 2.0, 1.0));
    inputs.setProperty(QStringLiteral("_beatPos"), t * 2.0);
    inputs.setProperty(QStringLiteral("_beatCount"), int(t * 2.0));
    QJSValue colours = eng->newArray(quint32(m_previewColours.size()));
    for (int k = 0; k < m_previewColours.size(); ++k)
    {
        QJSValue c = eng->newObject();
        c.setProperty(QStringLiteral("r"), m_previewColours.at(k).red());
        c.setProperty(QStringLiteral("g"), m_previewColours.at(k).green());
        c.setProperty(QStringLiteral("b"), m_previewColours.at(k).blue());
        colours.setProperty(quint32(k), c);
    }
    QJSValue look = eng->newObject();
    look.setProperty(QStringLiteral("colors"), colours);
    look.setProperty(QStringLiteral("dimmer"), 1.0);
    QJSValue palettes = eng->newObject();
    palettes.setProperty(QStringLiteral("look"), look);
    QJSValue params = eng->newObject();
    for (const EffectScript::ParamDef &d : run->script.paramDefs())
        if (d.type != QLatin1String("path") && d.type != QLatin1String("image"))
            params.setProperty(d.name, double(d.defaultValue));

    const QJSValue result = run->script.callTick(fixtures, inputs, palettes, params, run->state,
                                                 eng->newObject());
    if (!result.isArray())
        return pv;
    pv.kind = Preview::Matrix;
    pv.cols = kBoxCols;
    pv.rows = kBoxRows;
    pv.lights.fill(QColor(30, 30, 34), n);
    for (int i = 0; i < n; ++i)
    {
        const QJSValue o = result.property(quint32(i));
        if (!o.isObject())
            continue;
        const auto level = [&o](const char *k) {
            return qBound(0, qRound(num(o.property(QLatin1String(k)))), 255);
        };
        QColor c;
        if (o.hasProperty(QStringLiteral("w")) && !o.hasProperty(QStringLiteral("r")))
        {
            const int w = level("w");
            c = QColor(w, qRound(w * 0.96), qRound(w * 0.88));      // a white pixel: a touch warm
        }
        else if (o.hasProperty(QStringLiteral("r")))
        {
            const int w = level("w");
            c = QColor(qMin(255, level("r") + w), qMin(255, level("g") + w), qMin(255, level("b") + w));
        }
        else
            continue;
        if (c.red() + c.green() + c.blue() > 0)
            pv.lights[at.at(i).y() * kBoxCols + at.at(i).x()] = c;
    }
    return pv;
}
