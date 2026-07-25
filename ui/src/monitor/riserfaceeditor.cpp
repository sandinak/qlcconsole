/*
  Q Light Controller Plus
  riserfaceeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

#include <QResizeEvent>
#include <QShowEvent>

#include "riserfaceeditor.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "truss.h"        // FixtureRigProps
#include "fixture.h"
#include "qlcfixturemode.h"
#include "monitorfixtureitem.h"
#include "doc.h"

RiserFaceEditor::RiserFaceEditor(Doc *doc, StagePlatform *platform, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_platform(platform)
    , m_face(FixtureRigProps::RiserFront)
{
    setWindowTitle(tr("Riser Face — %1").arg(platform->name().isEmpty()
                        ? tr("Platform %1").arg(platform->id()) : platform->name()));
    resize(720, 460);

    QVBoxLayout *vl = new QVBoxLayout(this);

    // Top row: face selector + add fixture.
    QHBoxLayout *top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Face:")));
    m_faceCombo = new QComboBox(this);
    m_faceCombo->addItem(tr("Front (downstage)"), FixtureRigProps::RiserFront);
    m_faceCombo->addItem(tr("Top"),               FixtureRigProps::RiserTop);
    top->addWidget(m_faceCombo);
    top->addSpacing(16);
    top->addWidget(new QLabel(tr("Add fixture:")));
    m_addCombo = new QComboBox(this);
    m_addCombo->setMinimumWidth(200);
    top->addWidget(m_addCombo, 1);
    QPushButton *addBtn = new QPushButton(tr("Add"), this);
    top->addWidget(addBtn);
    QPushButton *rmBtn = new QPushButton(tr("Remove selected"), this);
    top->addWidget(rmBtn);
    QPushButton *distBtn = new QPushButton(tr("Distribute evenly"), this);
    top->addWidget(distBtn);
    vl->addLayout(top);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setBackgroundBrush(QColor(30, 30, 34));
    vl->addWidget(m_view, 1);

    QLabel *hint = new QLabel(tr("Drag fixtures onto the face. Front face: X across, "
                                 "height up. Positions are stored relative to the riser."), this);
    hint->setEnabled(false);
    vl->addWidget(hint);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vl->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_faceCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotFaceChanged(int)));
    connect(addBtn,  &QPushButton::clicked, this, &RiserFaceEditor::slotAddFixture);
    connect(rmBtn,   &QPushButton::clicked, this, &RiserFaceEditor::slotRemoveSelected);
    connect(distBtn, &QPushButton::clicked, this, &RiserFaceEditor::slotDistribute);

    rebuildScene();
}

void RiserFaceEditor::faceSize(qreal &fw, qreal &fh) const
{
    fw = m_platform->width();
    fh = (m_face == FixtureRigProps::RiserTop) ? m_platform->depth() : m_platform->height();
    if (fw <= 0.0) fw = 0.1;
    if (fh <= 0.0) fh = 0.1;
}

void RiserFaceEditor::rebuildScene()
{
    m_scene->clear();
    m_markers.clear();
    m_markerSize.clear();

    qreal fw, fh;
    faceSize(fw, fh);

    // Face rectangle.
    QGraphicsRectItem *face = m_scene->addRect(0, 0, fw * m_ppm, fh * m_ppm,
                                               QPen(QColor(120, 130, 150), 2),
                                               QBrush(QColor(60, 64, 74)));
    face->setZValue(-1);
    m_scene->setSceneRect(-20, -20, fw * m_ppm + 40, fh * m_ppm + 40);

    // Existing markers: fixtures mounted on THIS platform + face.
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, props->fixtureItemsID())
    {
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.riserPlatformId == m_platform->id() && rp.riserFace == m_face)
            addMarker(fid, rp.riserU, rp.riserV);
    }

    // Add-combo: any fixture in the workspace that is FREE — not already mounted
    // on a riser or bound to a truss — even if it isn't on the 2D view yet.
    m_addCombo->clear();
    foreach (Fixture *f, m_doc->fixtures())
    {
        if (f == nullptr)
            continue;
        const quint32 fid = f->id();
        if (m_markers.contains(fid))
            continue;                                   // already on this face
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.trussId != Truss::invalidId() || rp.onRiser())
            continue;                                   // attached to something else
        m_addCombo->addItem(f->name(), fid);
    }

    fitFace();
}

QSizeF RiserFaceEditor::fixtureSizeM(quint32 fid) const
{
    Fixture *f = m_doc->fixture(fid);
    qreal wMm = 0.0, hMm = 0.0;
    if (f != nullptr)
    {
        const QLCFixtureMode *mode = f->fixtureMode();
        if (mode != nullptr)
        {
            wMm = mode->physical().width();
            hMm = mode->physical().height();
        }
    }
    // No declared physical size: assume a linear fixture (LED bar) — long and
    // thin, one row of heads (mirrors MonitorFixtureItem's fallback).
    if (wMm <= 0.0 || hMm <= 0.0)
    {
        const int n = f ? qMax(1, f->heads()) : 1;
        wMm = 300.0;
        hMm = (n > 1) ? 300.0 / n : 60.0;
    }
    return QSizeF(wMm / 1000.0, hMm / 1000.0);   // metres
}

void RiserFaceEditor::fitFace()
{
    if (m_scene != nullptr)
        m_view->fitInView(m_scene->itemsBoundingRect().adjusted(-10, -10, 10, 10),
                          Qt::KeepAspectRatio);
}

void RiserFaceEditor::resizeEvent(QResizeEvent *e)
{
    QDialog::resizeEvent(e);
    fitFace();
}

void RiserFaceEditor::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    fitFace();
}

void RiserFaceEditor::addMarker(quint32 fid, qreal u, qreal v)
{
    qreal fw, fh;
    faceSize(fw, fh);
    u = qBound<qreal>(0.0, u, fw);
    v = qBound<qreal>(0.0, v, fh);

    // The fixture's TRUE physical size in the same scale as the face — no
    // minimum clamp, or a thin strip would render disproportionately tall
    // relative to the (unclamped) face. fitInView makes it visible.
    const QSizeF szM = fixtureSizeM(fid);
    const qreal w = szM.width()  * m_ppm;
    const qreal h = szM.height() * m_ppm;

    // (u,v) is the fixture CENTRE on the face. Front face: v is height up → invert Y.
    const qreal cx = u * m_ppm;
    const qreal cy = (m_face == FixtureRigProps::RiserTop) ? (v * m_ppm)
                                                           : ((fh - v) * m_ppm);

    // Reuse the real monitor fixture rendering (heads, gel, proportions).
    MonitorFixtureItem *m = new MonitorFixtureItem(m_doc, fid);
    m->setSize(QSize(qMax(1, qRound(w)), qMax(1, qRound(h))));
    m->setMovable(true);
    m->showLabel(true);
    m->setPos(cx - w / 2.0, cy - h / 2.0);   // centre the item at (cx,cy)
    m_scene->addItem(m);

    m_markers.insert(fid, m);
    m_markerSize.insert(fid, QSizeF(w, h));
}

void RiserFaceEditor::readMarkers(QHash<quint32, QPointF> &out) const
{
    qreal fw, fh;
    const_cast<RiserFaceEditor *>(this)->faceSize(fw, fh);
    for (auto it = m_markers.constBegin(); it != m_markers.constEnd(); ++it)
    {
        const QSizeF sz = m_markerSize.value(it.key());
        // Centre of the item in scene units (item origin is its top-left).
        const QPointF c = it.value()->pos() + QPointF(sz.width() / 2.0, sz.height() / 2.0);
        qreal u = qBound<qreal>(0.0, c.x() / m_ppm, fw);
        qreal v;
        if (m_face == FixtureRigProps::RiserTop)
            v = qBound<qreal>(0.0, c.y() / m_ppm, fh);
        else
            v = qBound<qreal>(0.0, fh - c.y() / m_ppm, fh);
        out.insert(it.key(), QPointF(u, v));
    }
}

void RiserFaceEditor::slotFaceChanged(int index)
{
    m_face = m_faceCombo->itemData(index).toInt();
    rebuildScene();
}

void RiserFaceEditor::slotAddFixture()
{
    if (m_addCombo->count() == 0)
        return;
    const quint32 fid = m_addCombo->currentData().toUInt();
    qreal fw, fh;
    faceSize(fw, fh);
    addMarker(fid, fw / 2.0, fh / 2.0);   // drop it in the middle
    // Remove from the add-combo.
    m_addCombo->removeItem(m_addCombo->currentIndex());
}

void RiserFaceEditor::slotRemoveSelected()
{
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        auto *r = dynamic_cast<MonitorFixtureItem *>(gi);
        if (r == nullptr) continue;
        const quint32 fid = r->fixtureID();
        if (!m_markers.contains(fid)) continue;
        m_markers.remove(fid);
        m_markerSize.remove(fid);
        m_scene->removeItem(r);
        delete r;
        Fixture *f = m_doc->fixture(fid);
        m_addCombo->addItem(f ? f->name() : tr("Fixture %1").arg(fid), fid);
    }
}

/** Natural (human) string compare: runs of digits are compared as numbers, so
 *  "US #2" < "US #10". Case-insensitive on the non-digit parts. Returns
 *  <0 / 0 / >0 like strcmp. */
static int naturalCompare(const QString &a, const QString &b)
{
    int i = 0, j = 0;
    const int na = a.size(), nb = b.size();
    while (i < na && j < nb)
    {
        const QChar ca = a.at(i);
        const QChar cb = b.at(j);
        if (ca.isDigit() && cb.isDigit())
        {
            int i2 = i, j2 = j;
            while (i2 < na && a.at(i2).isDigit()) i2++;
            while (j2 < nb && b.at(j2).isDigit()) j2++;
            const qulonglong va = a.mid(i, i2 - i).toULongLong();
            const qulonglong vb = b.mid(j, j2 - j).toULongLong();
            if (va != vb)
                return va < vb ? -1 : 1;
            i = i2;
            j = j2;
        }
        else
        {
            const int c = QString::compare(QString(ca), QString(cb), Qt::CaseInsensitive);
            if (c != 0)
                return c;
            i++;
            j++;
        }
    }
    return (na - i) - (nb - j);
}

/** The last contiguous run of digits in @p s parsed as a number (e.g.
 *  "US1 #12" → 12), or -1 if the name contains no digits. Used to order
 *  step-row fixtures by their "#N" index regardless of prefix. */
static qlonglong trailingNumber(const QString &s)
{
    int end = s.size();
    while (end > 0 && !s.at(end - 1).isDigit())
        end--;
    if (end == 0)
        return -1;
    int start = end;
    while (start > 0 && s.at(start - 1).isDigit())
        start--;
    return s.mid(start, end - start).toLongLong();
}

void RiserFaceEditor::slotDistribute()
{
    if (m_markers.isEmpty())
        return;
    qreal fw, fh;
    faceSize(fw, fh);

    // Order fixtures top-to-bottom / left-to-right by their label's trailing
    // index (the "#N" step number). Sorting by that number FIRST means an
    // inconsistent prefix — e.g. "US1 #1..#4" mixed with "US #5..#8" — still
    // lays out #1..#8 in order (a plain natural compare would put "US #5"
    // before "US1 #1" because ' ' < '1'). Falls back to natural compare when
    // the trailing numbers tie or are absent.
    QList<quint32> ids = m_markers.keys();
    std::sort(ids.begin(), ids.end(), [this](quint32 a, quint32 b) {
        Fixture *fa = m_doc->fixture(a);
        Fixture *fb = m_doc->fixture(b);
        const QString na = fa ? fa->name() : QString::number(a);
        const QString nb = fb ? fb->name() : QString::number(b);
        const qlonglong ta = trailingNumber(na);
        const qlonglong tb = trailingNumber(nb);
        if (ta >= 0 && tb >= 0 && ta != tb)
            return ta < tb;
        const int c = naturalCompare(na, nb);
        return (c != 0) ? (c < 0) : (a < b);
    });
    const int n = ids.size();

    // Auto-orient: fixtures WIDER than tall (e.g. LED bars) stack VERTICALLY;
    // taller-than-wide fixtures spread HORIZONTALLY. Decide from the total.
    qreal sumW = 0.0, sumH = 0.0;
    foreach (quint32 id, ids)
    {
        const QSizeF s = m_markerSize.value(id);
        sumW += s.width();
        sumH += s.height();
    }
    const bool stackVertically = (sumW >= sumH);

    for (int i = 0; i < n; i++)
    {
        qreal u, v;
        if (stackVertically)
        {
            // Stack up the face, centred horizontally. Label order = top→bottom,
            // and on the front face the top is the HIGHEST V.
            u = fw / 2.0;
            v = fh * (n - i - 0.5) / n;
        }
        else
        {
            // Spread across the face, centred vertically. Label order left→right.
            u = fw * (i + 0.5) / n;
            v = fh / 2.0;
        }
        MonitorFixtureItem *m = m_markers.value(ids.at(i));
        const QSizeF sz = m_markerSize.value(ids.at(i));
        const qreal cx = u * m_ppm;
        const qreal cy = (m_face == FixtureRigProps::RiserTop) ? (v * m_ppm)
                                                               : ((fh - v) * m_ppm);
        m->setPos(cx - sz.width() / 2.0, cy - sz.height() / 2.0);
    }
}

void RiserFaceEditor::applyToRig()
{
    MonitorProperties *props = m_doc->monitorProperties();

    // Clear existing mounts on THIS platform + face (removed markers unmount).
    foreach (quint32 fid, props->fixtureItemsID())
    {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.riserPlatformId == m_platform->id() && rp.riserFace == m_face)
        {
            rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
            props->setFixtureRigProps(fid, rp);
        }
    }

    // Write the current markers.
    QHash<quint32, QPointF> pos;
    readMarkers(pos);
    for (auto it = pos.constBegin(); it != pos.constEnd(); ++it)
    {
        // A fixture that was never placed on the 2D view has no monitor entry —
        // create one so it renders (its position is then derived from the riser).
        if (!props->containsFixture(it.key()))
            props->setFixturePosition(it.key(), 0, 0, QVector3D(0, 0, 0));

        FixtureRigProps rp = props->fixtureRigProps(it.key());
        rp.trussId = Truss::invalidId();          // a riser mount replaces truss binding
        rp.riserPlatformId = m_platform->id();
        rp.riserFace = m_face;
        rp.riserU = float(it.value().x());
        rp.riserV = float(it.value().y());
        props->setFixtureRigProps(it.key(), rp);
    }
    m_doc->setModified();
}
