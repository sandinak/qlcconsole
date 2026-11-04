/*
  Q Light Controller Plus
  shotdark.h

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

#ifndef SHOTDARK_H
#define SHOTDARK_H

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

/** The dark theme for a screenshot test (the lead reviews shots dark): Fusion
 *  and a dark palette on the whole application. Call before building widgets. */
inline void shotUseDarkTheme()
{
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    const QColor base(30, 31, 36), text(222, 224, 228), button(48, 50, 57);
    p.setColor(QPalette::Window, QColor(40, 42, 48));
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(36, 38, 44));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, QColor(52, 54, 62));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, QColor(48, 110, 190));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(140, 143, 150));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 122, 128));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 122, 128));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 122, 128));
    qApp->setPalette(p);
}

#endif
