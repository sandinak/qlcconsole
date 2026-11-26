/*
  Q Light Controller Plus
  pageset.h

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

#ifndef PAGESET_H
#define PAGESET_H

#include <QStringList>
#include <QString>
#include <QList>
#include <QHash>
#include <QDir>

#include "qlcengine_export.h"

class QLCInputProfile;
class SurfaceLayout;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A page set (docs/design/SURFACE_PAGES.md, Q331 a): labelled pages of
 * operating jobs for one surface type, in its own small file
 * (<name>.pageset.json) that names the layout it fits. A page maps layout
 * controls to an attribute of the selection ("pan", "red": EncoderBank's
 * slots by name) or, on the PMJ, to one of its two built-in numbered rules
 * (Q335 a). "always" controls are on every page (colour on the faders while
 * the knobs page).
 *
 * Whether a control sets or turns comes from the layout (a fader sets with
 * pick-up, a knob or an encoder turns), never from the file. The checker
 * (check()) holds a set to its layout: every control placed, of a kind that
 * can do the job, every attribute known, and never a page job on the desk's
 * GM, STOP, GO or Back.
 *
 * Built-in sets ship as resources next to the layouts and are read-only;
 * a user's sets (imported) live in the user profile folder's PageSets.
 */
class QLC_ENGINE_EXPORT PageSet
{
public:
    /** A control's job on a page. */
    struct Binding
    {
        quint32 channel = 0xFFFFFFFF;
        /** An attribute ("pan"), a rule ("look-level", "group-intensity"),
         *  or "none". */
        QString param;
        /** Turned by the fine step (a turning control only). */
        bool fine = false;
        /** The rule's number (fader N): 1-based; 0 for an attribute. */
        int index = 0;
        /** The label on the map; empty: from the parameter ("Pan fine"). */
        QString label;

        bool isRule() const;
        /** "Pan fine", "Red", "Level 3" */
        QString text() const;
    };

    struct Page
    {
        QString name;
        /** The key that brings it (pressed again: the next page of the same
         *  key); invalid when it has none. */
        quint32 key = 0xFFFFFFFF;
        QList<Binding> controls;
    };

    PageSet() {}

    bool isValid() const { return !m_profile.isEmpty() && !m_name.isEmpty() && !m_pages.isEmpty(); }
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    /** The input profile it fits (QLCInputProfile::name()). */
    QString profileName() const { return m_profile; }
    /** The layout file format it was made for. */
    int layoutFormat() const { return m_layoutFormat; }
    /** "PMJ Black 1 · Pages (built in)" for a person. */
    QString description() const { return m_description; }

    /** A built-in (read-only) set. */
    bool isBuiltIn() const { return m_builtIn; }
    /** Where it was read from (a resource path for a built-in). */
    QString filePath() const { return m_file; }

    const QList<Page> &pages() const { return m_pages; }
    const QList<Binding> &always() const { return m_always; }
    quint32 nextKey() const { return m_next; }
    quint32 previousKey() const { return m_previous; }
    /** A turning control that steps the pages (the APC64's encoder). */
    quint32 stepper() const { return m_stepper; }
    /** The board's encoders turn the other way (the PMJ's). */
    bool invertTurns() const { return m_invert; }

    /** The binding of @p channel (a control's own channel) on page
     *  @p page, or in "always"; null: none. */
    const Binding *binding(int page, quint32 channel) const;
    /** Every key the set listens to (page keys, next, previous, stepper). */
    QList<quint32> keys() const;
    /** Every control a page or "always" uses on page @p page, keys too. */
    QList<quint32> controlsOn(int page) const;
    /** Labels of page @p page (its controls, "always" and the keys). */
    QHash<quint32, QString> labels(int page) const;
    /** The page key @p key brings from page @p from; -1: not a key. */
    int pageForKey(quint32 key, int from) const;

    /*********************************************************************
     * The vocabulary
     *********************************************************************/
    /** Every attribute and rule a binding may name. */
    static QStringList params();
    /** The rules (Q335 a: the PMJ's). */
    static bool isRule(const QString &param);
    /** EncoderBank's bank and slot of attribute @p param; false: none (a
     *  rule, "none", or unknown). */
    static bool bankSlot(const QString &param, int *bank, int *slot);
    /** "Pan", "Gobo rotation" */
    static QString paramName(const QString &param);
    /** A colour attribute's colour (the faders drawn so); invalid: none. */
    static QString paramColour(const QString &param);

    /*********************************************************************
     * Files
     *********************************************************************/
    static PageSet fromJson(const QByteArray &json, QString *error = nullptr);
    static PageSet fromFile(const QString &path, QString *error = nullptr);
    QByteArray toJson() const;
    /** Write it to @p path (Export); false and @p error when not. */
    bool save(const QString &path, QString *error = nullptr) const;

    /** Every problem of this set against @p layout (and @p profile when
     *  given): a control the layout lacks, a job a control's kind can't do
     *  (a fader marked fine, a button given an attribute, a rule off a
     *  fader), an unknown attribute, a page job on a desk role (GM, STOP,
     *  GO, Back) or on a page key, a page key that is not a button, a
     *  control twice on one page. Empty: it holds. */
    QStringList check(const SurfaceLayout &layout, const QLCInputProfile *profile = nullptr) const;

    /** The built-in sets (resources), read once. */
    static QList<PageSet> builtIns();
    /** Every set of @p profileName: built-ins first, then this user's. */
    static QList<PageSet> forProfile(const QString &profileName);
    /** This user's sets of @p profileName that the checker refused as they
     *  were read ("x.pageset.json is not used: ..."), each not offered. */
    static QStringList refusedFor(const QString &profileName);
    /** The set named @p name of @p profileName; invalid when none. */
    static PageSet find(const QString &profileName, const QString &name);
    /** The board's default set (its first built-in); invalid when none. */
    static PageSet defaultFor(const QString &profileName);

    /** This user's page sets folder (made when missing). */
    static QDir userDirectory();
    /** Tests: another folder (empty: back to the user's). */
    static void setUserDirectory(const QString &path);

    /** Import @p path: read, checked against its layout, copied into the
     *  user folder. False and @p error (naming each problem) when it does
     *  not fit a board with a layout, or a built-in has its name. */
    static bool importFile(const QString &path, QString *error = nullptr, PageSet *imported = nullptr);

private:
    QString m_profile;
    QString m_name;
    QString m_description;
    QString m_file;
    int m_layoutFormat = 1;
    bool m_builtIn = false;
    bool m_invert = false;
    quint32 m_next = 0xFFFFFFFF;
    quint32 m_previous = 0xFFFFFFFF;
    quint32 m_stepper = 0xFFFFFFFF;
    QList<Binding> m_always;
    QList<Page> m_pages;
};

/** @} */

#endif
