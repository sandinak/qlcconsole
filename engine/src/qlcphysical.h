/*
  Q Light Controller Plus
  qlcphysical.h

  Copyright (C) Heikki Junnila
                Massimo Callegari

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

#ifndef QLCPHYSICAL_H
#define QLCPHYSICAL_H

#include <QString>
#include <QSize>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCPhysical QStringLiteral("Physical")

#define KXMLQLCPhysicalBulb         QStringLiteral("Bulb")
#define KXMLQLCPhysicalBulbType     QStringLiteral("Type")
#define KXMLQLCPhysicalBulbLumens   QStringLiteral("Lumens")
#define KXMLQLCPhysicalBulbColourTemperature QStringLiteral("ColourTemperature")

#define KXMLQLCPhysicalLens             QStringLiteral("Lens")
#define KXMLQLCPhysicalLensName         QStringLiteral("Name")
#define KXMLQLCPhysicalLensDegreesMin   QStringLiteral("DegreesMin")
#define KXMLQLCPhysicalLensDegreesMax   QStringLiteral("DegreesMax")
#define KXMLQLCPhysicalLensBeamsPerHead QStringLiteral("BeamsPerHead")

#define KXMLQLCPhysicalDimensions       QStringLiteral("Dimensions")
#define KXMLQLCPhysicalDimensionsWeight QStringLiteral("Weight")
#define KXMLQLCPhysicalDimensionsWidth  QStringLiteral("Width")
#define KXMLQLCPhysicalDimensionsHeight QStringLiteral("Height")
#define KXMLQLCPhysicalDimensionsDepth  QStringLiteral("Depth")

#define KXMLQLCPhysicalLayout QStringLiteral("Layout")

/* qlcconsole extension -- QLC+ skips an unknown Physical tag, so a file
   carrying it still opens there. */
#define KXMLQLCPhysicalBody         QStringLiteral("Body")
#define KXMLQLCPhysicalBodyShape    QStringLiteral("Shape")
#define KXMLQLCPhysicalBodyEmission QStringLiteral("Emission")
#define KXMLQLCPhysicalBodyLEDs     QStringLiteral("LEDs")
#define KXMLQLCPhysicalBodyLayout   QStringLiteral("Layout")
#define KXMLQLCPhysicalBodyGlass    QStringLiteral("Glass")

#define KXMLQLCPhysicalFocus        QStringLiteral("Focus")
#define KXMLQLCPhysicalFocusType    QStringLiteral("Type")
#define KXMLQLCPhysicalFocusPanMax  QStringLiteral("PanMax")
#define KXMLQLCPhysicalFocusTiltMax QStringLiteral("TiltMax")

#define KXMLQLCPhysicalTechnical                    QStringLiteral("Technical")
#define KXMLQLCPhysicalTechnicalPowerConsumption    QStringLiteral("PowerConsumption")
#define KXMLQLCPhysicalTechnicalDmxConnector        QStringLiteral("DmxConnector")


/**
 * QLCPhysical represents the physical properties of a fixture (in a certain
 * mode). These properties include weight, dimensions, light source, lens,
 * movement capabilities and beam width.
 */
class QLCPhysical final
{
    /************************************************************************
     * Initialization
     ************************************************************************/
public:
    QLCPhysical();
    QLCPhysical(const QLCPhysical &other);

    virtual ~QLCPhysical();

    QLCPhysical& operator=(const QLCPhysical& physical);

    bool isEmpty() const;

    /************************************************************************
     * Properties
     ************************************************************************/
public:
    void setBulbType(const QString& type);
    QString bulbType() const;

    void setBulbLumens(int lumens);
    int bulbLumens() const;

    void setBulbColourTemperature(int temp);
    int bulbColourTemperature() const;

    void setWeight(double weight);
    double weight() const;

    void setWidth(int width);
    int width() const;

    void setHeight(int height);
    int height() const;

    void setDepth(int depth);
    int depth() const;

    void setLensName(const QString& name);
    QString lensName() const;

    void setLensDegreesMin(double degrees);
    double lensDegreesMin() const;

    void setLensDegreesMax(double degrees);
    double lensDegreesMax() const;

    /** How many separate beams each head throws through this lens. A head is
     *  a set of channels controlled together, so a fixture whose arm carries
     *  three emitters sharing one colour is ONE head with three beams -- a
     *  fact the channels cannot express. Visual only; 1 (the default) is
     *  never written, so existing definitions stay byte-identical and
     *  applications that do not know the attribute simply ignore it. */
    void setLensBeamsPerHead(int beams);
    int lensBeamsPerHead() const;

    void setFocusType(const QString& type);
    QString focusType() const;

    void setFocusPanMax(int pan);
    int focusPanMax() const;

    void setFocusTiltMax(int tilt);
    int focusTiltMax() const;

    void setLayoutSize(QSize size);
    QSize layoutSize() const;

    void setPowerConsumption(int watt);
    int powerConsumption() const;

    /** True when the definition says how much light it puts out: declared
     *  lumens, or a power consumption (declared, or guessed from the bulb's
     *  watts). The rig view draws a fixture that says neither at a guess. */
    bool declaresOutput() const { return bulbLumens() > 0 || powerConsumption() > 0; }

    void setDmxConnector(const QString& type);
    QString dmxConnector() const;

    /** What the fixture looks like, where a body is drawn: a box sized by
     *  its dimensions (the default), or a lantern -- a cage around a glowing
     *  core, hung by its top. */
    enum BodyShape { BoxBody = 0, LanternBody };
    void setBodyShape(BodyShape shape) { m_bodyShape = shape; }
    BodyShape bodyShape() const { return m_bodyShape; }

    /** How its light leaves it: a beam (a cone, the lens's angle) or all
     *  round (a bare source -- a lantern, a bulb -- lighting what is near
     *  it, falling off with distance). */
    enum Emission { BeamEmission = 0, OmniEmission };
    void setEmission(Emission emission) { m_emission = emission; }
    Emission emission() const { return m_emission; }

    /** What makes the light: one point (0, the default) or that many LEDs --
     *  a lantern's sixteen on a wire, whose effects run along them. */
    void setLeds(int count) { m_leds = qMax(0, count); }
    int leds() const { return m_leds; }

    /** How the LEDs sit in the body, the first at the top: a hanging cluster
     *  (a wire string spread through the cage) or LED tape (wound tight
     *  round a core in the middle). */
    enum LedLayout { ClusterLeds = 0, TapeLeds };
    void setLedLayout(LedLayout layout) { m_ledLayout = layout; }
    LedLayout ledLayout() const { return m_ledLayout; }
    static QString ledLayoutToString(LedLayout layout);
    static LedLayout stringToLedLayout(const QString &str);

    /** What the light is seen through: diffusing glass (the default -- it
     *  glows, the LEDs a blur behind it) or clear (each LED seen). */
    void setClearGlass(bool clear) { m_clearGlass = clear; }
    bool clearGlass() const { return m_clearGlass; }

protected:
    QString m_bulbType;
    int m_bulbLumens;
    int m_bulbColourTemperature;

    double m_weight;
    int m_width;
    int m_height;
    int m_depth;

    QString m_lensName;
    double m_lensDegreesMin;
    double m_lensDegreesMax;
    int m_lensBeamsPerHead;

    QString m_focusType;
    int m_focusPanMax;
    int m_focusTiltMax;
    QSize m_layout;

    int m_powerConsumption;
    QString m_dmxConnector;

    BodyShape m_bodyShape = BoxBody;
    Emission m_emission = BeamEmission;
    int m_leds = 0;
    LedLayout m_ledLayout = ClusterLeds;
    bool m_clearGlass = false;

    /************************************************************************
     * Load & Save
     ************************************************************************/
public:
    /** Load physical values from the given QXmlStreamReader */
    bool loadXML(QXmlStreamReader &doc);

    /** Save physical values to the given XML tag in the given document */
    bool saveXML(QXmlStreamWriter *doc) const;
};

/** @} */

#endif
