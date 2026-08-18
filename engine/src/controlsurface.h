/*
  Q Light Controller Plus
  controlsurface.h

  Device-agnostic vocabulary for the control-surface engine. A physical board
  (PMJ, APC40 mk2, …) is described as a set of typed Controls; what each control
  MEANS is a logical Role; the LED shown on a control reflects the Role's
  context-aware State. See CONTROL_SURFACE_DESIGN.md.

  Pure types — no MIDI, no UI. A device overlay maps its real controls to these
  and knows how to push LEDs.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CONTROLSURFACE_H
#define CONTROLSURFACE_H

#include <QString>
#include <QMetaType>

namespace ControlSurface
{
    /** What a physical control is. */
    enum class Kind { Button, Encoder, Fader };

    /** The device-independent MEANING bound to a control. `index` disambiguates
     *  within a type (strip number, encoder number, page id, …). */
    enum class RoleType
    {
        None,
        Select,     //!< index = strip N — select / fire item N on the current page
        Load,       //!< index = strip N — load item N into the programmer
        Level,      //!< index = strip N (-1 = master) — a fader level
        Reset,      //!< index = strip N — reset that fader's controlled value to default
        Param,      //!< index = encoder E — adjust parameter E on the current page
        Page,       //!< index = page id — switch the surface page/mode
        Transport,  //!< index = a Transport value below
        Static      //!< index = a Static value below (never pages)
    };

    enum Transport { Go, Back, Next, Prev, Left, Right };
    enum Static    { Blackout, GrandMaster, Blind, Tap, AutoMIB, Clear, Record };

    struct Role
    {
        RoleType type = RoleType::None;
        int      index = -1;
        Role() = default;
        Role(RoleType t, int i = -1) : type(t), index(i) {}
        bool operator==(const Role &o) const { return type == o.type && index == o.index; }
        bool isValid() const { return type != RoleType::None; }
    };

    /** Context-aware LED state for a role, computed from the app right now. */
    enum class State
    {
        Empty,     //!< nothing here / not applicable → LED off
        Valid,     //!< available but idle → dim
        Selected,  //!< the current selection / armed → mid
        Active     //!< live / running / the active page → bright
    };

    /** A physical control on a device. `ledId` is opaque to the engine — the
     *  device overlay interprets it (PMJ: a MIDI note on ch 9). */
    struct Control
    {
        Kind    kind = Kind::Button;
        QString name;
        int     ledId = -1;      //!< < 0 = no LED (faders, dumb buttons)
        bool    hasLed() const { return ledId >= 0; }
    };

    /** Map a role State to a 0..maxBrightness LED level. Kept here so every device
     *  is lit consistently; a colour-capable board can override in its sink. */
    inline int brightness(State s, int maxBrightness)
    {
        switch (s)
        {
            case State::Empty:    return 0;
            case State::Valid:    return maxBrightness / 4;
            case State::Selected: return (maxBrightness * 2) / 3;
            case State::Active:   return maxBrightness;
        }
        return 0;
    }
}

Q_DECLARE_METATYPE(ControlSurface::Role)

#endif // CONTROLSURFACE_H
