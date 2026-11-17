/*
  Q Light Controller Plus
  testchannels.h

  Test mode on Connections' DMX map: the operator sets single channels to a
  value to see what a fixture does ("test this address"). Not a new playback
  path: an OutputLayer at Universe::Highlight over whatever is playing, like
  Highlight, that never drives a Show-only fixture. Turning Test off lets go
  of every value at once and each channel goes back to what the show gives
  it. The emergency stop and All Off turn it off, and it stays off until it
  is turned on again; Operate refuses it, and entering Operate turns it off.
  Never saved: it lives in the Doc object only.
*/

#ifndef TESTCHANNELS_H
#define TESTCHANNELS_H

#include <QObject>
#include <QHash>

#include "outputlayer.h"
#include "qlcengine_export.h"

class Doc;

class QLC_ENGINE_EXPORT TestChannels final : public QObject
{
    Q_OBJECT

public:
    explicit TestChannels(Doc *doc);
    ~TestChannels() override;

    bool isOn() const { return m_on; }
    /** Why Test is refused now ("" when it is allowed). */
    QString refusal() const;
    /** Turn Test on or off. On is refused (false, @p why set) in Operate;
     *  off always works and lets go of every value. */
    bool setOn(bool on, QString *why = nullptr);

    /** Hold @p address (0-based) of @p universe (0-based) at @p value, over
     *  what plays. Refused unless Test is on. */
    bool setValue(quint32 universe, quint32 address, uchar value, QString *why = nullptr);
    /** Let go of one channel (it goes back to the show's value). */
    void release(quint32 universe, quint32 address);
    int heldCount() const { return m_values.size(); }
    bool holds(quint32 universe, quint32 address) const;
    uchar value(quint32 universe, quint32 address) const;

    /** The banner: "TEST MODE: N channels held over the show. Click Test to
     *  let them go." (with none held yet, how to hold one). */
    QString bannerText() const;
    /** The footer chip: "Test: N held"; shown only while some are held. */
    QString chipText() const;
    bool chipShown() const { return m_on && !m_values.isEmpty(); }
    /** End Test mode because the patch moved under it (a fixture added,
     *  removed or repatched, a universe added or removed): every value is
     *  let go as when Test is turned off, and endedBecause() says why. */
    void endBecause(const QString &why);
    /** Why Test ended by itself last, or empty (cleared when it is turned on). */
    QString endedBecause() const { return m_endedBecause; }

signals:
    /** Test went on or off, or the held count changed. */
    void changed();

private:
    static quint32 key(quint32 universe, quint32 address) { return universe * 512 + address; }
    void apply();

    Doc *m_doc;
    bool m_on = false;
    QHash<quint32, uchar> m_values;
    QHash<quint32, uchar> m_pre;   //!< each held channel as it was before the test touched it
    QString m_endedBecause;
    QList<quint32> m_applied;   //!< the addresses the layer held last time
    OutputLayer m_layer;
};

#endif
