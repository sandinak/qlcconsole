/*
  Q Light Controller Plus
  effectbrowser.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QPainter>
#include <QTabBar>
#include <QTimer>
#include <QtMath>
#include <cmath>

#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectbrowser.h"
#include "effectscript.h"
#include "doc.h"

#define SETTINGS_RECENT "lookeditor/recentEffects"

namespace
{
// The look editor's effect kinds (lookeditor.cpp): header, script, preset, WLED.
enum { KindHeader = 0, KindScript = 1, KindPreset = 2, KindWled = 3 };
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

        // The strip: sixteen lights running it now.
        const QVector<QColor> frame = m_browser->m_frames.value(index.data(PathRole).toString());
        const int cell = kStripW / kLights;
        const QRect strip(r.left(), r.center().y() - cell / 2 - 1, kStripW, cell + 2);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(20, 20, 24));
        p->drawRoundedRect(strip.adjusted(-2, -2, 2, 2), 3, 3);
        for (int i = 0; i < kLights; ++i)
        {
            const QColor c = frame.isEmpty() ? QColor(50, 50, 56) : frame.value(i);
            p->setBrush(c);
            p->drawEllipse(QRectF(strip.left() + i * cell + 0.5, strip.top() + 1.5, cell - 1, cell - 1));
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
        p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(bold).elidedText(name, Qt::ElideRight, nameRect.width()));
        QFont small = opt.font;
        small.setPointSizeF(qMax(8.0, small.pointSizeF() - 1.0));
        p->setFont(small);
        QColor dim = text;
        dim.setAlpha(150);
        p->setPen(dim);
        const QRect descRect(tx, r.top() + r.height() / 2, r.right() - tx, r.height() / 2);
        p->drawText(descRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(small).elidedText(e.description, Qt::ElideRight, descRect.width()));
        p->restore();
    }

private:
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

    m_tabs = new QTabBar(this);
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
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
        e.isPreset = (kind == KindPreset);
        if (cache != nullptr)
        {
            if (kind == KindScript)
                e.scriptPath = lastScriptPath = cache->scriptPath(name);
            else if (kind == KindPreset)
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
        if (kind == KindScript && e.description.isEmpty() && cache != nullptr)
            e.description = cache->scriptMeta(name).description;
        m_entries << e;
    }

    m_filling = true;
    const QString was = m_tabs->currentIndex() >= 0 ? m_tabs->tabText(m_tabs->currentIndex()) : QString();
    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);
    m_tabs->addTab(tr("Recent"));
    foreach (const QString &c, categories)
        m_tabs->addTab(c);

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

    QList<int> shown;
    if (!q.isEmpty())
    {
        // Searching looks everywhere, whatever tab is open.
        for (int i = 0; i < m_entries.size(); ++i)
        {
            const Entry &e = m_entries.at(i);
            if (e.label.contains(q, Qt::CaseInsensitive) || e.description.contains(q, Qt::CaseInsensitive)
                || e.category.contains(q, Qt::CaseInsensitive))
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

    foreach (int i, shown)
    {
        const Entry &e = m_entries.at(i);
        QListWidgetItem *item = new QListWidgetItem(e.label, m_list);
        item->setData(EntryRole, i);
        item->setData(PathRole, e.scriptPath);
        item->setToolTip(e.description);
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
        m_frames.insert(path, previewFrame(path));
    }
    // A cache of runs, not of every script ever scrolled past.
    if (m_runs.size() > 48)
        for (auto it = m_runs.begin(); it != m_runs.end(); )
            it = done.contains(it.key()) ? std::next(it) : m_runs.erase(it);
    m_list->viewport()->update();
}

QVector<QColor> EffectBrowser::previewFrame(const QString &path)
{
    QVector<QColor> out;
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
        return out;

    QJSEngine *eng = run->script.engine();
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
        fixtures.setProperty(quint32(i), o);
    }
    const double t = m_clock.elapsed() / 1000.0;
    QJSValue inputs = eng->newObject();
    for (const EffectScript::InputDef &d : run->script.inputDefs())
        inputs.setProperty(d.name, double(d.defaultValue));
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
        if (d.type != QLatin1String("path"))
            params.setProperty(d.name, double(d.defaultValue));

    const QJSValue result = run->script.callTick(fixtures, inputs, palettes, params, run->state,
                                                 eng->newObject());
    if (!result.isArray())
    {
        m_runs.insert(path, QSharedPointer<Run>());
        return out;
    }
    out.resize(kLights);
    QList<int> aimedAt;                 // position effects: where each points
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
        else if (o.hasProperty(QStringLiteral("pan")))
        {
            // A position effect: where it points, as a light along the strip.
            const double pan = qBound(-1e6, num(o.property(QStringLiteral("pan"))), 1e6);
            aimedAt << qBound(0, qRound((pan / 540.0) * (kLights - 1)), kLights - 1);
            out[i] = QColor(30, 30, 34);
        }
        else
            out[i] = QColor(30, 30, 34);
    }
    foreach (int at, aimedAt)
        out[at] = QColor(220, 220, 240);
    return out;
}
