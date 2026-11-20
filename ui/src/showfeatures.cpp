/*
  Q Light Controller Plus - qlcconsole
  showfeatures.cpp

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

#include <QGlobalStatic>
#include <QStringList>

#include "showfeatures.h"

Q_GLOBAL_STATIC(ShowFeatures, s_features)

ShowFeatures::ShowFeatures(QObject *parent)
    : QObject(parent)
{
}

ShowFeatures *ShowFeatures::instance()
{
    return s_features();
}

bool ShowFeatures::isOn(Feature f)
{
    // At exit the holder may be gone: everything is on then.
    return s_features.isDestroyed() || s_features()->on(f);
}

bool ShowFeatures::on(Feature f) const
{
    return !m_off.contains(int(f));
}

void ShowFeatures::setOn(Feature f, bool on)
{
    if (f < 0 || f >= FeatureCount || this->on(f) == on)
        return;
    if (on)
        m_off.remove(int(f));
    else
        m_off.insert(int(f));
    emit changed();
}

void ShowFeatures::reset()
{
    setOffSet(QSet<int>());
}

void ShowFeatures::setOffSet(const QSet<int> &off)
{
    QSet<int> clean;
    for (int f : off)
        if (f >= 0 && f < FeatureCount)
            clean.insert(f);
    if (clean == m_off)
        return;
    m_off = clean;
    emit changed();
}

QString ShowFeatures::key(Feature f)
{
    switch (f)
    {
    case Venues:          return QStringLiteral("Venues");
    case Power:           return QStringLiteral("Power");
    case GrandMaster:     return QStringLiteral("GrandMaster");
    case SpeedMasters:    return QStringLiteral("SpeedMasters");
    case EngineLoad:      return QStringLiteral("EngineLoad");
    case Review:          return QStringLiteral("Review");
    case ControlSurfaces: return QStringLiteral("ControlSurfaces");
    case FocusCalls:      return QStringLiteral("FocusCalls");
    default:              return QString();
    }
}

QString ShowFeatures::label(Feature f)
{
    switch (f)
    {
    case Venues:          return tr("Venues");
    case Power:           return tr("Power estimate");
    case GrandMaster:     return tr("Grand Master");
    case SpeedMasters:    return tr("Speed masters");
    case EngineLoad:      return tr("Engine load");
    case Review:          return tr("Show Review");
    case ControlSurfaces: return tr("Control surfaces");
    case FocusCalls:      return tr("Focus calls");
    default:              return QString();
    }
}

QString ShowFeatures::description(Feature f)
{
    switch (f)
    {
    case Venues:
        return tr("The venue chip, View ▸ Venue…, and the Lighting Studio's Focus ▸ Venues and "
                  "Focus Report…");
    case Power:
        return tr("The footer's power estimate, the Fixtures tab's Power pane and Power menu, and "
                  "the plot's Power overlay");
    case GrandMaster:
        return tr("The footer's Grand Master fader (the Grand Master itself still works)");
    case SpeedMasters:
        return tr("The footer's speed masters (what follows them still does)");
    case EngineLoad:
        return tr("The footer's engine load");
    case Review:
        return tr("Show Review's footer chip, its marks on cues, its re-check after each change, "
                  "and the readiness dialog's Show Review… button");
    case ControlSurfaces:
        return tr("The Control Map's triggers and encoder bank, Learn Trigger…, and the Run tab's "
                  "Pad button while the pad is not running. The emergency stop always stays");
    case FocusCalls:
        return tr("Venue Focus…, Edit Focus Calls… and a venue focus cue, in the Lighting Studio, "
                  "the Show Tree and the Run tab");
    default:
        return QString();
    }
}

QString ShowFeatures::offAttribute() const
{
    QStringList out;
    for (int f = 0; f < FeatureCount; ++f)
        if (m_off.contains(f))
            out << key(Feature(f));
    return out.join(QLatin1Char('|'));
}

void ShowFeatures::setFromOffAttribute(const QString &attr)
{
    QSet<int> off;
    const QStringList keys = attr.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (int f = 0; f < FeatureCount; ++f)
        if (keys.contains(key(Feature(f))))
            off.insert(f);
    setOffSet(off);
}
