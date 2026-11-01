/*
  Q Light Controller Plus
  teachingtips.cpp

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

#include <QWidget>

#include "teachingsettings.h"
#include "teachingglossary.h"
#include "teachingtips.h"
#include "doc.h"

QString TeachingTips::tip(const Doc *doc, const QString &baseTip, const QStringList &terms)
{
    if (doc == nullptr || doc->teachingSettings() == nullptr || !doc->teachingSettings()->enabled())
        return baseTip;
    QStringList lines;
    for (const QString &term : terms)
    {
        const QString line = TeachingGlossary::line(term);
        if (!line.isEmpty())
            lines << line;
    }
    if (lines.isEmpty())
        return baseTip;
    return baseTip.isEmpty() ? lines.join(QLatin1Char('\n'))
                             : baseTip + QStringLiteral("\n\n") + lines.join(QLatin1Char('\n'));
}

void TeachingTips::attach(Doc *doc, QWidget *widget, const QString &baseTip, const QStringList &terms)
{
    if (doc == nullptr || widget == nullptr)
        return;
    auto apply = [doc, widget, baseTip, terms]() {
        const QString t = tip(doc, baseTip, terms);
        widget->setToolTip(t);
        // What's This: the glossary alone, the touch screen's way to ask.
        QString what;
        for (const QString &term : terms)
            what += (what.isEmpty() ? QString() : QStringLiteral("\n")) + TeachingGlossary::line(term);
        widget->setWhatsThis(doc->teachingSettings()->enabled() ? what : QString());
    };
    apply();
    if (doc->teachingSettings() != nullptr)
        QObject::connect(doc->teachingSettings(), &TeachingSettings::teachingChanged, widget, apply);
}
