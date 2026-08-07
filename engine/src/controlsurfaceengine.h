/*
  Q Light Controller Plus
  controlsurfaceengine.h

  The device-agnostic core of the control-surface layer. Devices register their
  controls + the Role each control plays + an LED sink; the app supplies a
  state-provider (context-aware LED states) and connects roleActivated to actions.
  refresh() runs the single generic feedback loop that repaints every board's LEDs
  from the current app state — the "LEDs show what's valid right now" behaviour,
  for every device at once. See CONTROL_SURFACE_DESIGN.md.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CONTROLSURFACEENGINE_H
#define CONTROLSURFACEENGINE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <functional>

#include "controlsurface.h"

class ControlSurfaceEngine : public QObject
{
    Q_OBJECT

public:
    explicit ControlSurfaceEngine(QObject *parent = nullptr);

    /** One registered board. `roleOf` maps a control index (into `controls`) to
     *  the Role it plays; `ledSink` pushes a 0..maxBrightness level to a control. */
    struct Device
    {
        QString id;
        QList<ControlSurface::Control> controls;
        QHash<int, ControlSurface::Role> roleOf;
        int maxBrightness = 15;
        std::function<void(const ControlSurface::Control &, int level)> ledSink;
    };

    void registerDevice(const Device &dev);
    void unregisterDevice(const QString &id);
    bool hasDevice(const QString &id) const { return m_devices.contains(id); }

    /** The app tells the engine each Role's current State (drives the LEDs).
     *  Unset → State::Empty. */
    void setStateProvider(std::function<ControlSurface::State(const ControlSurface::Role &)> p)
    { m_stateProvider = std::move(p); }

    /** Recompute + push every LED on every device. Call on any app change that can
     *  alter validity (selection, mode, page, cue, populated items). */
    void refresh();
    /** Refresh a single device (e.g. right after it registers). */
    void refreshDevice(const QString &id);

    /** Route a physical input event to its Role. The overlay calls this with the
     *  control's index into its `controls` list. Emits roleActivated if bound. */
    void inputReceived(const QString &deviceId, int controlIndex, uchar value);

    /** The Role a control plays, or an invalid Role. */
    ControlSurface::Role roleAt(const QString &deviceId, int controlIndex) const;

signals:
    /** A bound control was actuated. The app performs the action (select fixture,
     *  switch page, adjust param, fire cue…). `value` is the raw input value
     *  (button 0/127, encoder relative delta already decoded upstream, fader 0-127). */
    void roleActivated(const QString &deviceId, ControlSurface::Role role, uchar value);

private:
    void pushDeviceLeds(const Device &dev);

    QHash<QString, Device> m_devices;
    std::function<ControlSurface::State(const ControlSurface::Role &)> m_stateProvider;
};

#endif // CONTROLSURFACEENGINE_H
