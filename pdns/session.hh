/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef SESSION_HH
#define SESSION_HH

#include <string>
#include <cerrno>

#ifndef WIN32
# include <sys/stat.h>
# include <netdb.h>
# include <unistd.h>
# include <sys/time.h>
# include <sys/socket.h>
# include <arpa/inet.h>  
# include <netinet/in.h>
# include <sys/types.h>
# include <strings.h>

#endif // WIN32

#include "ahuexception.hh"

class SessionException: public AhuException
{
public:
  SessionException(const string &reason) : AhuException(reason){}
};

class SessionTimeoutException: public SessionException
{
public:
  SessionTimeoutException(const string &reason) : SessionException(reason){}
};

//! The Session class represents a TCP/IP session, which can either be created or run on an existing socket
class Session
{
public:
  bool getLine(string &); //!< Read a line from the remote
  bool haveLine(); //!< returns true if a line is available
  bool putLine(const string &s); //!< Write a line to the remote
  bool sendFile(int fd); //!< Send a file out
  int timeoutRead(int s,char *buf, size_t len);

  Session(int s, struct sockaddr_in r); //!< Start a session on an existing socket, and inform this class of the remotes name

  /** Create a session to a remote host and port. This function reads a timeout value from the ArgvMap class 
      and does a nonblocking connect to support this timeout. It should be noted that nonblocking connects 
      suffer from bad portability problems, so look here if you see weird problems on new platforms */
  Session(const string &remote, int port, int timeout=0); 
  Session(u_int32_t ip, int port, int timeout=0);

  Session(const Session &s); 
  
  ~Session();
  int getSocket(); //!< return the filedescriptor for layering violations
  string getRemote();
  u_int32_t getRemoteAddr();
  string getRemoteIP();
  void beVerbose();
  int close(); //!< close and disconnect the connection
  void setTimeout(unsigned int seconds);
private:
  void doConnect(u_int32_t ip, int port);
  bool d_verbose;
  char *rdbuf;
  int d_bufsize;
  int rdoffset;
  int wroffset;
  int clisock;
  struct sockaddr_in remote;
  void init();
  int d_timeout;

};

//! The server class can be used to create listening servers
class Server
{
public:
  Server(int p, const string &localaddress=""); //!< port on which to listen
  Session* accept(); //!< Call accept() in an endless loop to accept new connections
private:
  int s;
  int port;
  int backlog;

  string d_localaddress;
};

class Exception
{
public:
  Exception(){reason="Unspecified";};
  Exception(string r){reason=r;};
  string reason;
};

#endif /* SESSION_HH */
