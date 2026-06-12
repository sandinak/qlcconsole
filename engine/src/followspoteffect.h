/*
  Q Light Controller Plus
  followspoteffect.h

  A DMXSource that reads two joystick axes (from any InputOutputMap input)
  and continuously writes Pan/Tilt DMX values to the target fixtures.
  The operator picks fixtures via the programmer selection; this effect
  then steers all of them in sync as if they are a single followspot.

  Input binding:
    - "Bind X" captures the next input event as the pan (horizontal) axis.
    - "Bind Y" captures the next input event as the tilt (vertical) axis.
  Input values (0–255) are mapped 0→0°, 255→focusPanMax (or 360 default).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef FOLLOWSPOTEFFECT_H
#define FOLLOWSPOTEFFECT_H

#include <QMutex>
#include <QObject>
#include <QList>
#include <QHash>
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
     * Input bindings
     *********************************************************************/

    /** Start capturing the next input event as the X (pan) axis.
     *  Once captured, isXBound() becomes true. */
    void startBindX();
    /** Start capturing the next input event as the Y (tilt) axis. */
    void startBindY();
    /** Clear both bindings. */
    void clearBindings();

    bool isXBound() const;
    bool isYBound() const;
    bool isCapturingX() const { return m_capturingX; }
    bool isCapturingY() const { return m_capturingY; }

    /** Human-readable description of the current X/Y binding, e.g.
     *  "Universe 1, Ch 1" or "—" if unbound. */
    QString xBindingLabel() const;
    QString yBindingLabel() const;

    /*********************************************************************
     * Settings
     *********************************************************************/
    float sensitivity() const { return m_sensitivity; }
    void setSensitivity(float s);

    float deadzone() const { return m_deadzone; }
    void setDeadzone(float d);

    /*********************************************************************
     * Enable / disable
     *********************************************************************/
    void setActive(bool active);
    bool isActive() const { return m_active; }

    /*********************************************************************
     * DMXSource
     *********************************************************************/
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

signals:
    void activeChanged(bool active);
    /** Emitted when a binding is captured (X or Y settled). */
    void bindingChanged();

private slots:
    void slotInputValueChanged(quint32 universe, quint32 channel,
                               uchar value, const QString& key);

private:
    void connectInput();
    void disconnectInput();

    Doc *m_doc;
    QList<quint32> m_fixtures;

    quint32 m_xUniverse = 0, m_xChannel = 0;
    quint32 m_yUniverse = 0, m_yChannel = 0;
    bool m_xBound = false;
    bool m_yBound = false;
    bool m_capturingX = false;
    bool m_capturingY = false;
    bool m_inputConnected = false;

    float m_xNorm = 0.5f;   // 0.0–1.0, updated from joystick events
    float m_yNorm = 0.5f;

    float m_sensitivity = 1.0f;  // multiplied against mapped degrees
    float m_deadzone    = 0.05f; // fraction of half-range treated as center

    bool m_active      = false;
    bool m_registered  = false;

    QMutex m_mutex;
};

#endif // FOLLOWSPOTEFFECT_H
