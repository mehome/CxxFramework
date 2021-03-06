/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/**
 * @file IdleTask.cpp
 *
 * IdleTasks are identical to normal tasks (see task.h) with one
 * exception:
 *   You can schedule them for timeouts. If you call SetIdleTimer
 *   on one, after the Time has elapsed the task object will receive
 *   an OS_IDLE event.
 */

#include <CF/Thread/IdleTask.h>
#include <CF/Core/Time.h>

using namespace CF::Thread;

// IDLE_TASK_THREAD IMPLEMENTATION:
IdleTaskThread *IdleTask::sIdleThread = nullptr;

IdleTaskThread::~IdleTaskThread() {
  Assert(fIdleHeap.CurrentHeapSize() == 0);
}

/**
 * @brief 设置 IDLE 唤醒定时器
 *
 * @note 更灵活的 IdleTimer 调度，支持提前唤醒
 */
void IdleTaskThread::SetIdleTimer(IdleTask *activeObj, SInt64 msec) {
  Core::MutexLocker locker(&fHeapMutex);

  SInt64 theMsec = Core::Time::Milliseconds() + msec;
  if (activeObj->fIdleElem.IsMemberOfAnyHeap()) {
    fIdleHeap.Update(&activeObj->fIdleElem, theMsec, Heap::heapUpdateFlagExpectUp);
  } else {
    activeObj->fIdleElem.SetValue(theMsec);
    fIdleHeap.Insert(&activeObj->fIdleElem);
  }

  fHeapCond.Signal();
}

void IdleTaskThread::CancelTimeout(IdleTask *idleObj) {
  Assert(idleObj != nullptr);
  if (nullptr == idleObj) return;
  Core::MutexLocker locker(&fHeapMutex);
  fIdleHeap.Remove(&idleObj->fIdleElem);
}

void IdleTaskThread::Entry() {
  // 空闲任务线程启动后，该函数运行。主要由一个大循环构成:

  Core::MutexLocker locker(&fHeapMutex);

  while (true) {
    // if there are no events to process, block.
    while (fIdleHeap.CurrentHeapSize() == 0) {
      if (IsStopRequested()) return;
      fHeapCond.Wait(&fHeapMutex, 1000);
    }

    SInt64 msec = Core::Time::Milliseconds();

    // pop elements out of the Heap as long as their timeout Time has arrived
    while ((fIdleHeap.CurrentHeapSize() > 0) &&
        (fIdleHeap.PeekMin()->GetValue() <= msec)) {
      auto *elem = (IdleTask *) fIdleHeap.ExtractMin()->GetEnclosingObject();
      Assert(elem != nullptr);
      elem->Signal(Task::kIdleEvent);
    }

    // we are done sending idle events. If there is a lowest tick count, then
    // we need to sleep until that Time.
    if (fIdleHeap.CurrentHeapSize() > 0) {
      SInt64 timeoutTime = fIdleHeap.PeekMin()->GetValue();
      // because sleep takes a 32 bit number
      timeoutTime -= msec;
      Assert(timeoutTime > 0);
      auto smallTime = (UInt32) timeoutTime;
      fHeapCond.Wait(&fHeapMutex, smallTime);
    }
  }
}

void IdleTask::Initialize() {
  if (!sIdleThread) {
    sIdleThread = new IdleTaskThread();
    sIdleThread->Start();
  }
}

IdleTask::~IdleTask() {
  // clean up stuff used by idle Thread routines
  Assert(sIdleThread);

  Core::MutexLocker locker(&sIdleThread->fHeapMutex);

  // Check to see if there is a pending timeout. If so, get this object
  // out of the Heap
  if (fIdleElem.IsMemberOfAnyHeap())
    sIdleThread->CancelTimeout(this);
}
