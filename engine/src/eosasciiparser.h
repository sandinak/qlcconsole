/*
  Q Light Controller
  eosasciiparser.h

  Copyright (C) Branson Matheson

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

#ifndef EOSASCIIPARSER_H
#define EOSASCIIPARSER_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QMap>

/** @addtogroup engine Engine
 * @{
 */

/** One parameter level as an Eos ASCII file writes it: a hard level, a
 *  reference to a palette / preset, or a release. */
class EosLevel
{
public:
    enum Kind
    {
        Value,          //!< @a value is the level (0-255, or 0-65535 for a 16 bit parameter)
        PaletteRef,     //!< IPn / FPn / CPn / BPn: @a refKind 'I' 'F' 'C' 'B', @a ref the number
        PresetRef,      //!< PRn
        Release,        //!< nulled / released: back to where it would be with nothing
        Unsupported     //!< a cue reference (Qn) or something unknown
    };

    Kind kind = Value;
    int value = 0;
    QChar refKind;
    QString ref;
    QString raw;        //!< as written, for warnings
};

/** Levels by channel, then by Eos parameter type (1 = intensity). */
typedef QMap<int, QMap<int, EosLevel>> EosLevels;

/** $ParamType: one Eos parameter (Intens, Red, Pan...). */
class EosParamType
{
public:
    int id = 0;
    int category = 0;   //!< 1 Intensity, 2 Focus, 3 Color, 4 Image, 5 Form, 6 Shutter, 7 Control
    QString name;
};

/** $$PersChan: where a parameter sits in a personality's footprint. */
class EosPersChannel
{
public:
    int param = 0;      //!< EosParamType::id
    int size = 1;       //!< 1 = 8 bit, 2 = 16 bit
    int offset = 0;     //!< 1-based DMX offset (MSB when 16 bit)
    int lsbOffset = 0;  //!< 1-based offset of the LSB (16 bit only)
    int home = 0;
};

/** $Personality: an Eos fixture type (manufacturer, model including its
 *  mode, footprint, channel layout). */
class EosPersonality
{
public:
    int id = 0;
    QString manufacturer;
    QString model;
    QString dcid;
    int footprint = 0;
    QList<EosPersChannel> channels;

    /** The channel carrying parameter @a param (null: none). */
    const EosPersChannel *channel(int param) const;
};

/** $Patch (or a conventional Patch): one Eos channel. */
class EosPatch
{
public:
    int channel = 0;
    int personality = 0;    //!< 0: a conventional dimmer (no personality)
    QString personalityName;
    int address = 0;        //!< 1-based absolute DMX address (513 = universe 2, 1); 0 unpatched
    int part = 1;
    QString label;
};

/** $Group / Group: a channel list. */
class EosGroup
{
public:
    QString number;
    QString label;
    QList<int> channels;
};

/** One cue (parts already flattened into it). */
class EosCue
{
public:
    QString number;         //!< "1", "0.5", "83.3"
    int list = 1;
    QString label;
    double up = -1;         //!< seconds; -1: not given
    double down = -1;
    double upDelay = 0;
    double downDelay = 0;
    double follow = -1;     //!< auto-follow, from this cue's start
    double hang = -1;       //!< auto-follow, from this cue's completion
    QString link;           //!< a cue (or list/cue) this one links to
    int parts = 1;          //!< parts flattened in
    bool block = false;
    EosLevels moves;        //!< $$Param + $$ChanMove: what this cue changes
    QMap<int, int> tracked; //!< Chan: the tracked intensity state (0-255)
    bool hasTracked = false;
};

class EosCueList
{
public:
    int number = 1;
    QString label;
    QList<EosCue> cues;     //!< in cue-number order
};

/** IP / FP / CP / BP palettes and presets. */
class EosPalette
{
public:
    QChar kind;             //!< 'I' 'F' 'C' 'B', or 'P' for a preset
    QString number;
    QString label;
    EosLevels levels;
};

/** Sub (submaster) records. */
class EosSub
{
public:
    QString number;
    QString label;
    double up = -1;
    double down = -1;
    EosLevels levels;
};

/** Everything an ASCII file said, as plain data. */
class EosShowData
{
public:
    QString console;        //!< "Eos"
    QString manufacturer;   //!< "ETC"
    QString format;         //!< $$Format, "3.20"
    QString software;
    QString title;

    QMap<int, EosParamType> paramTypes;
    QMap<int, EosPersonality> personalities;
    QList<EosPatch> patch;
    QList<EosGroup> groups;
    QMap<int, EosCueList> cueLists;
    QList<EosPalette> palettes;     //!< IP/FP/CP/BP
    QList<EosPalette> presets;
    QList<EosSub> subs;

    /** Records read but not imported, by keyword ("$Effect" -> 18). */
    QMap<QString, int> skipped;
    QStringList warnings;

    /** A parameter's short name ("Intens", "Red"); "Param N" if unknown. */
    QString paramName(int id) const;
    /** IP/FP/CP/BP (@a kind 'I' 'F' 'C' 'B') or preset ('P') @a number. */
    const EosPalette *findPalette(QChar kind, const QString &number) const;
    const EosPatch *findPatch(int channel) const;
    int cueCount() const;
};

/**
 * Reader for the USITT ASCII Text Representation of Lighting Console Data
 * (1992), ETC Eos dialect (File > Export > USITT ASCII, $$Format 3.20).
 * Pure parsing: the result is plain data; EosImporter turns it into a Doc.
 *
 * Records read (case-insensitive; an Eos export's own comments list them):
 * Ident, Manufacturer, Console, $$Format, $$Software, $$Title, Clear, Set,
 * EndData; $ParamType, $Personality ($$Manuf $$Model $$Dcid $$Footprint
 * $$PersChan), $Patch ($$Pers, Text), Patch (conventional), $Group / Group,
 * $CueList, Cue / $Cue (Text, Up, Down, $$TimeUp, $$TimeDown, FollowOn,
 * $$Follow, $$Hang, Link, $$Link, Part, $$Block, $$ChanMove, Chan, $$Param),
 * $IntensPalette / $FocusPalette / $ColorPalette / $BeamPalette, $Preset,
 * Sub / $Sub.
 *
 * Anything else -- $Effect, $Curve, $FaderDef, $MagicSheet, $Macro, unknown
 * $-records -- is skipped and counted, never fatal: the format is 34 years
 * old and ETC keeps extending it. A cue's parts are flattened into the cue,
 * with a warning.
 *
 * See EOS_IMPORT_DESIGN.md § "USITT ASCII — what to parse".
 */
class EosAsciiParser final
{
public:
    EosAsciiParser();
    ~EosAsciiParser();

    /** Parse the file at @p path into @p data. False only when the file
     *  can't be read or isn't USITT ASCII at all (no record of any kind);
     *  the reason is in @p data.warnings. */
    bool parse(const QString &path, EosShowData &data);

    /** The same, from text already read. */
    bool parseText(const QString &text, EosShowData &data);

    /** "1:30" / "4.5" / "5" -> seconds; -1 when it isn't a time. */
    static double parseTime(const QString &text);
    /** One level ("H33", "FL", "50", "CP3", "r0", "b127"...). @p percent: a
     *  bare number is a percentage (Chan / $$ChanMove), scaled to 0-255;
     *  else it is a raw level ($$Param). */
    static EosLevel parseLevel(const QString &text, bool percent);
    /** "1 Thru 5 8 10>12" -> 1..5, 8, 10..12. */
    static QList<int> parseChannelList(const QStringList &tokens);
};

/** @} */

#endif
