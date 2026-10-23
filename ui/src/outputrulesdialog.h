/*
  Q Light Controller Plus
  outputrulesdialog.h

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

#ifndef OUTPUTRULESDIALOG_H
#define OUTPUTRULESDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>

#include "fixture.h"

class QCheckBox;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Output rules for one or more fixtures (Fixture::OutputRules): ignore
 * Blackout, ignore the Grand Master, switch (a relay), on when not in use.
 * A rule some of the fixtures have and some do not shows partly checked and
 * is left as each has it unless it is clicked to on or off.
 */
class OutputRulesDialog final : public QDialog
{
    Q_OBJECT

public:
    OutputRulesDialog(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent = nullptr);

    /** The rules @p fixtureId ends up with, as the boxes stand */
    Fixture::OutputRules rulesFor(quint32 fixtureId) const;

    /** Write the boxes to the fixtures; true when any changed */
    bool apply();

    /** The fixtures of @p groups and @p fixtures, each once, in order */
    static QList<quint32> collect(Doc *doc, const QList<quint32> &groups, const QList<quint32> &fixtures);

    /** Open it for @p groups' members and @p fixtures, and apply it as one
     *  rig undo step. True when anything changed. */
    static bool edit(QWidget *parent, Doc *doc, const QList<quint32> &groups,
                     const QList<quint32> &fixtures);

    /** The menu text: "Output Rules…" */
    static QString actionText();

    QCheckBox *box(Fixture::OutputRule rule) const { return m_boxes.value(rule); }

private:
    Doc *m_doc;
    QList<quint32> m_fixtures;
    QMap<Fixture::OutputRule, QCheckBox *> m_boxes;
};

/** @} */

#endif
