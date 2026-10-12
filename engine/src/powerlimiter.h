/*
  Q Light Controller Plus
  powerlimiter.h

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

#ifndef POWERLIMITER_H
#define POWERLIMITER_H

#include <QObject>
#include <QHash>
#include <QPair>
#include <QSet>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Keeps each universe's power limits (Universe::setPowerLimits()) in step
 * with the show: every fixture with a measured PowerProfile and a maximum
 * draw is held to it on what its universe sends. Rebuilt when fixtures are
 * added, removed or changed, and when universes come and go.
 */
class PowerLimiter final : public QObject
{
    Q_OBJECT

public:
    explicit PowerLimiter(Doc *doc);

    /** Rebuild the limits now (normally done for you, coalesced). */
    void rebuild();

    /** Let @p fid's output through unlimited while it is being measured. */
    void setMeasuring(quint32 fid, bool on);
    bool isMeasuring(quint32 fid) const { return m_measuring.contains(fid); }

    /** The fixtures held back as of the last output, across universes:
     *  fixture -> (amps it asks for, amps it is held to). */
    QHash<quint32, QPair<double, double> > heldBack() const;

    /** How many fixtures are held to a maximum (limited or not right now). */
    int limitedCount() const { return m_limitedCount; }

private:
    void scheduleRebuild();

    Doc *m_doc;
    QSet<quint32> m_measuring;
    bool m_pending = false;
    int m_limitedCount = 0;
};

/** @} */

#endif
