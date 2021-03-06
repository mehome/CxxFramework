//
// Created by james on 8/25/17.
//

#ifndef __CF_ENV_H__
#define __CF_ENV_H__

#include <CF/CFDef.h>
#include <CF/StrPtrLen.h>

namespace CF {

class CFConfigure;

namespace Net {
class TCPListenerSocket;
}

class CFEnv {
 public:

  /**
   * @brief initialize environment information
   *
   * @note this function is the first executor when program start
   */
  static void Initialize();

  /**
   * @brief 注册服务配置类
   *
   * @note configure 的内存将交由 CFEnv 管理
   */
  static void Register(CFConfigure *configure) { sConfigure = configure; }

  static CFConfigure *GetConfigure() { return sConfigure; }

  static CF_Error WillExit() { return sExitCode; };
  static void Exit(UInt32 exitCode) { sExitCode = exitCode; }

  static bool AddListenerSocket(Net::TCPListenerSocket *socket);


  // SERVER NAME & VERSION
  static StrPtrLen &GetServerName() { return sServerNameStr; }
  static StrPtrLen &GetServerVersion() { return sServerVersionStr; }
  static StrPtrLen &GetServerPlatform() { return sServerPlatformStr; }
  static StrPtrLen &GetServerBuildDate() { return sServerBuildDateStr; }
  static StrPtrLen &GetServerHeader() { return sServerHeaderStr; }
  static StrPtrLen &GetServerBuild() { return sServerBuildStr; }
  static StrPtrLen &GetServerComment() { return sServerCommentStr; }

 private:
  CFEnv() = default;

  static void makeServerHeader();

  static SInt32 sExitCode;

  static CFConfigure *sConfigure;

  enum {
    kMaxServerHeaderLen = 1000
  };

  static UInt32 sServerAPIVersion;
  static StrPtrLen sServerNameStr;
  static StrPtrLen sServerVersionStr;
  static StrPtrLen sServerBuildStr;
  static StrPtrLen sServerCommentStr;
  static StrPtrLen sServerPlatformStr;
  static StrPtrLen sServerBuildDateStr;
  static char sServerHeader[kMaxServerHeaderLen];
  static StrPtrLen sServerHeaderStr;
};

}

#endif //__CF_ENV_H__
