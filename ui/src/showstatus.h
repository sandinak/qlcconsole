/*
  Q Light Controller Plus - qlcconsole
  showstatus.h

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

#ifndef SHOWSTATUS_H
#define SHOWSTATUS_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>

/**
 * A small registry that lets independent parts of the app declare whether
 * they are currently a reason the show is not ready, without each one
 * having to know about the footer or about each other.
 *
 * The problem this replaces: the footer's readiness label used to read
 * from exactly one source (InputOutputMap::danglingOutputPatches()) with
 * that check's result written straight into a single QString member. That
 * was fine while output patching was the only thing that could make a show
 * not-ready. It stops being fine the moment a second source shows up (PMJ
 * hardware gone missing, a fixture profile that failed to load, ...), since
 * each new source would otherwise need its own hand-wired footer logic,
 * fighting the others over the same one label -- whichever ran its update
 * last would win, silently hiding the others.
 *
 * A source registers a named entry (setStatus) or clears it (clearStatus)
 * whenever its own condition changes. Anything that wants to show overall
 * readiness (today: App's footer) asks for the worst currently-registered
 * entry (worst()) and renders whatever it says, generically -- it does not
 * need to know "output.dangling" or any other key exists, so a future
 * source needs no footer changes at all, only a setStatus()/clearStatus()
 * call of its own.
 */
class ShowStatus : public QObject
{
    Q_OBJECT

public:
    enum Severity { Ok, Warning, Error };

    struct Entry
    {
        QString key;
        Severity severity = Ok;
        QString summary;    //!< short -- one line in the footer chip's tooltip
        QString detail;     //!< a sentence or two of context, may be empty --
                             //!< shown above @a items (or alone, if @a items
                             //!< is empty) in the full-detail dialog
        QStringList items;  //!< optional: one line per individual affected
                             //!< thing (a universe, a device, ...), rendered
                             //!< as an actual list in that dialog rather than
                             //!< everything folded into one paragraph
    };

    static ShowStatus *instance();

    /** Register (or replace) the status entry under @p key. Call again with
     *  the same key to update it -- there is no separate "update". */
    void setStatus(const QString &key, Severity severity, const QString &summary,
                   const QString &detail = QString(), const QStringList &items = QStringList());
    /** Remove the entry under @p key: that source is no longer a problem.
     *  A no-op if @p key was not registered. */
    void clearStatus(const QString &key);

    /** The single worst entry currently registered. Severity is Ok and
     *  summary/detail are empty when nothing is registered. */
    Entry worst() const;
    /** Every entry currently registered, worst-severity first. */
    QList<Entry> allEntries() const;

signals:
    /** Fires whenever any entry is set or cleared with a value different
     *  from what it already held. Connect this instead of polling to keep
     *  a footer/status widget in sync. */
    void changed();

private:
    ShowStatus();

    QMap<QString, Entry> m_entries;
};

#endif
