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
#ifndef DYNMESSENGER_HH
#define DYNMESSENGER_HH

#include <string>
#include <sys/types.h>

#ifndef WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/un.h>
# include <unistd.h>
# include <libgen.h>

#else
# include "pdnsservice.hh"

#endif // WIN32

#include <errno.h>
#include "ahuexception.hh"

using namespace std;

//! The DynMessenger can send messages to UNIX domain sockets
class DynMessenger
{
  int d_s;

#ifndef WIN32
  struct sockaddr_un d_local; // our local address
  struct sockaddr_un d_remote; // our remote address

#else
  HANDLE m_pipeHandle; // Named pipe handle.

#endif // WIN32
  

  DynMessenger(const DynMessenger &); // NOT IMPLEMENTED
  
public:
  // CREATORS

  DynMessenger(const string &ldir, const string &filename);  //!< Create a DynMessenger sending to this file
  ~DynMessenger();

  // ACCESSORS
  int send(const string &message) const; //!< Send a message to a DynListener
  string receive() const; //!< receive an answer from a DynListener
};
 
#endif
