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
#include <QList>
#include <QPair>

#include "qlcui_export.h"

class QLabel;
class QPushButton;
class DeskSetupPanel;
class SurfaceMaps;
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
     *  "gm", "pmj", "web", "pads", "devices" (a device plugged in that no
     *  input has), "collisions" (Who has it now: every control
     *  two listeners have, with how to fix it; no Set Up…, and shown only
     *  while there is one), "places" (Triggers by place: each control with
     *  a trigger at a place, what it does where and which trigger has it
     *  now, Q296; read only, shown only while there is one). */
    static QStringList keys();

    /** The status line of row @p key, as the row shows it. */
    QString status(const QString &key) const;
    /** True while row @p key's status is an open warning, drawn amber
     *  (the stop that stops nothing: UX-32). */
    bool statusWarns(const QString &key) const;
    QPushButton *setUpButton(const QString &key) const { return m_buttons.value(key); }
    /** This computer's desk, at the top (DeskSetupPanel). */
    DeskSetupPanel *deskPanel() const { return m_desk; }
    /** Each patched board drawn (SurfaceMaps). */
    SurfaceMaps *surfaceMaps() const { return m_maps; }

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
    /** What the PMJ's lit lamps mean (the help's "What the lamps mean"). */
    static QString pmjLampsText();
    QString vcText() const;
    QString mastersText() const;
    QString grandMasterText() const;
    QString devicesText() const;
    /** Each hardware input device plugged in that no universe's input
     *  has: its name, and whether it is a game pad. */
    QList<QPair<QString, bool>> unpatchedDevices() const;
    QString webText() const;
    QString encoderText() const;
    QString stopText() const;
    QString collisionsText() const;
    QString placesText() const;
    /** The Triggers by place row only (who has each control now). */
    void refreshPlaces();

protected:
    void showEvent(QShowEvent *event) override;

private:

    Doc *m_doc;
    DeskSetupPanel *m_desk = nullptr;
    SurfaceMaps *m_maps = nullptr;
    QLabel *m_featureOff = nullptr;
    QMap<QString, QLabel *> m_status;
    QMap<QString, QLabel *> m_what;
    QMap<QString, QPushButton *> m_buttons;
    QString m_lastSummary;
};

/** @} */

#endif
