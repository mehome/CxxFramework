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
	File:       Task.h

	Contains:   Tasks are objects that can be scheduled. To schedule a task,
				you call its signal method, and pass in an event (events are
				bits and all events are defined below).

				Once Signal() is called, the task object will be scheduled.
                When it runs, its Run() function will get called. In order to
                clear the event, the derived task object must call GetEvents()
                (which returns the events that were sent).

				Calling GetEvents() implicitly "clears" the events returned.
				All events must be cleared before the Run() function returns,
				or Run() will be invoked again immediately.

*/

#ifndef __TASK_H__
#define __TASK_H__

#include <atomic>
#include <CF/Heap.h>
#include <CF/ConcurrentQueue.h>
#include <CF/Core/RWMutex.h>

#ifndef DEBUG_TASK
#define DEBUG_TASK 0
#else
#undef  DEBUG_TASK
#define DEBUG_TASK 1
#endif

namespace CF {
namespace Thread {

class TaskThread;

/**
 * Task 实例是可执行对象，是 CxxFramework 线程模型下的基本调度单元。
 * Task 具有事件驱动模型，可以被重复调度，但在同一时刻不会存在多个并发执行流。
 *
 * @note  删除一个 Task 的最佳方式是，通过 Signal 发送 kKillEvent 信号，
 *        在 Run 处理中检测到 kKillEvent 信号后，返回 -1，将清理职责交给调度器。
 */
class Task {
 public:

  /**
   * EVENTS
   * here are all the events that can be sent to a task
   */
  enum {
    // these are all of type "EventFlags"
    kKillEvent    = 0x01U << 0U,
    kIdleEvent    = 0x01U << 1U,
    kStartEvent   = 0x01U << 2U,
    kTimeoutEvent = 0x01U << 3U,

    /* Socket events */
    kReadEvent    = 0x01U << 4U,
    kWriteEvent   = 0x01U << 5U,

    /* update event */
    kUpdateEvent  = 0x01U << 6U,
  };

  typedef unsigned int EventFlags;

  // CONSTRUCTOR / DESTRUCTOR
  // You must assign priority at create Time.
  Task();
  virtual ~Task() = default;

  /**
   * @return >0 invoke me after this number of MilSecs with a kIdleEvent
   * @return  0 don't reinvoke me at all.
   * @return -1 delete me
   *
   * Suggested practice is that any task should be deleted by returning true
   * from the Run function. That way, we know that the Task is not running at
   * the time it is deleted. This object provides no protection against calling
   * a method, such as Signal, at the same time the object is being deleted
   * (because it can't really), so watch those dangling references!
   */
  virtual SInt64 Run() = 0;

  // Send an event to this task.
  void Signal(EventFlags eventFlags);

  void GlobalUnlock();

  bool Valid(); // for debugging
  char fTaskName[48];

  void SetTaskName(char const *name);

  void SetDefaultThread(TaskThread *defaultThread) {
    fDefaultThread = defaultThread;
  }

  void SetThreadPicker(std::atomic_uint *picker);

  static std::atomic_uint *GetBlockingTaskThreadPicker() {
    return &sBlockingTaskThreadPicker;
  }

 protected:

  // Only the tasks themselves may find out what events they have received
  EventFlags GetEvents();

  /**
   * A task, inside its run function, may want to ensure that the same task
   * thread is used for subsequent calls to Run(). This may be the case if the
   * task is holding a mutex between calls to run. By calling this function,
   * the task ensures that the same task thread will be used for the next call
   * to Run(). It only applies to the next call to run.
   */
  void ForceSameThread();

  SInt64 CallLocked() {
    ForceSameThread();
    fWriteLock = true;
    return (SInt64) 10; // minimum of 10 milliseconds between locks
  }

 private:

  enum {
    kAlive = 0x80000000,   // EventFlags, again
    kAliveOff = 0x7fffffff
  };

  void SetTaskThread(TaskThread *thread) {
    fUseThisThread = thread;
  }

  /* 当事件发生时，Task 进入调度队列，并设置相应的 event flag。
   * Task 进入调度队列时设置 alive 标志位，执行完毕后撤销 alive 标志位。
   * Task 在某一时刻，只会处于唯一调度队列。 */
  std::atomic<EventFlags> fEvents;

  TaskThread *fUseThisThread; /* 强制执行线程 */
  TaskThread *fDefaultThread; /* 默认执行线程 */
  bool fWriteLock;

#if DEBUG_TASK
  // The whole premise of a task is that the Run function cannot be re-entered.
  // This debugging variable ensures that that is always the case
  volatile UInt32 fInRunCount;
#endif

  // This could later be optimized by using a timing wheel instead of a Heap,
  // and that way we wouldn't need both a Heap elem and a Queue elem here (just Queue elem)
  HeapElem fTimerHeapElem;
  QueueElem fTaskQueueElem;

  std::atomic_uint *pickerToUse;

  // Variable used for assigning tasks to threads in a round-robin fashion
  static std::atomic_uint sShortTaskThreadPicker; // default picker
  static std::atomic_uint sBlockingTaskThreadPicker;

  friend class TaskThread;
};

/**
 * 任务执行线程，非抢占式任务调度器
 */
class TaskThread : public Core::Thread {
 public:

  // Implementation detail: all tasks get run on TaskThreads.

  TaskThread() : Thread(), fTaskThreadPoolElem() {
    fTaskThreadPoolElem.SetEnclosingObject(this);
  }

  ~TaskThread() override { this->StopAndWaitForThread(); }

 private:

  enum {
    kMinWaitTimeInMilSecs = 10  //UInt32
  };

  void Entry() override;

  Task *WaitForTask();

  QueueElem fTaskThreadPoolElem;

  // use heap for time-sequence task, only in TaskThread, not concurrent.
  Heap fHeap;               /* 时序-优先队列 */
  BlockingQueue fTaskQueue; /* 事件-触发队列 */

  friend class Task;
  friend class TaskThreadPool;
};

/**
 * @brief 任务线程池静态类，该类负责生成、删除以及维护内部的任务线程列表。
 *
 * Because task threads share a global queue of tasks to execute,
 * there can only be one pool of task threads. That is why this object
 * is static.
 */
class TaskThreadPool {
 public:

  /**
   * @brief Adds some threads to the pool
   *
   * creates the threads: takes NumShortTaskThreads + NumBLockingThreads,
   * sets num short task threads.
   */
  static bool CreateThreads(UInt32 numShortTaskThreads, UInt32 numBlockingThreads);

  static void RemoveThreads();

  static TaskThread *GetThread(UInt32 index);

  static UInt32 GetNumThreads() { return sNumTaskThreads; }

 private:
  TaskThreadPool() = default;

  static TaskThread **sTaskThreadArray; // ShortTaskThreads + BlockingTaskThreads
  static UInt32 sNumTaskThreads;
  static UInt32 sNumShortTaskThreads;
  static UInt32 sNumBlockingTaskThreads;

  static Core::RWMutex sRWMutex;

  friend class Task;
  friend class TaskThread;
};

} // namespace Task
} // namespace CF

#endif
