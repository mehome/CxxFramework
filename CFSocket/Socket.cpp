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
    File:       Socket.cpp

    Contains:   implements Socket class

*/

#include <string.h>
#include <errno.h>

#include <CF/Net/Socket/Socket.h>
#include <CF/Net/Socket/SocketUtils.h>

#if !__WinSock__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#endif

#ifdef USE_NETLOG
#include <netlog.h>
#else

#if defined(__sgi__) || defined(__osf__) || defined(__hpux__)
typedef int socklen_t; // missing from some platform includes
#endif

#endif

using namespace CF::Net;

EventThread *Socket::sEventThread = nullptr;

Socket::Socket(CF::Thread::Task *inNotifyTask, UInt32 inSocketType)
    : EventContext(EventContext::kInvalidFileDesc, sEventThread),
      fState(inSocketType),
      fLocalAddrStrPtr(nullptr),
      fLocalDNSStrPtr(nullptr),
      fPortStr(fPortBuffer, kPortBufSizeInBytes) {
  fLocalAddr.sin_addr.s_addr = 0;
  fLocalAddr.sin_port = 0;

  fDestAddr.sin_addr.s_addr = 0;
  fDestAddr.sin_port = 0;

  /* SetTask 是 EventContext 的成员函数,执行 "fTask = inNotifyTask" 操作 */
  this->SetTask(inNotifyTask);

#if DEBUG_SOCKET
  fLocalAddrStr.Set(fLocalAddrBuffer, sizeof(fLocalAddrBuffer));
#endif

}

OS_Error Socket::Open(int theType) {
  Assert(fFileDesc == EventContext::kInvalidFileDesc);
  fFileDesc = ::socket(PF_INET, theType, 0);
  if (fFileDesc == EventContext::kInvalidFileDesc)
    return (OS_Error) Core::Thread::GetErrno();

  //
  // Setup this Socket's event context

  if (fState & kNonBlockingSocketType)
    this->InitNonBlocking(fFileDesc);

  if (fState & kEdgeTriggeredSocketMode)
    this->SetMode(true);

  return OS_NoErr;
}

void Socket::ReuseAddr() {
  int one = 1;
  int err = ::setsockopt(
      fFileDesc, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int));
  Assert(err == 0);
}

void Socket::NoDelay() {
  int one = 1;
  int err = ::setsockopt(
      fFileDesc, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(int));
  Assert(err == 0);
}

void Socket::KeepAlive() {
  int one = 1;
  int err = ::setsockopt(
      fFileDesc, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(int));
  Assert(err == 0);
}

void Socket::SetSocketBufSize(UInt32 inNewSize) {

#if DEBUG_SOCKET
  int value;
  int buffSize = sizeof(value);
  int error = ::getsockopt(
      fFileDesc, SOL_SOCKET, SO_SNDBUF, (char *) &value, (socklen_t *) &buffSize);
#endif

  int bufSize = inNewSize;
  int err = ::setsockopt(
      fFileDesc, SOL_SOCKET, SO_SNDBUF, (char *) &bufSize, sizeof(int));
  AssertV(err == 0, Core::Thread::GetErrno());

#if DEBUG_SOCKET
  int setValue;
  error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_SNDBUF, (char *) &setValue, (socklen_t *) &buffSize);
  s_printf("Socket::SetSocketBufSize ");
  if (fState & kBound) {
    if (nullptr != this->GetLocalAddrStr())
      this->GetLocalAddrStr()->PrintStr(":");
    if (nullptr != this->GetLocalPortStr())
      this->GetLocalPortStr()->PrintStr(" ");
  } else
    s_printf("unbound ");
  s_printf("Socket=%d old SO_SNDBUF =%d inNewSize=%d setValue=%d\n",
              (int) fFileDesc, value, bufSize, setValue);
#endif

}

OS_Error Socket::SetSocketRcvBufSize(UInt32 inNewSize) {
#if DEBUG_SOCKET
  int value;
  int buffSize = sizeof(value);
  int error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_RCVBUF,
      (char *) &value, (socklen_t *) &buffSize);
#endif

  int bufSize = inNewSize;
  int err = ::setsockopt(
      fFileDesc, SOL_SOCKET, SO_RCVBUF, (char *) &bufSize, sizeof(int));

#if DEBUG_SOCKET
  int setValue;
  error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_RCVBUF,
      (char *) &setValue, (socklen_t *) &buffSize);
  s_printf("Socket::SetSocketRcvBufSize ");
  if (fState & kBound) {
    if (nullptr != this->GetLocalAddrStr())
      this->GetLocalAddrStr()->PrintStr(":");
    if (nullptr != this->GetLocalPortStr())
      this->GetLocalPortStr()->PrintStr(" ");
  } else
    s_printf("unbound ");
  s_printf("Socket=%d old SO_RCVBUF =%d inNewSize=%d setValue=%d\n",
           (int) fFileDesc, value, bufSize, setValue);
#endif

  if (err == -1)
    return (OS_Error) Core::Thread::GetErrno();

  return OS_NoErr;
}

OS_Error Socket::Bind(UInt32 addr, UInt16 port, bool test) {
  socklen_t len = sizeof(fLocalAddr);
  ::memset(&fLocalAddr, 0, sizeof(fLocalAddr));
  fLocalAddr.sin_family = AF_INET;
  fLocalAddr.sin_port = htons(port);
  fLocalAddr.sin_addr.s_addr = htonl(addr);

  int err;

#if 0
  if (test) { // pick some ports or conditions to return an error on.
      if (6971 == port) {
          fLocalAddr.sin_port = 0;
          fLocalAddr.sin_addr.s_addr = 0;
          return EINVAL;
      } else {
          err = ::bind(fFileDesc, (sockaddr *)&fLocalAddr, sizeof(fLocalAddr));
      }
  }
  else
#endif
  err = ::bind(fFileDesc, (sockaddr *) &fLocalAddr, sizeof(fLocalAddr));

  if (err == -1) {
    fLocalAddr.sin_port = 0;
    fLocalAddr.sin_addr.s_addr = 0;
    return (OS_Error) Core::Thread::GetErrno();
  } else {
    // get the kernel to fill in unspecified values
    ::getsockname(fFileDesc, (sockaddr *) &fLocalAddr, &len);
  }
  fState |= kBound;
  return OS_NoErr;
}

CF::StrPtrLen *Socket::GetLocalAddrStr() {
  //Use the array of IP addr strings to locate the string formatted version
  //of this IP address.
  if (fLocalAddrStrPtr == nullptr) {
    for (UInt32 x = 0; x < SocketUtils::GetNumIPAddrs(); x++) {
      if (SocketUtils::GetIPAddr(x) == ntohl(fLocalAddr.sin_addr.s_addr)) {
        fLocalAddrStrPtr = SocketUtils::GetIPAddrStr(x);
        break;
      }
    }
  }

#if DEBUG_SOCKET
  if (fLocalAddrStrPtr == nullptr) {
    // shouldn't happen but no match so it was probably a failed socket
    // connection or accept. addr is probably 0.

    fLocalAddrBuffer[0] = 0;
    fLocalAddrStrPtr = &fLocalAddrStr;
    struct in_addr theAddr;
    theAddr.s_addr = ntohl(fLocalAddr.sin_addr.s_addr);
    SocketUtils::ConvertAddrToString(theAddr, &fLocalAddrStr);

    printf("Socket::GetLocalAddrStr Search IPs failed, numIPs=%d\n",
           SocketUtils::GetNumIPAddrs());
    for (UInt32 x = 0; x < SocketUtils::GetNumIPAddrs(); x++) {
      printf("ip[%"   _U32BITARG_   "]=", x);
      SocketUtils::GetIPAddrStr(x)->PrintStr("\n");
    }
    printf("this ip = %d = ", theAddr.s_addr);
    fLocalAddrStrPtr->PrintStr("\n");

    if (theAddr.s_addr == 0 || fLocalAddrBuffer[0] == 0)
      fLocalAddrStrPtr = nullptr; // so the caller can test for failure
  }
#endif

  Assert(fLocalAddrStrPtr != nullptr);
  return fLocalAddrStrPtr;
}

CF::StrPtrLen *Socket::GetLocalDNSStr() {
  //Do the same thing as the above function, but for DNS names
  Assert(fLocalAddr.sin_addr.s_addr != INADDR_ANY);
  if (fLocalDNSStrPtr == nullptr) {
    for (UInt32 x = 0; x < SocketUtils::GetNumIPAddrs(); x++) {
      if (SocketUtils::GetIPAddr(x) == ntohl(fLocalAddr.sin_addr.s_addr)) {
        fLocalDNSStrPtr = SocketUtils::GetDNSNameStr(x);
        break;
      }
    }
  }

  //if we weren't able to get this DNS name, make the DNS name the same as the IP addr str.
  if (fLocalDNSStrPtr == nullptr)
    fLocalDNSStrPtr = this->GetLocalAddrStr();

  Assert(fLocalDNSStrPtr != nullptr);
  return fLocalDNSStrPtr;
}

CF::StrPtrLen *Socket::GetLocalPortStr() {
  if (fPortStr.Len == kPortBufSizeInBytes) {
    int temp = ntohs(fLocalAddr.sin_port);
    s_sprintf(fPortBuffer, "%d", temp);
    fPortStr.Len = static_cast<UInt32>(::strlen(fPortBuffer));
  }
  return &fPortStr;
}

OS_Error Socket::
Send(char const *inData, const UInt32 inLength, UInt32 *outLengthSent) {
  Assert(inData != nullptr);

  if (!(fState & kConnected))
    return (OS_Error) ENOTCONN;

  long err;
  do {
    err = ::send(fFileDesc, inData, inLength, 0);
  } while ((err == -1) && (Core::Thread::GetErrno() == EINTR));

  if (err == -1) {
    // Are there any errors that can happen if the client is connected?
    // Yes... EAGAIN. Means the Socket is now flow-controleld
    int theErr = Core::Thread::GetErrno();
    if ((theErr != EAGAIN) && (this->IsConnected()))
      fState ^= kConnected;//turn off connected state flag
    return (OS_Error) theErr;
  }

  *outLengthSent = static_cast<UInt32>(err);
  return OS_NoErr;
}

OS_Error Socket::
WriteV(const struct iovec *iov, const UInt32 numIOvecs, UInt32 *outLenSent) {
  Assert(iov != nullptr);

  if (!(fState & kConnected))
    return (OS_Error) ENOTCONN;

  long err;
  do {
#if __WinSock__
    DWORD theBytesSent = 0;
    err = ::WSASend(fFileDesc, (LPWSABUF)iov, numIOvecs, &theBytesSent, 0, nullptr, nullptr);
    if (err == 0)
        err = theBytesSent;
#else
    err = ::writev(fFileDesc, iov, numIOvecs); // return ssize_t
#endif
  } while ((err == -1) && (Core::Thread::GetErrno() == EINTR));

  if (err == -1) {
    // Are there any errors that can happen if the client is connected?
    // Yes... EAGAIN. Means the Socket is now flow-controlled
    int theErr = Core::Thread::GetErrno();
    if ((theErr != EAGAIN) && (this->IsConnected()))
      fState ^= kConnected;//turn off connected state flag
    return (OS_Error) theErr;
  }

  if (outLenSent != nullptr)
    *outLenSent = (UInt32) err;

  return OS_NoErr;
}

OS_Error Socket::Read(void *buffer, const UInt32 length, UInt32 *outRecvLenP) {
  Assert(outRecvLenP != nullptr);
  Assert(buffer != nullptr);

  if (!(fState & kConnected))
    return (OS_Error) ENOTCONN;

  //int theRecvLen = ::recv(fFileDesc, buffer, length, 0);//flags??
  long theRecvLen;
  do {
    theRecvLen = ::recv(fFileDesc, (char *) buffer, length, 0); //flags??
#if __WinSock__
    } while ((theRecvLen == SOCKET_ERROR) && (::WSAGetLastError() == WSAEINTR));

    if (theRecvLen == SOCKET_ERROR) {
      int theErr = ::WSAGetLastError();
      if ((theErr != WSAEWOULDBLOCK) && (this->IsConnected()))
#else
  } while ((theRecvLen == -1) && (Core::Thread::GetErrno() == EINTR));

  if (theRecvLen == -1) {
    // Are there any errors that can happen if the client is connected?
    // Yes... EAGAIN. Means the Socket is now flow-controled
    int theErr = Core::Thread::GetErrno();
    if ((theErr != EAGAIN) && (this->IsConnected()))
#endif
      fState ^= kConnected; // turn off connected state flag
    return (OS_Error) theErr;
  } else if (theRecvLen == 0) {
    // if we get 0 bytes back from read, that means the client has disconnected.
    // Note that and return the proper error to the caller
    fState ^= kConnected;
    return (OS_Error) ENOTCONN;
  }
  Assert(theRecvLen > 0);
  *outRecvLenP = (UInt32) theRecvLen;
  return OS_NoErr;
}
