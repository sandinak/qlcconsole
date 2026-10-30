/*
  Q Light Controller Plus
  focusmenu.cpp

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

#include <QAbstractButton>
#include <QAction>
#include <QMenu>

#include "programmercontroller.h"
#include "venuecorrections.h"
#include "focuscallsdialog.h"
#include "focussession.h"
#include "focuscalls.h"
#include "focusmenu.h"
#include "venuemenu.h"
#include "fixture.h"
#include "doc.h"

namespace
{
QList<quint32> allFixtures(Doc *doc)
{
    QList<quint32> all;
    for (Fixture *fx : doc->fixtures())
        all << fx->id();
    return all;
}

QList<quint32> callFixtures(Doc *doc)
{
    QList<quint32> fids;
    for (const FocusCall &c : doc->programmer()->focusCalls()->calls())
        for (quint32 fid : FocusCalls::fixturesOf(doc, c))
            if (!fids.contains(fid))
                fids << fid;
    return fids;
}
}

QString FocusMenu::quickFocusBlocked(Doc *doc)
{
    if (FocusSession::focusable(doc, allFixtures(doc)).isEmpty())
        return tr("No lights are placed on the plot. Place them in the Lighting Studio first.");
    return QString();
}

QString FocusMenu::venueFocusBlocked(Doc *doc)
{
    if (doc->programmer()->focusCalls()->count() == 0)
        return tr("No focus calls yet. Add them with Focus \342\226\270 Edit focus calls\342\200\246");
    if (FocusSession::focusable(doc, callFixtures(doc)).isEmpty())
        return tr("None of the focus calls has a light placed on the plot. Place them in the "
                  "Lighting Studio, or change the calls.");
    return QString();
}

QString FocusMenu::practiceBlocked(Doc *doc)
{
    if (doc->programmer()->focusCalls()->count() == 0)
        return tr("No focus calls yet to practise on. Add them with Focus \342\226\270 Edit focus calls\342\200\246");
    if (FocusSession::movingHeads(doc, callFixtures(doc)).isEmpty())
        return tr("No moving head in the focus calls is placed on the plot: a scramble moves "
                  "moving heads.");
    if (doc->programmer()->venueCorrections()->filePath().isEmpty())
        return tr("Save the show first: a practice is kept as a venue in a file beside it.");
    if (doc->programmer()->venueCorrections()->isPracticeActive())
        return tr("A practice is applied already: finish it first.");
    return QString();
}

QString FocusMenu::reportBlocked(Doc *doc)
{
    if (doc->programmer()->venueCorrections()->venueNames().isEmpty())
        return tr("No venue yet: a focus report is a venue's. Make one with Venues \342\226\270 New venue\342\200\246");
    return QString();
}

QString FocusMenu::venuesBlocked(Doc *doc)
{
    if (doc->programmer()->venueCorrections()->filePath().isEmpty())
        return tr("Save the show first: a venue's corrections are kept in a file beside it.");
    return QString();
}

QString FocusMenu::venueRigBlocked(Doc *doc)
{
    if (!doc->programmer()->venueCorrections()->isActive())
        return tr("No venue is applied. Apply one, or make one with New venue\342\200\246");
    return QString();
}

QString FocusMenu::focusCueTip(Doc *doc)
{
    return doc->programmer()->padText(
        tr("Move, zoom, focus and recolour this cue from the stage with the controller: "
           "{LT}/{RT} step layers, {LB}/{RB} fixtures, {Y} the scope, {A} keeps"));
}

QString FocusMenu::focusCueBlocked(Doc *doc)
{
    if (doc->fixtures().isEmpty())
        return tr("No lights in the show yet to focus. Add fixtures first.");
    return QString();
}

QString FocusMenu::venueFocusCueBlocked(Doc *doc)
{
    if (doc->programmer()->focusCalls()->count() == 0)
        return tr("No focus calls yet for this cue to run. Add them in the Lighting Studio with "
                  "Focus \342\226\270 Edit focus calls\342\200\246");
    return QString();
}

void FocusMenu::setAvailable(QAction *action, const QString &blocked, const QString &tip)
{
    action->setEnabled(blocked.isEmpty());
    action->setToolTip(blocked.isEmpty() ? tip : blocked);
}

void FocusMenu::setAvailable(QAbstractButton *button, const QString &blocked, const QString &tip)
{
    button->setEnabled(blocked.isEmpty());
    button->setToolTip(blocked.isEmpty() ? tip : blocked);
}

void FocusMenu::fill(QMenu *menu, Doc *doc, QWidget *parent, const std::function<void()> &quickFocus)
{
    menu->clear();
    // clear() leaves the submenus (children, not actions): last time's Venues.
    for (QMenu *sub : menu->findChildren<QMenu *>(Qt::FindDirectChildrenOnly))
        delete sub;
    menu->setToolTipsVisible(true);
    VenueCorrections *venues = doc->programmer()->venueCorrections();

    QAction *quick = menu->addAction(tr("Quick focus\342\200\246"), parent, [quickFocus]() {
        if (quickFocus)
            quickFocus();
    });
    quick->setObjectName(QStringLiteral("focusQuick"));
    setAvailable(quick, quickFocusBlocked(doc),
                 tr("Aim each light at its reference in turn and nudge its calibration until it lands "
                    "there; saves into the show (controller: %1)")
                 .arg(doc->programmer()->padText(QStringLiteral("{View} + {Menu}"))));

    QAction *venue = menu->addAction(tr("Venue focus\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::startVenueFocus(doc, parent);
    });
    venue->setObjectName(QStringLiteral("focusVenue"));
    setAvailable(venue, venueFocusBlocked(doc),
                 venues->isActive() ? tr("Run the focus calls for %1, group by group to each point; "
                                         "corrections are saved for this venue, not the show")
                                      .arg(venues->activeVenue())
                                    : tr("Run the focus calls: name the venue first, then group by group "
                                         "to each point; corrections are saved for the venue, not the show"));

    QAction *edit = menu->addAction(tr("Edit focus calls\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::run(doc, parent);
    });
    edit->setObjectName(QStringLiteral("focusEditCalls"));
    edit->setToolTip(tr("The show's focus calls, in the order they are run: groups, the point they "
                        "focus to, a note for the crew; and the venue"));

    menu->addSeparator();
    /* The venue menu (Q150): the same one the app's venue chip and View ▸
       Venue… open, filled by the same code. */
    QMenu *venuesMenu = menu->addMenu(tr("Venues"));
    venuesMenu->setObjectName(QStringLiteral("focusVenuesMenu"));
    venuesMenu->menuAction()->setObjectName(QStringLiteral("focusVenues"));
    VenueMenu::fill(venuesMenu, doc, parent);

    menu->addSeparator();
    QAction *practice = menu->addAction(tr("Practice (scramble)\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::practice(doc, parent);
    });
    practice->setObjectName(QStringLiteral("focusPractice"));
    setAvailable(practice, practiceBlocked(doc),
                 tr("Knocks the moving heads in the focus calls off their targets by a random amount; "
                    "you focus them back, scored on degrees off, time and saves. Your real venues are "
                    "never touched."));

    QAction *report = menu->addAction(tr("Focus report\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::exportAppliedReport(doc, parent);
    });
    report->setObjectName(QStringLiteral("focusReport"));
    setAvailable(report, reportBlocked(doc),
                 tr("A venue's focus, light by light: a PDF or web page to print, or a CSV for Lightwright"));
}
