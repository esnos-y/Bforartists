/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_TimerManager.h"

#include <algorithm>

#include "GHOST_TimerTask.h"

GHOST_TimerManager::GHOST_TimerManager()
{
}

GHOST_TimerManager::~GHOST_TimerManager()
{
  disposeTimers();
}

uint32_t GHOST_TimerManager::getNumTimers()
{
  return (uint32_t)m_timers.size();
}

bool GHOST_TimerManager::getTimerFound(GHOST_TimerTask *timer)
{
  TTimerVector::const_iterator iter = std::find(m_timers.begin(), m_timers.end(), timer);
  return iter != m_timers.end();
}

GHOST_TSuccess GHOST_TimerManager::addTimer(GHOST_TimerTask *timer)
{
  GHOST_TSuccess success;
  if (!getTimerFound(timer)) {
    // Add the timer task
    m_timers.push_back(timer);
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_TimerManager::removeTimer(GHOST_TimerTask *timer)
{
  GHOST_TSuccess success;
  TTimerVector::iterator iter = std::find(m_timers.begin(), m_timers.end(), timer);
  if (iter != m_timers.end()) {
    // Remove the timer task
    m_timers.erase(iter);
    delete timer;
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

uint64_t GHOST_TimerManager::nextFireTime()
{
  uint64_t smallest = GHOST_kFireTimeNever;
  TTimerVector::iterator iter;

  for (iter = m_timers.begin(); iter != m_timers.end(); ++iter) {
    uint64_t next = (*iter)->getNext();

    if (next < smallest)
      smallest = next;
  }

  return smallest;
}

bool GHOST_TimerManager::fireTimers(uint64_t time)
{
  TTimerVector::iterator iter;
  bool anyProcessed = false;

  for (iter = m_timers.begin(); iter != m_timers.end(); ++iter) {
    if (fireTimer(time, *iter))
      anyProcessed = true;
  }

  return anyProcessed;
}

bool GHOST_TimerManager::fireTimer(uint64_t time, GHOST_TimerTask *task)
{
  uint64_t next = task->getNext();

  // Check if the timer should be fired
  if (time > next) {
    // Fire the timer
    GHOST_TimerProcPtr timerProc = task->getTimerProc();
    uint64_t start = task->getStart();
    timerProc(task, time - start);

    // Update the time at which we will fire it again
    uint64_t interval = task->getInterval();
    uint64_t numCalls = (next - start) / interval;
    numCalls++;
    next = start + numCalls * interval;
    task->setNext(next);

    return true;
  }
  else {
    return false;
  }
}

void GHOST_TimerManager::disposeTimers()
{
  while (m_timers.empty() == false) {
    delete m_timers[0];
    m_timers.erase(m_timers.begin());
  }
}
