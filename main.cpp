#include <OS.h>
#include <Socket.h>
#include <SocketUtils.h>
#include <TimeoutTask.h>
#include <HTTPSessionInterface.h>
#include "HTTPListenerSocket.h"

bool isStop = false;

void willExit() {
  isStop = true;
}

int main() {

  atexit(willExit);

  // Initialize utility classes
  OS::Initialize();
  OSThread::Initialize();

  Socket::Initialize();
  SocketUtils::Initialize(false);

#if !MACOSXEVENTQUEUE

#ifndef __Win32__
  ::epollInit();
#else
  //initialize the select() implementation of the event queue
  ::select_startevents();
#endif

#endif

  OSThread::SetPersonality("root", "root");

  UInt32 numShortTaskThreads = 0;
  UInt32 numBlockingThreads = 0;
  UInt32 numThreads = 0;
  UInt32 numProcessors = 0;

  if (OS::ThreadSafe()) {
//    numShortTaskThreads =
//        sServer->GetPrefs()->GetNumThreads(); // whatever the prefs say
    if (numShortTaskThreads == 0) {
      numProcessors = OS::GetNumProcessors();
      // 1 worker thread per processor, up to 2 threads.
      // Note: Limiting the number of worker threads to 2 on a MacOS X system
      //     with > 2 cores results in better performance on those systems, as
      //     of MacOS X 10.5.  Future improvements should make this limit
      //     unnecessary.
      if (numProcessors > 2)
        numShortTaskThreads = 2;
      else
        numShortTaskThreads = numProcessors;
    }

//    numBlockingThreads =
//        sServer->GetPrefs()->GetNumBlockingThreads(); // whatever the prefs say
    if (numBlockingThreads == 0)
      numBlockingThreads = 1;
  }

  if (numShortTaskThreads == 0)
    numShortTaskThreads = 1;

  if (numBlockingThreads == 0)
    numBlockingThreads = 1;

  numThreads = numShortTaskThreads + numBlockingThreads;
  qtss_printf("Add threads shortask=%lu blocking=%lu\n",
              numShortTaskThreads, numBlockingThreads);

  TaskThreadPool::SetNumShortTaskThreads(numShortTaskThreads);
  TaskThreadPool::SetNumBlockingTaskThreads(numBlockingThreads);
  TaskThreadPool::AddThreads(numThreads);

  // Start up the server's global tasks, and start listening

  // The TimeoutTask mechanism is task based,
  // we therefore must do this after adding task threads.
  // this be done before starting the sockets and server tasks
  TimeoutTask::Initialize();

  // Make sure to do this stuff last. Because these are all the threads that
  // do work in the server, this ensures that no work can go on while the server
  // is in the process of staring up
  IdleTask::Initialize();
  Socket::StartThread();
  OSThread::Sleep(1000);

  HTTPSessionInterface::Initialize();

  HTTPListenerSocket *httpSocket = new HTTPListenerSocket();
  httpSocket->Initialize(SocketUtils::GetIPAddr(0), 8081);
  httpSocket->RequestEvent(EV_RE);

  while (!isStop) {
#ifdef __sgi__
    OSThread::Sleep(999);
#else
    OSThread::Sleep(1000);
#endif
  }

  // Now, make sure that the server can't do any work
  TaskThreadPool::RemoveThreads();

  return 0;
}