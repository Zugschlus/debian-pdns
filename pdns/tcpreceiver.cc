/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002-2007  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "utility.hh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <iostream>
#include <string>
#include "tcpreceiver.hh"

#include <errno.h>
#include <signal.h>

#include "ueberbackend.hh"
#include "dnspacket.hh"
#include "nameserver.hh"
#include "distributor.hh"
#include "lock.hh"
#include "logger.hh"
#include "arguments.hh"
#include "packetcache.hh"
#include "packethandler.hh"
#include "statbag.hh"
#include "resolver.hh"
#include "communicator.hh"
using namespace boost;

extern PacketCache PC;
extern StatBag S;

/**
\file tcpreceiver.cc
\brief This file implements the tcpreceiver that receives and answers questions over TCP/IP
*/

pthread_mutex_t TCPNameserver::s_plock = PTHREAD_MUTEX_INITIALIZER;
Semaphore *TCPNameserver::d_connectionroom_sem;
PacketHandler *TCPNameserver::s_P; 
int TCPNameserver::s_timeout;
NetmaskGroup TCPNameserver::d_ng;

void TCPNameserver::go()
{
  L<<Logger::Error<<"Creating backend connection for TCP"<<endl;
  s_P=0;
  try {
    s_P=new PacketHandler;
  }
  catch(AhuException &ae) {
    L<<Logger::Error<<Logger::NTLog<<"TCP server is unable to launch backends - will try again when questions come in"<<endl;
    L<<Logger::Error<<"TCP server is unable to launch backends - will try again when questions come in: "<<ae.reason<<endl;
  }
  pthread_create(&d_tid, 0, launcher, static_cast<void *>(this));
}

void *TCPNameserver::launcher(void *data)
{
  static_cast<TCPNameserver *>(data)->thread();
  return 0;
}

// throws AhuException if things didn't go according to plan, returns 0 if really 0 bytes were read
int readnWithTimeout(int fd, void* buffer, unsigned int n, bool throwOnEOF=true)
{
  unsigned int bytes=n;
  char *ptr = (char*)buffer;
  int ret;
  while(bytes) {
    ret=read(fd, ptr, bytes);
    if(ret < 0) {
      if(errno==EAGAIN) {
	ret=waitForData(fd, 5);
	if(ret < 0)
	  throw AhuException("Waiting for data read");
	if(!ret)
	  throw AhuException("Timeout reading data");
	continue;
      }
      else
	throw AhuException("Reading data: "+stringerror());
    }
    if(!ret) {
      if(!throwOnEOF && n == bytes)
	return 0;
      else
	throw AhuException("Did not fulfill read from TCP due to EOF");
    }
    
    ptr += ret;
    bytes -= ret;
  }
  return n;
}

// ditto
void writenWithTimeout(int fd, const void *buffer, unsigned int n)
{
  unsigned int bytes=n;
  const char *ptr = (char*)buffer;
  int ret;
  while(bytes) {
    ret=write(fd, ptr, bytes);
    if(ret < 0) {
      if(errno==EAGAIN) {
	ret=waitForRWData(fd, false, 5, 0);
	if(ret < 0)
	  throw AhuException("Waiting for data write");
	if(!ret)
	  throw AhuException("Timeout writing data");
	continue;
      }
      else
	throw AhuException("Writing data: "+stringerror());
    }
    if(!ret) {
      throw AhuException("Did not fulfill TCP write due to EOF");
    }
    
    ptr += ret;
    bytes -= ret;
  }
}

void connectWithTimeout(int fd, struct sockaddr* remote, size_t socklen)
{
  int err;
  Utility::socklen_t len=sizeof(err);

#ifndef WIN32
  if((err=connect(fd, remote, socklen))<0 && errno!=EINPROGRESS) 
#else
  if((err=connect(clisock, remote, socklen))<0 && WSAGetLastError() != WSAEWOULDBLOCK ) 
#endif // WIN32
    throw AhuException("connect: "+stringerror());

  if(!err)
    goto done;
  
  err=waitForRWData(fd, false, 5, 0);
  if(err == 0)
    throw AhuException("Timeout connecting to remote");
  if(err < 0)
    throw AhuException("Error connecting to remote");

  if(getsockopt(fd, SOL_SOCKET,SO_ERROR,(char *)&err,&len)<0)
    throw AhuException("Error connecting to remote: "+stringerror()); // Solaris

  if(err)
    throw AhuException("Error connecting to remote: "+string(strerror(err)));

 done:
  ;
}

void TCPNameserver::sendPacket(shared_ptr<DNSPacket> p, int outsock)
{
  const char *buf=p->getData();
  uint16_t len=htons(p->len);
  writenWithTimeout(outsock, &len, 2);
  writenWithTimeout(outsock, buf, p->len);
}


void TCPNameserver::getQuestion(int fd, char *mesg, int pktlen, const ComboAddress &remote)
try
{
  readnWithTimeout(fd, mesg, pktlen);
}
catch(AhuException& ae) {
    throw AhuException("Error reading DNS data from TCP client "+remote.toString()+": "+ae.reason);
}

static void proxyQuestion(shared_ptr<DNSPacket> packet)
{
  int sock=socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0)
    throw AhuException("Error making TCP connection socket to recursor: "+stringerror());

  Utility::setNonBlocking(sock);
  ServiceTuple st;
  st.port=53;
  parseService(arg()["recursor"],st);

  try {
    ComboAddress recursor(st.host, st.port);
    connectWithTimeout(sock, (struct sockaddr*)&recursor, recursor.getSocklen());
    const string &buffer=packet->getString();
    
    uint16_t len=htons(buffer.length()), slen;
    
    writenWithTimeout(sock, &len, 2);
    writenWithTimeout(sock, buffer.c_str(), buffer.length());
    
    int ret;
    
    ret=readnWithTimeout(sock, &len, 2);
    len=ntohs(len);

    char answer[len];
    ret=readnWithTimeout(sock, answer, len);

    slen=htons(len);
    writenWithTimeout(packet->getSocket(), &slen, 2);
    
    writenWithTimeout(packet->getSocket(), answer, len);
  }
  catch(AhuException& ae) {
    close(sock);
    throw AhuException("While proxying a question to recursor "+st.host+": " +ae.reason);
  }
  close(sock);
  return;
}

void *TCPNameserver::doConnection(void *data)
{
  shared_ptr<DNSPacket> packet;
  // Fix gcc-4.0 error (on AMD64)
  int fd=(int)(long)data; // gotta love C (generates a harmless warning on opteron)
  pthread_detach(pthread_self());
  Utility::setNonBlocking(fd);
  try {
    char mesg[512];
    
    DLOG(L<<"TCP Connection accepted on fd "<<fd<<endl);
    
    for(;;) {
      ComboAddress remote;
      socklen_t remotelen=sizeof(remote);
      if(getpeername(fd, (struct sockaddr *)&remote, &remotelen) < 0) {
	L<<Logger::Error<<"Received question from socket which had no remote address, dropping ("<<stringerror()<<")"<<endl;
	break;
      }

      uint16_t pktlen;
      if(!readnWithTimeout(fd, &pktlen, 2, false))
	break;
      else
	pktlen=ntohs(pktlen);

      if(pktlen>511) {
	L<<Logger::Error<<"Received an overly large question from "<<remote.toString()<<", dropping"<<endl;
	break;
      }
      
      getQuestion(fd,mesg,pktlen,remote);
      S.inc("tcp-queries");      

      packet=shared_ptr<DNSPacket>(new DNSPacket);
      packet->setRemote(&remote);
      packet->d_tcp=true;
      packet->setSocket(fd);
      if(packet->parse(mesg, pktlen)<0)
	break;
      
      if(packet->qtype.getCode()==QType::AXFR) {
	if(doAXFR(packet->qdomain, packet, fd)) 
	  S.inc("tcp-answers");  
	continue;
      }

      shared_ptr<DNSPacket> reply; 
      shared_ptr<DNSPacket> cached= shared_ptr<DNSPacket>(new DNSPacket);

      if(!packet->d.rd && (PC.get(packet.get(), cached.get()))) { // short circuit - does the PacketCache recognize this question?
	cached->setRemote(&packet->remote);
	cached->spoofID(packet->d.id);
	sendPacket(cached, fd);
	S.inc("tcp-answers");
	continue;
      }
	
      {
	Lock l(&s_plock);
	if(!s_P) {
	  L<<Logger::Error<<"TCP server is without backend connections, launching"<<endl;
	  s_P=new PacketHandler;
	}
	bool shouldRecurse;

	reply=shared_ptr<DNSPacket>(s_P->questionOrRecurse(packet.get(), &shouldRecurse)); // we really need to ask the backend :-)

	if(shouldRecurse) {
	  proxyQuestion(packet);
	  continue;
	}
      }

      if(!reply)  // unable to write an answer?
	break;
	
      S.inc("tcp-answers");
      sendPacket(reply, fd);
    }
  }
  catch(DBException &e) {
    Lock l(&s_plock);
    delete s_P;
    s_P = 0;

    L<<Logger::Error<<"TCP Connection Thread unable to answer a question because of a backend error, cycling"<<endl;
  }
  catch(AhuException &ae) {
    Lock l(&s_plock);
    delete s_P;
    s_P = 0; // on next call, backend will be recycled
    L<<Logger::Error<<"TCP nameserver had error, cycling backend: "<<ae.reason<<endl;
  }
  catch(exception &e) {
    L<<Logger::Error<<"TCP Connection Thread died because of STL error: "<<e.what()<<endl;
  }
  catch( ... )
  {
    L << Logger::Error << "TCP Connection Thread caught unknown exception." << endl;
  }
  d_connectionroom_sem->post();
  Utility::closesocket(fd);

  return 0;
}

bool TCPNameserver::canDoAXFR(shared_ptr<DNSPacket> q)
{
  if(arg().mustDo("disable-axfr"))
    return false;

  if( arg()["allow-axfr-ips"].empty() || d_ng.match( (ComboAddress *) &q->remote ) )
    return true;

  extern CommunicatorClass Communicator;

  if(Communicator.justNotified(q->qdomain, q->getRemote())) { // we just notified this ip 
    L<<Logger::Warning<<"Approved AXFR of '"<<q->qdomain<<"' from recently notified slave "<<q->getRemote()<<endl;
    return true;
  }

  return false;
}

/** do the actual zone transfer. Return 0 in case of error, 1 in case of success */
int TCPNameserver::doAXFR(const string &target, shared_ptr<DNSPacket> q, int outsock)
{
  shared_ptr<DNSPacket> outpacket;
  if(!canDoAXFR(q)) {
    L<<Logger::Error<<"AXFR of domain '"<<target<<"' denied to "<<q->getRemote()<<endl;

    outpacket=shared_ptr<DNSPacket>(q->replyPacket());
    outpacket->setRcode(RCode::Refused); 
    // FIXME: should actually figure out if we are auth over a zone, and send out 9 if we aren't
    sendPacket(outpacket,outsock);
    return 0;
  }
  L<<Logger::Error<<"AXFR of domain '"<<target<<"' initiated by "<<q->getRemote()<<endl;
  outpacket=shared_ptr<DNSPacket>(q->replyPacket());

  DNSResourceRecord soa;  
  DNSResourceRecord rr;

  SOAData sd;
  sd.db=(DNSBackend *)-1; // force uncached answer
  {
    Lock l(&s_plock);
    
    // find domain_id via SOA and list complete domain. No SOA, no AXFR
    
    DLOG(L<<"Looking for SOA"<<endl);
    if(!s_P) {
      L<<Logger::Error<<"TCP server is without backend connections in doAXFR, launching"<<endl;
      s_P=new PacketHandler;
    }

    if(!s_P->getBackend()->getSOA(target,sd)) {
      L<<Logger::Error<<"AXFR of domain '"<<target<<"' failed: not authoritative"<<endl;
      outpacket->setRcode(9); // 'NOTAUTH'
      sendPacket(outpacket,outsock);
      return 0;
    }

  }
  PacketHandler P; // now open up a database connection, we'll need it

  sd.db=(DNSBackend *)-1; // force uncached answer
  if(!P.getBackend()->getSOA(target, sd)) {
      L<<Logger::Error<<"AXFR of domain '"<<target<<"' failed: not authoritative in second instance"<<endl;
    outpacket->setRcode(9); // 'NOTAUTH'
    sendPacket(outpacket,outsock);
    return 0;
  }

  soa.qname=target;
  soa.qtype=QType::SOA;
  soa.content=serializeSOAData(sd);
  soa.ttl=sd.ttl;
  soa.domain_id=sd.domain_id;
  soa.d_place=DNSResourceRecord::ANSWER;
    
  if(!sd.db || sd.db==(DNSBackend *)-1) {
    L<<Logger::Error<<"Error determining backend for domain '"<<target<<"' trying to serve an AXFR"<<endl;
    outpacket->setRcode(RCode::ServFail);
    sendPacket(outpacket,outsock);
    return 0;
  }
 
  DLOG(L<<"Issuing list command - opening dedicated database connection"<<endl);

  DNSBackend *B=sd.db; // get the RIGHT backend

  // now list zone
  if(!(B->list(target, sd.domain_id))) {  
    L<<Logger::Error<<"Backend signals error condition"<<endl;
    outpacket->setRcode(2); // 'SERVFAIL'
    sendPacket(outpacket,outsock);
    return 0;
  }
  /* write first part of answer */

  DLOG(L<<"Sending out SOA"<<endl);
  outpacket->addRecord(soa); // AXFR format begins and ends with a SOA record, so we add one
  sendPacket(outpacket, outsock);

  /* now write all other records */

  int count=0;
  int chunk=100; // FIXME: this should probably be autosizing
  if(arg().mustDo("strict-rfc-axfrs"))
    chunk=1;

  outpacket=shared_ptr<DNSPacket>(q->replyPacket());
  outpacket->setCompress(false);

  while(B->get(rr)) {
    if(rr.qtype.getCode()==6)
      continue; // skip SOA - would indicate end of AXFR

    outpacket->addRecord(rr);

    if(!((++count)%chunk)) {
      count=0;
    
      sendPacket(outpacket, outsock);

      outpacket=shared_ptr<DNSPacket>(q->replyPacket());
      outpacket->setCompress(false);
      // FIXME: Subsequent messages SHOULD NOT have a question section, though the final message MAY.
    }
  }
  if(count) {
    sendPacket(outpacket, outsock);
  }

  DLOG(L<<"Done writing out records"<<endl);
  /* and terminate with yet again the SOA record */
  outpacket=shared_ptr<DNSPacket>(q->replyPacket());
  outpacket->addRecord(soa);
  sendPacket(outpacket, outsock);
  DLOG(L<<"last packet - close"<<endl);
  L<<Logger::Error<<"AXFR of domain '"<<target<<"' to "<<q->getRemote()<<" finished"<<endl;

  return 1;
}

TCPNameserver::~TCPNameserver()
{
  delete d_connectionroom_sem;
}

TCPNameserver::TCPNameserver()
{
//  sem_init(&d_connectionroom_sem,0,arg().asNum("max-tcp-connections"));
  d_connectionroom_sem = new Semaphore( arg().asNum( "max-tcp-connections" ));

  s_timeout=10;
  vector<string>locals;
  stringtok(locals,arg()["local-address"]," ,");

  vector<string>locals6;
  stringtok(locals6,arg()["local-ipv6"]," ,");

  if(locals.empty() && locals6.empty())
    throw AhuException("No local address specified");

  d_highfd=0;

  vector<string> parts;
  stringtok( parts, arg()["allow-axfr-ips"], ", \t" ); // is this IP on the guestlist?
  for( vector<string>::const_iterator i = parts.begin(); i != parts.end(); ++i ) {
    d_ng.addMask( *i );
  }

#ifndef WIN32
  signal(SIGPIPE,SIG_IGN);
#endif // WIN32
  FD_ZERO(&d_rfds);  

  for(vector<string>::const_iterator laddr=locals.begin();laddr!=locals.end();++laddr) {
    int s=socket(AF_INET,SOCK_STREAM,0); 

    if(s<0) 
      throw AhuException("Unable to acquire TCP socket: "+stringerror());

    ComboAddress local(*laddr, arg().asNum("local-port"));
      
    int tmp=1;
    if(setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char*)&tmp,sizeof tmp)<0) {
      L<<Logger::Error<<"Setsockopt failed"<<endl;
      exit(1);  
    }

    if(bind(s, (sockaddr*)&local, local.getSocklen())<0) {
      L<<Logger::Error<<"binding to TCP socket: "<<strerror(errno)<<endl;
      throw AhuException("Unable to bind to TCP socket");
    }
    
    listen(s,128);
    L<<Logger::Error<<"TCP server bound to "<<local.toStringWithPort()<<endl;
    d_sockets.push_back(s);
    FD_SET(s, &d_rfds);
    d_highfd=max(s,d_highfd);
  }

#if !WIN32 && HAVE_IPV6
  for(vector<string>::const_iterator laddr=locals6.begin();laddr!=locals6.end();++laddr) {
    int s=socket(AF_INET6,SOCK_STREAM,0); 

    if(s<0) 
      throw AhuException("Unable to acquire TCPv6 socket: "+stringerror());

    ComboAddress local(*laddr, arg().asNum("local-port"));

    int tmp=1;
    if(setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char*)&tmp,sizeof tmp)<0) {
      L<<Logger::Error<<"Setsockopt failed"<<endl;
      exit(1);  
    }

    if(bind(s, (const sockaddr*)&local, local.getSocklen())<0) {
      L<<Logger::Error<<"binding to TCP socket: "<<strerror(errno)<<endl;
      throw AhuException("Unable to bind to TCPv6 socket");
    }
    
    listen(s,128);
    L<<Logger::Error<<"TCPv6 server bound to "<<local.toStringWithPort()<<endl;
    d_sockets.push_back(s);
    FD_SET(s, &d_rfds);
    d_highfd=max(s,d_highfd);
  }
#endif // WIN32
}


//! Start of TCP operations thread, we launch a new thread for each incoming TCP question
void TCPNameserver::thread()
{
  struct timeval tv;
  tv.tv_sec=1;
  tv.tv_usec=0;
  try {
    for(;;) {
      int fd;
      struct sockaddr_in remote;
      Utility::socklen_t addrlen=sizeof(remote);

      fd_set rfds=d_rfds; 

      int ret=select(d_highfd+1, &rfds, 0, 0,  0); // blocks, forever if need be
      if(ret <= 0)
	continue;

      int sock=-1;
      for(vector<int>::const_iterator i=d_sockets.begin();i!=d_sockets.end();++i) {
	if(FD_ISSET(*i, &rfds)) {
	  sock=*i;
	  addrlen=sizeof(remote);

	  if((fd=accept(sock, (sockaddr*)&remote, &addrlen))<0) {
	    L<<Logger::Error<<"TCP question accept error: "<<strerror(errno)<<endl;
	    
	    if(errno==EMFILE) {
	      L<<Logger::Error<<Logger::NTLog<<"TCP handler out of filedescriptors, exiting, won't recover from this"<<endl;
	      exit(1);
	    }
	  }
	  else {
	    pthread_t tid;
	    d_connectionroom_sem->wait(); // blocks if no connections are available

	    int room;
	    d_connectionroom_sem->getValue( &room);
	    if(room<1)
	      L<<Logger::Warning<<Logger::NTLog<<"Limit of simultaneous TCP connections reached - raise max-tcp-connections"<<endl;

	    if(pthread_create(&tid, 0, &doConnection, (void *)fd)) {
	      L<<Logger::Error<<"Error creating thread: "<<stringerror()<<endl;
	      d_connectionroom_sem->post();
	    }
	  }
	}
      }
    }
  }
  catch(AhuException &AE) {
    L<<Logger::Error<<"TCP Namerserver thread dying because of fatal error: "<<AE.reason<<endl;
  }
  catch(...) {
    L<<Logger::Error<<"TCPNameserver dying because of an unexpected fatal error"<<endl;
  }
  exit(1); // take rest of server with us
}


