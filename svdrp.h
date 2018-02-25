/*
 * Copied shamelessly from VDR svdrp.c
 */

#ifndef SVDRP_H_
#define SVDRP_H_

#include <netinet/in.h>
#include <vdr/tools.h>


enum eSvdrpFetchFlags {
  sffNone   = 0b0000,
  sffTimers = 0b0001,
  };


class cIpAddress {
private:
  cString address;
  int port;
  cString connection;
public:
  cIpAddress(void);
  cIpAddress(const char *Address, int Port);
  const char *Address(void) const { return address; }
  int Port(void) const { return port; }
  void Set(const char *Address, int Port);
  void Set(const sockaddr *SockAddr);
  const char *Connection(void) const { return connection; }
  };

class cSocket {
private:
  int port;
  bool tcp;
  int sock;
  cIpAddress lastIpAddress;
  bool IsOwnInterface(sockaddr_in *Addr);
public:
  cSocket(int Port, bool Tcp);
  ~cSocket();
  bool Listen(void);
  bool Connect(const char *Address);
  void Close(void);
  int Port(void) const { return port; }
  int Socket(void) const { return sock; }
  static bool SendDgram(const char *Dgram, int Port, const char *Address = NULL);
  int Accept(void);
  cString Discover(void);
  const cIpAddress *LastIpAddress(void) const { return &lastIpAddress; }
  };

class cSVDRPClient {
private:
  cIpAddress ipAddress;
  cSocket socket;
  cString serverName;
  int timeout;
  cTimeMs pingTime;
  cFile file;
  int fetchFlags;
  void Close(void);
public:
  cSVDRPClient(const char *Address, int Port, const char *ServerName, int Timeout);
  ~cSVDRPClient();
  const char *ServerName(void) const { return serverName; }
  const char *Connection(void) const { return ipAddress.Connection(); }
  bool HasAddress(const char *Address, int Port) const;
  bool Send(const char *Command);
  bool Process(cStringList *Response = NULL);
  bool Execute(const char *Command, cStringList *Response = NULL);
  void SetFetchFlag(eSvdrpFetchFlags Flag);
  bool HasFetchFlag(eSvdrpFetchFlags Flag);
  };

#endif /* SVDRP_H_ */
