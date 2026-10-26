/*
  Q Light Controller Plus
  stagepiecedialog.h

  The properties of one stage piece (MonitorProperties::StagePiece): shown
  and locked, and where it is as a crew measures it -- the curtain line
  upstage of the front of the deck, the number line downstage of the
  curtain line, the apron's depth from the curtain line and its front
  curve, each Auto until set; the centre piece's DSC mark and full-depth
  line. Every distance is entered and read in the show's unit, as it is
  when the dialog opens. The fields change the show as they are edited
  (changed()); the caller makes that one undo step, and Cancel puts it back.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEPIECEDIALOG_H
#define STAGEPIECEDIALOG_H

#include <QDialog>
#include <functional>

class MonitorProperties;
class FeetInchesSpinBox;
class QCheckBox;
class QLabel;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class StagePieceDialog final : public QDialog
{
    Q_OBJECT

public:
    StagePieceDialog(MonitorProperties *props, int piece, QWidget *parent = nullptr);

    int piece() const { return m_piece; }
    /** Show the piece as it is now (after a drag, or an undo). */
    void sync();

signals:
    /** A field changed the show. */
    void changed();

private:
    /** A distance row: Auto, the value in the show's unit, and in words. */
    FeetInchesSpinBox *addDistance(class QFormLayout *form, const QString &title,
                                   const QString &name, const QString &tip,
                                   QCheckBox **autoBox, QLabel **words);
    double toDisplay(float metres) const;
    float toMetres(double shown) const;

    MonitorProperties *m_props;
    int m_piece;
    bool m_feet;
    bool m_syncing = false;
    QCheckBox *m_visible = nullptr;
    QCheckBox *m_locked = nullptr;
    QCheckBox *m_centreMark = nullptr;
    QCheckBox *m_centreLine = nullptr;
    QCheckBox *m_auto = nullptr;
    FeetInchesSpinBox *m_value = nullptr;
    FeetInchesSpinBox *m_curve = nullptr;
    QLabel *m_words = nullptr;
    QCheckBox *m_heightAuto = nullptr;
    FeetInchesSpinBox *m_height = nullptr;
    QLabel *m_heightWords = nullptr;
};

/** @} */

#endif
