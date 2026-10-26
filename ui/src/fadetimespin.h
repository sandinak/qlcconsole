/*
  Q Light Controller Plus
  fadetimespin.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FADETIMESPIN_H
#define FADETIMESPIN_H

#include <QDoubleSpinBox>

class Scene;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * A time in seconds, typed with either decimal mark: "1.5" and "1,5" are
 * both one and a half seconds, in every language. A plain QDoubleSpinBox
 * takes the locale's group separator as a thousands mark, so in German
 * "1.5" was 15 s, with no warning (L1). A time has no thousands, so both
 * marks mean the decimal point here.
 */
class SecondsSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

public:
    explicit SecondsSpinBox(QWidget *parent = nullptr);

    /** @p text with each '.' and ',' as @p locale's decimal point. */
    static QString withDecimalPoint(const QString &text, const QLocale &locale);

protected:
    QValidator::State validate(QString &input, int &pos) const override;
    double valueFromText(const QString &text) const override;
    void fixup(QString &input) const override;

private:
    /** @p text with its number's '.' and ',' as the decimal point. */
    QString normalised(const QString &text) const;
};

/**
 * The one way a fade or transition time is entered: seconds, two decimals,
 * 0 to 600, " s" -- the scene's fade, a look's fade, a transition's time all
 * the same. Where a time can be left to something else (a look's fade
 * following the scene's), the spin's lowest value is that, named by
 * @p defaultLabel ("scene", "snap"); ms() is then -1.
 */
class FadeTimeSpin final : public SecondsSpinBox
{
    Q_OBJECT

public:
    explicit FadeTimeSpin(QWidget *parent = nullptr, const QString &defaultLabel = QString());

    /** The time in ms; -1 for the default value (only with a default label). */
    int ms() const;
    /** Show @p ms (-1 = the default value), without signalling. */
    void setMs(int ms);

    bool hasDefault() const { return !m_defaultLabel.isEmpty(); }

    /** How a time reads where it is shown but not edited: "1.5 s", or the
     *  default label for -1. */
    static QString text(int ms, const QString &defaultLabel);

signals:
    /** The user changed it (ms as ms()). */
    void msChanged(int ms);

private:
    QString m_defaultLabel;
};

/** Which fades a look in a scene has -- one rule for the canvas and the look
 *  editor. An effect has no fade in (the scene brings it in) but a release
 *  fade out; a colour or dimmer feeding an effect has neither (it is a value
 *  the effect reads, not something painted); anything else has both. */
namespace LookFades
{
    bool hasFadeIn(Doc *doc, const Scene *scene, quint32 paletteId);
    bool hasFadeOut(Doc *doc, const Scene *scene, quint32 paletteId);
    /** What the default (-1) of a look's fade means, for its spin's label. */
    QString defaultLabel(Doc *doc, quint32 paletteId);
}

/** @} */

#endif
