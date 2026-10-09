/*
  Q Light Controller Plus
  colourswatchbutton.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QColorDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>

#include "colourswatchbutton.h"

ColourSwatchButton::ColourSwatchButton(const QColor &colour, const QString &pickerTitle,
                                       QWidget *parent)
    : QWidget(parent)
    , m_colour(colour)
    , m_title(pickerTitle)
{
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    m_swatch = new QPushButton(this);
    m_swatch->setObjectName(QStringLiteral("colourSwatch"));
    m_swatch->setToolTip(pickerTitle);
    m_swatch->setMinimumWidth(90);
    lay->addWidget(m_swatch, 1);
    m_reset = new QToolButton(this);
    m_reset->setObjectName(QStringLiteral("colourReset"));
    m_reset->setText(tr("Reset"));
    m_reset->setVisible(false);
    lay->addWidget(m_reset);

    connect(m_swatch, &QPushButton::clicked, this, &ColourSwatchButton::pick);
    connect(m_reset, &QToolButton::clicked, this, [this]() { change(m_resetColour); });
    showColour();
}

void ColourSwatchButton::setColour(const QColor &colour)
{
    m_colour = colour;
    m_mixed = false;
    showColour();
}

void ColourSwatchButton::setMixed()
{
    m_colour = QColor();
    m_mixed = true;
    showColour();
}

void ColourSwatchButton::setResetColour(const QColor &colour)
{
    m_resetColour = colour;
    m_reset->setVisible(true);
    m_reset->setToolTip(colour.isValid() ? tr("Back to %1").arg(colour.name())
                                         : tr("No colour"));
    showColour();
}

void ColourSwatchButton::pick()
{
    const QColor c = QColorDialog::getColor(m_colour.isValid() ? m_colour : QColor(Qt::white),
                                            this, m_title);
    if (c.isValid())
        change(c);
}

void ColourSwatchButton::change(const QColor &colour)
{
    if (colour == m_colour && !m_mixed)
        return;
    m_colour = colour;
    m_mixed = false;
    showColour();
    emit colourChanged(m_colour);
}

void ColourSwatchButton::showColour()
{
    if (m_mixed)
    {
        m_swatch->setText(tr("Mixed"));
        m_swatch->setStyleSheet(QString());
    }
    else if (!m_colour.isValid())
    {
        m_swatch->setText(tr("None"));
        m_swatch->setStyleSheet(QString());
    }
    else
    {
        m_swatch->setText(m_colour.name().toUpper());
        m_swatch->setStyleSheet(QStringLiteral("background:%1; color:%2")
                                .arg(m_colour.name(), m_colour.lightness() > 128 ? "#000" : "#fff"));
    }
    m_reset->setEnabled(m_mixed || m_colour != m_resetColour);
}
