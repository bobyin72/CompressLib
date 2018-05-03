#ifndef _MY_SCOKET_UTIL_
#define _MY_SCOKET_UTIL_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h> 


#pragma warning(disable:4786) 

#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <deque>
#include <set>
#include <utility>
#include <hash_map> 

using namespace std; 


#ifdef WIN32    //WINDOWS
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <process.h>
#include <winsock.h> 

#define socklen_t int
#define bzero(a,b) (memset((a),0,(b))) 

#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#ifndef ENAMETOOLONG
//#define ENAMETOOLONG            WSAENAMETOOLONG
#endif
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#ifndef ENOTEMPTY
//#define ENOTEMPTY               WSAENOTEMPTY
#endif
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE 

#else           //LINUX 

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h> 

#define SOCKET int
#define WSAGetLastError()       errno 

#define INVALID_SOCKET          (-1)
#define SOCKET_ERROR            (-1) 

#endif 

#define TIMEOUT_NONE 0
#define TIMEOUT_ALL  1
#define TIMEOUT_ACCEPT 2
#define TIMEOUT_SEND 3
#define TIMEOUT_RECV 4
#define TIMEOUT_CONNECT 5 

#define ALLTIMEOUT  120*1000     //120 seconds
#define RECVTIMEOUT     120*1000     
#define SENDTIMEOUT     120*1000     
#define ACCPTTIMEOUT    120*1000     
#define CONNECTTIMEOUT  120*1000     
#define MAXBUFFERSIZE   65536
#define LISTENQNUM      128 

class MSockAddr
{
public:
 MSockAddr()
    {
    }
    MSockAddr(sockaddr_in addr) :
        m_addr(addr)
    {
    }
    MSockAddr(char* lpszIp, unsigned short port)
    {
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_addr.s_addr = inet_addr(lpszIp);
        m_addr.sin_port = htons(port);
    }
    MSockAddr(unsigned long Ip, unsigned short port)
    {
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_addr.s_addr = htonl(Ip);
        m_addr.sin_port = htons(port);
    }
 MSockAddr(string& host,unsigned short port)
 {
  memset(&m_addr,0,sizeof(m_addr));
  m_addr.sin_family=AF_INET;
  if(host=="")
   m_addr.sin_addr.s_addr =INADDR_ANY;
  else if ( (m_addr.sin_addr.s_addr = inet_addr(host.c_str ())) == (unsigned long)(-1))
  {
   hostent* hp = gethostbyname(host.c_str ());
   if (hp == 0) 
    return;
   memcpy(&(m_addr.sin_addr.s_addr), hp->h_addr, hp->h_length);
  }
  m_addr.sin_port = htons(port);
 } 

 char* GetIPStr() const
 {
  return inet_ntoa(m_addr.sin_addr);
 } 

 string ToString()
 {
  char result[256];
  sprintf(result,"%u.%u.%u.%u:%d",m_addr.sin_addr.s_addr&0xff,(m_addr.sin_addr.s_addr>>8)&0xff,(m_addr.sin_addr.s_addr>>16)&0xff,(m_addr.sin_addr.s_addr>>24)&0xff,GetPort());
  return string(result);
 }
 
 unsigned long GetIP() const {return ntohl(m_addr.sin_addr.s_addr);}
 unsigned short  GetPort()const {return ntohs(m_addr.sin_port);}
 
 sockaddr_in  GetAddr(){return m_addr;}
 void    SetAddr(sockaddr_in addr){m_addr = addr;} 

 bool operator==(const MSockAddr cmper) const
 {
  return (GetIP()==cmper.GetIP())&&(GetPort()==cmper.GetPort());
 } 

 bool operator!=(const MSockAddr cmper) const
 {
  return (GetIP()!=cmper.GetIP())||(GetPort()!=cmper.GetPort());
 } 

 void operator=(MSockAddr cmper)
 {
  m_addr = cmper.GetAddr();
 } 

private:
 sockaddr_in m_addr;
}; 

class MSockErr {
 int  m_err;
public:
 MSockErr (): m_err (0) {}
 MSockErr (int e): m_err (e) {}
 int ErrCode() const { return m_err; }
 const char* ToString() const
 {
#ifdef WIN32
  LPVOID lpMsgBuf;
  FormatMessage( 
   FORMAT_MESSAGE_ALLOCATE_BUFFER | 
   FORMAT_MESSAGE_FROM_SYSTEM | 
   FORMAT_MESSAGE_IGNORE_INSERTS,
   NULL,
   m_err,
   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
   (LPTSTR) &lpMsgBuf,
   0,
   NULL 
  );
  return (char*)lpMsgBuf;
#else
  return strerror(m_err);
#endif
 }
}; 

class MBlockSocket  
{
public:
    MBlockSocket():
  _bUDP(false),
        _binit(false),
        _bkilled(false),
        _bBlock(true),
  _nTimeOut(0),
  _socket(-1),
  _nTimeOutType(TIMEOUT_NONE)
    {
  Init();
    }
 MBlockSocket(bool bUDP):
  _bUDP(bUDP),
        _binit(false),
        _bkilled(false),
        _bBlock(true),
  _nTimeOut(0),
  _socket(-1),
  _nTimeOutType(TIMEOUT_NONE)
    {
  Init();
  if (_bUDP)
   Create(SOCK_DGRAM);
  else
   Create();
    }
 virtual ~MBlockSocket()
    {
        Close();
    } 

 void Init()
 {
  m_tLastPacket = time(NULL);
  m_nSendSpeedBs =0;
        m_nRecvSpeedBs =0;
  m_nSendBytesCount = 0;
  m_nRecvBytesCount = 0;
  m_nMaxSendBs = 0x7FFFFFFF;
        m_nMaxRecvBs = 0x7FFFFFFF;
        m_nRecvBytesCount =0;
        m_nSendBytesCount =0;
 } 

 bool _bUDP;
    bool    _binit;
    bool    _bkilled;
    bool    _bBlock;
 int  _nTimeOut;
 int  _nTimeOutType;
    SOCKET  _socket; 

    struct sockaddr_in _cliaddr;         //host be a server
    MSockAddr _servaddr;
    struct sockaddr_in _remaddr;         //host be a client
    MSockAddr _locaaddr;    

 static bool InitSocketSystem()
 {
#ifdef WIN32
  WSADATA wsadata;   
  unsigned short winsock_version=0x0101;
  if(WSAStartup(winsock_version,&wsadata))
  { 
   return false;
  }
#endif
  return true;
 } 

    int SetBlock(bool bBlock = true)
    {
        int bufsize = MAXBUFFERSIZE;
     setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize));
     setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, sizeof(bufsize));
        
        int rt;
        if (!bBlock)    //Nonblocking mode
        {
            unsigned long rb = 1; //nonzero
#ifdef WIN32
      rt = ioctlsocket(_socket, FIONBIO, &rb);
#else
      rt = ioctl(_socket, FIONBIO, &rb); 
#endif
        }
        return rt;
    } 

 time_t m_tLastPacket;
 //! Last send count time
 time_t m_tLastSendCount;
 time_t m_tLastRecvCount;
 //! Max send speed in one second.
 unsigned long m_nMaxSendBs;
 unsigned long m_nMaxRecvBs;
 //! count bytes since last send count time
 unsigned long m_nSendBytesCount;
 unsigned long m_nRecvBytesCount;
 //! current send speed,Bs
 unsigned long m_nSendSpeedBs;
 unsigned long m_nRecvSpeedBs; 

 void SetSendSpeed(unsigned long MaxSendBs)
 {
  if(MaxSendBs)
   m_nMaxSendBs=MaxSendBs;
 } 

 void SetRecvSpeed(unsigned long MaxRecvBs)
 {
  if(MaxRecvBs)
   m_nMaxRecvBs=MaxRecvBs;
 }
 unsigned long GetSendSpeed()
 {
  if(m_nSendSpeedBs==0)
  {
   if(time(NULL)!=m_tLastSendCount)
    m_nSendSpeedBs=m_nSendBytesCount/(time(NULL)-m_tLastSendCount);
   else
    m_nSendSpeedBs=m_nSendBytesCount;
  }
  return m_nSendSpeedBs;
 } 

    int SetTimeOut(int ms = 0, int type = TIMEOUT_ALL)
    {
        if (ms != 0){
            _nTimeOut = ms;
        }
        else {
            _nTimeOut = ALLTIMEOUT;
        }
        
  return _nTimeOut;
    } 

    inline int Setsockopt(int level, int optname, const char* optval, int optlen)
    {
        return setsockopt(_socket, level, optname, optval, optlen);
    } 

    bool Create(int type = SOCK_STREAM)
    {
        _socket = socket(AF_INET, type, 0); 

        if (_socket != INVALID_SOCKET){
            _binit = true;
   if (type != SOCK_STREAM)
    _bUDP = true;
            return true;
        }
        return false;
    } 

    int Close()
    {
        if (!_binit)
            return 0;
        _binit = false;
#ifdef WIN32
        return closesocket(_socket);
#else
        return close(_socket);
#endif
    } 

    void Attach(SOCKET socket)
    {
        Close();
        _socket = socket; 

        if (socket != INVALID_SOCKET)
            _binit = true;
    } 

    SOCKET Detach()
    {
        SOCKET rt = _socket; 

        _binit = false;
        _socket = INVALID_SOCKET; 

        return rt;
    }
    
 //If no error occurs, this function returns zero. 
 //If an error occurs, it returns SOCKET_ERROR, 
 //and a specific error code can be retrieved by calling WSAGetLastError.
    int Bind(const sockaddr* name, int namelen)
    {
        int flag = 1; 

        setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR , (char*)&flag, sizeof(flag)); 

        return bind(_socket, name, namelen);
    } 

    int Bind(unsigned short port, char* lpszIp = NULL)
    {
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        if (lpszIp == NULL)
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        else
            addr.sin_addr.s_addr = inet_addr(lpszIp);
        addr.sin_port = htons(port); 

        return Bind((sockaddr*)&addr, sizeof(addr));
    }
    
 //If no error occurs, listen returns zero. Otherwise, a value of SOCKET_ERROR is returned
    int Listen(int backlog = LISTENQNUM)
    {
        return listen(_socket, backlog);
    }
    
    //prt should be delete by caller
    MBlockSocket* Accept()
    {
        socklen_t clilen;
        struct sockaddr_in cliaddr;
        clilen = sizeof(cliaddr); 

        SOCKET connfd;
  
  if (_nTimeOut != 0)
  {
   fd_set rset;
   struct timeval tv; 

   FD_ZERO(&rset);
   FD_SET(_socket, &rset); 

   tv.tv_sec = _nTimeOut / 1000;
   tv.tv_usec = _nTimeOut % 1000; 

   if (select(_socket+1, &rset, NULL, NULL, &tv) <= 0)
    return NULL;
  } 

        if ( (connfd = accept(_socket, (sockaddr*)&cliaddr, &clilen)) == INVALID_SOCKET )
        {
            return NULL;
        }
        
        MBlockSocket* prt = new MBlockSocket;
        prt->Attach(connfd);
        _cliaddr = cliaddr;
        
        return prt;
    }
    
 //If no error occurs, connect returns zero. Otherwise, it returns SOCKET_ERROR
    int Connect(const struct sockaddr* name, int namelen)
    {
        return connect(_socket, name, namelen);
    } 

 int Connect(MSockAddr& addr)
 {
  return Connect(addr.GetIPStr(), addr.GetPort());
 } 

    int Connect(const char* lpszIp, unsigned short port)
    {
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(lpszIp);
        addr.sin_port = htons(port); 

        return Connect((sockaddr*)&addr, sizeof(addr));
    } 

 int Select()
 {
  fd_set wset, rset;
  struct timeval tv; 

  FD_ZERO(&wset);
  FD_ZERO(&rset);
  FD_SET(_socket, &wset);
  FD_SET(_socket, &rset);
  
  tv.tv_sec = 1;
  tv.tv_usec = 0; 

  if (select(_socket+1, &rset, &wset, NULL, &tv) <= 0)
   return SOCKET_ERROR; 

  if (FD_ISSET(_socket, &rset))
   return 1;
  if (FD_ISSET(_socket, &wset))
   return 2; 

  return SOCKET_ERROR;
  
 }
    
 bool WaitSendable(int second)
 {
  if (_socket == -1)
   return false;
  fd_set fd;
  FD_ZERO(&fd);
  FD_SET(_socket, &fd);
  struct timeval  val = {second,0};
  int selret = select(_socket+1,&fd,NULL,NULL,&val);
  if(selret <= 0)
   return false;
  return true;
 } 

    int Send(const char* buf, int len)
    {
  if (_nTimeOut != 0)
  {
   fd_set wset;
   struct timeval tv; 

   FD_ZERO(&wset);
   FD_SET(_socket, &wset); 

   tv.tv_sec = _nTimeOut / 1000;
   tv.tv_usec = _nTimeOut % 1000; 

   if (select(_socket+1, NULL, &wset, NULL, &tv) <= 0)
    return SOCKET_ERROR;
  } 

        return send(_socket, buf, len, 0);
    } 

    int SafeSend(const char* buf, int len)
    {
        int send_count = 0;
        const char* pbuf = buf;
        while (len > 0)
        {
            int rt = Send(pbuf, len);
            if (rt < 0){
                break;
            }
            else if (rt == 0){
                break;
            }
            send_count += rt;
            len -= rt;
            pbuf += rt;
        }
        return send_count;
    }
    
    int Recv(char* buf, int len)
    {
  if (_nTimeOut != 0)
  {
   fd_set rset;
   struct timeval tv; 

   FD_ZERO(&rset);
   FD_SET(_socket, &rset); 

   tv.tv_sec = _nTimeOut / 1000;
   tv.tv_usec = _nTimeOut % 1000; 

   if (select(_socket+1, &rset, NULL, NULL, &tv) <= 0)
    return SOCKET_ERROR;
  } 

        return recv(_socket, buf, len, 0);
    } 

    int SafeRecv(char* buf, int len)
    {
        int recv_count = 0;
        char* pbuf = buf;
        while (len > 0)
        {
            int rt = Recv(pbuf, len);
            if (rt < 0){
                break;
            }
            else if (rt == 0){
                break;
            }
            recv_count += rt;
            len -= rt;
            pbuf += rt;
        }
        return recv_count;
    } 

    int Shutdown(int how = 1 ) //receives = 0, sends = 1, both = 2
    {
        return shutdown(_socket, how);
    } 

    MSockAddr GetPeerName()
    {
        if (_binit){
   sockaddr_in addr_inet;
   socklen_t addrlen = sizeof(addr_inet);
   getpeername(_socket, (sockaddr*)&addr_inet, &addrlen);
   _cliaddr = addr_inet;
   return MSockAddr(addr_inet);
  }
  return MSockAddr();
    } 

 MSockAddr GetLocalName()
 {
  if( _binit)
  {
   sockaddr_in addr_inet;
   socklen_t addrlen=sizeof(addr_inet);
   getsockname(_socket,(sockaddr*)&addr_inet,&addrlen);
   _locaaddr = MSockAddr(addr_inet);
   return _locaaddr;
  }
  return MSockAddr();
 } 

    int RecvPacket(char* &buf, int len = 0)
    {
        bool bInterBuf = false;
        if (len == 0) {
            bInterBuf = true;
        }
        
        char* pbuf = new char[sizeof(len)];
        int rt = SafeRecv(pbuf, sizeof(len));
        if (rt != sizeof(len))
            return SOCKET_ERROR;
        memcpy((void*)&len, pbuf, sizeof(len));
        delete[] pbuf;
        
        if (bInterBuf) {
            buf = new char[len]; //should be delete outside
        } 

        return SafeRecv(buf, len);
    } 

    int SendPacket(const char* buf, int len)
    {
        char* pbuf = new char[sizeof(len)];
        int rt; 

        memcpy(pbuf, (void*)&len, sizeof(len));
        
        rt = SafeSend(pbuf, sizeof(len));
        delete[] pbuf;
        if (rt != sizeof(len))
            return SOCKET_ERROR;
        rt = SafeSend(buf, len);
        return rt;
    } 

 int RecvFrom(char* buf, int len, MSockAddr* RemoteAddr)
 {
  int rt;
  sockaddr_in addr_inet;
  socklen_t addrlen = sizeof(addr_inet); 

  rt = recvfrom(_socket, buf, len, 0, (sockaddr*)&addr_inet, &addrlen); 

  RemoteAddr->SetAddr(addr_inet); 

  return rt;  
 } 

 int SendTo(const char* buf, int len, MSockAddr RemoteAddr)
 {
  int rt;
  sockaddr_in addr_inet;
  addr_inet = RemoteAddr.GetAddr();
  socklen_t addrlen = sizeof(addr_inet); 

  rt = sendto(_socket, buf, len, 0, (sockaddr*)&addr_inet, addrlen); 

  return rt;  
 } 

};
#endif