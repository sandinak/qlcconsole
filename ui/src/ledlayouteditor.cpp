/*
  Q Light Controller Plus
  ledlayouteditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPainter>
#include <QLabel>
#include <QTimer>
#include <QMap>
#include <QtMath>

#include "monitorproperties.h"
#include "genericdmxsource.h"
#include "ledlayouteditor.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

/* Each head's place among the heads of its kind, 0..1: what runs a colour
   kind's gradient, so the rainbow along a row says the heads' order. */
static QVector<double> kindShades(const QLCFixtureMode *mode, int n, QStringList *kinds = nullptr)
{
    QVector<double> shade(n, 0.0);
    if (mode == nullptr)
        return shade;
    QHash<QString, QList<int> > of;
    QStringList k;
    for (int h = 0; h < n; ++h)
    {
        k << HeadKinds::kindOf(mode, h);
        of[k.last()] << h;
    }
    for (auto it = of.constBegin(); it != of.constEnd(); ++it)
        for (int i = 0; i < it.value().size(); ++i)
            shade[it.value().at(i)] = it.value().size() > 1 ? double(i) / (it.value().size() - 1) : 0.0;
    if (kinds != nullptr)
        *kinds = k;
    return shade;
}

QColor ledKindColour(const QString &kind, double shade)
{
    if (kind == QLatin1String("W"))  return QColor(240, 236, 222);
    if (kind == QLatin1String("A"))  return QColor(255, 160, 40);
    if (kind == QLatin1String("UV")) return QColor(140, 60, 220);
    if (kind == QLatin1String("I"))  return QColor(214, 206, 150);
    if (kind == QLatin1String("R"))  return QColor(220, 50, 50);
    if (kind == QLatin1String("G"))  return QColor(50, 190, 70);
    if (kind == QLatin1String("B"))  return QColor(60, 90, 230);
    if (kind == QLatin1String("C"))  return QColor(40, 200, 220);
    if (kind == QLatin1String("M"))  return QColor(220, 60, 200);
    if (kind == QLatin1String("Y"))  return QColor(235, 220, 40);
    if (kind == QLatin1String("L"))  return QColor(170, 230, 40);
    if (kind.size() > 1 && !kind.contains(QLatin1String("UV")) && kind != QLatin1String("In"))
    {
        // A mix of colours: a gradient red -> violet in head order; paler
        // when it has a white of its own.
        const int hue = int(qBound(0.0, shade, 1.0) * 280.0);
        return QColor::fromHsv(hue, kind.contains(QLatin1Char('W')) ? 110 : 200, 235);
    }
    if (kind.endsWith(QLatin1String("UV")))
        return QColor(150, 80, 230);
    return QColor::fromHsv(int(qHash(kind) % 360), 120, 220);
}

static QColor textOn(const QColor &c)
{
    return (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) > 150 ? Qt::black : Qt::white;
}

/* Dimmed towards the background, for the heads the preview is not on. */
static QColor dimmed(const QColor &c, double level)
{
    const QColor bg(28, 30, 36);
    return QColor(int(bg.red() + (c.red() - bg.red()) * level),
                  int(bg.green() + (c.green() - bg.green()) * level),
                  int(bg.blue() + (c.blue() - bg.blue()) * level));
}

static const int kTrail = 4;    // heads behind the preview's, fading

/*****************************************************************************
 * LedMapWidget
 *****************************************************************************/

LedMapWidget::LedMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ledMapGrid"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(40);
}

void LedMapWidget::setMap(const Fixture *like, const HeadLayout &map)
{
    m_like = like;
    m_map = map;
    m_picked = -1;
    m_shade = kindShades(like != nullptr ? like->fixtureMode() : nullptr, map.cells.size());
    updateGeometry();
    update();
}

void LedMapWidget::setLit(int head)
{
    m_lit = head;
    update();
}

QSize LedMapWidget::sizeHint() const
{
    return QSize(qMin(900, m_map.size.width() * 30 + 2), qMin(420, m_map.size.height() * 30 + 2));
}

QSize LedMapWidget::minimumSizeHint() const
{
    return QSize(qMin(600, m_map.size.width() * 12 + 2), qMin(300, m_map.size.height() * 12 + 2));
}

double LedMapWidget::cellSize() const
{
    const int cols = qMax(1, m_map.size.width()), rows = qMax(1, m_map.size.height());
    return qBound(6.0, qMin(double(width() - 2) / cols, double(height() - 2) / rows), 44.0);
}

QPointF LedMapWidget::origin() const
{
    const double c = cellSize();
    return QPointF(qMax(1.0, (width() - c * m_map.size.width()) / 2.0),
                   qMax(1.0, (height() - c * m_map.size.height()) / 2.0));
}

QRectF LedMapWidget::cellRect(const QPoint &cell) const
{
    const double c = cellSize();
    const QPointF o = origin();
    return QRectF(o.x() + cell.x() * c + 1, o.y() + cell.y() * c + 1, c - 2, c - 2);
}

QPoint LedMapWidget::cellAt(const QPointF &pos) const
{
    const double c = cellSize();
    const QPointF o = origin();
    const int x = int(qFloor((pos.x() - o.x()) / c)), y = int(qFloor((pos.y() - o.y()) / c));
    if (x < 0 || y < 0 || x >= m_map.size.width() || y >= m_map.size.height())
        return QPoint(-1, -1);
    return QPoint(x, y);
}

int LedMapWidget::headIn(const QPoint &cell) const
{
    for (int h = 0; h < m_map.cells.size(); ++h)
        if (m_map.cells.at(h) == cell)
            return h;
    return -1;
}

void LedMapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(28, 30, 36));
    const QLCFixtureMode *mode = m_like != nullptr ? m_like->fixtureMode() : nullptr;

    // Every cell, so the gaps -- where a head can be moved to -- show.
    p.setPen(QPen(QColor(70, 74, 84), 1.0));
    p.setBrush(QColor(36, 38, 46));
    for (int y = 0; y < m_map.size.height(); ++y)
        for (int x = 0; x < m_map.size.width(); ++x)
            p.drawRect(cellRect(QPoint(x, y)));

    QFont f = p.font();
    f.setPixelSize(qBound(7, int(cellSize() * 0.36), 13));
    p.setFont(f);
    const bool numbers = cellSize() >= 14.0;
    for (int h = 0; h < m_map.cells.size(); ++h)
    {
        const QPoint cell = m_map.cells.at(h);
        if (cell.x() < 0 || cell.y() < 0)
            continue;
        const QRectF r = cellRect(cell);
        const QString kind = HeadKinds::kindOf(mode, h);
        QColor fill = ledKindColour(kind, m_shade.value(h));
        if (m_lit >= 0)
        {
            // The preview: this head full, the few before it fading, the rest dim.
            const int behind = m_lit - h;
            fill = (behind == 0) ? fill.lighter(125)
                 : (behind > 0 && behind <= kTrail) ? dimmed(fill, 1.0 - 0.15 * behind)
                 : dimmed(fill, 0.28);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRect(r);
        if (kind.size() > 1 && kind.contains(QLatin1Char('W')) && kind != QLatin1String("W"))
        {
            // A head with a white of its own: a white core.
            p.setBrush(QColor(250, 250, 245));
            p.drawRect(r.adjusted(r.width() * 0.32, r.height() * 0.32, -r.width() * 0.32, -r.height() * 0.32));
        }
        if (h == m_lit)
        {
            p.setPen(QPen(QColor(255, 255, 255), 3.0));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(-1, -1, 1, 1));
        }
        p.setPen(h == m_picked ? QPen(QColor(255, 200, 0), 3.0) : QPen(QColor(0, 0, 0, 110), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        if (numbers || h == m_lit)
        {
            p.setPen(textOn(fill));
            p.drawText(r, Qt::AlignCenter, QString::number(h + 1));
        }
    }
}

void LedMapWidget::mousePressEvent(QMouseEvent *e)
{
    const QPoint cell = cellAt(e->position());
    if (cell.x() < 0)
        return;
    const int h = headIn(cell);
    if (m_picked < 0)
    {
        if (h >= 0)
            m_picked = h;
        update();
        return;
    }
    if (h == m_picked)
    {
        m_picked = -1;
        update();
        return;
    }
    // The second click: the two heads trade cells, or the head moves to a free one.
    if (h >= 0)
        qSwap(m_map.cells[m_picked], m_map.cells[h]);
    else
        m_map.cells[m_picked] = cell;
    m_picked = -1;
    update();
    emit mapEdited();
}

/*****************************************************************************
 * LedOrderStrip
 *****************************************************************************/

LedOrderStrip::LedOrderStrip(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ledOrderStrip"));
    setMinimumHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(tr("The heads in number order. Click one to light it."));
}

void LedOrderStrip::setFixture(const Fixture *like)
{
    const QLCFixtureMode *mode = like != nullptr ? like->fixtureMode() : nullptr;
    const int n = mode != nullptr ? mode->heads().size() : 0;
    m_kinds.clear();
    m_shade = kindShades(mode, n, &m_kinds);
    m_lit = -1;
    update();
}

void LedOrderStrip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(28, 30, 36));
    const int n = m_kinds.size();
    if (n == 0)
        return;
    const double w = double(width()) / n;
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    for (int h = 0; h < n; ++h)
    {
        QColor c = ledKindColour(m_kinds.at(h), m_shade.value(h));
        if (m_lit >= 0 && h != m_lit)
            c = dimmed(c, (m_lit - h > 0 && m_lit - h <= kTrail) ? 1.0 - 0.15 * (m_lit - h) : 0.35);
        const QRectF r(h * w, 3, qMax(1.0, w - (w > 4 ? 1 : 0)), height() - 6);
        p.fillRect(r, c);
        if (h == m_lit)
        {
            p.setPen(QPen(Qt::white, 2));
            p.drawRect(r.adjusted(-1, -2, 1, 2));
            p.setPen(textOn(c));
            if (w >= 14)
                p.drawText(r, Qt::AlignCenter, QString::number(h + 1));
        }
    }
}

void LedOrderStrip::mousePressEvent(QMouseEvent *e)
{
    const int n = m_kinds.size();
    if (n == 0 || width() <= 0)
        return;
    const int h = qBound(0, int(e->position().x() * n / width()), n - 1);
    emit headClicked(h);
}

/*****************************************************************************
 * LedLayoutEditor
 *****************************************************************************/

LedLayoutEditor::LedLayoutEditor(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("ledLayoutEditor"));
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_grid = new LedMapWidget(this);
    root->addWidget(m_grid, 1);

    m_legend = new QLabel(this);
    m_legend->setTextFormat(Qt::RichText);
    root->addWidget(m_legend);
    QLabel *hint = new QLabel(tr("Numbers are head numbers; a colour kind runs red to violet in "
                                 "number order. Click a head, then another to swap them, or an "
                                 "empty cell to move it there."), this);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    root->addWidget(hint);

    QHBoxLayout *rowsLine = new QHBoxLayout;
    rowsLine->addWidget(new QLabel(tr("Rows, top to bottom:"), this));
    m_rowsEdit = new QLineEdit(this);
    m_rowsEdit->setObjectName(QStringLiteral("ledRowsEdit"));
    m_rowsEdit->setPlaceholderText(tr("e.g. RGB, W, RGB, W, RGB"));
    m_rowsEdit->setToolTip(tr("The kind of head in each row, top to bottom, separated by commas "
                              "(or / or spaces). Each row is filled with the next heads of its "
                              "kind, in head order."));
    rowsLine->addWidget(m_rowsEdit, 1);
    QPushButton *apply = new QPushButton(tr("Apply Rows"), this);
    apply->setObjectName(QStringLiteral("ledApplyRows"));
    rowsLine->addWidget(apply);
    root->addLayout(rowsLine);
    m_rowNote = new QLabel(this);
    m_rowNote->setObjectName(QStringLiteral("ledRowNote"));
    m_rowNote->setStyleSheet(QStringLiteral("color:#e07070;"));
    m_rowNote->setWordWrap(true);
    m_rowNote->hide();
    root->addWidget(m_rowNote);

    QHBoxLayout *tools = new QHBoxLayout;
    tools->addWidget(new QLabel(tr("Grid:"), this));
    m_colsSpin = new QSpinBox(this);
    m_colsSpin->setObjectName(QStringLiteral("ledColsSpin"));
    m_colsSpin->setToolTip(tr("Columns"));
    tools->addWidget(m_colsSpin);
    tools->addWidget(new QLabel(QStringLiteral("×"), this));
    m_rowsSpin = new QSpinBox(this);
    m_rowsSpin->setObjectName(QStringLiteral("ledRowsSpin"));
    m_rowsSpin->setToolTip(tr("Rows"));
    tools->addWidget(m_rowsSpin);
    tools->addSpacing(12);
    QPushButton *flipH = new QPushButton(tr("Flip ↔"), this);
    flipH->setObjectName(QStringLiteral("ledFlipH"));
    flipH->setToolTip(tr("Mirror left to right"));
    tools->addWidget(flipH);
    QPushButton *flipV = new QPushButton(tr("Flip ↕"), this);
    flipV->setObjectName(QStringLiteral("ledFlipV"));
    flipV->setToolTip(tr("Mirror top to bottom"));
    tools->addWidget(flipV);
    QPushButton *order = new QPushButton(tr("Head Order"), this);
    order->setObjectName(QStringLiteral("ledHeadOrder"));
    order->setToolTip(tr("Back to the heads in number order, row by row -- what the "
                         "definition alone says"));
    tools->addWidget(order);
    tools->addStretch();
    root->addLayout(tools);

    QHBoxLayout *preview = new QHBoxLayout;
    m_previewButton = new QPushButton(tr("▶ Preview Order"), this);
    m_previewButton->setObjectName(QStringLiteral("ledPreview"));
    m_previewButton->setCheckable(true);
    m_previewButton->setToolTip(tr("Light the heads one after another in number order, to check "
                                   "the map against the real fixture"));
    preview->addWidget(m_previewButton);
    m_strip = new LedOrderStrip(this);
    preview->addWidget(m_strip, 1);
    m_lightFixture = new QCheckBox(tr("Light the fixture too"), this);
    m_lightFixture->setObjectName(QStringLiteral("ledLightFixture"));
    m_lightFixture->setToolTip(tr("While previewing, light each head on the real fixture as well "
                                  "(full, one at a time); it goes back as it was afterwards"));
    m_lightFixture->setVisible(m_doc != nullptr);
    preview->addWidget(m_lightFixture);
    root->addLayout(preview);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(160);

    connect(apply, &QPushButton::clicked, this, &LedLayoutEditor::slotApplyRows);
    connect(m_rowsEdit, &QLineEdit::returnPressed, this, &LedLayoutEditor::slotApplyRows);
    connect(flipH, &QPushButton::clicked, this, &LedLayoutEditor::flipHorizontally);
    connect(flipV, &QPushButton::clicked, this, &LedLayoutEditor::flipVertically);
    connect(order, &QPushButton::clicked, this, &LedLayoutEditor::resetHeadOrder);
    connect(m_colsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &LedLayoutEditor::slotGridSize);
    connect(m_rowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &LedLayoutEditor::slotGridSize);
    connect(m_grid, &LedMapWidget::mapEdited, this, &LedLayoutEditor::slotMapEdited);
    connect(m_previewButton, &QPushButton::toggled, this, &LedLayoutEditor::setPreviewRunning);
    connect(m_previewTimer, &QTimer::timeout, this, &LedLayoutEditor::stepPreview);
    connect(m_strip, &LedOrderStrip::headClicked, this, &LedLayoutEditor::lightHead);
    connect(m_lightFixture, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            lightOnFixture(litHead());
        else
            stopFixtureLight();
    });
}

LedLayoutEditor::~LedLayoutEditor()
{
    stopFixtureLight();
}

void LedLayoutEditor::setFixture(const Fixture *like, const HeadLayout &layout)
{
    setPreviewRunning(false);
    stopFixtureLight();
    m_like = like;
    m_strip->setFixture(like);
    m_grid->setLit(-1);
    setRowProblem(QString());

    // The kinds, each in its colour, and how many heads of it.
    QStringList parts;
    const QLCFixtureMode *mode = like != nullptr ? like->fixtureMode() : nullptr;
    if (mode != nullptr)
    {
        QMap<QString, int> count;
        QStringList order;
        for (int h = 0; h < mode->heads().size(); ++h)
        {
            const QString k = HeadKinds::kindOf(mode, h);
            if (!count.contains(k))
                order << k;
            count[k]++;
        }
        for (const QString &k : order)
        {
            // A chip of its colour -- a colour kind's gradient as three.
            QString chip;
            const bool graded = ledKindColour(k, 0.0) != ledKindColour(k, 1.0);
            for (double shade : graded ? QList<double>{ 0.0, 0.5, 1.0 } : QList<double>{ 0.0 })
                chip += QStringLiteral("<span style=\"background-color:%1;\">%2</span>")
                            .arg(ledKindColour(k, shade).name(),
                                 graded ? QStringLiteral("&nbsp;") : QStringLiteral("&nbsp;&nbsp;&nbsp;"));
            parts << QStringLiteral("%1 %2 × %3").arg(chip).arg(count.value(k))
                         .arg((k.isEmpty() ? tr("other") : k).toHtmlEscaped());
        }
    }
    m_legend->setText(parts.join(QStringLiteral(" &nbsp; ")));
    showLayout(layout, false);
}

HeadLayout LedLayoutEditor::layout() const
{
    return m_grid->map();
}

void LedLayoutEditor::showLayout(const HeadLayout &l, bool edited)
{
    m_grid->setMap(m_like, l);
    refreshControls();
    if (edited)
        emit layoutChanged();
}

void LedLayoutEditor::refreshControls()
{
    const HeadLayout l = m_grid->map();
    const int n = qMax(1, l.cells.size());
    m_updating = true;
    m_colsSpin->setRange(1, n);
    m_rowsSpin->setRange(1, n);
    m_colsSpin->setValue(qMax(1, l.size.width()));
    m_rowsSpin->setValue(qMax(1, l.size.height()));
    m_rowsEdit->setText(rowPatternText());
    m_updating = false;
}

QString LedLayoutEditor::rowPatternText() const
{
    QStringList rows = m_grid->map().rowKinds(m_like);
    for (QString &r : rows)
        if (r.isEmpty())
            r = QStringLiteral("?");
    return rows.join(QStringLiteral(", "));
}

QString LedLayoutEditor::rowProblem() const
{
    return m_rowNote->isHidden() ? QString() : m_rowNote->text();
}

void LedLayoutEditor::setRowProblem(const QString &why)
{
    m_rowNote->setText(why);
    m_rowNote->setVisible(!why.isEmpty());
}

void LedLayoutEditor::slotApplyRows()
{
    applyRowPattern(m_rowsEdit->text());
}

bool LedLayoutEditor::applyRowPattern(const QString &text)
{
    if (m_like == nullptr || m_like->fixtureMode() == nullptr)
        return false;
    if (m_rowsEdit->text() != text)
        m_rowsEdit->setText(text);
    const QLCFixtureMode *mode = m_like->fixtureMode();
    const QStringList kinds = HeadKinds::kindsOf(m_like);
    const QStringList rows = HeadLayout::parseRowKinds(text, kinds);
    if (rows.isEmpty())
    {
        setRowProblem(tr("Type the kind of each row, top to bottom: %1.").arg(kinds.join(QStringLiteral(", "))));
        return false;
    }
    QStringList unknown;
    for (const QString &r : rows)
        if (!kinds.contains(r) && !unknown.contains(r))
            unknown << r;
    if (!unknown.isEmpty())
    {
        setRowProblem(tr("This model has no %1 heads; its kinds are %2.")
                          .arg(unknown.join(QStringLiteral(", ")), kinds.join(QStringLiteral(", "))));
        return false;
    }

    const int cols = m_grid->map().size.width();
    HeadLayout l = HeadLayout::fromRowPattern(m_like, rows, cols);
    /* A strip with no declared grid is one long row: "RGB, W" on its 32
       heads means two rows of 16, so try the width the rows imply. */
    const int n = mode->heads().size();
    if (!l.isValid() && n % rows.size() == 0 && n / rows.size() != cols)
        l = HeadLayout::fromRowPattern(m_like, rows, n / rows.size());
    if (!l.isValid())
    {
        // Say what does not fit, rather than guess.
        QMap<QString, int> heads, cells;
        for (int h = 0; h < n; ++h)
            heads[HeadKinds::kindOf(mode, h)]++;
        for (const QString &k : rows)
            cells[k] += cols;
        QStringList why;
        for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
            if (cells.value(it.key()) != it.value())
                why << tr("%1 rows hold %2 heads, it has %3").arg(it.key())
                           .arg(cells.value(it.key())).arg(it.value());
        setRowProblem(tr("Those rows do not fit: %1.").arg(why.join(QStringLiteral("; "))));
        return false;
    }
    setRowProblem(QString());
    showLayout(l, true);
    return true;
}

void LedLayoutEditor::flipHorizontally()
{
    showLayout(m_grid->map().flippedHorizontally(), true);
}

void LedLayoutEditor::flipVertically()
{
    showLayout(m_grid->map().flippedVertically(), true);
}

void LedLayoutEditor::resetHeadOrder()
{
    setRowProblem(QString());
    showLayout(HeadLayout::headOrder(m_like), true);
}

bool LedLayoutEditor::setGridSize(const QSize &size)
{
    const HeadLayout l = m_grid->map().resized(size);
    if (!l.isValid())
        return false;
    showLayout(l, true);
    return true;
}

void LedLayoutEditor::slotGridSize()
{
    if (m_updating)
        return;
    const int n = m_grid->map().cells.size();
    int cols = m_colsSpin->value(), rows = m_rowsSpin->value();
    // Never fewer cells than heads: the other side grows to fit.
    if (cols * rows < n)
    {
        if (sender() == m_colsSpin)
            rows = (n + cols - 1) / cols;
        else
            cols = (n + rows - 1) / rows;
    }
    if (!setGridSize(QSize(cols, rows)))
        refreshControls();
}

void LedLayoutEditor::slotMapEdited()
{
    refreshControls();
    emit layoutChanged();
}

/*****************************************************************************
 * The order preview
 *****************************************************************************/

void LedLayoutEditor::setPreviewRunning(bool on)
{
    if (m_previewButton->isChecked() != on)
    {
        m_previewButton->setChecked(on);   // comes back here
        return;
    }
    m_previewButton->setText(on ? tr("■ Stop") : tr("▶ Preview Order"));
    if (on)
    {
        lightHead(-1);
        stepPreview();
        m_previewTimer->start();
    }
    else
    {
        m_previewTimer->stop();
        lightHead(-1);
    }
}

bool LedLayoutEditor::previewRunning() const
{
    return m_previewTimer->isActive();
}

void LedLayoutEditor::stepPreview()
{
    const int n = m_grid->map().cells.size();
    if (n == 0)
        return;
    lightHead((litHead() + 1) % n);
}

int LedLayoutEditor::litHead() const
{
    return m_grid->lit();
}

void LedLayoutEditor::lightHead(int head)
{
    m_grid->setLit(head);
    m_strip->setLit(head);
    lightOnFixture(head);
}

void LedLayoutEditor::lightOnFixture(int head)
{
    if (m_doc == nullptr || m_like == nullptr || !m_lightFixture->isChecked())
        return;
    const QLCFixtureMode *mode = m_like->fixtureMode();
    if (mode == nullptr)
        return;
    if (head < 0)
    {
        stopFixtureLight();
        return;
    }
    if (m_source == nullptr)
    {
        m_source = new GenericDMXSource(m_doc);
        m_source->setOutputEnabled(true);
        // The fixture's own master dimmer (in no head) open, or nothing shows.
        for (quint32 ci = 0; ci < quint32(mode->channels().size()); ++ci)
        {
            const QLCChannel *ch = mode->channel(ci);
            if (ch != nullptr && mode->headForChannel(ci) < 0
                && ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour)
                m_source->set(m_like->id(), ci, 255);
        }
    }
    /* Every head's intensities: this one full, the rest at zero. Set, not
       unset -- unsetAll() would drop what is set after it in the same tick. */
    for (int h = 0; h < mode->heads().size(); ++h)
        foreach (quint32 ci, mode->heads().at(h).channels())
        {
            const QLCChannel *ch = mode->channel(ci);
            if (ch != nullptr && ch->group() == QLCChannel::Intensity)
                m_source->set(m_like->id(), ci, h == head ? 255 : 0);
        }
}

void LedLayoutEditor::stopFixtureLight()
{
    // Its faders go with it: the fixture is back as it was.
    delete m_source;
    m_source = nullptr;
}

/*****************************************************************************
 * LedLayoutDialog
 *****************************************************************************/

bool LedLayoutDialog::canEdit(Doc *doc, quint32 fixtureId)
{
    const Fixture *fx = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    return fx != nullptr && !HeadLayout::keyFor(fx).isEmpty() && fx->fixtureMode() != nullptr
           && fx->fixtureMode()->heads().size() > 1;
}

bool LedLayoutDialog::edit(Doc *doc, quint32 fixtureId, QWidget *parent)
{
    if (!canEdit(doc, fixtureId))
        return false;
    LedLayoutDialog dlg(doc, fixtureId, parent);
    return dlg.exec() == QDialog::Accepted;
}

LedLayoutDialog::LedLayoutDialog(Doc *doc, quint32 fixtureId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fid(fixtureId)
{
    setObjectName(QStringLiteral("ledLayoutDialog"));
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#led-layout"));
    const Fixture *fx = m_doc->fixture(m_fid);
    m_key = HeadLayout::keyFor(fx);
    const QLCFixtureDef *def = fx != nullptr ? fx->fixtureDef() : nullptr;
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    const QString model = def != nullptr ? def->manufacturer() + QLatin1Char(' ') + def->model() : QString();
    setWindowTitle(tr("LED Layout — %1").arg(model));

    QVBoxLayout *root = new QVBoxLayout(this);
    int users = 0;
    foreach (Fixture *other, m_doc->fixtures())
        if (HeadLayout::keyFor(other) == m_key)
            users++;
    QLabel *info = new QLabel(this);
    info->setWordWrap(true);
    info->setText(tr("<b>%1</b>, mode %2: %3 heads. Where each head really is in the fixture -- "
                     "one layout for every %1 in this mode (%4 in this show).")
                      .arg(model.toHtmlEscaped(), mode != nullptr ? mode->name().toHtmlEscaped() : QString())
                      .arg(mode != nullptr ? mode->heads().size() : 0).arg(users));
    root->addWidget(info);
    m_source = new QLabel(this);
    m_source->setObjectName(QStringLiteral("ledLayoutSource"));
    m_source->setEnabled(false);
    root->addWidget(m_source);

    m_editor = new LedLayoutEditor(m_doc, this);
    root->addWidget(m_editor, 1);

    QHBoxLayout *store = new QHBoxLayout;
    m_saveDefault = new QPushButton(tr("Save as Default for This Model"), this);
    m_saveDefault->setObjectName(QStringLiteral("ledSaveDefault"));
    m_saveDefault->setToolTip(tr("Keep this layout on this computer for every show: new shows, and "
                                 "shows with no layout of their own for this model, use it. "
                                 "(A show's own layout still comes first.)"));
    store->addWidget(m_saveDefault);
    m_useDefault = new QPushButton(tr("Use Default"), this);
    m_useDefault->setObjectName(QStringLiteral("ledUseDefault"));
    m_useDefault->setToolTip(tr("Load the default for this model into the editor (head order when "
                                "there is none)"));
    store->addWidget(m_useDefault);
    store->addStretch();
    root->addLayout(store);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Save to Show"));
    bb->button(QDialogButtonBox::Ok)->setToolTip(tr("Keep this layout in the show: everything that "
                                                    "draws or groups this model's heads uses it"));
    root->addWidget(bb);

    connect(m_saveDefault, &QPushButton::clicked, this, &LedLayoutDialog::saveAsDefault);
    connect(m_useDefault, &QPushButton::clicked, this, &LedLayoutDialog::useDefault);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_editor->setFixture(fx, m_doc->monitorProperties()->headLayoutFor(fx));
    refreshSource();
    resize(qMax(560, m_editor->sizeHint().width() + 40), 520);
}

void LedLayoutDialog::refreshSource()
{
    const HeadLayout lib = HeadLayoutLibrary::load(m_key);
    QString where;
    if (m_doc->monitorProperties()->hasShowHeadLayout(m_key))
        where = lib.isValid() ? tr("This show has its own layout for this model (it comes before "
                                   "your default).")
                              : tr("This show has its own layout for this model.");
    else if (lib.isValid())
        where = tr("Using your default for this model.");
    else
        where = tr("Nothing saved for this model yet: heads in number order.");
    m_source->setText(where);
    m_useDefault->setEnabled(true);
}

void LedLayoutDialog::saveAsDefault()
{
    const HeadLayout l = m_editor->layout();
    if (!HeadLayoutLibrary::save(l))
    {
        m_source->setText(tr("Could not save the default in %1.")
                              .arg(HeadLayoutLibrary::userDirectory().absolutePath()));
        return;
    }
    refreshSource();
    m_source->setText(m_source->text() + QLatin1Char(' ')
                      + tr("Saved as your default for this model."));
}

void LedLayoutDialog::useDefault()
{
    const Fixture *fx = m_doc->fixture(m_fid);
    HeadLayout lib = HeadLayoutLibrary::load(m_key);
    if (!lib.isValid())
        lib = HeadLayout::headOrder(fx);
    m_editor->setFixture(fx, lib);
}

void LedLayoutDialog::accept()
{
    m_editor->setPreviewRunning(false);
    const HeadLayout l = m_editor->layout();
    MonitorProperties *mp = m_doc->monitorProperties();
    if (l.isValid() && (!mp->hasShowHeadLayout(m_key) || !(mp->headLayoutFor(m_doc->fixture(m_fid)) == l)))
    {
        mp->setHeadLayout(l);
        m_doc->setModified();
    }
    QDialog::accept();
}
