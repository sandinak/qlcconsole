/*
  Q Light Controller Plus
  iodeferral.cpp

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

#include <deque>

#include "iodeferral.h"

namespace
{
struct Deferred
{
    int depth = 0;
    std::deque<std::function<bool()> > calls;
};

thread_local Deferred t_deferred;
}

IODeferral::IODeferral()
    : m_outer(begin())
{
}

IODeferral::~IODeferral()
{
    end(m_outer);
}

bool IODeferral::begin()
{
    return t_deferred.depth++ == 0;
}

void IODeferral::end(bool outer)
{
    if (outer)
        runQueued();
    t_deferred.depth--;
}

bool IODeferral::finish()
{
    if (m_outer == false)
        return true;
    return runQueued();
}

bool IODeferral::runQueued()
{
    /* Still open while they run: a call that leads to another patch change
       (a plugin's signal, said directly) queues behind them, and runs here
       too -- in order, and with no lock of that change held. */
    bool ok = true;
    while (t_deferred.calls.empty() == false)
    {
        std::function<bool()> call = std::move(t_deferred.calls.front());
        t_deferred.calls.pop_front();
        if (call() == false)
            ok = false;
    }
    return ok;
}

bool IODeferral::run(const std::function<bool()> &call)
{
    if (t_deferred.depth == 0)
        return call();
    t_deferred.calls.push_back(call);
    return true;
}

bool IODeferral::active()
{
    return t_deferred.depth > 0;
}
