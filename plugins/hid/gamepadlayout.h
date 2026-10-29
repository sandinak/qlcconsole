/*
  Q Light Controller Plus
  gamepadlayout.h

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

#ifndef GAMEPADLAYOUT_H
#define GAMEPADLAYOUT_H

#include <QString>
#include <QtGlobal>

/**
 * The Xbox family of controllers, as they reach a Mac, put onto ONE layout:
 * the one the Xbox profile (xbox.qxhid) names -- axes 0-5 are LX, LY, LT, RX,
 * RY, RT and buttons 0-15 are A, B, X, Y, LB, RB, View, Menu, LS, RS, Xbox,
 * D-pad up/down/left/right, Share. Every model and connection is read into
 * this order, so the profile's names and gamepad roles (and so Focus mode and
 * the follow spot) are right whichever pad it is and however it is connected.
 *
 * They do not all report the same way:
 *  - over USB, Apple's driver hands over the controller's own GIP input
 *    report (ID 0x20) -- parseGipReport();
 *  - an Xbox One S on its first Bluetooth firmware (product 02E0) is a plain
 *    HID gamepad: buttons 1-10 in a row, right stick on Rx/Ry, triggers on
 *    Z/Rz;
 *  - every later Bluetooth or Bluetooth LE firmware (One S updated, Series
 *    X|S, Elite Series 2, Adaptive) leaves gaps in the button numbers (A=1,
 *    B=2, X=4, Y=5, LB=7, RB=8, View=11, Menu=12, Xbox=13, LS=14, RS=15),
 *    puts the right stick on Z/Rz and the triggers on the Simulation page's
 *    Brake (LT) and Accelerator (RT), and may send View, Xbox and Share as
 *    Consumer-page keys instead.
 * Where Apple's GameController framework is available the plugin reads that
 * instead (it names every element itself); this is what it falls back to.
 *
 * Plain C++ on Qt Core: no IOKit, so the tables can be tested anywhere.
 */
namespace GamepadLayout
{
    const quint16 kMicrosoftVendorId = 0x045E;
    /** Sony: the DualShock 4 and DualSense (board Q139) are read into the
     *  same layout -- Cross is A, Circle B, Square X, Triangle Y, L1/R1
     *  the bumpers, Create (Share) View, Options Menu, PS the guide button,
     *  the touchpad click Share -- so every tool sees one pad. */
    const quint16 kSonyVendorId = 0x054C;

    enum Axis { LeftX = 0, LeftY, LeftTrigger, RightX, RightY, RightTrigger, AxisCount };

    enum Button
    {
        A = 0, B, X, Y, LB, RB,
        View,       ///< "Back" in the profile
        Menu,       ///< "Start" in the profile
        LS, RS,
        Guide,      ///< the Xbox button
        DpadUp, DpadDown, DpadLeft, DpadRight,
        Share,      ///< Series X|S only; no gamepad role
        ButtonCount
    };

    /** A PlayStation pad's touchpad (board Q170), on the channels after the
     *  buttons -- ButtonCount + these -- so no profile index moves: each
     *  finger's place (0..255, 0 = left / top) and its touch (255 down).
     *  Only Apple's GameController framework reads them. */
    enum Touch { Touch1X = 0, Touch1Y, Touch1Down, Touch2X, Touch2Y, Touch2Down, TouchCount };

    enum Transport { UnknownTransport, Usb, Bluetooth, BluetoothLE };

    /** How a pad's reports are read into the layout. */
    enum Family
    {
        NotXbox,        ///< not a Microsoft pad: its elements in the order it lists them
        XboxGip,        ///< USB through Apple's driver: the GIP report 0x20
        XboxBtClassic,  ///< Xbox One S, first Bluetooth firmware
        XboxBtModern,   ///< every later Bluetooth / Bluetooth LE firmware
        /** A DualShock 4 or DualSense: through Apple's GameController
         *  framework (its light bar too), else its HID elements by usage. */
        PlayStation
    };

    /** A transport as IOKit names it ("USB", "Bluetooth", "Bluetooth Low
     *  Energy"...). */
    Transport transportFromString(const QString &name);
    /** The transport in a macOS hidapi path ("<transport>_<vid>_<pid>_<location>"). */
    Transport transportFromPath(const QString &path);
    QString transportName(Transport transport);

    /** A Sony pad this layout knows (DualShock 4, DualSense). */
    bool isPlayStation(quint16 vendorId, quint16 productId);

    /** What kind of pad it is, for the names the tools give its controls
     *  (QLCIOPlugin::inputDeviceKind()): "dualsense", "dualshock4", "xbox";
     *  empty for anything else. */
    QString deviceKind(quint16 vendorId, quint16 productId);

    /** The model's name ("Xbox Series X|S"), empty for one not in the table. */
    QString modelName(quint16 vendorId, quint16 productId);
    /** The transport a product ID implies (each Bluetooth/BLE firmware has
     *  its own ID), UnknownTransport when it does not say. */
    Transport transportOfProduct(quint16 vendorId, quint16 productId);

    /** How to read this pad. @p hasStandardElements: IOKit found Generic
     *  Desktop axes or Button-page buttons on it (USB through Apple's driver
     *  shows only a vendor element). @p maxButtonUsage: the highest
     *  Button-page usage it has, @p hasSimulationTriggers: it has Brake or
     *  Accelerator -- they tell the Bluetooth firmwares apart for a product
     *  ID the table does not know. */
    Family family(quint16 vendorId, quint16 productId, Transport transport,
                  bool hasStandardElements, int maxButtonUsage = 0,
                  bool hasSimulationTriggers = false);
    QString familyName(Family family);

    enum ElementKind { Unmapped, AxisElement, ButtonElement, HatElement };
    struct Mapping
    {
        ElementKind kind = Unmapped;
        int index = -1;            ///< Axis or Button; -1 for a hat or nothing
    };

    /** Where HID element (@p usagePage, @p usage) goes on a pad of @p family
     *  (not NotXbox, which keeps its own order). A PlayStation pad's: buttons
     *  1-14 are Square, Cross, Circle, Triangle, L1, R1, L2, R2, Create,
     *  Options, L3, R3, PS, touchpad (L2/R2 also come as Rx/Ry, which are
     *  the triggers here); the right stick is Z/Rz. */
    Mapping mapElement(Family family, quint32 usagePage, quint32 usage);

    /** The D-pad a hat switch's @p value says, as up/down/left/right. Eight
     *  directions from @p logicalMin, clockwise from up; anything else
     *  (the null state) is centred. */
    void hatToDpad(int value, int logicalMin, int logicalMax, bool dpad[4]);

    /** A pad's state in the layout: axes as the profile gets them (0..255;
     *  sticks centred at 127/128 with 0 = left / pushed UP; triggers 0 =
     *  released). */
    struct PadState
    {
        uchar axes[AxisCount];
        bool buttons[ButtonCount];
        uchar touch[TouchCount];
    };
    /** Sticks centred, triggers released, nothing pressed, no finger on
     *  the touchpad (its places in the middle). */
    void centre(PadState *state);

    /** A touchpad finger as Apple's GameController framework reports it
     *  (x, y -1..1, up positive), into @p state's finger @p finger (0 or
     *  1). The framework has no "finger down": a lifted finger reads
     *  exactly (0, 0) -- so that is a lift, and the place stays where the
     *  finger left it (no jump to the middle); a touch anywhere else, the
     *  middle's neighbours too, is down. */
    void readTouch(float x, float y, int finger, PadState *state);

    /** Read a GIP report as Apple's driver passes it (the report ID at
     *  [0]) into @p state: 0x20 is the controller's state (everything but
     *  the Xbox button), 0x07 the Xbox button alone. False, and @p state
     *  untouched, for anything else or a short report. */
    bool parseGipReport(const uchar *report, int length, PadState *state);

    /** A signed 16-bit stick reading as 0..255 (-32768 = 0). */
    uchar stickToByte(int raw);
    /** @p value in @p min..@p max as 0..255. */
    uchar scaleToByte(int value, int min, int max);

    /** The profile's names and gamepad roles for the layout, for the
     *  Controller test window and the logs. */
    QString axisName(int axis);
    QString buttonName(int button);
    QString axisRole(int axis);
    /** A button's, and past ButtonCount the touchpad's (Touch). */
    QString buttonRole(int button);

    /** A HID element as a person can read it: "Button 4", "Rz", "Brake",
     *  "Hat switch", "Consumer 0x224 (AC Back)", "Page 0xff00 usage 0x20". */
    QString elementName(quint32 usagePage, quint32 usage);
}

#endif
