/*
  Q Light Controller Plus
  patchlist.h

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

#ifndef PATCHLIST_H
#define PATCHLIST_H

#include <QStringList>
#include <QVector3D>
#include <QString>
#include <QList>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The patch list: the paperwork that goes to the crew hanging and addressing
 * the rig. A row per fixture -- what it is, where it is patched, where that
 * universe goes, what it hangs on -- in universe and address order, the
 * fixtures still in the inventory last. Derived on demand from the patch,
 * the I/O map and the plot; nothing here is stored.
 */
class PatchList
{
public:
    struct Row
    {
        quint32 id = 0;
        QString name;
        QString manufacturer;
        QString model;
        QString mode;
        quint32 channels = 0;

        bool    patched = false;
        bool    clash = false;           //!< asked for channels another fixture holds
        quint32 universe = 0;            //!< zero-based; valid when patched
        QString universeName;
        quint32 address = 0;             //!< zero-based; valid when patched

        bool    network = false;         //!< its own endpoint (Fixture::Network)
        QString ip;                      //!< network fixtures
        QString transport;               //!< network fixtures: "Art-Net", "WLED"
        QString output;                  //!< where its universe goes, in words

        QString mountedOn;               //!< MonitorProperties::mountDescription()
        bool    onPlot = false;
        QVector3D position;              //!< metres, when onPlot (fixtureRigPosition())

        /** "12" .. "23" as the crew reads it: one-based. */
        quint32 firstChannel() const { return address + 1; }
        quint32 lastChannel() const { return address + qMax(channels, quint32(1)); }
        /** "Chauvet Rogue R2", once when the model repeats the maker
         *  ("Generic"). */
        QString makeModel() const;
        /** "1: Stage left · 12–23", "not patched", "clash · 1 · 12–23". */
        QString patchText() const;
    };

    /** Every fixture in the show, or only @p ids, in patch order. */
    static PatchList build(Doc *doc, const QList<quint32> &ids = QList<quint32>());

    QList<Row> rows() const { return m_rows; }
    bool isEmpty() const { return m_rows.isEmpty(); }
    /** Lengths in feet rather than metres (the plot's units). */
    bool feet() const { return m_feet; }

    /** A spreadsheet: one line per fixture, a header line first. */
    QString toCsv() const;
    /** The printable list, a table per universe (what the PDF is made from). */
    QString toHtml(const QString &title) const;
    /** Write the PDF to @p path (landscape Letter). False, with @p error, if
     *  the file can't be written. */
    bool writePdf(const QString &path, const QString &title, QString *error = nullptr) const;

    /** "Art-Net → CS1 (172.18.2.211) port 1" for every output of
     *  @p universe, or empty when it goes nowhere. */
    static QString universeOutput(Doc *doc, quint32 universe);

private:
    QList<Row> m_rows;
    bool m_feet = false;
};

/** @} */

#endif
