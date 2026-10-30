/*
  Q Light Controller Plus
  vceffectcontrol.h

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

#ifndef VCEFFECTCONTROL_H
#define VCEFFECTCONTROL_H

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QHash>
#include <QList>

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup ui_vc_widgets
 * @{
 */

#define KXMLQLCVCEffectControl          QStringLiteral("EffectControl")
#define KXMLQLCVCEffectControlTarget    QStringLiteral("Target")
#define KXMLQLCVCEffectControlPalette   QStringLiteral("Palette")
#define KXMLQLCVCEffectControlGroup     QStringLiteral("Group")
#define KXMLQLCVCEffectControlRole      QStringLiteral("Role")

/**
 * A VC control on an effect's parameter (docs/design/VC_EFFECT_CONTROLS.md):
 * which effect (a palette, the one on a group, or the current one) and which
 * parameter by role, mapped to each effect's own names. A move is a live
 * override of the running effect, gone when it stops; store() keeps it.
 */
class VCEffectControl
{
public:
    enum Role { Speed = 0, Size, Rate, Spread, Direction, Intensity, Hue, RoleCount };
    enum Target { TargetPalette = 0, TargetGroup, TargetCurrent };

    static QString roleToString(Role role);         //!< XML
    static Role stringToRole(const QString &str);
    static QString roleName(Role role);             //!< for people
    static QString targetToString(Target target);
    static Target stringToTarget(const QString &str);

    Target target = TargetCurrent;
    quint32 palette = 0xFFFFFFFF;   //!< TargetPalette: the effect palette
    quint32 group = 0xFFFFFFFF;     //!< TargetGroup, or TargetCurrent's scope (invalid: all)
    Role role = Speed;

    /** What the control reaches now. */
    struct Resolved
    {
        enum Kind { None, Palette, EFX, RGBMatrix };
        Kind kind = None;
        quint32 id = 0xFFFFFFFF;    //!< palette or function id
        QString name;               //!< the effect's
        bool playing = false;
        bool available = false;     //!< it has the role
        QString param;              //!< a script param or a host key
        double min = 0, max = 1;
        QStringList values;         //!< a list param's choices
    };
    Resolved resolve(Doc *doc) const;

    /** Where the slider sits (0..255) for the value now. */
    static int sliderValue(Doc *doc, const Resolved &r, Role role);
    /** "1.50", "Clockwise", "×2.00", "n/a", "not playing". */
    static QString valueText(Doc *doc, const Resolved &r, Role role);
    /** Set it from a slider position: a live override. False: nothing to set. */
    static bool apply(Doc *doc, const Resolved &r, Role role, int value);
    /** Keep the overrides on @p r's effect (into the palette, or the
     *  function as it is now). False: nothing overridden. */
    static bool store(Doc *doc, const Resolved &r);
    /** Whether @p r's effect has a live override now. */
    static bool isOverridden(Doc *doc, const Resolved &r);

    bool loadXML(QXmlStreamReader &root);
    void saveXML(QXmlStreamWriter *doc) const;

    bool operator==(const VCEffectControl &o) const
    {
        return target == o.target && palette == o.palette && group == o.group && role == o.role;
    }
};

/**
 * Which effects play and in what order they were started or selected, for
 * "the current effect" (one per Doc). Event-driven: the effect runner's and
 * the master timer's start/stop signals and Build's selected palette.
 */
class VCEffectTracker final : public QObject
{
    Q_OBJECT

public:
    static VCEffectTracker *instance(Doc *doc);

    struct Effect
    {
        VCEffectControl::Resolved::Kind kind = VCEffectControl::Resolved::None;
        quint32 id = 0xFFFFFFFF;
        bool operator==(const Effect &o) const { return kind == o.kind && id == o.id; }
        bool isValid() const { return kind != VCEffectControl::Resolved::None; }
    };

    /** The most recently started (and, @p selected, selected) effect that
     *  plays, on a fixture of @p group (invalid: any). */
    Effect current(quint32 group = 0xFFFFFFFF, bool selected = true) const;
    bool isPlaying(const Effect &e) const;
    /** The fixtures it plays on. */
    QList<quint32> fixturesOf(const Effect &e) const;

    /** Function overrides (EFX, RGB Matrix): its value before the first
     *  change, put back when it stops. */
    void rememberOriginal(quint32 fid, int role, const QVariant &value);
    bool hasOriginal(quint32 fid, int role) const;
    QVariant original(quint32 fid, int role) const;
    void forgetOriginals(quint32 fid);
    /** Its intensity override (Function::requestAttributeOverride()). */
    int intensityOverride(quint32 fid) const { return m_intensity.value(fid, -1); }
    void setIntensityOverride(quint32 fid, int attrId) { m_intensity.insert(fid, attrId); }

public slots:
    /** An effect palette shown in Build (selected). */
    void slotPaletteSelected(quint32 paletteId);

signals:
    /** What plays, or the order, changed: re-resolve. */
    void changed();

private slots:
    void slotEffectStarted(quint32 paletteId);
    void slotEffectStopped(quint32 paletteId);
    void slotFunctionStarted(quint32 fid);
    void slotFunctionStopped(quint32 fid);
    void slotDocCleared();

private:
    explicit VCEffectTracker(Doc *doc);
    void bringToFront(const Effect &e, bool started);
    void restoreFunction(quint32 fid);

    Doc *m_doc;
    QList<Effect> m_recent;                         //!< started or selected, newest first
    QList<Effect> m_started;                        //!< started, newest first
    QHash<quint32, QHash<int, QVariant>> m_originals;
    QHash<quint32, int> m_intensity;
};

/** @} */

#endif
