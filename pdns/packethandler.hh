/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002  PowerDNS.COM BV

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
#ifndef PACKETHANDLER_HH
#define PACKETHANDLER_HH

#ifndef WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif // WIN32

#include "ueberbackend.hh"
#include "dnspacket.hh"
#include "packetcache.hh"

using namespace std;

// silly Solaris people define PC
#undef PC

/** Central DNS logic according to RFC1034. Ask this class a question in the form of a DNSPacket
    and it will return, synchronously, a DNSPacket answer, suitable for 
    sending out over the network. 

    The PacketHandler gives your question to the PacketCache for possible inclusion
    in the cache.

    In order to do so, the PacketHandler contains a reference to the global extern PacketCache PC

    It also contains an UeberBackend instance for answering the subqueries needed to generate
    a complete reply.

*/

class PacketHandler
{
public:
  template<class T> class Guard
  {
  public:
    Guard(T **guard)
    {
      d_guard=guard;
    }
    
    ~Guard()
    {
      if(*d_guard)
	delete *d_guard;
    }
    
  private:
    T **d_guard;
  };

  DNSPacket *questionOrRecurse(DNSPacket *, bool* shouldRecurse); //!< hand us a DNS packet with a question, we'll tell you answer, or that you should recurse
  DNSPacket *question(DNSPacket *); //!< hand us a DNS packet with a question, we give you an answer
  PacketHandler(); 
  ~PacketHandler(); // defined in packethandler.cc, and does --count
  static int numRunning(){return s_count;}; //!< Returns the number of running PacketHandlers. Called by Distributor
 
  void soaMagic(DNSResourceRecord *rr);
  DNSBackend *getBackend();


private:
  int processNotify(DNSPacket *);
  void addRootReferral(DNSPacket *r);
  int trySuperMaster(DNSPacket *p);
  int makeCanonic(DNSPacket *p, DNSPacket *r, string &target);
  int doWildcardRecords(DNSPacket *p, DNSPacket *r, string &target);
  int findMboxFW(DNSPacket *p, DNSPacket *r, string &target);
  int findUrl(DNSPacket *p, DNSPacket *r, string &target);
  int doFancyRecords(DNSPacket *p, DNSPacket *r, string &target);
  int doDNSCheckRequest(DNSPacket *p, DNSPacket *r, string &target);
  int doVersionRequest(DNSPacket *p, DNSPacket *r, string &target);
  bool getAuth(DNSPacket *p, SOAData *sd, const string &target, int *zoneId);
  bool getTLDAuth(DNSPacket *p, SOAData *sd, const string &target, int *zoneId);
  int doAdditionalProcessingAndDropAA(DNSPacket *p, DNSPacket *r);
  
  static int s_count;
  bool d_doFancyRecords;
  bool d_doRecursion;
  bool d_doWildcards;
  bool d_doCNAME;
  bool d_logDNSDetails;
  bool d_doIPv6AdditionalProcessing;

  UeberBackend B; // every thread an own instance
};

#endif /* PACKETHANDLER */
