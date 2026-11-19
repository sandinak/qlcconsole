/*
  Q Light Controller Plus
  venuerigdialog.cpp

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

#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLabel>
#include <algorithm>

#include "programmercontroller.h"
#include "venuecorrections.h"
#include "inputoutputmap.h"
#include "venuerigdialog.h"
#include "venuefromshowdialog.h"
#include "roomoutputpicker.h"
#include "fixturegroup.h"
#include "venuerig.h"
#include "rigowner.h"
#include "fixture.h"
#include "doc.h"

namespace
{
VenueCorrections *venuesOf(Doc *doc)
{
    return doc->programmer()->venueCorrections();
}

/* "all", "401–512, 1–24" for universe @p u. */
QString notOursCell(const QList<VenueRig::NotOurs> &list, quint32 u)
{
    QStringList out;
    for (const VenueRig::NotOurs &n : list)
    {
        if (n.universe != u)
            continue;
        if (n.whole())
            return QObject::tr("all");
        out << (n.from == n.to ? QString::number(n.from + 1)
                               : QStringLiteral("%1-%2").arg(n.from + 1).arg(n.to + 1));
    }
    return out.join(QStringLiteral(", "));
}
}

VenueRigDialog::VenueRigDialog(Doc *doc, const QString &venue, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_venue(venue)
{
    setObjectName(QStringLiteral("VenueRigDialog"));
    setWindowTitle(tr("%1: rig and universes").arg(venue));
    setProperty("helpTopic", QStringLiteral("venues.md"));
    QVBoxLayout *vl = new QVBoxLayout(this);

    m_state = new QLabel(this);
    m_state->setObjectName(QStringLiteral("vrState"));
    m_state->setWordWrap(true);
    vl->addWidget(m_state);

    // The house rig: the room's own fixtures and the groups they fill.
    QGroupBox *house = new QGroupBox(tr("House rig"), this);
    QVBoxLayout *hl = new QVBoxLayout(house);
    m_house = new QLabel(house);
    m_house->setObjectName(QStringLiteral("vrHouse"));
    m_house->setWordWrap(true);
    m_house->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hl->addWidget(m_house);
    m_moveIn = new QPushButton(house);
    m_moveIn->setObjectName(QStringLiteral("vrMoveIn"));
    m_moveIn->setToolTip(tr("Fixtures set to House while no venue was applied are still saved in the show. "
                            "Move them into this venue's house rig."));
    hl->addWidget(m_moveIn, 0, Qt::AlignLeft);
    connect(m_moveIn, &QPushButton::clicked, this, [this]() {
        venuesOf(m_doc)->moveShowHouseIn();
        refresh();
    });
    vl->addWidget(house);

    /* A venue made from another show (docs/design/VENUE_FROM_SHOW.md): its
       own universe, where that goes in this room, and its groups still
       waiting for a match. */
    m_room = new QGroupBox(tr("This venue's universe"), this);
    m_room->setObjectName(QStringLiteral("vrRoom"));
    QVBoxLayout *rl = new QVBoxLayout(m_room);
    m_roomText = new QLabel(m_room);
    m_roomText->setObjectName(QStringLiteral("vrRoomText"));
    m_roomText->setWordWrap(true);
    rl->addWidget(m_roomText);
    QHBoxLayout *ol = new QHBoxLayout;
    m_roomOutput = new RoomOutputPicker(doc, m_room);
    ol->addWidget(m_roomOutput, 1);
    m_useRoomOutput = new QPushButton(tr("Use This Output"), m_room);
    m_useRoomOutput->setObjectName(QStringLiteral("vrUseRoomOutput"));
    m_useRoomOutput->setToolTip(tr("This room's output for the venue's universe; on at once while the venue "
                                   "is applied"));
    ol->addWidget(m_useRoomOutput, 0, Qt::AlignTop);
    rl->addLayout(ol);
    QHBoxLayout *wl = new QHBoxLayout;
    m_waiting = new QLabel(m_room);
    m_waiting->setObjectName(QStringLiteral("vrWaiting"));
    m_waiting->setWordWrap(true);
    wl->addWidget(m_waiting, 1);
    m_matchGroups = new QPushButton(tr("Match Groups\342\200\246"), m_room);
    m_matchGroups->setObjectName(QStringLiteral("vrMatchGroups"));
    m_matchGroups->setToolTip(tr("Choose which of this show's groups the room's groups fill"));
    wl->addWidget(m_matchGroups, 0, Qt::AlignTop);
    rl->addLayout(wl);
    m_room->hide();     // only for a venue made from another show
    vl->addWidget(m_room);
    connect(m_roomOutput, &RoomOutputPicker::changed, this, [this]() {
        m_useRoomOutput->setEnabled(m_roomOutput->isComplete());
    });
    connect(m_useRoomOutput, &QPushButton::clicked, this, [this]() {
        QString why;
        if (!venuesOf(m_doc)->setRoomOutput(m_venue, m_roomOutput->output(), &why))
            QMessageBox::information(this, windowTitle(), why);
        refresh();
    });
    connect(m_matchGroups, &QPushButton::clicked, this, [this]() { openMatchGroups(); });

    // The slots: the show's universes, with this room's patch and what it keeps.
    QGroupBox *unis = new QGroupBox(tr("Universes"), this);
    QVBoxLayout *ul = new QVBoxLayout(unis);
    m_table = new QTableWidget(unis);
    m_table->setObjectName(QStringLiteral("vrUniverses"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({ tr("Universe"), tr("Slot"), tr("Output in this venue"), tr("Not ours") });
    m_table->horizontalHeaderItem(NotOursCol)->setToolTip(
        tr("Channels the room keeps for itself (the house desk's house lights): \"all\", or ranges "
           "such as \"401-512\". Nothing of ours is sent there."));
    m_table->horizontalHeaderItem(SlotCol)->setToolTip(
        tr("Which kind of fixture the show means to put in this universe: Tour or House"));
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(PatchCol, QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    ul->addWidget(m_table);
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
        if (!m_loading && it->column() == NotOursCol)
            commitNotOurs(it->row());
    });
    QHBoxLayout *pl = new QHBoxLayout;
    m_keepPatch = new QPushButton(tr("Keep This Patch for %1").arg(venue), unis);
    m_keepPatch->setObjectName(QStringLiteral("vrKeepPatch"));
    m_keepPatch->setToolTip(tr("The outputs on the Connections tab now become this venue's, for every "
                               "universe. The show keeps its own."));
    m_forgetPatch = new QPushButton(tr("Use the Show's Patch"), unis);
    m_forgetPatch->setObjectName(QStringLiteral("vrForgetPatch"));
    m_forgetPatch->setToolTip(tr("Forget this venue's outputs: the show's own go back on"));
    pl->addWidget(m_keepPatch);
    pl->addWidget(m_forgetPatch);
    pl->addStretch(1);
    ul->addLayout(pl);
    connect(m_keepPatch, &QPushButton::clicked, this, [this]() {
        QString why;
        if (!venuesOf(m_doc)->keepPatch(&why))
            QMessageBox::information(this, windowTitle(), why);
        refresh();
    });
    connect(m_forgetPatch, &QPushButton::clicked, this, [this]() {
        QString why;
        if (!venuesOf(m_doc)->forgetPatch(&why))
            QMessageBox::information(this, windowTitle(), why);
        refresh();
    });
    vl->addWidget(unis, 1);

    // The clash check: the house rig against the show's tour package.
    QGroupBox *clash = new QGroupBox(tr("Clash check"), this);
    QVBoxLayout *cl = new QVBoxLayout(clash);
    m_noClashes = new QLabel(tr("No clashes: the house rig and the tour package share no addresses, "
                                "and nothing is on channels this venue keeps."), clash);
    m_noClashes->setObjectName(QStringLiteral("vrNoClashes"));
    m_noClashes->setWordWrap(true);
    cl->addWidget(m_noClashes);
    m_clashes = new QListWidget(clash);
    m_clashes->setObjectName(QStringLiteral("vrClashes"));
    cl->addWidget(m_clashes);
    vl->addWidget(clash);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    VenueCorrections *vc = venuesOf(m_doc);
    connect(vc, &VenueCorrections::houseRigChanged, this, &VenueRigDialog::refresh);
    connect(vc, &VenueCorrections::activeVenueChanged, this, &VenueRigDialog::refresh);
    // A room output waiting for its network shows when it gets there.
    connect(m_doc->inputOutputMap(), &InputOutputMap::offlinePatchesChanged, this, &VenueRigDialog::refresh);
    connect(vc, &VenueCorrections::venuesChanged, this, [this]() {
        if (!venuesOf(m_doc)->hasVenue(m_venue))
            close();        // renamed or deleted under it
    });
    resize(760, 560);
    refresh();
}

VenueRigDialog *VenueRigDialog::open(Doc *doc, QWidget *parent, const QString &venue)
{
    if (venue.isEmpty() || !venuesOf(doc)->hasVenue(venue))
    {
        QMessageBox::information(parent, tr("Rig and universes"),
                                 tr("Choose a venue first (or make one with Apply Now): a house rig, "
                                    "a patch and what is not ours belong to a venue."));
        return nullptr;
    }
    VenueRigDialog *d = new VenueRigDialog(doc, venue, parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->show();
    return d;
}

bool VenueRigDialog::confirmClashes(Doc *doc, QWidget *parent, const QString &venue)
{
    VenueCorrections *vc = venuesOf(doc);
    if (venue.isEmpty() || !vc->hasVenue(venue) || venue == vc->activeVenue())
        return true;
    const QList<VenueRig::Clash> clashes = vc->clashes(venue);
    if (clashes.isEmpty())
        return true;
    QStringList lines;
    for (const VenueRig::Clash &c : clashes)
        lines << c.text;
    QMessageBox box(QMessageBox::Warning, tr("Apply %1").arg(venue),
                    tr("%1 clashes with the show. A house fixture on a tour fixture's addresses goes in "
                       "unpatched, so it never drives the tour's channels.").arg(venue),
                    QMessageBox::Cancel, parent);
    box.setObjectName(QStringLiteral("vrClashConfirm"));
    box.setDetailedText(lines.join(QLatin1Char('\n')));
    box.setInformativeText(lines.mid(0, 6).join(QLatin1Char('\n'))
                           + (lines.size() > 6 ? tr("\n... and %n more", "", int(lines.size()) - 6) : QString()));
    QPushButton *anyway = box.addButton(tr("Apply anyway"), QMessageBox::AcceptRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == anyway;
}

void VenueRigDialog::refresh()
{
    VenueCorrections *vc = venuesOf(m_doc);
    if (!vc->hasVenue(m_venue))
        return;
    m_loading = true;
    const bool applied = vc->activeVenue() == m_venue;
    const VenueCorrections::Venue v = vc->venue(m_venue);
    m_state->setText(applied
        ? tr("<b>%1</b> is applied. Set a fixture or a fixture group to House (Fixture Manager, Fixture "
             "Properties) and it goes into this venue's file, not the show's; change an output on the "
             "Connections tab and it is this venue's patch.").arg(m_venue.toHtmlEscaped())
        : tr("<b>%1</b> is not applied. Apply it (Apply Now) to change its house rig or its patch; "
             "what is not ours and the clash check work either way.").arg(m_venue.toHtmlEscaped()));

    // The house rig, as the venue keeps it (written as it changes).
    QStringList names;
    for (const VenueRig::HouseFixture &hf : v.rig.fixtures)
        names << hf.name;
    QStringList fills;
    for (const VenueRig::Fill &f : v.rig.fills)
    {
        const FixtureGroup *g = m_doc->fixtureGroup(f.group);
        fills << tr("%1 (%n heads)", "", int(f.heads.size())).arg(g != nullptr ? g->name() : f.groupName);
    }
    QString houseText;
    if (names.isEmpty())
        houseText = tr("No house fixtures: this venue uses the show's rig as it is.");
    else
    {
        houseText = tr("%n house fixture(s): ", "", int(names.size()))
                    + names.mid(0, 8).join(QStringLiteral(", "))
                    + (names.size() > 8 ? tr(" and %n more", "", int(names.size()) - 8) : QString());
        if (!fills.isEmpty())
            houseText += QStringLiteral("\n") + tr("Fills: %1").arg(fills.join(QStringLiteral(", ")));
    }
    m_house->setText(houseText);
    const int showHouse = applied ? int(vc->rig()->showHouseFixtures().size()) : 0;
    m_moveIn->setText(tr("Move the Show's House Fixtures In (%1)").arg(showHouse));
    m_moveIn->setVisible(applied && showHouse > 0);

    // The slots.
    InputOutputMap *iom = m_doc->inputOutputMap();
    const QList<Universe *> unis = iom->universes();
    const QMap<quint32, QList<Universe::OfflinePatch>> show = vc->rig()->showPatch();
    m_table->setRowCount(int(unis.size()));
    for (int i = 0; i < unis.size(); ++i)
    {
        QTableWidgetItem *name = new QTableWidgetItem(
            QStringLiteral("%1  %2").arg(i + 1).arg(iom->getUniverseNameByIndex(i)));
        name->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, UniverseCol, name);

        QComboBox *slot = qobject_cast<QComboBox *>(m_table->cellWidget(i, SlotCol));
        if (slot == nullptr)
        {
            slot = new QComboBox(m_table);
            slot->setObjectName(QStringLiteral("vrSlot%1").arg(i));
            slot->addItem(RigOwner::displayName(RigOwner::Tour), int(RigOwner::Tour));
            slot->addItem(RigOwner::displayName(RigOwner::House), int(RigOwner::House));
            connect(slot, &QComboBox::currentIndexChanged, this, [this, i, slot]() {
                if (m_loading)
                    return;
                m_doc->inputOutputMap()->setUniverseSlotOwner(i, RigOwner::Owner(slot->currentData().toInt()));
                m_doc->setModified();       // the slots are the show's
                refresh();
            });
            m_table->setCellWidget(i, SlotCol, slot);
        }
        slot->setCurrentIndex(slot->findData(int(unis.at(i)->slotOwner())));

        const QString patch = v.rig.patch.contains(quint32(i))
                              ? VenueRig::outputText(v.rig.patch.value(quint32(i)))
                              : tr("the show's: %1").arg(VenueRig::outputText(show.value(quint32(i))));
        QTableWidgetItem *out = new QTableWidgetItem(patch);
        out->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, PatchCol, out);

        QTableWidgetItem *keep = new QTableWidgetItem(notOursCell(v.rig.notOurs, quint32(i)));
        keep->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
        m_table->setItem(i, NotOursCol, keep);
    }
    m_table->resizeColumnToContents(UniverseCol);
    m_keepPatch->setEnabled(applied);
    m_forgetPatch->setEnabled(applied && vc->hasPatch(m_venue));

    // The clash check.
    m_clashes->clear();
    for (const VenueRig::Clash &c : vc->clashes(m_venue))
    {
        QListWidgetItem *it = new QListWidgetItem(QStringLiteral("%1 — %2")
                                                  .arg(VenueRig::kindName(c.kind), c.text), m_clashes);
        it->setData(Qt::UserRole, int(c.kind));
    }
    m_noClashes->setVisible(m_clashes->count() == 0);
    m_clashes->setVisible(m_clashes->count() > 0);

    // The room's own universe.
    const VenueRig::Room &room = v.rig.room;
    m_room->setVisible(room.isSet());
    if (room.isSet())
    {
        const QList<Universe::OfflinePatch> out = v.rig.patch.value(room.universe);
        QString here;
        if (!v.rig.patch.contains(room.universe))
            here = tr("the show's own (%1)").arg(VenueRig::outputText(show.value(room.universe)));
        else
        {
            here = VenueRig::outputText(out);
            // What the universe does now, not only what was chosen.
            if (applied && !out.isEmpty() && !vc->rig()->roomOutputLive(v.rig))
                here += tr(" (waiting for it: see the clash check)");
        }
        m_roomText->setText(tr("U%1 %2 is %3's own universe%4. Its output here: %5.")
                                .arg(room.universe + 1)
                                .arg(iom->getUniverseNameByIndex(int(room.universe)), m_venue,
                                     room.from.isEmpty() ? QString() : tr(" (its rig from %1)").arg(room.from),
                                     here));
        m_roomOutput->setImported(room.imported, room.from);
        m_roomOutput->refreshInterfaces();
        m_roomOutput->setOutput(out);
        m_useRoomOutput->setEnabled(m_roomOutput->isComplete());
        QStringList waiting;
        for (const VenueRig::Fill &g : room.groups)
            waiting << tr("%1 (%2)").arg(g.groupName).arg(g.heads.size());
        m_waiting->setText(waiting.isEmpty() ? tr("Every group of the room fills one of the show's.")
                                             : tr("Waiting for a match: %1").arg(waiting.join(QStringLiteral(", "))));
        m_matchGroups->setVisible(!waiting.isEmpty());
    }
    m_loading = false;
}

QDialog *VenueRigDialog::openMatchGroups()
{
    VenueCorrections *vc = venuesOf(m_doc);
    const VenueRig::Room room = vc->venue(m_venue).rig.room;
    if (room.groups.isEmpty())
        return nullptr;
    QDialog *d = new QDialog(this);
    d->setObjectName(QStringLiteral("vrMatchGroupsDialog"));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("%1: match the room's groups").arg(m_venue));
    d->setProperty("helpTopic", QStringLiteral("venues.md"));
    QVBoxLayout *l = new QVBoxLayout(d);
    QLabel *intro = new QLabel(tr("Choose the group of this show each of the room's groups fills. A cue on "
                                  "that group then plays on the room's lights in it."), d);
    intro->setWordWrap(true);
    l->addWidget(intro);
    GroupMatchTable *table = new GroupMatchTable(m_doc, d);
    QStringList names;
    QList<int> heads;
    QList<VenueImport::Match> matches;
    for (const VenueRig::Fill &g : room.groups)
    {
        names << g.groupName;
        heads << int(g.heads.size());
        matches << VenueImport::suggest(g.groupName, m_doc);
    }
    table->setRows(names, heads, matches);
    l->addWidget(table, 1);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, d);
    QPushButton *ok = bb->addButton(tr("Fill These Groups"), QDialogButtonBox::AcceptRole);
    ok->setObjectName(QStringLiteral("vrMatchGroupsOk"));
    auto sync = [table, ok]() {
        const QList<VenueImport::Match> m = table->matches();
        const int waiting = VenueImport::unconfirmed(m);
        int chosen = 0;
        for (const VenueImport::Match &x : m)
            chosen += x.confirmed ? 1 : 0;
        ok->setEnabled(waiting == 0 && chosen > 0);
        ok->setText(waiting > 0 ? (waiting == 1 ? tr("Confirm Each Match: 1 Left")
                                                : tr("Confirm Each Match: %1 Left").arg(waiting))
                                : tr("Fill These Groups"));
    };
    connect(table, &GroupMatchTable::matchesChanged, d, sync);
    sync();
    connect(bb, &QDialogButtonBox::rejected, d, &QDialog::reject);
    connect(ok, &QPushButton::clicked, d, [this, d, table]() {
        QMap<int, quint32> chosen;
        const QList<VenueImport::Match> m = table->matches();
        for (int i = 0; i < m.size(); ++i)
            if (m.at(i).confirmed)
                chosen.insert(i, m.at(i).group);
        venuesOf(m_doc)->matchRoomGroups(m_venue, chosen);
        d->accept();
        refresh();
    });
    l->addWidget(bb);
    d->resize(560, 360);
    d->show();
    return d;
}

bool VenueRigDialog::commitNotOurs(int row)
{
    QTableWidgetItem *it = m_table->item(row, NotOursCol);
    if (it == nullptr)
        return false;
    VenueCorrections *vc = venuesOf(m_doc);
    QList<VenueRig::NotOurs> list;
    for (const VenueRig::NotOurs &n : vc->notOurs(m_venue))
        if (n.universe != quint32(row))
            list << n;
    const QString text = it->text().trimmed();
    bool ok = true;
    if (text.compare(tr("all"), Qt::CaseInsensitive) == 0 || text.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0)
    {
        VenueRig::NotOurs n;
        n.universe = quint32(row);
        list << n;
    }
    else if (!text.isEmpty())
    {
        for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts))
        {
            VenueRig::NotOurs n;
            n.universe = quint32(row);
            if (!VenueRig::parseRange(part, n.from, n.to) || n.from < 0)
            {
                ok = false;
                break;
            }
            list << n;
        }
    }
    if (!ok)
    {
        const bool was = m_loading;
        m_loading = true;
        it->setBackground(QColor(200, 60, 60, 90));
        it->setToolTip(tr("Not a range: write \"all\", or ranges such as \"401-512\" (1 to 512)"));
        m_loading = was;
        return false;
    }
    QString why;
    if (!vc->setNotOurs(m_venue, list, &why))
    {
        QMessageBox::information(this, windowTitle(), why);
        return false;
    }
    refresh();
    return true;
}
