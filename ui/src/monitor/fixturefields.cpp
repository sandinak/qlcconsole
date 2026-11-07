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
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QLabel>
#include <QStandardItemModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QPointer>

#include "structurestudioview.h"
#include "monitorgraphicsview.h"
#include "patchinventorydialog.h"
#include "colourswatchbutton.h"
#include "monitorproperties.h"
#include "fixturefields.h"
#include "rigowner.h"
#include "fixturegroup.h"
#include "apputil.h"
#include "wledconfigcheck.h"
#include "wleddevicewrite.h"
#include "powerprofiledialog.h"
#include "showsettingsdialog.h"
#include "lightcontrols.h"
#include "monitor.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "tower.h"
#include "righeads.h"
#include "doc.h"
#include "itemicons.h"
#include "scenery.h"
#include "feetinchesspinbox.h"
#include "pixelpathdecor.h"
#include "stagetarget.h"
#include "pixelpath.h"
#include "fountain.h"
#include "sprinklespansdialog.h"

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

void FixtureFields::fillStageLookCombo(QComboBox *cb, const Fixture *fx)
{
    const bool was = cb->blockSignals(true);
    cb->clear();
    const Fixture::StageLook autoLook = fx != nullptr ? fx->autoStageLook() : Fixture::AutoLook;
    // A curtain on Auto is shown as a curtain (ItemIcons::autoLookIcon()).
    const QString autoName = Scenery::isCurtainByDefinition(fx) ? tr("Curtain")
                             : autoLook != Fixture::AutoLook
                             ? Fixture::stageLookName(autoLook)
                             : (fx != nullptr && fx->fixtureDef() != nullptr
                                ? QLCFixtureDef::typeToString(fx->fixtureDef()->type()) : QString());
    const QIcon autoIcon = ItemIcons::autoLookIcon(fx);
    cb->addItem(autoIcon, autoName.isEmpty() ? tr("Auto") : tr("Auto (%1)").arg(autoName),
                int(Fixture::AutoLook));
    int section = -1;
    foreach (Fixture::StageLook look, Fixture::stageLooks())
    {
        if (look == Fixture::AutoLook)
            continue;
        const int sec = Fixture::stageLookSection(look);
        if (sec != section)
        {
            section = sec;
            cb->insertSeparator(cb->count());
            cb->addItem(Fixture::stageLookSectionName(Fixture::StageLookSection(sec)));
            if (QStandardItemModel *m = qobject_cast<QStandardItemModel *>(cb->model()))
            {
                QStandardItem *heading = m->item(cb->count() - 1);
                heading->setFlags(heading->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
                QFont f = heading->font();
                f.setBold(true);
                heading->setFont(f);
            }
        }
        cb->addItem(QIcon(Fixture::stageLookIconResource(look)),
                    QStringLiteral("   ") + Fixture::stageLookName(look), int(look));
    }
    cb->blockSignals(was);
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
    /* What it looks like on stage: its icon in the lists, its shape on the
       plot and in the studio. */
    m_look = new QComboBox(parent);
    m_look->setObjectName(QStringLiteral("fixtureStageLook"));
    m_look->setIconSize(QSize(24, 24));
    m_look->setToolTip(tr("Its plot symbol, what it looks like on stage: its icon in the fixture lists and how "
                          "the plot and the studio draw it. Auto follows its definition's type. "
                          "Only the picture changes -- not how it is driven."));
    form->addRow(tr("Plot symbol:"), m_look);
    connect(m_look, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_loading || m_fids.isEmpty() || comboIsMixed(m_look))
            return;
        const QVariant v = m_look->currentData(Qt::UserRole);
        if (!v.isValid())
            return;                         // a section heading
        const Fixture::StageLook look = Fixture::StageLook(v.toInt());
        emit aboutToEdit(QStringLiteral("stageLook"));
        foreach (quint32 fid, m_fids)
            if (Fixture *fx = m_monitor->m_doc->fixture(fid))
                fx->setStageLook(look);     // changed() -> the plot, the lists
        finishEdit();
    });

    /* Who it belongs to (board Q137, docs/design/VENUES.md): the tour package or the
       venue's house rig. Not set follows its group. */
    m_owner = new QComboBox(parent);
    m_owner->setObjectName(QStringLiteral("fixtureOwner"));
    m_owner->setToolTip(tr("Tour and Rental fixtures travel with the show. A House fixture is the room's: "
                           "with a venue applied it is kept in that venue's file, and switching venue "
                           "swaps it. \"From its group\" follows the owner set on its fixture group."));
    form->addRow(tr("Owner:"), m_owner);
    connect(m_owner, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_loading || m_fids.isEmpty() || comboIsMixed(m_owner))
            return;
        const QVariant v = m_owner->currentData(Qt::UserRole);
        if (!v.isValid())
            return;
        emit aboutToEdit(QStringLiteral("owner"));
        foreach (quint32 fid, m_fids)
            if (Fixture *fx = m_monitor->m_doc->fixture(fid))
                fx->setOwner(RigOwner::Owner(v.toInt()));
        finishEdit();
    });

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
                          "outside of a truss it also moves to that side of the truss. "
                          "A fixed light (a par, a laser) is aimed that way too: Back "
                          "points it upstage, Bottom straight down."));
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
                          "angled up toward the cast. Positive tips its light up on an "
                          "upright face (Front, Back, Stage left or right), upstage on a "
                          "Top face. Angle above turns it flat within its face instead."));
    form->addRow(tr("Tilt:"), m_tilt);
    buildPixelRows(form, parent);

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
    QToolButton *wledFix = m_wledFix = new QToolButton(m_wledBox);
    wledFix->setObjectName(QStringLiteral("fixtureWledFix"));
    wledFix->setText(tr("Fix on Device…"));
    wledFix->setToolTip(tr("Show what would change in the WLED's own settings, then write it"));
    wledRow->addWidget(m_wledNote, 1);
    wledRow->addWidget(wledFix, 0, Qt::AlignTop);
    form->addRow(tr("WLED:"), m_wledBox);
    // A write to the device (Fix on Device's LED count): busy, then what it did.
    m_wledWriteNote = new QLabel(parent);
    m_wledWriteNote->setObjectName(QStringLiteral("fixtureWledWriteNote"));
    m_wledWriteNote->setWordWrap(true);
    m_wledWriteNote->setStyleSheet(QStringLiteral("color:#b0b4c0;"));
    form->addRow(QString(), m_wledWriteNote);
    connect(wledFix, &QToolButton::clicked, this, [this]() {
        if (WledConfigCheck::instance() != nullptr)
            WledConfigCheck::instance()->confirmFix(m_parent, m_fids);
    });
    if (WledConfigCheck::instance() != nullptr)
    {
        connect(WledConfigCheck::instance(), &WledConfigCheck::changed, this, [this]() {
            if (!m_fids.isEmpty())
                reload();
        });
        connect(WledConfigCheck::instance(), &WledConfigCheck::writeChanged, this, [this](const QString &ip) {
            const Fixture *fx = m_fids.size() == 1 ? m_monitor->m_doc->fixture(m_fids.first()) : nullptr;
            if (fx == nullptr || fx->ipAddress() != ip)
                return;
            bool busy = false;
            const QString text = WledConfigCheck::instance()->writeStatus(ip, &busy);
            m_wledWriteNote->setText(busy ? QStringLiteral("\u27f3 ") + text : text);
            row(m_wledWriteNote, !text.isEmpty());
            m_wledFix->setEnabled(!busy);
        });
    }

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
    m_copyPower = new QToolButton(powerBox);
    m_copyPower->setObjectName(QStringLiteral("fixtureCopyPower"));
    m_copyPower->setText(tr("Copy to Same Fixtures…"));
    m_copyPower->setToolTip(tr("Give its readings to every fixture of the same make, model and mode "
                               "in this show -- each keeps its own maximum"));
    powerRow->addWidget(profileBtn);
    powerRow->addWidget(measureBtn);
    powerRow->addWidget(m_copyPower);
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
    /* "an easy way to copy amperage profile to the same fixtures across a
       showfile": measure one, give its readings to all the others like it. */
    connect(m_copyPower, &QToolButton::clicked, this, [this]() {
        Doc *doc = m_monitor->m_doc;
        Fixture *src = powerSource(doc, m_fids, m_shown);
        if (src == nullptr)
            return;
        const QList<quint32> to = sameFixtures(doc, src);
        QMessageBox box(QMessageBox::Question, tr("Copy Power Readings"),
                        to.isEmpty()
                            ? tr("There are no other %1 (%2) fixtures in this show.")
                                  .arg(src->fixtureDef()->model(), src->fixtureMode()->name())
                            : tr("Give %1's readings to the %n other %2 (%3) fixture(s) in this show? "
                                 "Each keeps its own maximum.", "", to.size())
                                  .arg(src->name(), src->fixtureDef()->model(), src->fixtureMode()->name()),
                        QMessageBox::NoButton, m_parent);
        QPushButton *copy = to.isEmpty() ? nullptr : box.addButton(tr("Copy"), QMessageBox::AcceptRole);
        box.addButton(to.isEmpty() ? QMessageBox::Ok : QMessageBox::Cancel);
        if (copy != nullptr)
            copy->setObjectName(QStringLiteral("copyPowerConfirm"));
        box.exec();
        if (copy == nullptr || box.clickedButton() != copy)
            return;
        emit aboutToEdit(QStringLiteral("powerProfile"));
        copyPowerReadings(doc, src, to);
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
    for (QWidget *w : { static_cast<QWidget *>(m_look), static_cast<QWidget *>(m_owner),
                        static_cast<QWidget *>(m_mountOn), static_cast<QWidget *>(m_towerPos),
                        static_cast<QWidget *>(m_colour), static_cast<QWidget *>(m_orientation),
                        static_cast<QWidget *>(m_face), static_cast<QWidget *>(m_placement),
                        static_cast<QWidget *>(m_angle), static_cast<QWidget *>(m_tilt),
                        m_light->lightRow(), static_cast<QWidget *>(m_light->glassCombo()),
                        m_dmxBox, static_cast<QWidget *>(m_dmxNote), m_wledBox,
                        static_cast<QWidget *>(m_pixelPath), static_cast<QWidget *>(m_pixelTurns),
                        static_cast<QWidget *>(m_wandLength), static_cast<QWidget *>(m_wandStrip),
                        static_cast<QWidget *>(m_wandDiameter), static_cast<QWidget *>(m_wandStart),
                        static_cast<QWidget *>(m_heldBy),
                        static_cast<QWidget *>(m_holdHeight), static_cast<QWidget *>(m_holdTilt),
                        static_cast<QWidget *>(m_sprPoints), static_cast<QWidget *>(m_sprPer),
                        m_sprBox, static_cast<QWidget *>(m_sprNote) })
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
        /* A fixed light throws out of its face: the face aims it, and Points
           and Tilt in yoke read that aim (a yoke tilt already set stays). */
        if (props->fixtureFrameGroup(fid) == 0
            && RigHeads::isFixedUnit(m_monitor->m_doc->fixture(fid)))
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            RigHeads::aimAtFace(rp);
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
    /* A Teaching show asks first, as the Fixture Manager does; declined, the
       boxes go back to where the fixture is. */
    if (!ShowSettingsDialog::confirmPatchChange(doc, m_dmxBox, tr("Move %1 to U %2, address %3")
                                                    .arg(fx->name()).arg(uni + 1).arg(addr + 1)))
    {
        const QSignalBlocker bu(m_dmxUni), ba(m_dmxAddr);
        m_dmxUni->setValue(int(fx->universe()) + 1);
        m_dmxAddr->setValue(int(fx->address()) + 1);
        return;
    }
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
    /* Anything that pans or tilts has a base, and which way up it is matters
       to where it points -- a two-arm beam bar standing on the deck is as
       much upright as a mover is, though it lies like a strip ("they should
       be upright .. but I don't see a way to put that on the fixture"). */
    const bool pansOrTilts = fx != nullptr
        && (fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0) != QLCChannel::invalid()
            || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0) != QLCChannel::invalid());

    // Stage look: what it looks like, Auto naming what its type suggests.
    fillStageLookCombo(m_look, fx);
    m_look->blockSignals(true);
    m_look->setCurrentIndex(qMax(0, m_look->findData(int(fx->stageLook()), Qt::UserRole)));
    comboMixed(m_look, differs([doc, fx](quint32 f) {
        const Fixture *o = doc->fixture(f);
        return o != nullptr && o->stageLook() == fx->stageLook(); }));
    m_look->blockSignals(false);
    row(m_look, true);

    // Owner: its own, or "From its group" saying what that gives.
    {
        const QSignalBlocker block(m_owner);
        m_owner->clear();
        const RigOwner::Owner own = fx->owner();
        QString fromText = tr("From its group");
        if (own == RigOwner::Unset)
        {
            quint32 from = UINT_MAX;
            const RigOwner::Owner eff = RigOwner::effective(doc, m_shown, &from);
            const FixtureGroup *grp = from != UINT_MAX ? doc->fixtureGroup(from) : nullptr;
            fromText = grp != nullptr ? tr("From %1: %2").arg(grp->name(), RigOwner::displayName(eff))
                                      : tr("From its group (%1)").arg(RigOwner::displayName(eff));
        }
        m_owner->addItem(fromText, int(RigOwner::Unset));
        foreach (RigOwner::Owner o, RigOwner::owners())
            m_owner->addItem(RigOwner::displayName(o), int(o));
        m_owner->setCurrentIndex(qMax(0, m_owner->findData(int(own), Qt::UserRole)));
        comboMixed(m_owner, differs([doc, fx](quint32 f) {
            const Fixture *o = doc->fixture(f);
            return o != nullptr && o->owner() == fx->owner(); }));
    }
    row(m_owner, true);

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
    row(m_orientation, !linear || onTower || pansOrTilts);

    // Face, Angle
    /* A FIXED light (a par, a laser: no pan or tilt, not a strip) has a Face
       too -- the side it throws out of (Q172 A). It reads the side its light
       goes nearest (RigHeads::fixedThrow, as every view draws it), blank
       while it points in between; picking one aims it (applyFace()). */
    const bool fixedUnit = !frame && RigHeads::isFixedUnit(fx);
    auto shownFace = [doc, props, fixedUnit](quint32 f) {
        if (!fixedUnit)
            return int(props->fixtureRigProps(f).face());
        QVector3D d;
        return RigHeads::fixedThrow(doc, f, d) ? RigHeads::nearestFace(d) : -1;
    };
    const int face0 = shownFace(m_shown);
    m_face->blockSignals(true);
    comboMixed(m_face, false);
    m_face->setCurrentIndex(face0);
    if (differs([shownFace, face0](quint32 f) { return shownFace(f) == face0; }))
        comboMixed(m_face, true);
    m_face->blockSignals(false);
    row(m_face, (linear || frame || fixedUnit) && !onTower);

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
    /* A fixed light tips in its yoke (Tilt in yoke, in Aim & Mount), not by
       rolling its body; the row stays only to clear a roll it already has. */
    row(m_tilt, !fixedUnit || !qFuzzyIsNull(rp.studioTilt));
    showPixelRows(m_shown);

    // Its place on a tower: the one shown (a place is one fixture's).
    if (Tower *tw = (onTower && !many) ? props->tower(rp.towerId) : nullptr)
    {
        m_towerPos->blockSignals(true);
        m_towerPos->clear();
        auto addPos = [this](const QString &label, int side, int shelf) {
            m_towerPos->addItem(label);
            m_towerPos->setItemData(m_towerPos->count() - 1, side, Qt::UserRole);
            m_towerPos->setItemData(m_towerPos->count() - 1, shelf, Qt::UserRole + 1);
        };
        // Heights in Settings > Units.
        addPos(tr("Top (%1)").arg(MeasurementUnits::formatMetres(double(tw->height()))),
               FixtureRigProps::TowerTop, -1);
        for (int i = tw->shelfCount() - 1; i >= 0; --i)
            addPos(tr("Shelf %1 (%2)").arg(i + 1)
                       .arg(MeasurementUnits::formatMetres(double(tw->shelfHeight(i)))),
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
    {
        bool busy = false;
        const QString text = WledConfigCheck::instance() != nullptr && !many
            ? WledConfigCheck::instance()->writeStatus(fx->ipAddress(), &busy) : QString();
        m_wledWriteNote->setText(busy ? QStringLiteral("\u27f3 ") + text : text);
        row(m_wledWriteNote, !text.isEmpty() && WledConfigCheck::isWled(fx));
        m_wledFix->setEnabled(!busy);
        m_wledFix->setToolTip(busy ? tr("Waiting for the device: %1").arg(text)
                                   : tr("Show what would change in the WLED's own settings, then write it"));
    }

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
        Fixture *src = powerSource(pdoc, m_fids, m_shown);
        const int alike = src != nullptr ? sameFixtures(pdoc, src).size() : 0;
        m_copyPower->setEnabled(src != nullptr && alike > 0);
        if (many)
        {
            note = tr("%1 of %2 measured").arg(measured).arg(m_fids.size());
            if (src != nullptr && alike > 0)
                note += tr(" — Copy to Same Fixtures… gives %1's readings to the %n like it", "", alike)
                            .arg(src->name());
        }
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

void FixtureFields::buildPixelRows(QFormLayout *form, QWidget *parent)
{
    MonitorProperties *props = m_monitor->m_props;
    const bool isFeet = FeetInchesSpinBox::valuesInFeet();   // the fields' numbers only
    const double toDisp = isFeet ? 3.28084 : 1.0;

    m_pixelPath = new QComboBox(parent);
    m_pixelPath->setObjectName(QStringLiteral("fixturePixelPath"));
    for (int k = 0; k < PixelPath::kShapes; ++k)
        m_pixelPath->addItem(PixelPath::name(k), k);
    m_pixelPath->setToolTip(tr("Where its LEDs are: round a fountain's wall or base, wound up "
                               "its flame, or round a wand -- in the order of its heads"));
    form->addRow(tr("Pixel path:"), m_pixelPath);

    m_pixelTurns = new QDoubleSpinBox(parent);
    m_pixelTurns->setObjectName(QStringLiteral("fixturePixelTurns"));
    m_pixelTurns->setRange(0.0, 500.0);
    m_pixelTurns->setDecimals(1);
    m_pixelTurns->setSpecialValueText(tr("Auto"));
    m_pixelTurns->setToolTip(tr("Turns of the strip round the flame or the staff. Auto: a wand's "
                                "from its string length and staff diameter (no string length: "
                                "LED tape, %1 apart), a flame's about one every %2")
                                .arg(MeasurementUnits::formatLength(20.0), MeasurementUnits::formatLength(300.0)));
    form->addRow(tr("Turns:"), m_pixelTurns);

    m_wandLength = new FeetInchesSpinBox(isFeet, parent);
    m_wandLength->setObjectName(QStringLiteral("fixtureWandLength"));
    m_wandLength->setRange(0.1 * toDisp, 5.0 * toDisp);
    form->addRow(tr("Wand length:"), m_wandLength);

    m_wandStrip = new FeetInchesSpinBox(isFeet, parent);
    m_wandStrip->setObjectName(QStringLiteral("fixtureWandStrip"));
    m_wandStrip->setRange(0.0, 50.0 * toDisp);
    m_wandStrip->setSpecialValueText(tr("LED tape"));
    m_wandStrip->setToolTip(tr("How long the lit string is, first LED to last (a fairy string's "
                               "length). With the LED count it sets how far apart they are, and "
                               "Auto turns follow. Empty: LED tape, %1 apart")
                               .arg(MeasurementUnits::formatLength(20.0)));
    form->addRow(tr("String length:"), m_wandStrip);

    m_wandDiameter = new FeetInchesSpinBox(isFeet, parent);
    m_wandDiameter->setObjectName(QStringLiteral("fixtureWandDiameter"));
    m_wandDiameter->setDecimals(5);   // a fraction of an inch, kept exact
    m_wandDiameter->setRange(0.005 * toDisp, 0.2 * toDisp);
    m_wandDiameter->setToolTip(tr("How thick the staff is where the string is wound round it"));
    form->addRow(tr("Staff diameter:"), m_wandDiameter);

    m_wandStart = new QComboBox(parent);
    m_wandStart->setObjectName(QStringLiteral("fixtureWandStart"));
    m_wandStart->addItem(tr("Bottom (the grip)"), false);
    m_wandStart->addItem(tr("Top (the tip)"), true);
    m_wandStart->setToolTip(tr("Where the first LED is: the end the data comes in. Effects run "
                               "from there along the string"));
    form->addRow(tr("First LED at:"), m_wandStart);

    m_heldBy = new QComboBox(parent);
    m_heldBy->setObjectName(QStringLiteral("fixtureHeldBy"));
    m_heldBy->setToolTip(tr("A performer holding it: it is in their hand and moves with them"));
    form->addRow(tr("Held by:"), m_heldBy);

    m_holdHeight = new FeetInchesSpinBox(isFeet, parent);
    m_holdHeight->setObjectName(QStringLiteral("fixtureHoldHeight"));
    m_holdHeight->setRange(0.0, 3.0 * toDisp);
    m_holdHeight->setToolTip(tr("How high its middle is held, above where they stand"));
    form->addRow(tr("Held at:"), m_holdHeight);

    m_holdTilt = new QDoubleSpinBox(parent);
    m_holdTilt->setObjectName(QStringLiteral("fixtureHoldTilt"));
    m_holdTilt->setRange(-90.0, 90.0);
    m_holdTilt->setDecimals(0);
    m_holdTilt->setSuffix(QStringLiteral("°"));
    m_holdTilt->setToolTip(tr("How far it leans forward, toward the audience, from upright"));
    form->addRow(tr("Leaning:"), m_holdTilt);

    m_sprPoints = new QSpinBox(parent);
    m_sprPoints->setObjectName(QStringLiteral("fixtureSprinklePoints"));
    m_sprPoints->setRange(1, 64);
    m_sprPoints->setToolTip(tr("How many places round the U the string loops across the gap, "
                               "evenly spaced (when no span table says where)"));
    form->addRow(tr("Points:"), m_sprPoints);

    m_sprPer = new QSpinBox(parent);
    m_sprPer->setObjectName(QStringLiteral("fixtureSprinkleSpans"));
    m_sprPer->setRange(1, 32);
    m_sprPer->setToolTip(tr("How many times the string crosses the gap at each point, "
                            "inner wall to outer and back"));
    form->addRow(tr("Spans per point:"), m_sprPer);

    m_sprBox = new QWidget(parent);
    QHBoxLayout *sprRow = new QHBoxLayout(m_sprBox);
    sprRow->setContentsMargins(0, 0, 0, 0);
    m_sprEdit = new QPushButton(tr("Edit Spans…"), m_sprBox);
    m_sprEdit->setObjectName(QStringLiteral("fixtureSprinkleEdit"));
    m_sprEdit->setToolTip(tr("Which LEDs make each span and where it hangs"));
    m_sprRead = new QPushButton(tr("Read Segments"), m_sprBox);
    m_sprRead->setObjectName(QStringLiteral("fixtureSprinkleRead"));
    m_sprRead->setToolTip(tr("Make one span of each of the WLED's segments, read from the device"));
    m_sprWrite = new QPushButton(tr("Write Segments…"), m_sprBox);
    m_sprWrite->setObjectName(QStringLiteral("fixtureSprinkleWrite"));
    sprRow->addWidget(m_sprEdit);
    sprRow->addWidget(m_sprRead);
    sprRow->addWidget(m_sprWrite);
    sprRow->addStretch();
    form->addRow(tr("Spans:"), m_sprBox);
    connect(m_sprEdit, &QPushButton::clicked, this, &FixtureFields::editSprinkleSpans);
    connect(m_sprRead, &QPushButton::clicked, this, &FixtureFields::readSprinkleSegments);
    connect(m_sprWrite, &QPushButton::clicked, this, &FixtureFields::writeSprinkleSegments);

    m_sprNote = new QLabel(parent);
    m_sprNote->setObjectName(QStringLiteral("fixtureSprinkleNote"));
    m_sprNote->setWordWrap(true);
    m_sprNote->setStyleSheet(QStringLiteral("color:#b0b4c0;"));
    form->addRow(QString(), m_sprNote);

    MonitorGraphicsView *gv = m_monitor->m_graphicsView;
    auto each = [this, props, gv](const QString &field,
                                  const std::function<void(FixtureRigProps &)> &set) {
        if (m_loading || m_fids.isEmpty())
            return;
        emit aboutToEdit(field);
        foreach (quint32 fid, m_fids)
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            set(rp);
            props->setFixtureRigProps(fid, rp);
            if (gv != nullptr)
                gv->updateFixture(fid);
        }
        finishEdit();
    };
    connect(m_pixelPath, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, each](int) {
        const int shape = m_pixelPath->currentData().toInt();
        each(QStringLiteral("pixelPath"), [shape](FixtureRigProps &rp) { rp.pixelPath = shape; });
    });
    connect(m_pixelTurns, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [each](double v) {
        each(QStringLiteral("pixelTurns"), [v](FixtureRigProps &rp) { rp.pixelTurns = float(v); });
    });
    connect(m_wandLength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [each, toDisp](double v) {
        each(QStringLiteral("wandLength"), [v, toDisp](FixtureRigProps &rp) { rp.wandLength = float(v / toDisp); });
    });
    connect(m_wandStrip, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [each, toDisp](double v) {
        each(QStringLiteral("wandStripLength"), [v, toDisp](FixtureRigProps &rp) { rp.wandStripLength = float(v / toDisp); });
    });
    connect(m_wandDiameter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [each, toDisp](double v) {
        each(QStringLiteral("wandDiameter"), [v, toDisp](FixtureRigProps &rp) { rp.wandDiameter = float(v / toDisp); });
    });
    connect(m_wandStart, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, each](int) {
        const bool top = m_wandStart->currentData().toBool();
        each(QStringLiteral("wandFromTop"), [top](FixtureRigProps &rp) { rp.wandFromTop = top; });
    });
    connect(m_heldBy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, each](int) {
        const quint32 who = m_heldBy->currentData().toUInt();
        each(QStringLiteral("heldBy"), [who](FixtureRigProps &rp) { rp.heldBy = who; });
    });
    connect(m_holdHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [each, toDisp](double v) {
        each(QStringLiteral("holdHeight"), [v, toDisp](FixtureRigProps &rp) { rp.holdHeight = float(v / toDisp); });
    });
    connect(m_holdTilt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [each](double v) {
        each(QStringLiteral("holdTilt"), [v](FixtureRigProps &rp) { rp.holdTilt = float(v); });
    });
    connect(m_sprPoints, QOverload<int>::of(&QSpinBox::valueChanged), this, [each](int v) {
        each(QStringLiteral("sprinklePoints"), [v](FixtureRigProps &rp) { rp.sprinklePoints = v; });
    });
    connect(m_sprPer, QOverload<int>::of(&QSpinBox::valueChanged), this, [each](int v) {
        each(QStringLiteral("sprinkleSpansPerPoint"), [v](FixtureRigProps &rp) { rp.sprinkleSpansPerPoint = v; });
    });
}

void FixtureFields::showPixelRows(quint32 fid)
{
    Doc *doc = m_monitor->m_doc;
    MonitorProperties *props = m_monitor->m_props;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    // A string of LEDs (heads, or a WLED device's LEDs), or one already laid out.
    const bool pixels = PixelPathDecor::ledCount(doc, fid) > 1 || rp.pixelPath != PixelPath::None;
    if (!pixels)
        return;
    const double toDisp = FeetInchesSpinBox::valuesInFeet() ? 3.28084 : 1.0;   // the fields' numbers
    // The fountain paths only in a fountain.
    const bool inFountain = Fountain::ofFixture(props, fid).isValid()
                            || Fountain::ofBase(props, rp.deckPlatformId).isValid();
    if (auto *model = qobject_cast<QStandardItemModel *>(m_pixelPath->model()))
        for (int k = 0; k < model->rowCount(); ++k)
            if (QStandardItem *it = model->item(k))
                it->setEnabled(!PixelPath::needsFountain(k) || inFountain || rp.pixelPath == k);
    const QSignalBlocker b1(m_pixelPath), b2(m_pixelTurns), b3(m_wandLength), b4(m_heldBy),
                         b5(m_holdHeight), b6(m_holdTilt), b7(m_wandStrip), b8(m_wandDiameter),
                         b9(m_wandStart);
    m_pixelPath->setCurrentIndex(qMax(0, m_pixelPath->findData(rp.pixelPath)));
    row(m_pixelPath, true);
    const bool wound = rp.pixelPath == PixelPath::Wand || rp.pixelPath == PixelPath::FlameSpiral;
    m_pixelTurns->setValue(double(rp.pixelTurns));
    row(m_pixelTurns, wound);
    const bool wand = rp.pixelPath == PixelPath::Wand;
    m_wandLength->setValue(double(rp.wandLength) * toDisp);
    row(m_wandLength, wand);
    m_wandStrip->setValue(double(rp.wandStripLength) * toDisp);
    row(m_wandStrip, wand);
    m_wandDiameter->setValue(double(rp.wandDiameter) * toDisp);
    row(m_wandDiameter, wand);
    m_wandStart->setCurrentIndex(rp.wandFromTop ? 1 : 0);
    row(m_wandStart, wand);
    if (wand && rp.pixelTurns <= 0.0f)
    {
        // Auto: say what it worked out, so the number can be checked on the real staff.
        const double turns = PixelPath::wandTurns(rp, PixelPathDecor::ledCount(doc, fid));
        m_pixelTurns->setSpecialValueText(tr("Auto (%1)").arg(turns, 0, 'f', 1));
    }
    else
        m_pixelTurns->setSpecialValueText(tr("Auto"));
    m_heldBy->clear();
    m_heldBy->addItem(tr("Nobody (stands where placed)"), uint(UINT_MAX));
    foreach (StageTarget *t, props->stageTargets())
        if (t != nullptr && t->kind() == StageTarget::Kind::Person)
            m_heldBy->addItem(t->name(), uint(t->id()));
    m_heldBy->setCurrentIndex(qMax(0, m_heldBy->findData(uint(rp.heldBy))));
    row(m_heldBy, wand);
    m_holdHeight->setValue(double(rp.holdHeight) * toDisp);
    row(m_holdHeight, wand && rp.isHeld());
    m_holdTilt->setValue(double(rp.holdTilt));
    row(m_holdTilt, wand && rp.isHeld());

    const bool sprinkles = rp.pixelPath == PixelPath::SprinkleLoops;
    const QSignalBlocker bSprPoints(m_sprPoints), bSprPer(m_sprPer);
    m_sprPoints->setValue(rp.sprinklePoints);
    m_sprPer->setValue(rp.sprinkleSpansPerPoint);
    // With a table, it says where each span is; the counts only group a read.
    m_sprPoints->setEnabled(rp.sprinkleSpans.isEmpty());
    row(m_sprPoints, sprinkles);
    row(m_sprPer, sprinkles);
    const bool one = m_fids.size() == 1;
    m_sprEdit->setEnabled(one);
    const bool wled = WledConfigCheck::isWled(doc->fixture(fid));
    const bool writing = m_wledWrite != nullptr && m_wledWrite->isBusy();
    m_sprRead->setEnabled(one && wled && !writing);
    m_sprWrite->setEnabled(one && wled && !writing);
    m_sprWrite->setToolTip(!one ? tr("Select one fixture: each WLED has its own spans")
                         : !wled ? tr("Only a WLED on the network with an IP address keeps segments")
                         : writing ? tr("Writing to the device…")
                         : tr("Make the WLED's segments these spans: one a span, every second one "
                              "reversed, the LEDs between them dark. Asks first"));
    row(m_sprBox, sprinkles);
    if (sprinkles && !m_sprNote->property("sprinkleReading").toBool())
    {
        const int leds = PixelPathDecor::ledCount(doc, fid);
        const QVector<SprinkleSpan> spans = PixelPath::sprinkleLayout(rp, leds);
        int inSpans = 0;
        foreach (const SprinkleSpan &sp, spans)
            inSpans += sp.lastLed - sp.firstLed + 1;
        m_sprNote->setText(rp.sprinkleSpans.isEmpty()
            ? tr("Evenly spread: %n span(s), %1 LEDs each", "", spans.size())
                  .arg(spans.isEmpty() ? 0 : leds / spans.size())
            : tr("From its table: %n span(s), %1 LEDs between them", "", spans.size())
                  .arg(qMax(0, leds - inSpans)));
    }
    row(m_sprNote, sprinkles);
}

void FixtureFields::editSprinkleSpans()
{
    if (m_fids.size() != 1)
        return;
    const quint32 fid = m_fids.first();
    MonitorProperties *props = m_monitor->m_props;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const int leds = PixelPathDecor::ledCount(m_monitor->m_doc, fid);
    // Start from what it is laid out as now, so the even spread can be adjusted.
    SprinkleSpansDialog dlg(rp.sprinkleSpans.isEmpty() ? PixelPath::sprinkleLayout(rp, leds)
                                                       : rp.sprinkleSpans, leds, m_parent);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QVector<SprinkleSpan> spans = dlg.spans();
    if (spans == rp.sprinkleSpans)
        return;
    emit aboutToEdit(QStringLiteral("sprinkleSpans"));
    FixtureRigProps now = props->fixtureRigProps(fid);
    now.sprinkleSpans = spans;
    props->setFixtureRigProps(fid, now);
    if (m_monitor->m_graphicsView != nullptr)
        m_monitor->m_graphicsView->updateFixture(fid);
    finishEdit();
}

void FixtureFields::readSprinkleSegments()
{
    if (m_fids.size() != 1)
        return;
    const quint32 fid = m_fids.first();
    const Fixture *fx = m_monitor->m_doc->fixture(fid);
    if (!WledConfigCheck::isWled(fx))
    {
        takeWledSegments(fid, QByteArray(), false, tr("It is not a WLED on the network with an IP address."));
        return;
    }
    if (m_nam == nullptr)
    {
        m_nam = new QNetworkAccessManager(this);
        m_nam->setProxy(QNetworkProxy::NoProxy);    // WLEDs are on the show LAN
    }
    const QString ip = fx->ipAddress();
    QNetworkRequest req(QUrl(WledConfigCheck::baseUrl(ip) + QStringLiteral("/json/state")));
    req.setTransferTimeout(4000);
    QNetworkReply *reply = m_nam->get(req);
    m_sprRead->setEnabled(false);
    m_sprNote->setProperty("sprinkleReading", true);
    m_sprNote->setText(tr("Reading the segments from %1…").arg(ip));
    connect(reply, &QNetworkReply::finished, this, [this, reply, fid, ip]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        QString why;
        if (!ok)
            why = reply->error() == QNetworkReply::OperationCanceledError
                      || reply->error() == QNetworkReply::TimeoutError
                ? tr("%1 did not answer in time.").arg(ip)
                : tr("%1 did not answer: %2").arg(ip, reply->errorString());
        takeWledSegments(fid, ok ? reply->readAll() : QByteArray(), ok, why);
    });
}

void FixtureFields::takeWledSegments(quint32 fid, const QByteArray &body, bool ok, const QString &why)
{
    m_sprNote->setProperty("sprinkleReading", false);
    MonitorProperties *props = m_monitor->m_props;
    QString error = why;
    QVector<SprinkleSpan> spans;
    int points = 0;
    QList<int> notReversed;
    if (ok && props->containsFixture(fid))
    {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        if (PixelPath::sprinkleSpansFromWled(body, rp.sprinkleSpansPerPoint, spans, points,
                                             &error, &notReversed))
        {
            emit aboutToEdit(QStringLiteral("sprinkleSpans"));
            rp.sprinkleSpans = spans;
            rp.sprinklePoints = qMax(1, points);
            props->setFixtureRigProps(fid, rp);
            if (m_monitor->m_graphicsView != nullptr)
                m_monitor->m_graphicsView->updateFixture(fid);
            finishEdit();
            QString note = tr("Read %n segment(s) as spans at %1 point(s).", "", spans.size()).arg(points);
            if (!notReversed.isEmpty())
            {
                QStringList segs;
                foreach (int k, notReversed)
                    segs << QString::number(k);
                note += QStringLiteral(" ")
                      + tr("Reverse is not set as every second segment needs it on segment(s) %1.")
                            .arg(segs.join(QStringLiteral(", ")));
            }
            m_sprNote->setText(note);
            return;
        }
    }
    else if (ok)
        error = tr("The fixture is gone.");
    if (m_fids.size() == 1 && m_fids.first() == fid)
        m_sprRead->setEnabled(true);
    m_sprNote->setText(tr("Couldn't read the segments: %1").arg(error));
    m_sprNote->setVisible(true);
}

void FixtureFields::writeSprinkleSegments()
{
    if (m_fids.size() != 1 || (m_wledWrite != nullptr && m_wledWrite->isBusy()))
        return;
    const quint32 fid = m_fids.first();
    Doc *doc = m_monitor->m_doc;
    const Fixture *fx = doc->fixture(fid);
    m_sprNote->setVisible(true);
    if (!WledConfigCheck::isWled(fx))
    {
        m_sprNote->setText(tr("Nothing was written: it is not a WLED on the network with an IP address."));
        return;
    }
    const QString ip = fx->ipAddress();
    const QString held = WledDeviceWrite::heldReason(doc, ip);
    if (!held.isEmpty())
    {
        m_sprNote->setText(tr("Nothing was written to %1: %2").arg(WledDeviceWrite::deviceName(ip), held));
        return;
    }
    const QVector<SprinkleSpan> spans =
        PixelPath::sprinkleLayout(m_monitor->m_props->fixtureRigProps(fid), PixelPathDecor::ledCount(doc, fid));
    if (m_wledWrite == nullptr)
    {
        m_wledWrite = new WledDeviceWrite(this);
        connect(m_wledWrite, &WledDeviceWrite::progress, this, [this](const QString &text) {
            if (text.isEmpty())
                return;
            m_sprNote->setProperty("sprinkleReading", true);
            m_sprNote->setText(QStringLiteral("⟳ ") + text);
        });
        connect(m_wledWrite, &WledDeviceWrite::finished, this, [this](bool, const QString &message) {
            m_sprNote->setProperty("sprinkleReading", false);
            const bool one = m_fids.size() == 1;
            const bool wled = one && WledConfigCheck::isWled(m_monitor->m_doc->fixture(m_fids.first()));
            m_sprRead->setEnabled(wled);
            m_sprWrite->setEnabled(wled);
            m_sprNote->setText(message);
            m_sprNote->setVisible(true);
        });
    }
    m_sprRead->setEnabled(false);
    m_sprWrite->setEnabled(false);
    const QString name = fx->name();
    m_wledWrite->writeSegments(ip, spans, [this, name, ip](int segments, int existing) {
        return m_confirmWrite ? m_confirmWrite(name, ip, segments, existing)
                              : WledDeviceWrite::confirmSegments(m_parent, name, ip, segments, existing);
    });
}
