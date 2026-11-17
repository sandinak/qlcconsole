/*
  Q Light Controller Plus
  testchannels.cpp

  See testchannels.h.
*/

#include <QObject>

#include "testchannels.h"
#include "fadechannel.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

TestChannels::TestChannels(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_layer(doc, Universe::Highlight, FadeChannel::Override, QStringLiteral("Test"))
{
    // Testing a channel is not the show: a Show-only fixture keeps its rule.
    m_layer.setHoldsShowOnly(true);
}

TestChannels::~TestChannels()
{
}

QString TestChannels::refusal() const
{
    if (m_doc->mode() == Doc::Operate)
        return tr("Test mode is off in Operate: its values would sit on top of the live show. "
                  "Go back to Design to test channels.");
    return QString();
}

bool TestChannels::setOn(bool on, QString *why)
{
    if (on == m_on)
        return true;
    if (on)
    {
        const QString no = refusal();
        if (!no.isEmpty())
        {
            if (why != nullptr)
                *why = no;
            return false;
        }
        m_on = true;
        m_endedBecause.clear();
        emit changed();
        return true;
    }
    // Off: every value let go of at once.
    m_on = false;
    m_values.clear();
    apply();
    emit changed();
    return true;
}

bool TestChannels::setValue(quint32 universe, quint32 address, uchar value, QString *why)
{
    if (!m_on || address >= 512)
    {
        if (why != nullptr)
            *why = m_on ? tr("That address is past the end of the universe.") : tr("Test is off.");
        return false;
    }
    const bool added = !m_values.contains(key(universe, address));
    if (added)
    {
        // As it is now, to put it back on release: an LTP channel (pan, a
        // colour wheel, a gobo, fog) keeps whatever it was last given.
        uchar was = 0;
        if (Universe *u = m_doc->inputOutputMap()->universe(universe))
        {
            const QByteArray pre = u->preGMValues();
            if (int(address) < pre.size())
                was = uchar(pre.at(int(address)));
        }
        m_pre.insert(key(universe, address), was);
    }
    m_values.insert(key(universe, address), value);
    apply();
    if (added)
        emit changed();
    return true;
}

void TestChannels::release(quint32 universe, quint32 address)
{
    if (m_values.remove(key(universe, address)) > 0)
    {
        apply();
        emit changed();
    }
}

bool TestChannels::holds(quint32 universe, quint32 address) const
{
    return m_values.contains(key(universe, address));
}

uchar TestChannels::value(quint32 universe, quint32 address) const
{
    return m_values.value(key(universe, address), 0);
}

QString TestChannels::bannerText() const
{
    if (m_values.isEmpty())
        return tr("TEST MODE: click or drag a channel on the map to hold it over the show. "
                  "Click Test to stop.");
    return tr("TEST MODE: %1 held over the show. Click Test to let them go.")
        .arg(m_values.size() == 1 ? tr("1 channel") : tr("%1 channels").arg(m_values.size()));
}

QString TestChannels::chipText() const
{
    return tr("Test: %1 held").arg(m_values.size());
}

void TestChannels::endBecause(const QString &why)
{
    if (!m_on)
        return;
    setOn(false);
    m_endedBecause = why;
    emit changed();
}

void TestChannels::apply()
{
    /* Built without any lock held: the layer takes its own and the
       universes' on set(), and never waits on a device. */
    QList<OutputLayer::Level> levels;
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it)
    {
        const quint32 abs = it.key();
        const quint32 fid = m_doc->fixtureForAddress(abs);
        Fixture *fx = fid != Fixture::invalidId() ? m_doc->fixture(fid) : nullptr;
        if (fx != nullptr && fx->universeAddress() <= abs)
            levels.append({ fid, abs - fx->universeAddress(), it.value() });
        else
            levels.append({ Fixture::invalidId(), abs, it.value() });
    }
    m_layer.set(levels);

    /* Let go of a channel: it goes back to what it was before the test, or
       the rest value of a Show-only one. Pan, colour wheel, gobo and fog
       channels keep the last value written, so the layer letting go alone left
       the test value. setChannelDefaultValue() takes the universe's frame
       lock as a patch does: the value lands between two frames, never in
       one, and nothing waits on a device. */
    for (quint32 abs : m_applied)
    {
        if (m_values.contains(abs))
            continue;
        const uchar was = m_pre.take(abs);
        if (Universe *u = m_doc->inputOutputMap()->universe(abs / 512))
            u->setChannelDefaultValue(ushort(abs % 512), was);
    }
    m_applied = m_values.keys();
}
