/*
  Q Light Controller Plus - qlcconsole
  controlsurfacespanel.h

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

#ifndef CONTROLSURFACESPANEL_H
#define CONTROLSURFACESPANEL_H

#include <QWidget>
#include <QStringList>
#include <QMap>

#include "qlcui_export.h"

class QLabel;
class QPushButton;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * Connections ▸ Devices ▸ Show control & sync ▸ Control surfaces: every place
 * a control surface is set up, one row each -- what it is, what is set up
 * now, and a Set up… button that goes to the place. It keeps no settings of
 * its own: each status line is read live from the model that owns the answer
 * (docs/features/control-surfaces.md, "What hears a control").
 */
class QLC_UI_EXPORT ControlSurfacesPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ControlSurfacesPanel(Doc *doc, QWidget *parent = nullptr);

    /** The row keys, in order: "map", "estop", "encoder", "vc", "masters",
     *  "pmj", "web", "pads". */
    static QStringList keys();

    /** The status line of row @p key, as the row shows it. */
    QString status(const QString &key) const;
    QPushButton *setUpButton(const QString &key) const { return m_buttons.value(key); }

    /** One line for the Devices row: "3 triggers · emergency stop not learned". */
    QString summary() const;

public slots:
    /** Read every status line again. */
    void refresh();

signals:
    /** A Set up… was pressed. */
    void setUpRequested(const QString &key);
    /** Some status line changed (the Devices row follows). */
    void summaryChanged();

private:
    QString triggersText() const;
    QString padsText() const;
    QString pmjText() const;
    QString vcText() const;
    QString mastersText() const;
    QString webText() const;
    QString encoderText() const;
    QString stopText() const;

    Doc *m_doc;
    QLabel *m_featureOff = nullptr;
    QMap<QString, QLabel *> m_status;
    QMap<QString, QPushButton *> m_buttons;
    QString m_lastSummary;
};

/** @} */

#endif
