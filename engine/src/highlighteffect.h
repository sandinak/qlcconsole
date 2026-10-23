/*
  Q Light Controller Plus
  highlighteffect.h

  Forces selected fixtures to full-bright white so the operator can
  visually identify them in the rig, over all running functions for the
  duration: an OutputLayer at Universe::Highlight, with Override. Only Park
  is above it; the Grand Master and Blackout still apply (see
  Universe::FaderPriority for the whole model).

*/

#ifndef HIGHLIGHTEFFECT_H
#define HIGHLIGHTEFFECT_H

#include <QObject>
#include <QList>

#include "outputlayer.h"

class Doc;

class HighlightEffect final : public QObject
{
    Q_OBJECT

public:
    explicit HighlightEffect(Doc *doc);
    ~HighlightEffect() override;

    /** Replace the set of highlighted fixtures. Thread-safe; may be called
     *  from the UI thread at any time. Passing an empty list stops writing. */
    void setFixtures(const QList<quint32>& fixtureIds);

private:
    struct FixEntry {
        quint32 fixtureId;
        int universeId;
        QList<QPair<int, uchar>> writes; // (channel index, value)
    };

    QList<FixEntry> buildEntries(const QList<quint32>& fixtureIds) const;

    Doc *m_doc;
    /** The Highlight fader on each universe */
    OutputLayer m_layer;
};

#endif // HIGHLIGHTEFFECT_H
