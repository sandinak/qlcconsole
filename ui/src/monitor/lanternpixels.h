/*
  Q Light Controller Plus
  lanternpixels.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef LANTERNPIXELS_H
#define LANTERNPIXELS_H

#include <QObject>
#include <QColor>
#include <QVector>
#include <QString>
#include <QPair>
#include <QSet>
#include <QHash>
#include <QElapsedTimer>
#include <QSharedPointer>

class EffectScript;
class QJSValue;
class QLCIOPlugin;
class QTimer;
class Fixture;
class Doc;

/** @addtogroup ui_monitor
 * @{
 */

/**
 * What each LED of a device that runs its own looks is showing -- a WLED
 * lantern in effect mode, where the console sends "Chase, speed 128, palette
 * 6" and the device works out every LED itself.
 *
 * In order of truth:
 *  - Device: the device's own live view, when it is patched to an output
 *    that can ask it (QLCIOPlugin::targetPixels()) and has answered lately.
 *    The real thing, whatever its firmware, palettes and segments.
 *  - Script: the effect script that ports that WLED effect (effect.wled,
 *    lib/wled.js), run on the LEDs with the speed, intensity, palette and
 *    colours the device is sent. The same maths as the device; not in step
 *    with it (its clock, its random numbers).
 *  - Flat: every LED in the look's colour 1 at its dimmer -- right for a
 *    solid look; for an effect with no port, tag names the effect it is NOT
 *    showing.
 */
class LanternPixels final : public QObject
{
    Q_OBJECT

public:
    enum Source { Flat = 0, Device, Script };

    struct Frame
    {
        QVector<QColor> leds;   ///< one per LED, at its level (black = off)
        Source source = Flat;
        QString tag;            ///< "WLED: Chase" when the look is not shown
    };

    explicit LanternPixels(Doc *doc, QObject *parent = nullptr);

    ~LanternPixels() override;

    /** @p count LEDs of @p fx, its channel values @p vals, its look's colour
     *  @p flat at @p level (0..1) -- what fixtureLiveState() says. @p output
     *  says @p vals are what is going OUT: only then is what the device shows
     *  the same look (a previewed look is not on the device). */
    Frame frame(Fixture *fx, const QByteArray &vals, int count,
                const QColor &flat, double level, bool output);

    /** How many LEDs @p fx's device says it drives, 0 = not known. */
    int deviceLeds(Fixture *fx) const;

    /** Only while the view is on screen are devices asked: asking is
     *  network traffic. Off stops every watch. */
    void setActive(bool active);
    bool isActive() const { return m_active; }

signals:
    /** A device said something new: draw again. */
    void changed();

private:
    /** The plugins @p fx's universe is patched out through. */
    QList<QLCIOPlugin *> pluginsFor(Fixture *fx) const;
    void watch(QLCIOPlugin *plugin, quint32 universe);
    void unwatchAll();
    /** Run the port of @p effect on @p count LEDs; false when there is none. */
    bool runScript(Fixture *fx, const QByteArray &vals, const QString &effect,
                   int count, double level, Frame &f);

    Doc *m_doc;
    bool m_active = false;
    QSet<QPair<QLCIOPlugin *, quint32> > m_watched;
    QSet<QLCIOPlugin *> m_connected;

    QHash<QString, QSharedPointer<EffectScript> > m_scripts;   ///< by path; null = would not load
    QHash<quint32, QJSValue> m_states;                         ///< by fixture: its run's state
    QHash<quint32, QString> m_stateScript;                     ///< ...and which script it is for
    QElapsedTimer m_clock;
    QTimer *m_animate = nullptr;    ///< redraws while a script is running
    bool m_scriptRan = false;
};

/** @} */

#endif
