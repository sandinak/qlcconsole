/*
  Q Light Controller Plus
  controlmappanel.h

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

#ifndef CONTROLMAPPANEL_H
#define CONTROLMAPPANEL_H

#include <QWidget>

#include "controlowners.h"
#include "encoderbank.h"
#include "controlmap.h"
#include "qlcui_export.h"

class ControlCollisionBox;
class QTableWidget;
class QVBoxLayout;
class QGroupBox;
class QCheckBox;
class QPushButton;
class QAction;
class QLabel;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * Every Control Map binding of the show in one table (Connections ▸
 * Devices ▸ Show control & sync ▸ Control Map): the control, what it
 * fires, how a press acts (editable); Remove and Remove all. Bindings are
 * made where the thing is, with Learn trigger… (ControlLearnDialog).
 */
class QLC_UI_EXPORT ControlMapPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ControlMapPanel(Doc *doc, QWidget *parent = nullptr);

    /** One line for the Devices row: "3 triggers", "None". */
    QString summary() const;

    QTableWidget *table() const { return m_table; }

    /** The Encoder bank section (the scheme of a MIDI controller's
     *  endless encoders; docs/features/encoder-banks.md): the device's
     *  preset, its universe and relative encoding, learn the first
     *  encoder, the pushes, the bank buttons and the rings, what it is
     *  mapped to. */
    QGroupBox *encoderBox() const { return m_encoderBox; }
    /** The encoder learn's refusal (a control with another job). */
    ControlCollisionBox *encoderCollision() const { return m_encCollision; }
    /** The Emergency Stop… button (always shown). */
    QPushButton *stopButton() const { return m_estop; }

public slots:
    void refresh();
    /** A key row's control says "· Armed" in Design while it may fire. */
    void refreshArming();
    /** The "Its light" column: what each control's light shows now
     *  (ControlMap::lamp(), CS3) -- the same answer the desk's LED has. */
    void refreshLamps();

signals:
    void summaryChanged();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    void slotRemove();
    void slotRemoveAll();
    void slotEmergencyStop();
    void refreshStop();
    void slotFired(const QString &what);
    void slotEncoderPreset(int index);
    void slotEncoderEdited();
    void slotEncoderLearn();
    void slotEncoderLearnMore(QAction *action);
    void slotEncoderHeard(quint32 universe, quint32 channel);

private:
    QString controlText(const ControlMap::Binding &b) const;
    QString stopRowText(const ControlMap::Binding &b) const;
    /** What a binding's light says, in words ("Ready", "Running", "GO
     *  held", "Stopped", "Dark"), or why it has none (a computer key, a
     *  universe with no feedback line). */
    QString lampText(const ControlMap::Binding &b) const;
    /** What it fires and its light wrap, never cut (UX-36): the light's
     *  column is kept narrow, every row as tall as its words. */
    void fitRows();
    /** Every row as tall as its words, never lower than its pickers. */
    void fitRowHeights();

private:
    Doc *m_doc;
    QTableWidget *m_table;
    QPushButton *m_remove;
    QPushButton *m_removeAll;
    QPushButton *m_estop;
    /** Whether a ready control glows dim (CS3): off for a pad whose lights
     *  take a colour in the velocity and not a brightness. */
    QCheckBox *m_dim = nullptr;
    QLabel *m_empty;
    QLabel *m_last;
    /** "Control surfaces are off for this show": the triggers and the encoder
     *  bank put away (Q293); the emergency stop stays. */
    QLabel *m_featureOff = nullptr;

    /** What the next control heard is set as. */
    enum LearnTarget { LearnNone = 0, LearnEncoders, LearnPushes, LearnNext, LearnPrevious, LearnDirect,
                       LearnRings, ApplyDevice };
    /** Who else listens to @a channels of @a universe: (channel, owner),
     *  each owner once (the bank's own controls are not in the way). */
    QList<QPair<quint32, ControlOwners::Owner>> encoderTaken(quint32 universe,
                                                            const QList<quint32> &channels) const;
    /** Every control device @a d listens to (encoders, pushes, bank buttons). */
    static QList<quint32> encoderChannels(const EncoderBank::Device &d);
    /** Sets the bank's device (a preset, Custom on again, another universe)
     *  unless a control of it has another job: then refused with the
     *  learn's box, and set on Move It Here. False: refused. */
    bool setEncoderDevice(const EncoderBank::Device &d, const QString &key);
    void applyEncoderDevice(const EncoderBank::Device &d, const QString &key);
    EncoderBank::Device m_encPendingDevice;
    QString m_encPendingKey;
    void buildEncoderBox(QVBoxLayout *lay);
    void startEncoderLearn(int target);
    void endEncoderLearn();
    void refreshEncoder();
    /** Sets what learn @a target heard (the control @a channel of
     *  @a universe) on the encoder bank's device. */
    void applyEncoderLearn(int target, quint32 universe, quint32 channel);
    /** @a d as a learn of @a target heard on @a channel of @a universe
     *  leaves it (the whole bank on that universe). */
    static EncoderBank::Device learnedDevice(EncoderBank::Device d, int target,
                                             quint32 universe, quint32 channel);

    QGroupBox *m_encoderBox = nullptr;
    QComboBox *m_encPreset = nullptr;
    QComboBox *m_encUniverse = nullptr;
    QComboBox *m_encEncoding = nullptr;
    QCheckBox *m_encInvert = nullptr;
    QPushButton *m_encLearn = nullptr;
    QPushButton *m_encLearnMore = nullptr;
    QLabel *m_encNote = nullptr;
    QLabel *m_encMap = nullptr;
    int m_encLearning = LearnNone;
    bool m_encShowing = false;
    /** A learn refused (another listener has the control): it waits for
     *  Move It Here or Cancel. */
    ControlCollisionBox *m_encCollision = nullptr;
    int m_encPending = LearnNone;
};

/** @} */

#endif
