/*
  Q Light Controller Plus
  hidprofile.h

  Maps HID axis/button indices to human-readable names for a specific
  device identified by vendor ID + one or more product IDs.

  Profile files use the .qxhid extension and live in Resources/HIDProfiles/
  (or Library/Application Support/QLC+/HIDProfiles/ for user profiles).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef HIDPROFILE_H
#define HIDPROFILE_H

#include <QHash>
#include <QList>
#include <QString>
#include <QtGlobal>

class HIDProfile
{
public:
    HIDProfile() = default;

    QString name() const { return m_name; }
    QString path() const { return m_path; }
    unsigned short vendorId() const { return m_vendorId; }
    QList<unsigned short> productIds() const { return m_productIds; }
    bool matchesDevice(unsigned short vid, unsigned short pid) const;

    int axisCount() const { return m_axisNames.size(); }
    int buttonCount() const { return m_buttonNames.size(); }

    /** Human-readable name for axis @p index, or empty string if not in profile. */
    QString axisName(int index) const { return m_axisNames.value(index); }
    /** Human-readable name for button @p index, or empty string if not in profile. */
    QString buttonName(int index) const { return m_buttonNames.value(index); }

    /** Semantic slot for axis @p index: "pan", "tilt", or empty. */
    QString axisSlot(int index) const { return m_axisSlots.value(index); }
    /** Action string for button @p index: "chaser_step_forward", "chaser_step_back",
     *  "followspot_toggle", etc., or empty if not assigned. */
    QString buttonAction(int index) const { return m_buttonActions.value(index); }

    /** Returns the axis index with the given slot ("pan"/"tilt"), or -1 if none. */
    int axisForSlot(const QString &slot) const;

    /** Joystick sensitivity multiplier (applied to normalized axis travel). */
    float sensitivity() const { return m_sensitivity; }
    void  setSensitivity(float s) { m_sensitivity = qBound(0.1f, s, 3.0f); }

    /** Deadzone fraction around joystick centre to ignore (0–0.5). */
    float deadzone() const { return m_deadzone; }
    void  setDeadzone(float d) { m_deadzone = qBound(0.0f, d, 0.5f); }

    /* Setters — used by the profile editor */
    void setAxisName(int index, const QString &name)   { m_axisNames.insert(index, name); }
    void setAxisSlot(int index, const QString &slot)
    { if (slot.isEmpty()) m_axisSlots.remove(index); else m_axisSlots.insert(index, slot); }
    void setButtonName(int index, const QString &name) { m_buttonNames.insert(index, name); }
    void setButtonAction(int index, const QString &action)
    { if (action.isEmpty()) m_buttonActions.remove(index); else m_buttonActions.insert(index, action); }

    /** Save this profile back to its file.  Returns true on success. */
    bool save() const;

    /** Load a profile from @p path. Returns nullptr on failure (caller owns result). */
    static HIDProfile *load(const QString &path);

private:
    QString m_name;
    QString m_path;
    unsigned short m_vendorId = 0;
    QList<unsigned short> m_productIds; // empty = match all PIDs for this VID
    QHash<int, QString> m_axisNames;
    QHash<int, QString> m_buttonNames;
    QHash<int, QString> m_axisSlots;
    QHash<int, QString> m_buttonActions;
    float m_sensitivity = 1.0f;
    float m_deadzone    = 0.05f;
};

#endif // HIDPROFILE_H
