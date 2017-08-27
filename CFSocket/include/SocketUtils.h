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
    File:       SocketUtils.h

    Contains:   Some static routines for dealing with networking

*/

#ifndef __SOCKET_UTILS_H__
#define __SOCKET_UTILS_H__

#include <OSHeaders.h>
#include <StrPtrLen.h>
#include <OSMutex.h>

#if !__WinSock__
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#endif

#ifdef __solaris__
#ifndef INADDR_NONE
#define INADDR_NONE     0xffffffff      /* -1 return from inet_addr */
#endif
#endif

class SocketUtils {
 public:

  /**
   * set global variables about local ip address.
   *
   * (pass true for lookupDNSName if you want the hostname
   *  looked up via DNS during initialization -- %%sfu)
   *
   * <b>note:</b> Call initialize before using any socket functions.
   */
  static void Initialize(bool lookupDNSName = true);

  //static utility routines
  static bool IsMulticastIPAddr(UInt32 inAddress);
  static bool IsLocalIPAddr(UInt32 inAddress);

  //This function converts an integer IP address to a dotted-decimal string.
  //This function is NOT THREAD SAFE!!!
  static void ConvertAddrToString(const struct in_addr &theAddr,
                                  StrPtrLen *outAddr);

  // This function converts a dotted-decimal string IP address to a UInt32
  static UInt32 ConvertStringToAddr(const char *inAddr);

  //You can get at all the IP addrs and DNS names on this machine this way
  static UInt32 GetNumIPAddrs() { return sNumIPAddrs; }
  static inline UInt32 GetIPAddr(UInt32 inAddrIndex);
  static inline StrPtrLen *GetIPAddrStr(UInt32 inAddrIndex);
  static inline StrPtrLen *GetDNSNameStr(UInt32 inDNSIndex);

 private:

  //Utility function used by Initialize
#if !__WinSock__
  static bool IncrementIfReqIter(char **inIfReqIter, ifreq *ifr);
#endif
  //For storing relevent information about each IP interface
  struct IPAddrInfo {
    UInt32 fIPAddr;
    StrPtrLen fIPAddrStr;
    StrPtrLen fDNSNameStr;
  };

  static IPAddrInfo *sIPAddrInfoArray;
  static UInt32 sNumIPAddrs;
  static OSMutex sMutex;
};

inline UInt32 SocketUtils::GetIPAddr(UInt32 inAddrIndex) {
  Assert(sIPAddrInfoArray != nullptr);
  Assert(inAddrIndex < sNumIPAddrs);
  return sIPAddrInfoArray[inAddrIndex].fIPAddr;
}

inline StrPtrLen *SocketUtils::GetIPAddrStr(UInt32 inAddrIndex) {
  Assert(sIPAddrInfoArray != nullptr);
  Assert(inAddrIndex < sNumIPAddrs);
  return &sIPAddrInfoArray[inAddrIndex].fIPAddrStr;
}

inline StrPtrLen *SocketUtils::GetDNSNameStr(UInt32 inDNSIndex) {
  Assert(sIPAddrInfoArray != nullptr);
  Assert(inDNSIndex < sNumIPAddrs);
  return &sIPAddrInfoArray[inDNSIndex].fDNSNameStr;
}

#endif // __SOCKET_UTILS_H__

