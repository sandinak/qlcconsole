/*
  Q Light Controller Plus
  fixturefields.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <functional>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>

#include "structurestudioview.h"
#include "monitorgraphicsview.h"
#include "patchinventorydialog.h"
#include "colourswatchbutton.h"
#include "monitorproperties.h"
#include "fixturefields.h"
#include "apputil.h"
#include "wledconfigcheck.h"
#include "powerprofiledialog.h"
#include "lightcontrols.h"
#include "monitor.h"
#include "fixture.h"
#include "tower.h"
#include "doc.h"

namespace
{
/* "mixed": an extra item a combo shows while the fixtures differ, gone once
   one is chosen. Its data is -99 in every role a combo here reads. */
const int MixedData = -99;

void comboMixed(QComboBox *cb, bool mixed)
{
    const int at = cb->findData(MixedData, Qt::UserRole);
    if (mixed && at < 0)
    {
        cb->addItem(QObject::tr("— mixed —"));
        cb->setItemData(cb->count() - 1, MixedData, Qt::UserRole);
        cb->setItemData(cb->count() - 1, MixedData, Qt::UserRole + 1);
    }
    else if (!mixed && at >= 0)
    {
        cb->removeItem(at);
    }
    if (mixed)
        cb->setCurrentIndex(cb->findData(MixedData, Qt::UserRole));
}

bool comboIsMixed(const QComboBox *cb)
{
    return cb->currentIndex() < 0 || cb->currentData(Qt::UserRole).toInt() == MixedData;
}
}

/* "mixed" on a spin box: its lowest value, one step below its real range. */
void FixtureFields::spinMixed(QDoubleSpinBox *sp, bool mixed, double lowest)
{
    if (mixed)
    {
        sp->setMinimum(lowest - 1.0);
        sp->setSpecialValueText(QObject::tr("mixed"));
        sp->setValue(sp->minimum());
    }
    else
    {
        sp->setSpecialValueText(QString());
        sp->setMinimum(lowest);
    }
}

bool FixtureFields::spinIsMixed(const QDoubleSpinBox *sp)
{
    return !sp->specialValueText().isEmpty() && sp->value() <= sp->minimum();
}

FixtureFields::FixtureFields(Monitor *monitor, QFormLayout *form, QWidget *parent,
                             StructureStudioView *view)
    : QObject(parent)
    , m_monitor(monitor)
    , m_form(form)
    , m_parent(parent)
    , m_view(view)
    , m_shown(Fixture::invalidId())
{
    /* WHAT it is mounted on: how it sits ("Top", "Inside") says nothing about
       what it sits on. */
    m_mountOn = new QLabel(parent);
    m_mountOn->setStyleSheet("color:#b0b4c0;");
    m_mountOn->setWordWrap(true);
    form->addRow(tr("Mounted on:"), m_mountOn);

    /* Where on a tower: its top cap, a shelf, or its base -- a tower whose
       shelves stop short of its top has no other way to put a followspot on
       the cap. */
    m_towerPos = new QComboBox(parent);
    m_towerPos->setObjectName(QStringLiteral("fixtureTowerPosition"));
    m_towerPos->setToolTip(tr("Where on the tower: on its top cap, on a shelf, or on its base"));
    form->addRow(tr("Position:"), m_towerPos);

    m_colour = new ColourSwatchButton(QColor(), tr("Fixture colour"), parent);
    m_colour->setObjectName(QStringLiteral("fixtureColour"));
    m_colour->setResetColour(QColor());         // Reset: no gel
    m_colour->setToolTip(tr("Its gel -- or, for one that makes its own colour, how it is "
                            "drawn on the plot when it is dark"));
    form->addRow(tr("Colour:"), m_colour);

    m_orientation = new QComboBox(parent);
    m_orientation->setObjectName(QStringLiteral("fixtureOrientation"));
    m_orientation->setToolTip(tr("Base direction relative to the stage floor (gravity), "
                                 "not the mounting face. On a moving head, Hung vs Upright "
                                 "inverts pan/tilt."));
    form->addRow(tr("Orientation:"), m_orientation);

    /* Which way it faces, not just which plane it lies in. In
       FixtureRigProps::Face order. On the outside of a truss it also moves to
       that side of it -- see MonitorProperties::setFixtureFace(). */
    m_face = new QComboBox(parent);
    m_face->setObjectName(QStringLiteral("fixtureFace"));
    m_face->addItems({ tr("Top"), tr("Bottom"), tr("Front (downstage)"), tr("Back (upstage)"),
                       tr("Stage left"), tr("Stage right") });
    for (int i = 0; i < m_face->count(); ++i)
        m_face->setItemData(i, i, Qt::UserRole);
    m_face->setToolTip(tr("The face it is on, and so which way it faces out. On the "
                          "outside of a truss it also moves to that side of the truss."));
    form->addRow(tr("Face:"), m_face);

    /* WHERE it sits relative to what carries it: "Inside" is a unit between a
       truss's chords, or in a clear-topped step firing up through it. */
    m_placement = new QComboBox(parent);
    m_placement->setObjectName(QStringLiteral("fixturePlacement"));
    m_placement->addItem(tr("On the surface"), FixtureRigProps::OnSurface);
    m_placement->addItem(tr("Inside"),         FixtureRigProps::Inside);
    m_placement->setToolTip(tr("Bolted to the outside, or rigged WITHIN the structure — "
                               "inside a step, or between a truss's chords. An inside "
                               "fixture sits in the volume rather than on the face; drag "
                               "it in Front or Side to set its height in there, and give "
                               "the step a clear or open top to see it."));
    form->addRow(tr("Placement:"), m_placement);

    m_angle = new QDoubleSpinBox(parent);
    m_angle->setObjectName(QStringLiteral("fixtureAngle"));
    m_angle->setRange(-180, 180);
    m_angle->setDecimals(0);
    m_angle->setSuffix(QStringLiteral("°"));
    m_angle->setToolTip(tr("Which way it runs on its face: 0° along the face's "
                           "horizontal (level), 90° vertical."));
    form->addRow(tr("Angle:"), m_angle);

    /* ROLL about its own length, as distinct from Angle, which turns it
       within its face: a bar in a step built on a diagonal, firing up toward
       the cast. */
    m_tilt = new QDoubleSpinBox(parent);
    m_tilt->setObjectName(QStringLiteral("fixtureTilt"));
    m_tilt->setRange(-180, 180);
    m_tilt->setDecimals(0);
    m_tilt->setSuffix(QStringLiteral("°"));
    m_tilt->setToolTip(tr("Tip the fixture about its own length — a bar lying in a step "
                          "angled up toward the cast. Positive tips it upstage. Angle "
                          "above turns it flat within its face instead."));
    form->addRow(tr("Tilt:"), m_tilt);

    /* What makes its light and what it is seen through, where that is a
       question: a lantern round one bulb, LEDs, diffusing or clear glass. */
    m_light = new LightControls(parent);
    form->addRow(tr("Light:"), m_light->lightRow());
    form->addRow(tr("Glass:"), m_light->glassCombo());

    /* Its DMX home, editable where the fixture is looked at. A change moves it
       at once if the channels are free, and says why not if they are not;
       "Patch…" opens the Patch dialog on all of them, to move several as a
       block or give an unpatched one an address. */
    m_dmxBox = new QWidget(parent);
    QHBoxLayout *dmxRow = new QHBoxLayout(m_dmxBox);
    dmxRow->setContentsMargins(0, 0, 0, 0);
    dmxRow->setSpacing(4);
    m_dmxUni = new QSpinBox(m_dmxBox);
    m_dmxUni->setObjectName(QStringLiteral("fixtureUniverse"));
    m_dmxUni->setRange(1, 999);
    m_dmxUni->setPrefix(tr("U "));
    m_dmxUni->setKeyboardTracking(false);       // a typed number moves it once, on Enter
    m_dmxUni->setToolTip(tr("Universe"));
    m_dmxAddr = new QSpinBox(m_dmxBox);
    m_dmxAddr->setObjectName(QStringLiteral("fixtureAddress"));
    m_dmxAddr->setRange(1, 512);
    m_dmxAddr->setKeyboardTracking(false);
    m_dmxAddr->setToolTip(tr("Start address"));
    QToolButton *patchBtn = m_patch = new QToolButton(m_dmxBox);
    patchBtn->setObjectName(QStringLiteral("fixturePatch"));
    patchBtn->setText(tr("Patch…"));
    patchBtn->setToolTip(tr("Give a universe and address, or move to another -- these "
                            "fixtures together, reviewed before it is applied"));
    dmxRow->addWidget(m_dmxUni);
    dmxRow->addWidget(m_dmxAddr, 1);
    dmxRow->addWidget(patchBtn);
    form->addRow(tr("DMX:"), m_dmxBox);
    m_dmxNote = new QLabel(parent);
    m_dmxNote->setWordWrap(true);
    m_dmxNote->setStyleSheet("color:#e07070;");
    m_dmxNote->hide();
    form->addRow(QString(), m_dmxNote);

    /* A WLED whose own settings do not match how the show drives it
       (WledConfigCheck): what is wrong, and Fix on Device. */
    m_wledBox = new QWidget(parent);
    QHBoxLayout *wledRow = new QHBoxLayout(m_wledBox);
    wledRow->setContentsMargins(0, 0, 0, 0);
    wledRow->setSpacing(4);
    m_wledNote = new QLabel(m_wledBox);
    m_wledNote->setObjectName(QStringLiteral("fixtureWledNote"));
    m_wledNote->setWordWrap(true);
    m_wledNote->setStyleSheet("color:#e0a040;");
    QToolButton *wledFix = new QToolButton(m_wledBox);
    wledFix->setObjectName(QStringLiteral("fixtureWledFix"));
    wledFix->setText(tr("Fix on Device…"));
    wledFix->setToolTip(tr("Show what would change in the WLED's own settings, then write it"));
    wledRow->addWidget(m_wledNote, 1);
    wledRow->addWidget(wledFix, 0, Qt::AlignTop);
    form->addRow(tr("WLED:"), m_wledBox);
    connect(wledFix, &QToolButton::clicked, this, [this]() {
        if (WledConfigCheck::instance() != nullptr)
            WledConfigCheck::instance()->confirmFix(m_parent, m_fids);
    });
    if (WledConfigCheck::instance() != nullptr)
        connect(WledConfigCheck::instance(), &WledConfigCheck::changed, this, [this]() {
            if (!m_fids.isEmpty())
                reload();
        });

    /* What it draws and the most it may ("some fixtures are wattage hogs ..
       set maximum brightness per fixture .. to prevent overload"): the cap in
       amps, what has been measured, and where to type or measure it. */
    QWidget *powerBox = new QWidget(parent);
    QVBoxLayout *powerCol = new QVBoxLayout(powerBox);
    powerCol->setContentsMargins(0, 0, 0, 0);
    powerCol->setSpacing(2);
    QHBoxLayout *powerRow = new QHBoxLayout;
    powerRow->setSpacing(4);
    m_maxAmps = new QDoubleSpinBox(powerBox);
    m_maxAmps->setObjectName(QStringLiteral("fixtureMaxAmps"));
    m_maxAmps->setRange(0.0, 100.0);
    m_maxAmps->setDecimals(1);
    m_maxAmps->setSingleStep(0.1);
    m_maxAmps->setSuffix(QStringLiteral(" A"));
    m_maxAmps->setSpecialValueText(tr("No limit"));
    m_maxAmps->setToolTip(tr("The most it may draw. Whenever what it is sent would draw more, "
                             "its output is scaled down -- every colour together, so the colour "
                             "holds. Needs its draw measured (Measure… or Profile…)."));
    QToolButton *profileBtn = new QToolButton(powerBox);
    profileBtn->setObjectName(QStringLiteral("fixtureProfile"));
    profileBtn->setText(tr("Profile…"));
    profileBtn->setToolTip(tr("Type in its meter readings"));
    QToolButton *measureBtn = new QToolButton(powerBox);
    measureBtn->setObjectName(QStringLiteral("fixtureMeasure"));
    measureBtn->setText(tr("Measure…"));
    measureBtn->setToolTip(tr("Light it a step at a time and enter what the meter reads"));
    powerRow->addWidget(m_maxAmps, 1);
    powerRow->addWidget(profileBtn);
    powerRow->addWidget(measureBtn);
    m_powerNote = new QLabel(powerBox);
    m_powerNote->setObjectName(QStringLiteral("fixturePowerNote"));
    m_powerNote->setWordWrap(true);
    m_powerNote->setStyleSheet("color:#8a8e99;");
    powerCol->addLayout(powerRow);
    powerCol->addWidget(m_powerNote);
    form->addRow(tr("Max draw:"), powerBox);
    connect(m_maxAmps, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_loading || m_fids.isEmpty() || v < 0.0)
            return;                         // "mixed": nothing set yet
        emit aboutToEdit(QStringLiteral("maxAmps"));
        foreach (quint32 fid, m_fids)
            if (Fixture *fx = m_monitor->m_doc->fixture(fid))
            {
                PowerProfile pp = fx->powerProfile();
                pp.maxAmps = v;
                fx->setPowerProfile(pp);
            }
        finishEdit();
    });
    connect(profileBtn, &QToolButton::clicked, this, [this]() {
        if (m_fids.isEmpty())
            return;
        emit aboutToEdit(QStringLiteral("powerProfile"));
        PowerProfileDialog dlg(m_monitor->m_doc, m_fids, m_parent);
        if (dlg.exec() == QDialog::Accepted)
            finishEdit();
    });
    connect(measureBtn, &QToolButton::clicked, this, [this]() {
        if (m_fids.isEmpty())
            return;
        emit aboutToEdit(QStringLiteral("powerProfile"));
        PowerMeasureDialog dlg(m_monitor->m_doc, m_fids, m_parent);
        if (dlg.exec() == QDialog::Accepted)
            finishEdit();
    });

    MonitorProperties *props = monitor->m_props;
    MonitorGraphicsView *gv = monitor->m_graphicsView;

    /* Each field: the fixtures that differ on it read "mixed" -- nothing is
       written for that until it is set, then it is set on all of them. */
    auto eachRig = [this, props, gv](const QString &field,
                                     const std::function<void(quint32, FixtureRigProps &)> &set) {
        if (m_loading || m_fids.isEmpty())
            return;
        emit aboutToEdit(field);
        foreach (quint32 fid, m_fids)
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            set(fid, rp);
            props->setFixtureRigProps(fid, rp);
            if (gv != nullptr)
                gv->updateFixture(fid);
        }
        finishEdit();
    };

    connect(m_colour, &ColourSwatchButton::colourChanged, this, [this, props, gv](const QColor &c) {
        if (m_loading || m_fids.isEmpty())
            return;
        emit aboutToEdit(QStringLiteral("colour"));
        foreach (quint32 fid, m_fids)
        {
            props->setFixtureGelColor(fid, 0, 0, c);
            if (gv != nullptr)
            {
                gv->setFixtureGelColor(fid, c);
                gv->updateFixture(fid);
            }
        }
        finishEdit();
    });
    connect(m_orientation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, props, eachRig](int) {
        if (comboIsMixed(m_orientation))
            return;
        const int mount = m_orientation->currentData(Qt::UserRole).toInt();
        const int side = m_orientation->currentData(Qt::UserRole + 1).toInt();
        eachRig(QStringLiteral("orientation"), [mount, side, props](quint32, FixtureRigProps &rp) {
            rp.mountingType = static_cast<Truss::MountingType>(mount);
            if (side >= 0)
                rp.trussMountSide = side;       // on-top / under / centred on a bar
            // A mover/PAR has no Face control, so nothing else ever moves
            // studioMount off its stray default: the load path's correction.
            props->repairMountPlane(rp);
        });
    });
    connect(m_face, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!comboIsMixed(m_face))
            applyFace(m_face->currentData(Qt::UserRole).toInt());
    });
    connect(m_placement, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, eachRig](int) {
        if (comboIsMixed(m_placement))
            return;
        const int placement = m_placement->currentData(Qt::UserRole).toInt();
        eachRig(QStringLiteral("placement"), [placement](quint32, FixtureRigProps &rp) {
            rp.placement = placement;
        });
    });
    connect(m_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, eachRig](double v) {
        if (spinIsMixed(m_angle))
            return;
        eachRig(QStringLiteral("angle"), [v](quint32, FixtureRigProps &rp) { rp.studioAngle = float(v); });
    });
    connect(m_tilt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, eachRig](double v) {
        if (spinIsMixed(m_tilt))
            return;
        eachRig(QStringLiteral("tilt"), [v](quint32, FixtureRigProps &rp) { rp.studioTilt = float(v); });
    });
    connect(m_light, &LightControls::edited, this, [this, monitor, eachRig]() {
        Doc *doc = monitor->m_doc;
        eachRig(QStringLiteral("light"), [this, doc](quint32 fid, FixtureRigProps &rp) {
            m_light->store(doc->fixture(fid), rp);
        });
    });
    connect(m_towerPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, eachRig](int idx) {
        if (idx < 0)
            return;
        const int side = m_towerPos->itemData(idx, Qt::UserRole).toInt();
        const int shelf = m_towerPos->itemData(idx, Qt::UserRole + 1).toInt();
        eachRig(QStringLiteral("towerPosition"), [side, shelf](quint32, FixtureRigProps &rp) {
            if (rp.towerId == Tower::invalidId())
                return;
            // On the cap or the base it stands, as the plot's Mount on tower does.
            if (side != FixtureRigProps::TowerShelf && side != rp.towerMountSide)
                rp.mountingType = Truss::FloorMounted;
            rp.towerMountSide = side;
            if (shelf >= 0)
                rp.towerShelf = shelf;
        });
    });
    connect(m_dmxUni, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { moveDmx(); });
    connect(m_dmxAddr, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { moveDmx(); });
    connect(patchBtn, &QToolButton::clicked, this, [this, monitor]() {
        if (m_fids.isEmpty())
            return;
        emit aboutToEdit(QStringLiteral("patch"));
        PatchInventoryDialog pd(monitor->m_doc, m_fids, m_parent);
        if (pd.exec() == QDialog::Accepted)
        {
            if (monitor->m_graphicsView != nullptr)
            {
                foreach (quint32 fid, m_fids)
                    monitor->m_graphicsView->updateFixture(fid);
            }
            finishEdit();
        }
    });

    hideAll();
}

void FixtureFields::row(QWidget *w, bool visible)
{
    w->setVisible(visible);
    if (QWidget *l = m_form->labelForField(w))
        l->setVisible(visible);
}

void FixtureFields::hideAll()
{
    for (QWidget *w : { static_cast<QWidget *>(m_mountOn), static_cast<QWidget *>(m_towerPos),
                        static_cast<QWidget *>(m_colour), static_cast<QWidget *>(m_orientation),
                        static_cast<QWidget *>(m_face), static_cast<QWidget *>(m_placement),
                        static_cast<QWidget *>(m_angle), static_cast<QWidget *>(m_tilt),
                        m_light->lightRow(), static_cast<QWidget *>(m_light->glassCombo()),
                        m_dmxBox, static_cast<QWidget *>(m_dmxNote), m_wledBox })
        row(w, false);
}

void FixtureFields::finishEdit()
{
    m_monitor->m_doc->setModified();
    if (m_view != nullptr)
        m_view->reload();
    emit edited();
    reload();               // "mixed" gone where it was set; "Mounted on" follows
}

void FixtureFields::applyFace(int face)
{
    if (m_loading || m_fids.isEmpty() || face < 0)
        return;
    emit aboutToEdit(QStringLiteral("face"));
    MonitorProperties *props = m_monitor->m_props;
    foreach (quint32 fid, m_fids)
    {
        if (m_view != nullptr)
        {
            // Re-pins a frame-group fixture to that face of its frame too.
            m_view->setFixtureFacing(fid, FixtureRigProps::Face(face));
        }
        else
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            props->setFixtureFace(rp, FixtureRigProps::Face(face));
            props->setFixtureRigProps(fid, rp);
        }
        if (m_monitor->m_graphicsView != nullptr)
            m_monitor->m_graphicsView->updateFixture(fid);
    }
    finishEdit();
}

void FixtureFields::moveDmx()
{
    if (m_loading || m_fids.size() != 1)
        return;
    Doc *doc = m_monitor->m_doc;
    Fixture *fx = doc->fixture(m_fids.first());
    if (fx == nullptr || !fx->isPatched())
        return;
    const quint32 uni = quint32(m_dmxUni->value() - 1);
    const quint32 addr = quint32(m_dmxAddr->value() - 1);
    if (uni == fx->universe() && addr == fx->address())
        return;
    emit aboutToEdit(QStringLiteral("dmx"));
    QString why;
    if (!doc->patchFixtures({ Doc::FixturePatch{ fx->id(), uni, addr } }, &why))
    {
        // Left showing what was asked for, so the reason reads against it.
        m_dmxNote->setText(tr("Not moved: %1").arg(why));
        m_dmxNote->show();
        if (QWidget *l = m_form->labelForField(m_dmxNote))
            l->show();
        return;
    }
    if (m_monitor->m_graphicsView != nullptr)
        m_monitor->m_graphicsView->updateFixture(fx->id());
    finishEdit();
}

void FixtureFields::setFixtures(const QList<quint32> &fids, quint32 shown)
{
    Doc *doc = m_monitor->m_doc;
    MonitorProperties *props = m_monitor->m_props;
    const QList<quint32> wanted = fids;     // may be m_fids itself (reload())
    m_fids.clear();
    foreach (quint32 fid, wanted)
        if (doc->fixture(fid) != nullptr)
            m_fids << fid;
    m_shown = m_fids.contains(shown) ? shown
                                     : (m_fids.isEmpty() ? Fixture::invalidId() : m_fids.first());
    hideAll();
    Fixture *fx = doc->fixture(m_shown);
    if (fx == nullptr)
        return;

    m_loading = true;
    const FixtureRigProps rp = props->fixtureRigProps(m_shown);
    const bool many = m_fids.size() > 1;
    auto differs = [this](const std::function<bool(quint32)> &sameAsShown) {
        foreach (quint32 fid, m_fids)
            if (!sameAsShown(fid))
                return true;
        return false;
    };

    // Which rows THIS one has: Orientation for a body with a base (movers,
    // pars); Face + Angle ("which way it runs") for a linear strip/bar and any
    // frame-group member. On a tower shelf it is oriented ON or UNDER the
    // shelf -- Orientation, not Face/Angle, whatever its head count.
    const bool frame = props->fixtureFrameGroup(m_shown) != 0;
    const bool linear = m_monitor->isLinearFixture(fx);
    const bool onTower = rp.towerId != Tower::invalidId();

    m_mountOn->setText(many ? tr("%1 (and %n more)", "", m_fids.size() - 1)
                                  .arg(m_monitor->mountDescription(rp, m_shown))
                            : m_monitor->mountDescription(rp, m_shown));
    row(m_mountOn, true);

    // Colour
    const QColor gel = props->fixtureGelColor(m_shown, 0, 0);
    if (differs([props, gel](quint32 f) { return props->fixtureGelColor(f, 0, 0) == gel; }))
        m_colour->setMixed();
    else
        m_colour->setColour(gel);
    row(m_colour, true);

    // Orientation -- its options are what the mount allows (shared helper).
    m_monitor->fillOrientationCombo(m_orientation, rp);
    m_orientation->blockSignals(true);
    comboMixed(m_orientation, differs([props, rp](quint32 f) {
        return props->fixtureRigProps(f).mountingType == rp.mountingType; }));
    m_orientation->blockSignals(false);
    row(m_orientation, !linear || onTower);

    // Face, Angle
    m_face->blockSignals(true);
    m_face->setCurrentIndex(int(rp.face()));
    comboMixed(m_face, differs([props, rp](quint32 f) { return props->fixtureRigProps(f).face() == rp.face(); }));
    m_face->blockSignals(false);
    row(m_face, (linear || frame) && !onTower);

    m_angle->blockSignals(true);
    spinMixed(m_angle, differs([props, rp](quint32 f) {
        return qFuzzyCompare(1.0f + props->fixtureRigProps(f).studioAngle, 1.0f + rp.studioAngle); }), -180.0);
    if (!spinIsMixed(m_angle))
        m_angle->setValue(double(rp.studioAngle));
    m_angle->blockSignals(false);
    row(m_angle, (linear || frame) && !onTower);

    // Placement, Tilt
    m_placement->blockSignals(true);
    m_placement->setCurrentIndex(qBound(0, rp.placement, 1));
    comboMixed(m_placement, differs([props, rp](quint32 f) {
        return props->fixtureRigProps(f).placement == rp.placement; }));
    m_placement->blockSignals(false);
    row(m_placement, true);

    m_tilt->blockSignals(true);
    spinMixed(m_tilt, differs([props, rp](quint32 f) {
        return qFuzzyCompare(1.0f + props->fixtureRigProps(f).studioTilt, 1.0f + rp.studioTilt); }), -180.0);
    if (!spinIsMixed(m_tilt))
        m_tilt->setValue(double(rp.studioTilt));
    m_tilt->blockSignals(false);
    row(m_tilt, true);

    // Its place on a tower: the one shown (a place is one fixture's).
    if (Tower *tw = (onTower && !many) ? props->tower(rp.towerId) : nullptr)
    {
        const bool isFeet = (props->gridUnits() == MonitorProperties::Feet);
        const double toDisp = isFeet ? 3.28084 : 1.0;
        const QString unit = isFeet ? tr("ft") : tr("m");
        m_towerPos->blockSignals(true);
        m_towerPos->clear();
        auto addPos = [this](const QString &label, int side, int shelf) {
            m_towerPos->addItem(label);
            m_towerPos->setItemData(m_towerPos->count() - 1, side, Qt::UserRole);
            m_towerPos->setItemData(m_towerPos->count() - 1, shelf, Qt::UserRole + 1);
        };
        addPos(tr("Top (%1 %2)").arg(double(tw->height()) * toDisp, 0, 'f', 2).arg(unit),
               FixtureRigProps::TowerTop, -1);
        for (int i = tw->shelfCount() - 1; i >= 0; --i)
            addPos(tr("Shelf %1 (%2 %3)").arg(i + 1)
                       .arg(double(tw->shelfHeight(i)) * toDisp, 0, 'f', 2).arg(unit),
                   FixtureRigProps::TowerShelf, i);
        addPos(tr("Bottom"), FixtureRigProps::TowerBottom, -1);
        for (int i = 0; i < m_towerPos->count(); ++i)
        {
            const int side = m_towerPos->itemData(i, Qt::UserRole).toInt();
            const int shelf = m_towerPos->itemData(i, Qt::UserRole + 1).toInt();
            if (side == rp.towerMountSide
                && (side != FixtureRigProps::TowerShelf || shelf == int(rp.towerShelf)))
                m_towerPos->setCurrentIndex(i);
        }
        m_towerPos->blockSignals(false);
        row(m_towerPos, true);
    }

    // Light, Glass: for those whose definition makes light all round or of LEDs.
    QList<const Fixture *> fixtures;
    foreach (quint32 fid, m_fids)
        fixtures << doc->fixture(fid);
    const bool lit = LightControls::relevant(fx);
    if (lit)
        m_light->load(doc, fixtures);
    row(m_light->lightRow(), lit);
    row(m_light->glassCombo(), lit);

    // DMX: one fixture's address, moved here; several, or an unpatched one,
    // are given addresses by Patch….
    const bool patched = fx->isPatched();
    m_dmxUni->setValue(int(fx->universe()) + 1);
    m_dmxAddr->setValue(int(fx->address()) + 1);
    m_dmxUni->setEnabled(patched && !many);
    m_dmxAddr->setEnabled(patched && !many);
    m_dmxAddr->setSuffix(fx->channels() > 1 && !many
                             ? tr("–%1").arg(int(fx->address() + fx->channels()))
                             : QString());
    row(m_dmxBox, true);
    m_patch->setEnabled(AppUtil::patchAllowed(doc, m_fids));     // one rule (AppUtil)
    QString note;
    if (many)
        note = tr("%1 fixtures: Patch… addresses them together.").arg(m_fids.size());
    else if (fx->addressClash())
        note = tr("Unpatched: the show file puts it on channels another fixture uses. "
                  "Patch… gives it a home.");
    else if (!patched)
        note = tr("Unpatched (inventory). Patch… gives it an address.");
    m_dmxNote->setText(note);
    m_dmxNote->setStyleSheet(many ? "color:#8a8e99;" : "color:#e07070;");
    row(m_dmxNote, !note.isEmpty());

    // Its WLED's own settings, where they do not match how it is driven.
    QStringList wled;
    if (WledConfigCheck *check = WledConfigCheck::instance())
    {
        int wrong = 0;
        foreach (quint32 fid, m_fids)
        {
            const WledConfigCheck::Finding f = check->finding(fid);
            if (f.problems.isEmpty())
                continue;
            ++wrong;
            if (!many)
                wled = f.problems;
        }
        if (many && wrong > 0)
            wled << tr("%n of these is not set up as this show drives it", "", wrong);
    }
    m_wledNote->setText(wled.join(QLatin1Char('\n')));
    row(m_wledBox, !wled.isEmpty());

    // The most it may draw, and what has been measured.
    {
        Doc *pdoc = m_monitor->m_doc;
        const Fixture *first = pdoc->fixture(m_fids.isEmpty() ? Fixture::invalidId() : m_fids.first());
        const PowerProfile pp = first != nullptr ? first->powerProfile() : PowerProfile();
        bool maxDiffers = false;
        int measured = 0;
        foreach (quint32 fid, m_fids)
            if (const Fixture *fx = pdoc->fixture(fid))
            {
                maxDiffers = maxDiffers || fx->powerProfile().maxAmps != pp.maxAmps;
                if (fx->powerProfile().isMeasured())
                    ++measured;
            }
        if (maxDiffers)
        {
            m_maxAmps->setMinimum(-1.0);
            m_maxAmps->setSpecialValueText(tr("mixed"));
            m_maxAmps->setValue(-1.0);
        }
        else
        {
            m_maxAmps->setMinimum(0.0);
            m_maxAmps->setSpecialValueText(tr("No limit"));
            m_maxAmps->setValue(pp.maxAmps);
        }
        QString note;
        if (many)
            note = tr("%1 of %2 measured").arg(measured).arg(m_fids.size());
        else if (pp.isMeasured())
        {
            QStringList parts;
            for (auto it = pp.colourAmps.constBegin(); it != pp.colourAmps.constEnd(); ++it)
                parts << QStringLiteral("%1 %2").arg(powerColourName(it.key())).arg(it.value(), 0, 'f', 1);
            if (pp.fullAmps > 0.0)
                parts << tr("full %1").arg(pp.fullAmps, 0, 'f', 1);
            if (pp.halfAmps > 0.0)
                parts << tr("half %1").arg(pp.halfAmps, 0, 'f', 1);
            if (pp.standbyAmps > 0.0)
                parts << tr("off %1").arg(pp.standbyAmps, 0, 'f', 1);
            note = tr("Measured: %1 A").arg(parts.join(QStringLiteral(" · ")));
        }
        else
            note = tr("Not measured: its draw can't be modelled or capped yet");
        if (!pp.isMeasured() && pp.maxAmps > 0.0 && !many)
            note += QStringLiteral("\n") + tr("The maximum does nothing until it is measured.");
        m_powerNote->setText(note);
    }

    m_loading = false;
}
