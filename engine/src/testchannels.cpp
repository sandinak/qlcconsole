/*
  Q Light Controller Plus
  testchannels.cpp

  See testchannels.h.
*/

#include <QObject>

#include "testchannels.h"
#include "fadechannel.h"
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

QString TestChannels::statusText() const
{
    return tr("Testing channels: %1 held").arg(m_values.size());
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
}
