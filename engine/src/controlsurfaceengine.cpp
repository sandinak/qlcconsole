/*
  Q Light Controller Plus
  controlsurfaceengine.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "controlsurfaceengine.h"

using namespace ControlSurface;

ControlSurfaceEngine::ControlSurfaceEngine(QObject *parent)
    : QObject(parent)
{
}

void ControlSurfaceEngine::registerDevice(const Device &dev)
{
    if (dev.id.isEmpty())
        return;
    m_devices.insert(dev.id, dev);
    pushDeviceLeds(m_devices.value(dev.id));
}

void ControlSurfaceEngine::unregisterDevice(const QString &id)
{
    m_devices.remove(id);
}

ControlSurface::Role ControlSurfaceEngine::roleAt(const QString &deviceId, int controlIndex) const
{
    auto it = m_devices.constFind(deviceId);
    if (it == m_devices.constEnd())
        return Role();
    return it->roleOf.value(controlIndex, Role());
}

void ControlSurfaceEngine::inputReceived(const QString &deviceId, int controlIndex, uchar value)
{
    const Role r = roleAt(deviceId, controlIndex);
    if (r.isValid())
        emit roleActivated(deviceId, r, value);
}

void ControlSurfaceEngine::refresh()
{
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it)
        pushDeviceLeds(it.value());
}

void ControlSurfaceEngine::refreshDevice(const QString &id)
{
    auto it = m_devices.constFind(id);
    if (it != m_devices.constEnd())
        pushDeviceLeds(it.value());
}

void ControlSurfaceEngine::pushDeviceLeds(const Device &dev)
{
    if (!dev.ledSink)
        return;
    for (int i = 0; i < dev.controls.size(); ++i)
    {
        const Control &c = dev.controls.at(i);
        if (!c.hasLed())
            continue;
        const Role r = dev.roleOf.value(i, Role());
        // Unbound control or no provider → State::Empty (dark).
        const State st = (r.isValid() && m_stateProvider) ? m_stateProvider(r)
                                                           : State::Empty;
        dev.ledSink(c, brightness(st, dev.maxBrightness));
    }
}
