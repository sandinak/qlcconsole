/*
  Q Light Controller Plus
  fakehidbackend.h

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

#ifndef FAKEHIDBACKEND_H
#define FAKEHIDBACKEND_H

#include <QDeadlineTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

#include "hidbackend.h"

/**
 * Devices that are not there, for HIDPlugin::setTestBackend(): listed as
 * the test says, and opened -- into virtual pads -- as slowly as it says:
 * at once, after N ms, or not until release() (a pad that stopped
 * answering mid-open). Header only: the plugin's own test and the engine's
 * (which loads the plugin as the app does) share it.
 */
class FakeHIDBackend final : public HIDBackend
{
public:
    static constexpr int Forever = -1;

    /** List a pad at @p path; its open takes @p openMs (Forever: until
     *  release()). A second call with the same path lists it twice. */
    void list(const QString &path, unsigned short vid, unsigned short pid, const QString &product,
              int openMs = 0, quint64 instance = 0, bool withReader = false,
              const QString &serial = QString())
    {
        QMutexLocker l(&m_lock);
        HIDFound f;
        f.path = path;
        f.vendorId = vid;
        f.productId = pid;
        f.product = product;
        f.instance = instance;
        f.serial = serial;
        /* A pad that is read by a thread of its own, as a real one is:
           each read waits while setReaderBlocked(true) (a pad that stopped
           answering while it is read). */
        if (withReader)
            f.testRead = [this]() { read(); };
        m_listed << f;
        m_openMs.insert(path, openMs);
    }
    /** No longer listed (gone from the system), even mid-open. */
    void unlist(const QString &path)
    {
        QMutexLocker l(&m_lock);
        for (int i = m_listed.size() - 1; i >= 0; --i)
            if (m_listed.at(i).path == path)
                m_listed.removeAt(i);
    }
    /** Every read of a pad listed withReader hangs, until false again or
     *  release(). */
    void setReaderBlocked(bool on)
    {
        QMutexLocker l(&m_lock);
        m_readerBlocked = on;
        m_wake.wakeAll();
    }
    /** Reads in progress (one hung, while blocked). */
    int readsInProgress() const { QMutexLocker l(&m_lock); return m_inReads; }

    /** The listing itself hangs until release(). */
    void setEnumerateBlocks(bool on) { QMutexLocker l(&m_lock); m_enumerateBlocks = on; }
    /** Every blocked call returns (an open that was released makes its
     *  pad; @p openGives false: it finds nothing there). */
    void release(bool openGives = true)
    {
        QMutexLocker l(&m_lock);
        m_released = true;
        m_openGives = openGives;
        m_wake.wakeAll();
    }

    int opens(const QString &path) const { QMutexLocker l(&m_lock); return m_opens.value(path); }
    int enumerations() const { QMutexLocker l(&m_lock); return m_enumerations; }
    int maxConcurrentEnumerations() const { QMutexLocker l(&m_lock); return m_maxInEnumerate; }
    int callsInProgress() const { QMutexLocker l(&m_lock); return m_inCalls; }

    QList<HIDFound> enumerate() override
    {
        QMutexLocker l(&m_lock);
        m_enumerations++;
        m_inCalls++;
        m_inEnumerate++;
        m_maxInEnumerate = qMax(m_maxInEnumerate, m_inEnumerate);
        while (m_enumerateBlocks && !m_released)
            m_wake.wait(&m_lock);
        m_inEnumerate--;
        m_inCalls--;
        return m_listed;
    }

    HIDDevice *open(const HIDFound &found,
                    const std::function<HIDDevice *(const HIDFound &)> &virtualPad) override
    {
        QMutexLocker l(&m_lock);
        m_opens[found.path]++;
        m_inCalls++;
        const int ms = m_openMs.value(found.path);
        if (ms == Forever)
        {
            while (!m_released)
                m_wake.wait(&m_lock);
        }
        else if (ms > 0)
        {
            QDeadlineTimer until(ms);
            while (!m_released && m_wake.wait(&m_lock, until))
                ;
        }
        const bool gives = ms == Forever ? m_openGives : true;
        m_inCalls--;
        l.unlock();
        return gives ? virtualPad(found) : nullptr;
    }

private:
    void read()
    {
        QMutexLocker l(&m_lock);
        m_inReads++;
        while (m_readerBlocked && !m_released)
            m_wake.wait(&m_lock);
        m_inReads--;
    }

    mutable QMutex m_lock;
    QWaitCondition m_wake;
    QList<HIDFound> m_listed;
    QHash<QString, int> m_openMs;
    QHash<QString, int> m_opens;
    bool m_enumerateBlocks = false;
    bool m_readerBlocked = false;
    int m_inReads = 0;
    bool m_released = false;
    bool m_openGives = true;
    int m_enumerations = 0;
    int m_inEnumerate = 0;
    int m_maxInEnumerate = 0;
    int m_inCalls = 0;
};

#endif
