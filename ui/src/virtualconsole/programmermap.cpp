/*
  Q Light Controller Plus
  programmermap.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>

#include "programmermap.h"

namespace
{
    const QString kRoot          = QStringLiteral("ProgrammerMap");
    const QString kManufacturer  = QStringLiteral("Manufacturer");
    const QString kModel         = QStringLiteral("Model");
    const QString kProfile       = QStringLiteral("Profile");
    const QString kGridColumns   = QStringLiteral("GridColumns");
    const QString kMapping       = QStringLiteral("Mapping");
    const QString kAttrChannel   = QStringLiteral("channel");
    const QString kAttrKind      = QStringLiteral("kind");
    const QString kAttrRole      = QStringLiteral("role");
    const QString kAttrByte      = QStringLiteral("controlByte");
    const QString kAttrMode      = QStringLiteral("mode");
    const QString kAttrRow       = QStringLiteral("row");
    const QString kAttrColumn    = QStringLiteral("column");
    const QString kAttrPadMode   = QStringLiteral("padMode");
    const QString kAttrPadRow    = QStringLiteral("padRow");
    const QString kAttrPadCol    = QStringLiteral("padCol");
    const QString kAttrStyle     = QStringLiteral("style");
    const QString kAttrRowSpan   = QStringLiteral("rowSpan");
}

VCSlider::ParameterRole ProgrammerMap::parseRole(const QString& s)
{
    return VCSlider::stringToParameterRole(s);
}

int ProgrammerMap::parseControlByte(const QString& s)
{
    if (s.compare(QStringLiteral("LSB"), Qt::CaseInsensitive) == 0)
        return 1;
    return 0;
}

VCButton::SelectionMode ProgrammerMap::parseSelectionMode(const QString& s)
{
    return VCButton::stringToSelectionMode(s);
}

ProgrammerMap::WidgetKind ProgrammerMap::parseKind(const QString& s)
{
    if (s.compare(QStringLiteral("ParameterSlider"), Qt::CaseInsensitive) == 0)
        return ParameterSlider;
    if (s.compare(QStringLiteral("SelectFixturesButton"), Qt::CaseInsensitive) == 0)
        return SelectFixturesButton;
    if (s.compare(QStringLiteral("ClearSelectionButton"), Qt::CaseInsensitive) == 0)
        return ClearSelectionButton;
    if (s.compare(QStringLiteral("GrandMasterSlider"), Qt::CaseInsensitive) == 0)
        return GrandMasterSlider;
    if (s.compare(QStringLiteral("SaveProgrammerButton"), Qt::CaseInsensitive) == 0)
        return SaveProgrammerButton;
    if (s.compare(QStringLiteral("RevertProgrammerButton"), Qt::CaseInsensitive) == 0)
        return RevertProgrammerButton;
    if (s.compare(QStringLiteral("PadModeButton"), Qt::CaseInsensitive) == 0)
        return PadModeButton;
    if (s.compare(QStringLiteral("FixturePadCellButton"), Qt::CaseInsensitive) == 0)
        return FixturePadCellButton;
    if (s.compare(QStringLiteral("ChaserStepNextButton"), Qt::CaseInsensitive) == 0)
        return ChaserStepNextButton;
    if (s.compare(QStringLiteral("ChaserStepPrevButton"), Qt::CaseInsensitive) == 0)
        return ChaserStepPrevButton;
    return Unknown;
}

bool ProgrammerMap::load(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "ProgrammerMap: cannot open" << filePath << f.errorString();
        return false;
    }

    QXmlStreamReader r(&f);
    if (!r.readNextStartElement() || r.name() != kRoot)
    {
        qWarning() << "ProgrammerMap: not a ProgrammerMap file:" << filePath;
        return false;
    }

    m_entries.clear();

    while (r.readNextStartElement())
    {
        const QString name = r.name().toString();
        if (name == kManufacturer)
            m_manufacturer = r.readElementText();
        else if (name == kModel)
            m_model = r.readElementText();
        else if (name == kProfile)
            m_profile = r.readElementText();
        else if (name == kGridColumns)
            m_gridColumns = qMax(1, r.readElementText().toInt());
        else if (name == kMapping)
        {
            Entry e;
            const auto attrs = r.attributes();
            e.channel = attrs.value(kAttrChannel).toUInt();
            e.kind = parseKind(attrs.value(kAttrKind).toString());

            if (attrs.hasAttribute(kAttrRole))
                e.role = parseRole(attrs.value(kAttrRole).toString());
            if (attrs.hasAttribute(kAttrByte))
                e.controlByte = parseControlByte(attrs.value(kAttrByte).toString());
            if (attrs.hasAttribute(kAttrMode))
                e.selectionMode = parseSelectionMode(attrs.value(kAttrMode).toString());
            if (attrs.hasAttribute(kAttrPadMode))
            {
                const QString pm = attrs.value(kAttrPadMode).toString();
                if (pm.compare(QStringLiteral("FixtureSelect"), Qt::CaseInsensitive) == 0)
                    e.padMode = 1; // Doc::PadModeFixtureSelect
                else if (pm.compare(QStringLiteral("GoboSelect"), Qt::CaseInsensitive) == 0)
                    e.padMode = 2;
                else if (pm.compare(QStringLiteral("ColorPalette"), Qt::CaseInsensitive) == 0)
                    e.padMode = 3;
                else
                    e.padMode = 0;
            }
            if (attrs.hasAttribute(kAttrPadRow))
                e.padRow = attrs.value(kAttrPadRow).toInt();
            if (attrs.hasAttribute(kAttrPadCol))
                e.padCol = attrs.value(kAttrPadCol).toInt();
            if (attrs.hasAttribute(kAttrRow))
                e.row = attrs.value(kAttrRow).toInt();
            if (attrs.hasAttribute(kAttrColumn))
                e.column = attrs.value(kAttrColumn).toInt();
            if (attrs.hasAttribute(kAttrStyle))
            {
                const QString s = attrs.value(kAttrStyle).toString();
                if (s.compare(QStringLiteral("Knob"), Qt::CaseInsensitive) == 0)
                    e.style = VCSlider::WKnob;
                else
                    e.style = VCSlider::WSlider;
            }
            if (attrs.hasAttribute(kAttrRowSpan))
                e.rowSpan = qMax(1, attrs.value(kAttrRowSpan).toInt());

            if (e.kind != Unknown)
                m_entries.append(e);
            r.skipCurrentElement();
        }
        else
        {
            qWarning() << "ProgrammerMap: unknown tag" << name;
            r.skipCurrentElement();
        }
    }
    return !r.hasError();
}
