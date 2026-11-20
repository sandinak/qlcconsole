/*
  Q Light Controller Plus
  rolefilldialog.cpp

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
#include <QApplication>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>

#include "programmercontroller.h"
#include "venuecorrections.h"
#include "showtreecontroller.h"
#include "rolefilldialog.h"
#include "fixturemanager.h"
#include "fixturegroup.h"
#include "houserole.h"
#include "fixture.h"
#include "app.h"
#include "doc.h"

namespace
{
/* "Ch 15 Linear, Ch 24 Down and 3 more". */
QString nameList(Doc *doc, const QList<quint32> &ids, int max = 4)
{
    QStringList names;
    for (int i = 0; i < ids.count() && i < max; ++i)
        if (const Fixture *fx = doc->fixture(ids.at(i)))
            names << fx->name();
    QString s = names.join(QStringLiteral(", "));
    if (ids.count() > max)
        s = RoleFillDialog::tr("%1 and %n more", "", int(ids.count()) - max).arg(s);
    return s;
}
}

RoleFillDialog::RoleFillDialog(Doc *doc, const QString &venue, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_venue(venue)
{
    setObjectName(QStringLiteral("RoleFillDialog"));
    setWindowTitle(tr("%1: Roles").arg(venue));
    setProperty("helpTopic", QStringLiteral("venues.md#roles-house-lights-and-stage-light-in-any-room"));
    QVBoxLayout *vl = new QVBoxLayout(this);

    m_intro = new QLabel(this);
    m_intro->setObjectName(QStringLiteral("rfIntro"));
    m_intro->setWordWrap(true);
    vl->addWidget(m_intro);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("rfTable"));
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({ tr("Role"), tr("Lights from %1").arg(venue), tr("Why"), tr("Use"),
                                         QString() });
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(LightsCol, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(WhyCol, QHeaderView::Stretch);
    vl->addWidget(m_table, 1);

    m_notUsed = new QLabel(this);
    m_notUsed->setObjectName(QStringLiteral("rfNotUsed"));
    m_notUsed->setWordWrap(true);
    vl->addWidget(m_notUsed);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("rfStatus"));
    m_status->setWordWrap(true);
    m_status->hide();
    vl->addWidget(m_status);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_edit = bb->addButton(tr("Edit\342\200\246"), QDialogButtonBox::ActionRole);
    m_edit->setObjectName(QStringLiteral("rfEdit"));
    m_edit->setToolTip(tr("Open the selected role's group in Fixture Manager: add or take out lights by hand"));
    m_fill = bb->addButton(tr("Fill Roles"), QDialogButtonBox::ApplyRole);
    m_fill->setObjectName(QStringLiteral("rfFill"));
    m_fill->setDefault(true);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_fill, &QPushButton::clicked, this, [this]() { fillRoles(); });
    connect(m_edit, &QPushButton::clicked, this, [this]() { editRow(m_table->currentRow()); });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) {
        const HouseRole::Role r = m_rowRoles.value(row, HouseRole::None);
        m_edit->setEnabled(HouseRole::group(m_doc, r) != nullptr);
    });
    vl->addWidget(bb);

    VenueCorrections *vc = m_doc->programmer()->venueCorrections();
    connect(vc, &VenueCorrections::activeVenueChanged, this, &RoleFillDialog::refresh);
    connect(vc, &VenueCorrections::venuesChanged, this, [this]() {
        if (!m_doc->programmer()->venueCorrections()->hasVenue(m_venue))
            close();
    });
    resize(820, 440);
    refresh();
}

RoleFillDialog *RoleFillDialog::open(Doc *doc, QWidget *parent, const QString &venue)
{
    RoleFillDialog *d = new RoleFillDialog(doc, venue, parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->show();
    return d;
}

QCheckBox *RoleFillDialog::useBox(int row) const
{
    QWidget *w = m_table->cellWidget(row, UseCol);
    return w != nullptr ? w->findChild<QCheckBox *>() : nullptr;
}

QPushButton *RoleFillDialog::flashButton(int row) const
{
    return qobject_cast<QPushButton *>(m_table->cellWidget(row, FlashCol));
}

int RoleFillDialog::rowOf(HouseRole::Role role) const
{
    return int(m_rowRoles.indexOf(role));
}

void RoleFillDialog::refresh()
{
    VenueCorrections *vc = m_doc->programmer()->venueCorrections();
    const bool applied = vc->activeVenue() == m_venue && vc->rig()->isApplied();
    m_intro->setText(applied
        ? tr("Each role is a group of this show: a cue written on it (House to half, Stage up) plays on "
             "whichever lights fill it in the room. These are %1's lights suggested for each. Flash a row "
             "to see them in the room; untick what is wrong.").arg(m_venue)
        : tr("Apply %1 to fill its roles: its lights are on the rig only while it is applied.").arg(m_venue));

    m_suggested.clear();
    QList<HouseRole::Suggestion> notUsed;
    QMap<HouseRole::Role, QStringList> whys;
    for (const HouseRole::Suggestion &s : HouseRole::suggest(m_doc))
    {
        if (s.role == HouseRole::None)
        {
            notUsed << s;
            continue;
        }
        m_suggested[s.role] << s.fixture;
        if (!whys[s.role].contains(s.why))
            whys[s.role] << s.why;
    }

    m_rowRoles = HouseRole::roles();
    m_table->setRowCount(int(m_rowRoles.size()));
    for (int row = 0; row < m_rowRoles.size(); ++row)
    {
        const HouseRole::Role r = m_rowRoles.at(row);
        const QList<quint32> ids = m_suggested.value(r);
        const FixtureGroup *g = HouseRole::group(m_doc, r);
        QTableWidgetItem *name = new QTableWidgetItem(g != nullptr ? g->name() : HouseRole::groupName(r));
        m_table->setItem(row, RoleCol, name);

        QString lights;
        if (ids.isEmpty())
            lights = tr("none in this room");
        else
        {
            bool relays = false;
            for (quint32 id : ids)
                if (const Fixture *fx = m_doc->fixture(id))
                    relays = relays || (fx->outputRules() & Fixture::Switch);
            lights = QStringLiteral("%1  %2").arg(ids.count()).arg(nameList(m_doc, ids));
            if (relays)
                lights += tr(" (relays: half is on)");
        }
        QTableWidgetItem *li = new QTableWidgetItem(lights);
        li->setToolTip(nameList(m_doc, ids, 40));
        if (ids.isEmpty())
            li->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        m_table->setItem(row, LightsCol, li);
        const QStringList why = whys.value(r);
        QTableWidgetItem *wi = new QTableWidgetItem(why.mid(0, 2).join(QStringLiteral("; ")));
        wi->setToolTip(why.join(QLatin1Char('\n')));
        m_table->setItem(row, WhyCol, wi);

        QWidget *cell = new QWidget(m_table);
        QHBoxLayout *cl = new QHBoxLayout(cell);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setAlignment(Qt::AlignCenter);
        QCheckBox *use = new QCheckBox(cell);
        use->setObjectName(QStringLiteral("rfUse%1").arg(HouseRole::xmlName(r)));
        use->setChecked(!ids.isEmpty());
        use->setEnabled(applied && !ids.isEmpty());
        use->setToolTip(tr("Fill %1 with these lights").arg(HouseRole::groupName(r)));
        cl->addWidget(use);
        m_table->setCellWidget(row, UseCol, cell);

        QPushButton *flash = new QPushButton(tr("Flash"), m_table);
        flash->setObjectName(QStringLiteral("rfFlash%1").arg(HouseRole::xmlName(r)));
        flash->setEnabled(applied && !ids.isEmpty());
        flash->setToolTip(tr("Light these in the room for a moment, to see they are the right ones"));
        connect(flash, &QPushButton::clicked, this, [this, row]() { flashRow(row); });
        m_table->setCellWidget(row, FlashCol, flash);
    }
    m_table->resizeColumnToContents(RoleCol);
    m_table->resizeColumnToContents(UseCol);
    m_table->resizeColumnToContents(FlashCol);

    if (notUsed.isEmpty())
        m_notUsed->setText(applied ? tr("Every light of %1 is suggested for a role.").arg(m_venue) : QString());
    else
    {
        QStringList parts;
        for (int i = 0; i < notUsed.count() && i < 6; ++i)
        {
            const Fixture *fx = m_doc->fixture(notUsed.at(i).fixture);
            parts << tr("%1 (%2)").arg(fx != nullptr ? fx->name() : QString::number(notUsed.at(i).fixture),
                                       notUsed.at(i).why);
        }
        QString text = tr("Not used (%1): %2").arg(notUsed.count()).arg(parts.join(QStringLiteral(", ")));
        if (notUsed.count() > 6)
            text += tr(" and %n more", "", int(notUsed.count()) - 6);
        m_notUsed->setText(text);
    }
    m_fill->setEnabled(applied);
    m_fill->setToolTip(applied ? tr("The ticked rows become the roles' lights in %1").arg(m_venue)
                               : tr("Apply %1 first").arg(m_venue));
    m_edit->setEnabled(HouseRole::group(m_doc, m_rowRoles.value(m_table->currentRow(), HouseRole::None)) != nullptr);
}

void RoleFillDialog::flashRow(int row)
{
    const QList<quint32> ids = m_suggested.value(m_rowRoles.value(row, HouseRole::None));
    for (quint32 id : ids)
        m_doc->programmer()->flashFixture(id, 1500);
}

void RoleFillDialog::editRow(int row)
{
    const FixtureGroup *g = HouseRole::group(m_doc, m_rowRoles.value(row, HouseRole::None));
    FixtureManager *fm = FixtureManager::instance();
    if (g == nullptr || fm == nullptr)
        return;
    const auto widgets = qApp->topLevelWidgets();
    for (QWidget *w : widgets)
        if (App *app = qobject_cast<App *>(w))
            app->switchToTabContaining(fm);
    fm->selectGroup(g->id());
}

bool RoleFillDialog::fillRoles()
{
    QMap<HouseRole::Role, QList<quint32>> rows;
    for (int row = 0; row < m_rowRoles.size(); ++row)
    {
        QCheckBox *use = useBox(row);
        const HouseRole::Role r = m_rowRoles.at(row);
        if (use != nullptr && use->isChecked() && !m_suggested.value(r).isEmpty())
            rows.insert(r, m_suggested.value(r));
    }

    // His own groups by a role's name: use them, or make the roles beside them.
    QSet<HouseRole::Role> useExisting;
    const QList<HouseRole::Role> clashes = HouseRole::nameClashes(m_doc);
    if (!clashes.isEmpty())
    {
        QStringList names;
        for (HouseRole::Role r : clashes)
            names << QStringLiteral("\"%1\"").arg(HouseRole::groupName(r));
        int answer = m_useExistingAnswer;
        if (answer < 0)
        {
            QMessageBox box(QMessageBox::Question, windowTitle(),
                            tr("This show already has groups named %1. Use them as the roles, or make new "
                               "role groups beside them? Your groups are never renamed, and one that holds "
                               "tour lights is never used: a role holds the room's lights only.")
                                .arg(names.join(QStringLiteral(", "))),
                            QMessageBox::Cancel, this);
            box.setObjectName(QStringLiteral("rfUseExisting"));
            QPushButton *use = box.addButton(tr("Use My Groups"), QMessageBox::AcceptRole);
            QPushButton *make = box.addButton(tr("Make New Groups"), QMessageBox::ActionRole);
            box.setDefaultButton(use);
            box.exec();
            answer = box.clickedButton() == use ? 1 : box.clickedButton() == make ? 0 : -1;
            if (answer < 0)
                return false;
        }
        if (answer == 1)
            for (HouseRole::Role r : clashes)
                useExisting.insert(r);
    }

    HouseRole::Made made;
    QString why;
    if (!HouseRole::fillRoles(m_doc, m_venue, rows, useExisting, &made, &why))
    {
        QMessageBox::information(this, windowTitle(), why);
        return false;
    }
    QStringList madeText;
    if (made.groups > 0)
        madeText << tr("%n role group(s)", "", made.groups);
    if (made.palettes > 0)
        madeText << tr("%n palette(s)", "", made.palettes);
    if (made.cues > 0)
        madeText << tr("%n standard cue(s)", "", made.cues);
    if (made.hits > 0)
        madeText << tr("%n Global hit(s)", "", made.hits);
    QString text = tr("Filled %n role(s) in %1.", "", int(rows.size())).arg(m_venue);
    if (!madeText.isEmpty())
        text += QLatin1Char(' ') + tr("Made %1.").arg(madeText.join(QStringLiteral(", ")));
    for (const QString &g : made.besides)
        text += QLatin1Char(' ') + tr("Your group \"%1\" holds tour lights, so its role was made beside it: "
                                      "a house cue never drives a tour light.").arg(g);
    if (made.waiting)
        text += QLatin1Char(' ') + tr("The standard cues and their hits are made the first time you fill the "
                                      "roles after the show has a production.");
    m_status->setText(text);
    m_status->show();
    refresh();
    return true;
}
