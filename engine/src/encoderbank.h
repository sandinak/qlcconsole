/*
  Q Light Controller Plus
  encoderbank.h

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

#ifndef ENCODERBANK_H
#define ENCODERBANK_H

#include <QElapsedTimer>
#include <QStringList>
#include <QObject>
#include <QTimer>
#include <QString>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCEncoderBank QStringLiteral("EncoderBank")

/**
 * The Encoder bank scheme of the Control Map: a MIDI controller with endless
 * encoders (an X-Touch Mini, a Midi Fighter Twister, an APC40's knobs)
 * turns the attributes of what is selected in Build. One bank of eight
 * encoders reaches five banks of attributes (Intensity, Colour, Position,
 * Beam, Gobo / prism); a button steps the banks; an encoder's push is fine
 * while it is held with a turn, and home when it is tapped.
 *
 * This class is the controller side only: it reads the device (the relative
 * encodings, the pushes, the bank buttons), coalesces the turns, and lights
 * the device back (a ring per encoder, a lamp per bank). What a turn DOES
 * is the consumer's: the Workbench connects adjusted() / homed() and edits
 * the selection through the Look Editor's own path (one undo step per
 * gesture), then tells the rings where the values are (setRing()).
 * EncoderAttributes works out the channels of a fixture selection.
 *
 * Saved with the show inside the Control Map's element; a show without it
 * saves as before. GUI thread only. Off (no connection to the input at all)
 * until a device is set up; a turn costs no allocation.
 */
class QLC_ENGINE_EXPORT EncoderBank final : public QObject
{
    Q_OBJECT

public:
    explicit EncoderBank(Doc *doc);
    ~EncoderBank();

    /*********************************************************************
     * Banks and slots
     *********************************************************************/
public:
    enum Bank { IntensityBank = 0, ColourBank, PositionBank, BeamBank, GoboBank };
    static constexpr int bankCount() { return 5; }
    static constexpr int slotsPerBank() { return 8; }

    /** "Intensity", "Colour", "Position", "Beam", "Gobo / prism". */
    static QString bankName(int bank);
    /** How many of the eight encoders the bank uses. */
    static int slotCount(int bank);
    /** "Hue", "Saturation", "Red"... empty past slotCount(). */
    static QString slotName(int bank, int slot);

    /*********************************************************************
     * The device
     *********************************************************************/
public:
    /** How an encoder's value byte says which way it turned and by how
     *  much. The first three are the usual relative conventions (the same
     *  as QLCInputSource::RelativeEncoding); AbsoluteDelta reads a knob that
     *  sends its position (an APC40's by default) as the change from its
     *  last position -- it stops at the ends of its travel. */
    enum Encoding { TwosComplement = 0, SignedBit, BinaryOffset, AbsoluteDelta };

    /** A controller's mapping. Every control is an input channel number of
     *  the universe's input (the MIDI plugin's: a CC n is n, a note n is
     *  128 + n, plus 4096 for each MIDI channel above the first in an Omni
     *  patch); -1 is "none". Encoders, pushes and rings are consecutive
     *  from their first, one per encoder. */
    struct Device
    {
        QString name;
        quint32 universe = 0;
        Encoding encoding = TwosComplement;
        bool invert = false;
        int encoderCount = 8;
        qint32 encoders = -1;       //!< first encoder's turn
        qint32 pushes = -1;         //!< first encoder's push
        qint32 rings = -1;          //!< first ring (feedback line)
        qint32 bankNext = -1;       //!< the button that steps to the next bank
        qint32 bankPrevious = -1;
        qint32 bankDirect = -1;     //!< first of five buttons, one per bank
        qint32 bankLamps = -1;      //!< first of five lamps (feedback line)
    };

    /** The presets: decided (can change) -- three generic encodings and the
     *  X-Touch Mini, Midi Fighter Twister and APC40 mappings. */
    static QStringList presetKeys();
    static QString presetName(const QString &key);
    /** A preset's mapping (@a universe kept); false: no such key. */
    static bool preset(const QString &key, Device *device);
    /** A short note on the preset: what to set on the controller. */
    static QString presetNote(const QString &key);

    static QString encodingName(Encoding encoding);
    static QString encodingToString(Encoding encoding);
    static Encoding stringToEncoding(const QString &str);
    /** The signed number of detents a value byte (0-255, as the MIDI plugin
     *  scales a 0-127 data byte) says for a relative @a encoding; 0 for
     *  AbsoluteDelta, which needs the last position. */
    static int decode(Encoding encoding, uchar value);

    Device device() const { return m_device; }
    /** Set the controller up; a device with encoders turns the bank on.
     *  Marks the show modified. */
    void setDevice(const Device &device, const QString &presetKey = QString());
    /** The preset the device came from ("" for a hand-made mapping). */
    QString presetKey() const { return m_presetKey; }
    bool isEnabled() const { return m_enabled; }
    /** Turn the controller off (kept as set up; setEnabled(true) on again). */
    void setEnabled(bool enabled);
    /** The controller's control as a person reads it ("CC 16", "Note 8,
     *  MIDI ch 2"); empty for none. */
    static QString controlName(qint32 channel);

    /** Back to a new show's (off, no device); does not mark it modified. */
    void reset();

    /*********************************************************************
     * Banks
     *********************************************************************/
public:
    int bank() const { return m_bank; }
    void setBank(int bank);
    /** The next bank (+1) or the previous (-1), round the five. */
    void stepBank(int direction);
    /** Whether encoder @a slot's push is held now (the fine turn). */
    bool isFine(int slot) const { return (m_pushDown & (1u << slot)) != 0; }

    /*********************************************************************
     * Input
     *********************************************************************/
public slots:
    /** One input value (InputOutputMap::inputValueChanged). Public for the
     *  tests: it is what the controller does. */
    void handleInput(quint32 universe, quint32 channel, uchar value);

public:
    /** How long turns are gathered before they are sent on, in ms (the
     *  default keeps a fast spin to some forty edits a second; 0: sent at
     *  once). A timer of this period runs from the first turn of a gesture
     *  until the encoders have been still for a few periods, so a turn
     *  itself starts and stops nothing. */
    int coalesceMs() const { return m_timer.interval(); }
    void setCoalesceMs(int ms);
    /** Send what is gathered now. */
    void flush();

signals:
    /** Encoder @a slot of @a bank turned by @a detents (signed, as many as
     *  came in since the last), @a fine when its push was held. */
    void adjusted(int bank, int slot, int detents, bool fine);
    /** Encoder @a slot of @a bank was tapped: back to its default. */
    void homed(int bank, int slot);
    /** The bank stepped (or was set). */
    void bankChanged(int bank);
    /** The device was set up, turned on or off, or the show's cleared. */
    void deviceChanged();
    /** A turn that could not be used, and why ("Encoders are off in
     *  Operate"); at most one every two seconds. */
    void refused(const QString &why);
    /** The device was connected again: the consumer sends its rings. */
    void ringsWanted();
    /** Everything sent to the controller's feedback line (also when there
     *  is none patched), for the tests. */
    void feedbackSent(quint32 universe, quint32 channel, uchar value);

    /*********************************************************************
     * Feedback
     *********************************************************************/
public:
    /** Light encoder @a slot's ring to @a norm (0-1; below 0: unused,
     *  dark). Sent only when it changed. */
    void setRing(int slot, float norm);
    /** Every ring dark (no selection, a bank without that encoder). */
    void clearRings();
    /** The lamp of the current bank on, the others off. */
    void sendBankLamps();
    /** The lamps again and the rings wanted again. */
    void resendFeedback();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /** Writes nothing when the bank has never been set up. */
    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

private slots:
    void slotInputDeviceConnected();
    void slotTick();

private:
    void updateConnection();
    void turn(int slot, uchar value);
    void push(int slot, bool down);
    void queue(int slot, int detents, bool fine);
    void send(quint32 channel, uchar value);
    /** The index of @a channel in a run of @a count from @a first (-1: not). */
    static int indexIn(quint32 channel, qint32 first, int count)
    {
        if (first < 0 || channel < quint32(first))
            return -1;
        const quint32 i = channel - quint32(first);
        return i < quint32(count) ? int(i) : -1;
    }
    bool operateRefuses();

private:
    Doc *m_doc;
    Device m_device;
    QString m_presetKey;
    bool m_enabled = false;
    bool m_connected = false;
    int m_bank = IntensityBank;
    quint32 m_pushDown = 0;         //!< bit per encoder
    quint32 m_turned = 0;           //!< turned since its push went down
    int m_pending[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    bool m_pendingFine[8] = { false, false, false, false, false, false, false, false };
    int m_last[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };    //!< AbsoluteDelta
    int m_ring[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };    //!< last sent
    QTimer m_timer;
    int m_idleTicks = 0;
    QElapsedTimer m_refusedAt;
};

/** @} */

#endif
