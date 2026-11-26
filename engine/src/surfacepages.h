/*
  Q Light Controller Plus
  surfacepages.h

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

#ifndef SURFACEPAGES_H
#define SURFACEPAGES_H

#include <QObject>
#include <QHash>
#include <QMap>
#include <QSet>

#include "surfacelayout.h"
#include "pageset.h"
#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCSurfacePageSet QStringLiteral("PageSet")

/**
 * The surface pages of every patched board with a layout and a page set
 * (docs/design/SURFACE_PAGES.md, slice 1): the page on now, its keys and
 * their lamps, and what a control on it does.
 *
 * Design only (Q332 a): in Design a page's controls are the page's (no show
 * listener hears them: InputOutputMap::setPageControls()); a fader sets
 * its attribute (levelSet(): the consumer picks it up first), a knob or an
 * encoder turns it (adjusted(), EncoderBank's own signal shape). In Operate
 * the controls go back to the show's own assignments and a page does
 * nothing. The one consumer is the Workbench (workbenchencoders.cpp), which
 * edits through EncoderAttributes as the encoder bank does.
 *
 * Which set a board uses is this computer's (Q333 b), by the device's
 * identity (DeskRecord's); a show may name the set it was built with and
 * offers it when opened here with another (offeredSet()).
 *
 * A board whose own overlay reads its faders (the PMJ: setOverlaid()) keeps
 * hearing them; its rules ("look-level", "group-intensity") are the
 * overlay's, and its page keys light through the overlay. GUI thread.
 */
class QLC_ENGINE_EXPORT SurfacePages final : public QObject
{
    Q_OBJECT

public:
    explicit SurfacePages(Doc *doc);

    /** Every universe with a board that has a page set now. */
    QList<quint32> universes() const;
    /** @p universe's set (invalid: none). */
    PageSet pageSet(quint32 universe) const;
    /** The page on now (0 when none). */
    int page(quint32 universe) const;
    /** "Move"; empty when the board has no pages. */
    QString pageName(quint32 universe) const;
    /** A page with no jobs yet (the PMJ's Effects, Macros). */
    bool pageIsEmpty(quint32 universe) const;
    /** "Move", or "Effects: coming" for a page with no jobs yet. */
    QString pageText(quint32 universe) const;
    /** "APC40 · Move"; empty when none. */
    QString chipText(quint32 universe) const;
    void setPage(quint32 universe, int page);
    /** Key @p key of @p universe's board pressed: its page (again: the
     *  key's next page), or the next / previous. A board's overlay (the
     *  PMJ's mode keys) calls it; others are heard here. False: not a key. */
    bool pressKey(quint32 universe, quint32 key);
    /** The next (+1) or previous (-1) page, round the set. */
    void stepPage(quint32 universe, int direction);

    /** The labels of the page on now (each control's own channel). */
    QHash<quint32, QString> labels(quint32 universe) const;
    /** The job of the control that hears @p channel on the page now; null:
     *  none. */
    const PageSet::Binding *bindingFor(quint32 universe, quint32 channel) const;
    /** Whether the page has the control that hears @p channel now (Design,
     *  its job or one of its keys): the map draws it as the page's. */
    bool owns(quint32 universe, quint32 channel) const;
    /** The board's pages act now: Design (or Operate with Edit Live on),
     *  and the show's encoder bank is not on this board. */
    bool isActive(quint32 universe) const;

    /** Edit Live (docs/features/edit-live.md): in Operate the pages' knobs
     *  and faders trim the running stage (liveAdjusted(), liveLevelSet())
     *  instead of going back to the show. Every knob's last position is
     *  forgotten, so the first move after a switch reads as no step. */
    void setLive(bool live);
    bool isLive() const { return m_live; }

    /*********************************************************************
     * The set, per computer
     *********************************************************************/
    /** QSettings group of this computer's choices. */
    static QString settingsGroup();
    /** The set @p universe's board uses on this computer. */
    QString chosenSet(quint32 universe) const;
    /** Use set @p name on @p universe's board here; false: no such set. */
    bool chooseSet(quint32 universe, const QString &name);

    /** A board whose own overlay hears its controls (the PMJ). */
    void setOverlaid(const QString &profileName);
    bool isOverlaid(quint32 universe) const;

    /*********************************************************************
     * The show's sets (Q333 b)
     *********************************************************************/
    /** The set this show was built with on @p universe's board, when this
     *  computer uses another and has that one; empty otherwise. */
    QString offeredSet(quint32 universe) const;
    /** Use the show's set on @p universe here. */
    void useShowSet(quint32 universe);
    /** Not now: the offer goes for this session. */
    void declineShowSet(quint32 universe);
    /** The set the show names for @p profileName; empty: none. */
    QString showSet(const QString &profileName) const { return m_showSets.value(profileName); }
    /** Whether saveXML() writes anything. */
    bool hasShowSets() const;

    bool saveXML(QXmlStreamWriter *doc) const;
    /** One PageSet element (the Control Map reads them). */
    void loadXML(QXmlStreamReader &root);
    /** A new show: no show sets. */
    void reset();

public slots:
    /** One input value (InputOutputMap::inputValueChanged). Public for the
     *  tests. */
    void handleInput(quint32 universe, quint32 channel, uchar value);
    /** The boards again (a patch, a profile, a load, the mode). */
    void rescan();

signals:
    /** @p universe's page changed (the APC64's screen line 3 shows it). */
    void pageChanged(quint32 universe, const QString &name);
    /** A turning control on a page: EncoderBank::adjusted()'s shape. */
    void adjusted(int bank, int slot, int detents, bool fine);
    /** A fader on a page moved to @p value (0-255): the consumer picks it
     *  up (FaderPickup) and sets the attribute. */
    void levelSet(quint32 universe, quint32 channel, int bank, int slot, uchar value);
    /** A board, its set or the show's offer changed. */
    void setsChanged();
    /** Edit Live: a turning control on a page, in Operate. */
    void liveAdjusted(int bank, int slot, int detents, bool fine);
    /** Edit Live: a fader on a page moved, in Operate. */
    void liveLevelSet(quint32 universe, quint32 channel, int bank, int slot, uchar value);

private:
    struct Board
    {
        QString profile;
        QString identity;
        SurfaceLayout layout;
        PageSet set;
        int page = 0;
        QHash<quint32, int> last;       //!< a knob's last position (MIDI 0-127)
        bool lampsOurs = false;         //!< the page keys' lights were sent by the page
    };
    static constexpr int kNotAKnob = 0x7FFFFFFF;
    /** A learn listens (the desk's, the Control Map's): the pages rest. */
    bool standsDown() const;
    Board *board(quint32 universe);
    const Board *board(quint32 universe) const;
    QString settingsKey(const Board &b) const;
    PageSet setFor(const Board &b) const;
    void applyStatic();
    void sendLamps(quint32 universe);
    /** A turn of @p channel in detents: a relative control's decoded, a
     *  knob's @p knobDelta (kNotAKnob: not a position knob). */
    int turnDetents(Board &b, quint32 channel, uchar value, int knobDelta);
    bool encoderBankOn(quint32 universe) const;

private:
    Doc *m_doc;
    QMap<quint32, Board> m_boards;
    QSet<QString> m_overlaid;
    QSet<quint64> m_down;
    QHash<QString, QString> m_showSets;     //!< profile -> set name
    QSet<quint32> m_declined;
    bool m_live = false;
};

/** @} */

#endif
