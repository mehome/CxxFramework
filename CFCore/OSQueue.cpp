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
    File:       OSQueue.cpp

    Contains:   implements OSQueue class

*/

#include "OSQueue.h"
#include "MyAssert.h"

OSQueue::OSQueue() : fLength(0) {
  // 哨兵节点构成自环
  fSentinel.fNext = &fSentinel;
  fSentinel.fPrev = &fSentinel;
}

void OSQueue::EnQueue(OSQueueElem *elem) {
  OSMutexLocker theLocker(&fMutex);

  Assert(elem != nullptr);
  if (elem->fQueue == this)
    return;
  Assert(elem->fQueue == nullptr);
  elem->fNext = fSentinel.fNext;
  elem->fPrev = &fSentinel;
  elem->fQueue = this;
  fSentinel.fNext->fPrev = elem;
  fSentinel.fNext = elem;
  fLength++;
}

OSQueueElem *OSQueue::DeQueue() {
  OSMutexLocker theLocker(&fMutex);

  if (fLength > 0) {
    OSQueueElem *elem = fSentinel.fPrev;
    Assert(fSentinel.fPrev != &fSentinel);
    elem->fPrev->fNext = &fSentinel;
    fSentinel.fPrev = elem->fPrev;
    elem->fQueue = nullptr;
    fLength--;
    return elem;
  }

  return nullptr;
}

void OSQueue::Remove(OSQueueElem *elem) {
  OSMutexLocker theLocker(&fMutex);

  Assert(elem != nullptr);
  Assert(elem != &fSentinel);

  if (elem->fQueue == this) {
    elem->fNext->fPrev = elem->fPrev;
    elem->fPrev->fNext = elem->fNext;
    elem->fQueue = nullptr;
    fLength--;
  }
}

#if OSQUEUETESTING
bool OSQueue::Test()
{
    OSQueue theVictim;
    void *x = (void*)1;
    OSQueueElem theElem1(x);
    x = (void*)2;
    OSQueueElem theElem2(x);
    x = (void*)3;
    OSQueueElem theElem3(x);

    if (theVictim.GetHead() != nullptr)
        return false;
    if (theVictim.GetTail() != nullptr)
        return false;

    theVictim.EnQueue(&theElem1);
    if (theVictim.GetHead() != &theElem1)
        return false;
    if (theVictim.GetTail() != &theElem1)
        return false;

    OSQueueElem* theElem = theVictim.DeQueue();
    if (theElem != &theElem1)
        return false;

    if (theVictim.GetHead() != nullptr)
        return false;
    if (theVictim.GetTail() != nullptr)
        return false;

    theVictim.EnQueue(&theElem1);
    theVictim.EnQueue(&theElem2);

    if (theVictim.GetHead() != &theElem1)
        return false;
    if (theVictim.GetTail() != &theElem2)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem1)
        return false;

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem2)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem2)
        return false;

    theVictim.EnQueue(&theElem1);
    theVictim.EnQueue(&theElem2);
    theVictim.EnQueue(&theElem3);

    if (theVictim.GetHead() != &theElem1)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem1)
        return false;

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem2)
        return false;

    if (theVictim.GetHead() != &theElem3)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem3)
        return false;

    theVictim.EnQueue(&theElem1);
    theVictim.EnQueue(&theElem2);
    theVictim.EnQueue(&theElem3);

    OSQueueIter theIterVictim(&theVictim);
    if (theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != &theElem3)
        return false;
    theIterVictim.Next();
    if (theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != &theElem2)
        return false;
    theIterVictim.Next();
    if (theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != &theElem1)
        return false;
    theIterVictim.Next();
    if (!theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != nullptr)
        return false;

    theVictim.Remove(&theElem1);

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theVictim.Remove(&theElem1);

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theVictim.Remove(&theElem3);

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem2)
        return false;

    return true;
}
#endif

OSQueueIter::OSQueueIter(OSQueue *inQueue, OSQueueElem *startElemP)
    : fQueueP(inQueue) {
  if (startElemP) {
    Assert(startElemP->IsMember(*inQueue));
    fCurrentElemP = startElemP;
  } else
    fCurrentElemP = nullptr;
}

void OSQueueIter::Next() {
  if (fCurrentElemP == fQueueP->GetTail())
    fCurrentElemP = nullptr;
  else
    fCurrentElemP = fCurrentElemP->Prev();
}

OSQueueElem::~OSQueueElem() {
  Assert(fQueue == nullptr);
}

OSQueueElem *OSQueue_Blocking::DeQueueBlocking(OSThread *inCurThread,
                                               SInt32 inTimeoutInMilSecs) {
  OSMutexLocker theLocker(&fMutex);
  /*
   * 如果 fQueue.GetLength() == 0,则调用 fCond.Wait 即调用 pthread_cond_timedwait
   * 等待条件变量有效
   */
#ifdef __Win32_
  if (fQueue.GetLength() == 0) {
      fCond.Wait(&fMutex, inTimeoutInMilSecs);
      return nullptr;
  }
#else
  if (fQueue.GetLength() == 0)
    fCond.Wait(&fMutex, inTimeoutInMilSecs);
#endif

  /*
     fCond.wait 返回或者 fQueue.GetLength() != 0,调用 fQueue.DeQueue 返回队列
     里的任务对象。这里要注意一点,pthread_cond_timedwait 可能只是超时退出,所以
     fQueue.DeQueue 可能只是返回空指针。
   */
  OSQueueElem *retval = fQueue.DeQueue();
  return retval;
}

OSQueueElem *OSQueue_Blocking::DeQueue() {
  OSMutexLocker theLocker(&fMutex);
  OSQueueElem *retval = fQueue.DeQueue();
  return retval;
}

void OSQueue_Blocking::EnQueue(OSQueueElem *obj) {
  {
    OSMutexLocker theLocker(&fMutex);
    // 调用 OSQueue 类成员 fQueue.EnQueue 函数
    fQueue.EnQueue(obj);
  }
  // 调用 OSCond 类成员 fCond.Signal 函数即调用 pthread_cond_signal(&fCondition);
  fCond.Signal();
}
