/*
  Q Light Controller Plus
  stagecolourpicker.cpp

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

#include <QColorDialog>
#include <QHelpEvent>
#include <QToolTip>
#include <QImage>

#include "stagecolourpicker.h"
#include "palettenaming.h"

namespace {
const int kColumns = 8;
const int kRows = PaletteNaming::StageColourCount / kColumns;
}

StageColourPicker::StageColourPicker(QColorDialog *dialog)
    : QObject(dialog)
{
    const QList<PaletteNaming::StageColour> &all = PaletteNaming::stageColours();
    // The list reads row by row; Qt's grid is filled column by column.
    for (int i = 0; i < all.size(); ++i)
        QColorDialog::setStandardColor((i / kColumns) + (i % kColumns) * kRows, all.at(i).rgb);

    // Qt's swatch grid and colour field are private classes: find them by name.
    const QList<QWidget *> kids = dialog->findChildren<QWidget *>();
    for (QWidget *w : kids)
    {
        // (QtPrivate::QColorPicker in newer Qt)
        const QByteArray cls = w->metaObject()->className();
        if (cls.endsWith("QColorWell") && m_grid == nullptr)
            m_grid = w;
        else if (cls.endsWith("::QColorPicker") || cls == "QColorPicker")
        {
            if (m_field == nullptr)
                m_field = w;
        }
    }
    if (m_grid != nullptr)
        m_grid->installEventFilter(this);
    if (m_field != nullptr)
        m_field->installEventFilter(this);
}

StageColourPicker *StageColourPicker::install(QColorDialog *dialog)
{
    return new StageColourPicker(dialog);
}

QString StageColourPicker::describe(const QColor &rgb)
{
    bool exact = false;
    const QString name = PaletteNaming::stageColourName(rgb, &exact);
    return exact ? tr("%1  %2").arg(name, rgb.name())
                 : tr("≈ %1  %2").arg(name, rgb.name());
}

QColor StageColourPicker::emittersFor(const QColor &rgb)
{
    for (const PaletteNaming::StageColour &c : PaletteNaming::stageColours())
        if (c.rgb == rgb)
            return c.wauv;
    return QColor();
}

int StageColourPicker::swatchAt(const QPoint &pos) const
{
    if (m_grid == nullptr || m_grid->width() <= 0 || m_grid->height() <= 0)
        return -1;
    const int col = pos.x() * kColumns / m_grid->width();
    const int row = pos.y() * kRows / m_grid->height();
    if (pos.x() < 0 || pos.y() < 0 || col >= kColumns || row >= kRows)
        return -1;
    return row * kColumns + col;
}

bool StageColourPicker::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent *he = static_cast<QHelpEvent *>(event);
        QString text;
        if (watched == m_grid)
        {
            const int i = swatchAt(he->pos());
            if (i >= 0)
            {
                const PaletteNaming::StageColour &c = PaletteNaming::stageColours().at(i);
                text = tr("%1  %2").arg(c.name, c.rgb.name());
            }
        }
        else if (watched == m_field)
        {
            const QImage img = m_field->grab(QRect(he->pos(), QSize(1, 1))).toImage();
            if (!img.isNull())
                text = describe(img.pixelColor(0, 0));
        }
        if (text.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(he->globalPos(), text, static_cast<QWidget *>(watched));
        return true;
    }
    return QObject::eventFilter(watched, event);
}
