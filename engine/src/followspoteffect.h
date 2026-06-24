/*
  Q Light Controller Plus
  followspoteffect.h

  A DMXSource that steers fixtures based on pan/tilt normalized axis values
  (0.0–1.0). Input binding and routing is handled by ProgrammerController;
  this class is a pure "write pan/tilt DMX" engine.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef FOLLOWSPOTEFFECT_H
#define FOLLOWSPOTEFFECT_H

#include <QMutex>
#include <QObject>
#include <QList>
#include <QString>

#include "dmxsource.h"

class Doc;

class FollowSpotEffect final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    explicit FollowSpotEffect(Doc *doc);
    ~FollowSpotEffect() override;

    /*********************************************************************
     * Target fixtures
     *********************************************************************/
    void setFixtures(const QList<quint32>& fixtureIds);

    /*********************************************************************
     * Axis values (set by ProgrammerController from controller input)
     *********************************************************************/
    void setXNorm(float v);  // pan  0.0–1.0
    void setYNorm(float v);  // tilt 0.0–1.0

    /*********************************************************************
     * Settings
     *********************************************************************/
    float sensitivity() const { return m_sensitivity; }
    void setSensitivity(float s);

    float deadzone() const { return m_deadzone; }
    void setDeadzone(float d);

    /** Aim height in centimetres — tilt bias so beam hits head not feet. */
    int aimHeight() const { return m_aimHeight; }
    void setAimHeight(int cm);

    /*********************************************************************
     * Enable / disable
     *********************************************************************/
    void setActive(bool active);
    bool isActive() const { return m_active; }

    /*********************************************************************
     * Setpoint access (for save / commit after design-mode positioning)
     *********************************************************************/
    float panSetpoint()   const { return m_panSetpoint;   }
    float tiltSetpoint()  const { return m_tiltSetpoint;  }
    bool  setpointValid() const { return m_setpointValid; }
    /** Reset so the scene palette controls the fixture until the next
     *  stick movement.  Call after committing a saved position. */
    void  resetSetpoint();

    /*********************************************************************
     * DMXSource
     *********************************************************************/
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

signals:
    void activeChanged(bool active);

private:
    Doc *m_doc;
    QList<quint32> m_fixtures;

    float m_xNorm = 0.5f;
    float m_yNorm = 0.5f;

    // Setpoint/velocity mode: stick deflection moves position, release holds it.
    // m_setpointValid is false until the first non-zero stick input, so the scene
    // controls the fixture until the operator actually moves the stick.
    float m_panSetpoint  = 0.5f;
    float m_tiltSetpoint = 0.5f;
    bool  m_setpointValid = false;

    float m_sensitivity = 1.0f;
    float m_deadzone    = 0.05f;
    int   m_aimHeight   = 170;

    bool m_active     = false;
    bool m_registered = false;

    QMutex m_mutex;
};

#endif // FOLLOWSPOTEFFECT_H
