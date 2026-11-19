/*
  Q Light Controller Plus
  roomoutputpicker.h

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

#ifndef ROOMOUTPUTPICKER_H
#define ROOMOUTPUTPICKER_H

#include <QWidget>
#include <QList>

#include "universe.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Where a venue's own universe goes in that room
 * (docs/design/VENUE_FROM_SHOW.md section 6): a USB DMX interface, an
 * Art-Net node, not yet, or -- in New Venue from a Show only -- what the
 * other show had. Used by that dialog and by the venue page.
 */
class RoomOutputPicker final : public QWidget
{
    Q_OBJECT

public:
    enum Kind { Usb = 0, ArtNet, NotYet, AsImported };

    explicit RoomOutputPicker(Doc *doc, QWidget *parent = nullptr);

    /** What the other show sent the universe to, and its file's name: adds
     *  "As the file had it" (never chosen by itself). Empty: not offered. */
    void setImported(const QList<Universe::OfflinePatch> &imported, const QString &fileName);
    /** Show @p patch as the choice. */
    void setOutput(const QList<Universe::OfflinePatch> &patch);
    /** The chosen output; empty for Not yet. */
    QList<Universe::OfflinePatch> output() const;
    Kind kind() const;
    /** The choice can be used (an interface chosen, an address that reads). */
    bool isComplete() const;
    /** The USB DMX interfaces plugged in now. */
    QStringList usbInterfaces() const;
    /** Look again for USB DMX interfaces (one plugged in since). */
    void refreshInterfaces();

signals:
    void changed();

private:
    void sync();

private:
    Doc *m_doc;
    QList<Universe::OfflinePatch> m_imported;
    QComboBox *m_kind;
    QComboBox *m_usb;
    QLineEdit *m_ip;
    QSpinBox *m_artnetUniverse;
    QLabel *m_note;
};

/** @} */

#endif
