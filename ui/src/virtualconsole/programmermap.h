/*
  Q Light Controller Plus
  programmermap.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMERMAP_H
#define PROGRAMMERMAP_H

#include <QList>
#include <QString>

#include "vcbutton.h"
#include "vcslider.h"

/**
 * A ProgrammerMap is a small XML file (.qxpm) that, paired with a MIDI
 * input profile, says "for surface X, channel N is a parameter slider
 * for role Red MSB / a select-fixtures button in Replace mode / a
 * clear-selection button / etc.". The Generate Programmer Frame wizard
 * reads the map and the input profile together to lay out a fully wired
 * VCFrame matching the device.
 */
class ProgrammerMap
{
public:
    enum WidgetKind
    {
        Unknown,
        ParameterSlider,
        SelectFixturesButton,
        ClearSelectionButton,
        GrandMasterSlider,
        SaveProgrammerButton,
        RevertProgrammerButton,
        PadModeButton,
        FixturePadCellButton,
        ChaserStepNextButton,
        ChaserStepPrevButton
    };

    struct Entry
    {
        quint32 channel = 0;
        WidgetKind kind = Unknown;
        // Parameter-slider-only:
        VCSlider::ParameterRole role = VCSlider::RoleDimmer;
        int controlByte = 0;
        // Slider/GrandMaster only: presentation style. WSlider = tall
        // fader, WKnob = compact rotary. Default WSlider.
        VCSlider::SliderWidgetStyle style = VCSlider::WSlider;
        // SelectFixtures-button-only:
        VCButton::SelectionMode selectionMode = VCButton::SelectReplace;
        // PadModeButton-only: which pad-grid mode the button drives.
        int padMode = 0;          // Doc::PadModeOff
        // FixturePadCellButton-only: zero-based pad-grid coordinates
        // (independent of VC layout row/column).
        int padRow = -1;
        int padCol = -1;
        // Optional layout hint: 0-based grid position. -1 = unspecified.
        int row = -1;
        int column = -1;
        // Vertical span: how many rows this widget visually occupies.
        // Spanning widgets stretch across rows without inflating the
        // rows' heights, so e.g. a tall knob can sit alongside short
        // pad cells without making those pad rows oversized.
        int rowSpan = 1;
    };

    ProgrammerMap() = default;

    /** Load a map from a .qxpm file. Returns true on success. */
    bool load(const QString& filePath);

    /** Manufacturer + Model strings parsed from the map file. */
    QString manufacturer() const { return m_manufacturer; }
    QString model() const { return m_model; }

    /** Optional: the input-profile filename this map is intended to
        pair with (e.g. "Akai-APC40-mkII.qxi"). May be empty. */
    QString matchingProfile() const { return m_profile; }

    /** Suggested grid width for the auto-laid-out frame. */
    int gridColumns() const { return m_gridColumns; }

    QList<Entry> entries() const { return m_entries; }

    /** Convenience: convert role / mode strings used in the .qxpm
        format into the matching enum values. */
    static VCSlider::ParameterRole parseRole(const QString& s);
    static int parseControlByte(const QString& s);
    static VCButton::SelectionMode parseSelectionMode(const QString& s);
    static WidgetKind parseKind(const QString& s);

private:
    QString m_manufacturer;
    QString m_model;
    QString m_profile;
    int m_gridColumns = 8;
    QList<Entry> m_entries;
};

#endif
