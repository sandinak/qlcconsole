/*
  Q Light Controller Plus
  surfacelayout.h

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

#ifndef SURFACELAYOUT_H
#define SURFACELAYOUT_H

#include <QStringList>
#include <QString>
#include <QRectF>
#include <QSizeF>
#include <QColor>
#include <QList>
#include <QHash>

#include "qlcengine_export.h"

class QLCInputProfile;

/** @addtogroup engine Engine
 * @{
 */

/**
 * How a control surface is drawn, and the static roles it comes with
 * (docs/design/CONTROL_SCOPES.md section 3, the surface map). Data, not
 * code: one small JSON file per surface type, next to its input profile
 * (resources/inputprofiles/layouts/<profile file>.layout.json), built into
 * the engine as a resource.
 *
 * The board is a plane in grid units (a button is 1 x 1): each control of
 * the profile has its place and size there, the panel's sections their
 * labels. One physical control that sends on more than one profile
 * channel (the APC40's device knobs answer on the selected track's MIDI
 * channel) lists the others in "also". selfTest() holds a layout to its
 * profile: every channel placed exactly once, nothing overlapping, no
 * channel the profile lacks.
 *
 * "roles" are the board's default jobs for this computer's desk
 * (DeskRecord's GO, Back, STOP and Grand Master: "PLAY is GO"). A role
 * with "seed" true is learned for the desk the first time the board is
 * patched here, never over a role already learned (DeskRecord::
 * seedDefaults()); "seed" false is a job the board's own overlay already
 * does (the PMJ's Go, Back and Master), drawn as that role.
 */
class QLC_ENGINE_EXPORT SurfaceLayout
{
public:
    /** Indicator: a light only (the MaizeDMX's LEDs): it hears nothing. */
    enum Kind { Button, Pad, Fader, CrossFader, Knob, Encoder, Indicator };
    enum Lamp { NoLamp, Led, Rgb };
    /** How a control is cut: a key is a rounded rectangle, a transport
     *  button or an encoder cap is round ("shape": "round"). */
    enum Shape { Rectangle, Round };

    struct Control
    {
        quint32 channel = 0xFFFFFFFF;
        /** The same physical control on other profile channels. */
        QList<quint32> also;
        QRectF rect;            //!< in grid units
        Kind kind = Button;
        /** What a person calls it on the board ("PLAY", "Clip 1-1"). */
        QString face;
        /** What the board's own overlay does with it ("Level 1"), when it
         *  has one (the PMJ's); empty otherwise. */
        QString job;
        Lamp lamp = NoLamp;
        Shape shape = Rectangle;

        bool isFader() const { return kind == Fader || kind == CrossFader; }
        bool turns() const { return kind == Knob || kind == Encoder; }
        bool hears(quint32 ch) const { return channel == ch || also.contains(ch); }
    };

    struct Section
    {
        QString label;
        QRectF rect;
    };

    /** DeskRecord::Role's keys: "stop", "gm", "go", "back". */
    struct Role
    {
        QString role;
        quint32 channel = 0xFFFFFFFF;
        bool seed = false;
    };

    /** A named page of operating labels (the surface pages design: "Colour",
     *  "Position"): what a control does while that page is on. Read and
     *  kept; none of the built-in layouts has one yet. */
    struct Page
    {
        QString name;
        QHash<quint32, QString> labels;     //!< channel -> label
    };

    SurfaceLayout() {}

    bool isValid() const { return !m_profile.isEmpty() && !m_controls.isEmpty(); }
    /** The input profile it draws (QLCInputProfile::name()). */
    QString profileName() const { return m_profile; }
    /** "APC40 mkII" */
    QString name() const { return m_name; }
    /** For a footer chip: "APC40" */
    QString shortName() const { return m_short.isEmpty() ? m_name : m_short; }
    QSizeF size() const { return m_size; }
    /** Millimetres of the real board in one grid unit (its key sizes for
     *  the key label sheets); 0: not known. Measured off the board's
     *  published size, to be checked with a rule. */
    double unitMm() const { return m_unitMm; }

    const QList<Control> &controls() const { return m_controls; }
    const QList<Section> &sections() const { return m_sections; }
    const QList<Role> &roles() const { return m_roles; }
    const QList<Page> &pages() const { return m_pages; }

    /** The index of the control that hears profile channel @p channel
     *  (its own, or one of "also"); -1: none. */
    int indexOf(quint32 channel) const;
    /** The default role on @p channel's control; empty when none. */
    QString roleOn(quint32 channel) const;
    /** The channel of default role @p role ("go"); invalid when none. */
    quint32 roleChannel(const QString &role) const;

    /** The colour an RGB lamp shows for a value sent to it (0..255, as
     *  InputOutputMap::sendFeedBack() takes it). Invalid when the layout
     *  has no colour table. */
    QColor lampColour(uchar sent) const;
    bool hasLampColours() const { return !m_lampColours.isEmpty(); }
    /** "onOff": a light is on (any value above 0, at full) or off, and
     *  nothing else -- its colour is set on the device (the APC64 in Custom
     *  Mode: Akai's Project Editor); a blink is the app's, on and off by
     *  turns. Empty: the value is a level or a colour. */
    QString lampKind() const { return m_lampKind; }
    bool lampsOnOff() const { return m_lampKind == QLatin1String("onOff"); }

    /** The board's own screen, when it has one (the APC64's: 3 lines of 8
     *  characters): its size, and what each line shows (device setup
     *  writes them). 0 lines: no screen. */
    int displayLines() const { return m_displayLines; }
    int displayChars() const { return m_displayChars; }
    QStringList displayShows() const { return m_displayShows; }

    /** Reads a layout file. Invalid (and @p error says why) when it is not one. */
    static SurfaceLayout fromFile(const QString &path, QString *error = nullptr);
    static SurfaceLayout fromJson(const QByteArray &json, QString *error = nullptr);

    /** The built-in layout of input profile @p profileName; invalid when
     *  the type has none (the map falls back to the list rows). */
    static SurfaceLayout forProfile(const QString &profileName);
    /** Every built-in layout file (resource paths). */
    static QStringList builtInFiles();
    /** The shipped surface type profile @p fileName ("PMJ-Black-1.qxi"),
     *  as built into the engine: its resource path, to read with
     *  QLCInputProfile::loader(). The installed copy may be a user's. */
    static QString builtInTypePath(const QString &fileName);

    /** Every problem of this layout against @p profile: a channel of the
     *  profile not placed, or placed twice; a channel the profile lacks;
     *  two controls overlapping; a control off the board; a role on a
     *  channel no control has, a role twice, or one of an unknown name.
     *  Empty: it holds. */
    QStringList selfTest(const QLCInputProfile *profile) const;

private:
    QString m_profile;
    QString m_name;
    QString m_short;
    QString m_lampKind;
    int m_displayLines = 0;
    int m_displayChars = 0;
    QStringList m_displayShows;
    QSizeF m_size;
    double m_unitMm = 0;
    QList<Control> m_controls;
    QList<Section> m_sections;
    QList<Role> m_roles;
    QList<Page> m_pages;
    QList<QColor> m_lampColours;
    QHash<quint32, int> m_index;
};

/** @} */

#endif
