/*
  Q Light Controller Plus
  headtypesdialog.cpp

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
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPainter>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QRegularExpression>

#include "createfixturegroup.h"
#include "monitorproperties.h"
#include "headtypesdialog.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

static const double kCell = 26.0;   // px per LED cell

/* A head kind's colour in the grid. RGB is drawn as its three stripes. */
static QColor kindColour(const QString &kind)
{
    if (kind == QLatin1String("W"))  return QColor(236, 236, 228);
    if (kind == QLatin1String("A"))  return QColor(255, 160, 40);
    if (kind == QLatin1String("UV")) return QColor(140, 60, 220);
    if (kind == QLatin1String("I"))  return QColor(200, 200, 120);
    if (kind.contains(QLatin1Char('W')) && kind.startsWith(QLatin1String("RGB")))
        return QColor(250, 230, 240);
    return QColor::fromHsv(int(qHash(kind) % 360), 120, 220);
}

/*****************************************************************************
 * LedMapWidget
 *****************************************************************************/

LedMapWidget::LedMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(60);
}

void LedMapWidget::setMap(const Fixture *like, const HeadLayout &map)
{
    m_like = like;
    m_map = map;
    m_picked = -1;
    updateGeometry();
    update();
}

QSize LedMapWidget::sizeHint() const
{
    return QSize(int(m_map.size.width() * kCell) + 2, int(m_map.size.height() * kCell) + 2);
}

QRectF LedMapWidget::cellRect(const QPoint &cell) const
{
    return QRectF(1 + cell.x() * kCell, 1 + cell.y() * kCell, kCell - 2, kCell - 2);
}

int LedMapWidget::headAt(const QPointF &pos) const
{
    for (int h = 0; h < m_map.cells.size(); ++h)
        if (cellRect(m_map.cells.at(h)).contains(pos))
            return h;
    return -1;
}

void LedMapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(28, 30, 36));
    const QLCFixtureMode *mode = m_like != nullptr ? m_like->fixtureMode() : nullptr;
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    for (int h = 0; h < m_map.cells.size(); ++h)
    {
        const QRectF r = cellRect(m_map.cells.at(h));
        const QString kind = HeadKinds::kindOf(mode, h);
        if (kind.startsWith(QLatin1String("RGB")) && !kind.contains(QLatin1Char('W')))
        {
            const double w = r.width() / 3.0;
            p.fillRect(QRectF(r.left(), r.top(), w, r.height()), QColor(200, 60, 60));
            p.fillRect(QRectF(r.left() + w, r.top(), w, r.height()), QColor(60, 170, 70));
            p.fillRect(QRectF(r.left() + 2 * w, r.top(), w, r.height()), QColor(60, 90, 210));
        }
        else
            p.fillRect(r, kindColour(kind));
        p.setPen(h == m_picked ? QPen(QColor(255, 200, 0), 2.5) : QPen(QColor(0, 0, 0, 120), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        p.setPen(kind == QLatin1String("W") ? Qt::black : Qt::white);
        p.drawText(r, Qt::AlignCenter, QString::number(h + 1));
    }
}

void LedMapWidget::mousePressEvent(QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int h = headAt(e->position());
#else
    const int h = headAt(e->localPos());
#endif
    if (h < 0)
        return;
    if (m_picked < 0 || m_picked == h)
    {
        m_picked = (m_picked == h) ? -1 : h;
        update();
        return;
    }
    // The second click: the two heads trade cells.
    qSwap(m_map.cells[m_picked], m_map.cells[h]);
    m_picked = -1;
    update();
    emit mapEdited();
}

/*****************************************************************************
 * HeadTypesDialog
 *****************************************************************************/

bool HeadTypesDialog::anyMixed(Doc *doc, const QList<quint32> &fixtureIds)
{
    foreach (quint32 fid, fixtureIds)
        if (HeadKinds::isMixed(doc->fixture(fid)))
            return true;
    return false;
}

QString HeadTypesDialog::suggestName(Doc *doc, const QList<quint32> &fixtureIds)
{
    QString common;
    bool first = true;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = doc->fixture(fid);
        if (fx == nullptr)
            continue;
        if (first) { common = fx->name(); first = false; continue; }
        int n = 0;
        while (n < common.size() && n < fx->name().size() && common.at(n) == fx->name().at(n))
            ++n;
        common.truncate(n);
    }
    // Drop what is left of a trailing number and its separator.
    common.remove(QRegularExpression(QStringLiteral("[\\s#._-]*\\d*$")));
    return common.trimmed();
}

int HeadTypesDialog::offerSplit(Doc *doc, const QList<quint32> &fixtureIds, const QString &baseName,
                                QWidget *parent, QList<FixtureGroup *> *made)
{
    if (!anyMixed(doc, fixtureIds))
        return 0;
    QMessageBox box(QMessageBox::Question, tr("Fixture Group"),
                    tr("Some of these fixtures have more than one kind of head (RGB and white, "
                       "say). Make a group per kind of head, laid out from where each head "
                       "really is?"), QMessageBox::Cancel, parent);
    QPushButton *split = box.addButton(tr("Split by Head Type…"), QMessageBox::AcceptRole);
    QPushButton *one = box.addButton(tr("One Group"), QMessageBox::NoRole);
    box.setDefaultButton(split);
    box.exec();
    if (box.clickedButton() == one)
        return 0;
    if (box.clickedButton() != split)
        return -1;
    HeadTypesDialog dlg(doc, fixtureIds, baseName, parent);
    if (dlg.exec() != QDialog::Accepted)
        return -1;
    if (made != nullptr)
        *made = dlg.createdGroups();
    return 1;
}

HeadTypesDialog::HeadTypesDialog(Doc *doc, const QList<quint32> &fixtureIds,
                                 const QString &baseName, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_ids(fixtureIds)
{
    setWindowTitle(tr("Split by Head Type"));
    MonitorProperties *mp = m_doc->monitorProperties();

    // The mixed models among the fixtures, and every kind of head there is.
    QStringList kinds;
    foreach (quint32 fid, m_ids)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            continue;
        foreach (const QString &k, HeadKinds::kindsOf(fx))
            if (!kinds.contains(k))
                kinds << k;
        const QString key = HeadLayout::keyFor(fx);
        if (key.isEmpty() || m_maps.contains(key) || !HeadKinds::isMixed(fx))
            continue;
        m_maps.insert(key, mp->headLayoutFor(fx));
        m_like.insert(key, fid);
    }

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("These fixtures have more than one kind of head. Check where each "
                                  "head really is -- the LED map -- then make a fixture group per "
                                  "kind. The map is kept in your library and in this show."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    QHBoxLayout *modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel(tr("Model:"), this));
    m_modelCombo = new QComboBox(this);
    QStringList keys = m_maps.keys();
    keys.sort();
    foreach (const QString &k, keys)
        m_modelCombo->addItem(QString(k).replace(QLatin1Char('|'), QStringLiteral(" · ")), k);
    modelRow->addWidget(m_modelCombo, 1);
    root->addLayout(modelRow);
    m_modelInfo = new QLabel(this);
    root->addWidget(m_modelInfo);

    m_grid = new LedMapWidget(this);
    root->addWidget(m_grid, 0, Qt::AlignLeft);
    QLabel *gridHint = new QLabel(tr("Numbers are head numbers. Click two cells to swap their heads."), this);
    gridHint->setEnabled(false);
    root->addWidget(gridHint);

    QHBoxLayout *rowsLine = new QHBoxLayout;
    rowsLine->addWidget(new QLabel(tr("Rows, top to bottom:"), this));
    m_rowsBox = new QHBoxLayout;
    rowsLine->addLayout(m_rowsBox);
    QPushButton *apply = new QPushButton(tr("Apply Rows"), this);
    apply->setToolTip(tr("Fill each row with the next heads of its kind, in head order"));
    rowsLine->addWidget(apply);
    QPushButton *order = new QPushButton(tr("Head Order"), this);
    order->setToolTip(tr("Back to the heads in number order, row by row -- what the "
                         "definition alone says"));
    rowsLine->addWidget(order);
    rowsLine->addStretch();
    root->addLayout(rowsLine);
    m_rowNote = new QLabel(this);
    m_rowNote->setStyleSheet("color:#e07070;");
    m_rowNote->hide();
    root->addWidget(m_rowNote);

    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);
    /* One row per kind of head: make it or not, and its EXACT name. A single
       name field with the kind added on behind it made "T1-CW-RGB RGB" out
       of a name that already said RGB ("it appended RGB after I'd already
       put it there"). */
    QLabel *groupsLabel = new QLabel(tr("Fixture groups to make:"), this);
    root->addWidget(groupsLabel);
    QFormLayout *groupForm = new QFormLayout;
    m_viewCombo = new QComboBox(this);
    m_viewCombo->addItem(tr("From the front — as the audience sees it"), int(CreateFixtureGroup::ViewFront));
    m_viewCombo->addItem(tr("From above — the plan"), int(CreateFixtureGroup::ViewTop));
    m_viewCombo->addItem(tr("Automatic"), int(CreateFixtureGroup::ViewAuto));
    groupForm->addRow(tr("Laid out:"), m_viewCombo);
    m_closeGaps = new QCheckBox(tr("Close up the gaps"), this);
    m_closeGaps->setChecked(true);
    groupForm->addRow(QString(), m_closeGaps);
    foreach (const QString &k, kinds)
    {
        QCheckBox *cb = new QCheckBox(k, this);
        cb->setChecked(true);
        cb->setProperty("kind", k);
        QLineEdit *name = new QLineEdit(CreateFixtureGroup::headKindGroupName(baseName, k), this);
        connect(cb, &QCheckBox::toggled, name, &QLineEdit::setEnabled);
        groupForm->addRow(cb, name);
        m_kindChecks << cb;
        m_kindNames << name;
    }
    root->addLayout(groupForm);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    bb->addButton(tr("Make Groups"), QDialogButtonBox::AcceptRole);
    root->addWidget(bb);

    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HeadTypesDialog::slotModelChanged);
    connect(apply, &QPushButton::clicked, this, &HeadTypesDialog::slotApplyRows);
    connect(order, &QPushButton::clicked, this, &HeadTypesDialog::slotHeadOrder);
    connect(m_grid, &LedMapWidget::mapEdited, this, &HeadTypesDialog::slotMapEdited);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    slotModelChanged();
}

const Fixture *HeadTypesDialog::likeFor(const QString &key) const
{
    return m_doc->fixture(m_like.value(key, Fixture::invalidId()));
}

void HeadTypesDialog::slotModelChanged()
{
    showModel(m_modelCombo->currentData().toString());
}

void HeadTypesDialog::showModel(const QString &key)
{
    const Fixture *fx = likeFor(key);
    if (fx == nullptr)
        return;
    const QLCFixtureMode *mode = fx->fixtureMode();
    QMap<QString, int> count;
    for (int h = 0; h < mode->heads().size(); ++h)
        count[HeadKinds::kindOf(mode, h)]++;
    QStringList parts;
    for (auto it = count.constBegin(); it != count.constEnd(); ++it)
        parts << tr("%1 %2").arg(it.value()).arg(it.key());
    int n = 0;
    foreach (quint32 fid, m_ids)
        if (HeadLayout::keyFor(m_doc->fixture(fid)) == key)
            n++;
    m_modelInfo->setText(tr("%1 of these, %2 heads each: %3")
                             .arg(n).arg(mode->heads().size()).arg(parts.join(QStringLiteral(" + "))));
    m_grid->setMap(fx, m_maps.value(key));
    m_rowNote->hide();
    fillRowCombos();
}

void HeadTypesDialog::fillRowCombos()
{
    const QString key = m_modelCombo->currentData().toString();
    const Fixture *fx = likeFor(key);
    qDeleteAll(m_rowCombos);
    m_rowCombos.clear();
    if (fx == nullptr)
        return;
    const QStringList kinds = HeadKinds::kindsOf(fx);
    const HeadLayout map = m_maps.value(key);
    const QStringList rows = map.rowKinds(fx);
    for (int r = 0; r < map.size.height(); ++r)
    {
        QComboBox *cb = new QComboBox(this);
        cb->addItems(kinds);
        const int at = kinds.indexOf(rows.value(r));
        cb->setCurrentIndex(at >= 0 ? at : 0);
        m_rowsBox->addWidget(cb);
        m_rowCombos << cb;
    }
}

void HeadTypesDialog::slotApplyRows()
{
    const QString key = m_modelCombo->currentData().toString();
    const Fixture *fx = likeFor(key);
    if (fx == nullptr)
        return;
    QStringList rows;
    foreach (QComboBox *cb, m_rowCombos)
        rows << cb->currentText();
    const HeadLayout l = HeadLayout::fromRowPattern(fx, rows);
    if (!l.isValid())
    {
        // Say what does not fit, rather than guess.
        const QLCFixtureMode *mode = fx->fixtureMode();
        QMap<QString, int> heads, cells;
        for (int h = 0; h < mode->heads().size(); ++h)
            heads[HeadKinds::kindOf(mode, h)]++;
        foreach (const QString &k, rows)
            cells[k] += m_maps.value(key).size.width();
        QStringList why;
        for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
            if (cells.value(it.key()) != it.value())
                why << tr("%1 rows hold %2 heads, it has %3").arg(it.key())
                           .arg(cells.value(it.key())).arg(it.value());
        m_rowNote->setText(tr("Those rows do not fit: %1.").arg(why.join(QStringLiteral("; "))));
        m_rowNote->show();
        return;
    }
    m_rowNote->hide();
    m_maps.insert(key, l);
    m_grid->setMap(fx, l);
}

void HeadTypesDialog::slotHeadOrder()
{
    const QString key = m_modelCombo->currentData().toString();
    const Fixture *fx = likeFor(key);
    if (fx == nullptr)
        return;
    m_maps.insert(key, HeadLayout::headOrder(fx));
    showModel(key);
}

void HeadTypesDialog::slotMapEdited()
{
    m_maps.insert(m_modelCombo->currentData().toString(), m_grid->map());
}

void HeadTypesDialog::accept()
{
    // The maps first: the groups are laid out from them.
    MonitorProperties *mp = m_doc->monitorProperties();
    for (auto it = m_maps.constBegin(); it != m_maps.constEnd(); ++it)
    {
        mp->setHeadLayout(it.value());
        HeadLayoutLibrary::save(it.value());
    }
    const int view = m_viewCombo->currentData().toInt();
    const bool gaps = m_closeGaps->isChecked();
    QList<QPair<QString, QString> > named, again;
    QList<FixtureGroup *> same;
    QStringList existing;
    for (int i = 0; i < m_kindChecks.size(); ++i)
    {
        if (!m_kindChecks[i]->isChecked())
            continue;
        const QString kind = m_kindChecks[i]->property("kind").toString();
        const QPair<QString, QString> entry(kind, m_kindNames[i]->text().trimmed());
        // Not the same group twice by accident: is there one with these heads?
        FixtureGroup probe(m_doc);
        probe.setSize(QSize(1, 1));
        CreateFixtureGroup::layoutFixturesByStage(m_doc, m_ids, &probe, view, gaps, kind);
        if (FixtureGroup *g = CreateFixtureGroup::findSameGroup(m_doc, probe.headsMap()))
        {
            existing << tr("%1 (as “%2”)").arg(kind, g->name());
            same << g;
            again << entry;
            continue;
        }
        named << entry;
    }
    if (!existing.isEmpty())
    {
        /* Kept by default -- but a second layout of the same heads (from
           above as well as from the front) is a real want, so it can be made
           too, as the one-group dialog allows. */
        QMessageBox box(QMessageBox::Question, windowTitle(),
                        tr("These already exist with exactly the same heads: %1.")
                            .arg(existing.join(QStringLiteral(", "))),
                        QMessageBox::Cancel, this);
        QPushButton *keep = box.addButton(tr("Keep Them"), QMessageBox::AcceptRole);
        QPushButton *another = box.addButton(tr("Make Another"), QMessageBox::DestructiveRole);
        box.setDefaultButton(keep);
        box.exec();
        if (box.clickedButton() == keep)
            m_created << same;
        else if (box.clickedButton() == another)
            named << again;
        else
        {
            m_created.clear();
            return;
        }
    }
    m_created << CreateFixtureGroup::createByHeadKind(m_doc, m_ids, named, view, gaps);
    m_doc->setModified();
    QDialog::accept();
}
