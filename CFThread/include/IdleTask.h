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
/*
    File:       IdleTask.h

    Contains:   IdleTasks are identical to normal tasks (see task.h) with one exception:

                You can schedule them for timeouts. If you call SetIdleTimer
                on one, after the time has elapsed the task object will receive an
                OS_IDLE event.

*/


#ifndef _IDLETASK_H_
#define _IDLETASK_H_

#include "Task.h"
#include <memory>

class IdleTask;

/**
 * @brief IdleTask 守护线程
 *
 * 这套机制实际上是利用一个线程专门来处理一个定时事件，平时睡眠，一旦发现有
 * 任务到期，就会通过 signal 通知另外的线程处理这个事件。
 * 而别的模块可以通过 IdleTaskThread::SetIdleTimer 函数将自己的信息插入到
 * IdleTaskThread 的堆栈里。
 */
class IdleTaskThread : private OSThread {
 private:

  IdleTaskThread() : OSThread(), fHeapMutex() {}
  virtual ~IdleTaskThread();

  void SetIdleTimer(IdleTask *idleObj, SInt64 msec);
  void CancelTimeout(IdleTask *idleObj);

  virtual void Entry();

  OSHeap fIdleHeap;
  OSMutex fHeapMutex;
  OSCond fHeapCond;

  friend class IdleTask;
};

/**
 * @brief 定时任务
 */
class IdleTask : public Task {

 public:

  //Call Initialize before using this class
  static void Initialize();

  static void Release() {
    if (sIdleThread != nullptr) {
      sIdleThread->SendStopRequest();
      sIdleThread->Join();
      sIdleThread = nullptr;
    }
  }

  IdleTask() : Task(), fIdleElem() {
    this->SetTaskName("IdleTask");
    fIdleElem.SetEnclosingObject(this);
  }

  /**
   * This object does a "best effort" of making sure a timeout isn't
   * pending for an object being deleted. In other words, if there is
   * a timeout pending, and the destructor is called, things will get cleaned
   * up. But callers must ensure that SetIdleTimer isn't called at the same
   * time as the destructor, or all hell will break loose.
   */
  virtual ~IdleTask();

  /**
   * This object will receive an OS_IDLE event in the following number of
   * milliseconds. Only one timeout can be outstanding, if there is already
   * a timeout scheduled, this does nothing.
   */
  void SetIdleTimer(SInt64 msec) { sIdleThread->SetIdleTimer(this, msec); }

  /**
   * If there is a pending timeout for this object, this function cancels it.
   * If there is no pending timeout, this function does nothing.
   */
  void CancelTimeout() { sIdleThread->CancelTimeout(this); }

 private:

  OSHeapElem fIdleElem;

  //there is only one idle thread shared by all idle tasks.
  //static IdleTaskThread*  sIdleThread;

  static std::shared_ptr<IdleTaskThread> sIdleThread;

  friend class IdleTaskThread;
};
#endif
