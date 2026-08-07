/*
  Q Light Controller Plus - Unit test
  controlsurface_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CONTROLSURFACE_TEST_H
#define CONTROLSURFACE_TEST_H

#include <QObject>

class ControlSurface_Test : public QObject
{
    Q_OBJECT

private slots:
    /** Context-aware LED states map to the right brightness per control. */
    void ledStatesReflectContext();
    /** Unbound / no-LED controls stay dark. */
    void unboundAndFaderStayDark();
    /** A bound input event fires roleActivated with the control's Role. */
    void inputRoutesToRole();
    /** Two devices share one role vocabulary + provider (generic across boards). */
    void twoDevicesShareRoles();
};

#endif // CONTROLSURFACE_TEST_H
