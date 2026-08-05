/*
  Q Light Controller
  createfixturegroup.h

  Copyright (c) Heikki Junnila

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

#ifndef CREATEFIXTUREGROUP_H
#define CREATEFIXTUREGROUP_H

#include <QDialog>
#include <QList>

#include "ui_createfixturegroup.h"

class Doc;
class FixtureGroup;

/** @addtogroup ui_fixtures
 * @{
 */

class CreateFixtureGroup final : public QDialog, public Ui_CreateFixtureGroup
{
    Q_OBJECT
    Q_DISABLE_COPY(CreateFixtureGroup)

public:
    CreateFixtureGroup(QWidget* parent);
    ~CreateFixtureGroup();

    QString name() const;

    void setSize(const QSize& size);
    QSize size() const;

    /** The ONE way to create a fixture group from a selection, shared by the
     *  Fixture Manager, the Programming tab and the Studio so the flow is
     *  consistent everywhere: suggest a grid (from the fixtures' 2-D layout when
     *  placed, else a near-square of the head count), prompt name+size, then
     *  arrange the fixtures into the grid IN PHYSICAL ORDER (rows top→bottom,
     *  columns left→right). Returns the new group, or NULL if cancelled/empty. */
    static FixtureGroup *createFromFixtures(Doc *doc,
                                            const QList<quint32> &fixtureIds,
                                            QWidget *parent);

    /** Suggest a grid size for @p fixtureIds — a real cols×rows from their 2-D
     *  positions if placed, otherwise a near-square of the total head count. */
    static QSize suggestGrid(Doc *doc, const QList<quint32> &fixtureIds);
};

/** @} */

#endif
