/*
  Q Light Controller Plus
  artnetrdmflag.h

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

#ifndef ARTNETRDMFLAG_H
#define ARTNETRDMFLAG_H

#include <QtGlobal>

/**
 * Whether a node says it speaks RDM, held steady.
 *
 * The capability is Status1 bit 1 of every ArtPollReply, and a node that
 * fills its replies properly sends the same Status1 every time (OLA: 0xD2,
 * reply after reply). Not every node does: the office CR041R reuses an
 * uncleared buffer, so its Status1 cycles through 0x00, 0x74, 0x6F and
 * 0xA9, and the RDM bit is set in about a quarter of its replies. Read
 * straight, the Devices row flickered between "RDM" and "no RDM".
 *
 * So the bit is trusted only when it holds:
 *  - what is shown changes only when the last StableReplies replies agree
 *    on the bit *and* carry the same Status1 byte;
 *  - a node whose Status1 changed JunkChanges or more times within its last
 *    Window replies is sending junk: its bit is not trusted ("no RDM") until
 *    its Status1 has held for CalmReplies replies in a row;
 *  - a table-of-devices reply (ArtTodData) from the node is proof: it speaks
 *    RDM, whatever its Status1 says.
 * Until a node's replies have settled it reads "no RDM".
 */
class ArtNetRdmFlag
{
public:
    enum
    {
        StableReplies = 5,  //!< replies that must agree before what is shown changes
        Window = 16,        //!< replies looked back over for junk
        JunkChanges = 4,    //!< Status1 changes within Window that make it junk
        CalmReplies = 64    //!< an unchanging Status1 run that makes it trusted again
    };

    /** An ArtPollReply's Status1. True when what is shown changed. */
    bool replyHeard(uchar status1)
    {
        const bool was = rdmCapable();

        if (m_count > 0 && status1 != m_status[(m_next + Window - 1) % Window])
            m_sameRun = 1;
        else
            m_sameRun++;
        m_status[m_next] = status1;
        m_next = (m_next + 1) % Window;
        if (m_count < Window)
            m_count++;

        if (changesInWindow() >= JunkChanges)
            m_junk = true;
        else if (m_junk && m_sameRun >= CalmReplies)
            m_junk = false;

        if (m_junk)
        {
            m_shown = false;
        }
        else if (m_count >= StableReplies && m_sameRun >= StableReplies)
        {
            m_shown = (status1 & 0x02) != 0;
        }

        return rdmCapable() != was;
    }

    /** The node answered an ArtTodRequest: it speaks RDM. True when what is
     *  shown changed. */
    bool todHeard()
    {
        const bool was = rdmCapable();
        m_proven = true;
        return rdmCapable() != was;
    }

    /** What to show: RDM capable, as far as can be told. */
    bool rdmCapable() const
    {
        return m_proven || m_shown;
    }

    /** The node's Status1 is junk now (so its RDM bit is ignored). */
    bool statusIsJunk() const
    {
        return m_junk;
    }

private:
    int changesInWindow() const
    {
        int changes = 0;
        const int first = (m_next + Window - m_count) % Window;
        for (int i = 1; i < m_count; i++)
        {
            if (m_status[(first + i) % Window] != m_status[(first + i - 1) % Window])
                changes++;
        }
        return changes;
    }

    uchar m_status[Window] = {};
    int m_next = 0;         //!< where the next Status1 goes in m_status
    int m_count = 0;        //!< how many of m_status are filled
    int m_sameRun = 0;      //!< replies in a row with the latest Status1
    bool m_junk = false;
    bool m_shown = false;
    bool m_proven = false;
};

#endif
