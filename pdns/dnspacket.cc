/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2001 - 2007  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as 
    published by the Free Software Foundation

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

#include <cstdlib>
#include <sys/types.h>

#include <iostream>  

#include <string>
#include <errno.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>

#include "dns.hh"
#include "dnsbackend.hh"
#include "ahuexception.hh"
#include "dnspacket.hh"
#include "logger.hh"
#include "arguments.hh"
#include "dnswriter.hh"
#include "dnsparser.hh"

DNSPacket::DNSPacket() 
{
  d_wrapped=false;
  d_compress=true;
  d_tcp=false;
}

string DNSPacket::getString()
{
  return stringbuffer;
}

const char *DNSPacket::getData(void)
{
  if(!d_wrapped)
    wrapup();

  return stringbuffer.data();
}

const char *DNSPacket::getRaw(void)
{
  return stringbuffer.data();
}

string DNSPacket::getRemote() const
{
  return remote.toString();
}

uint16_t DNSPacket::getRemotePort() const
{
  return remote.sin4.sin_port;
}

DNSPacket::DNSPacket(const DNSPacket &orig)
{
  DLOG(L<<"DNSPacket copy constructor called!"<<endl);
  d_socket=orig.d_socket;
  remote=orig.remote;
  len=orig.len;
  d_qlen=orig.d_qlen;
  d_dt=orig.d_dt;
  d_compress=orig.d_compress;
  d_tcp=orig.d_tcp;
  qtype=orig.qtype;
  qclass=orig.qclass;
  qdomain=orig.qdomain;

  rrs=orig.rrs;

  d_wrapped=orig.d_wrapped;

  stringbuffer=orig.stringbuffer;
  d=orig.d;
}

void DNSPacket::setRcode(int v)
{
  d.rcode=v;
}

void DNSPacket::setAnswer(bool b)
{
  if(b) {
    stringbuffer.assign(12,(char)0);
    memset((void *)&d,0,sizeof(d));
    
    d.qr=b;
  }
}

void DNSPacket::setA(bool b)
{
  d.aa=b;
}

void DNSPacket::setID(uint16_t id)
{
  d.id=id;
}

void DNSPacket::setRA(bool b)
{
  d.ra=b;
}

void DNSPacket::setRD(bool b)
{
  d.rd=b;
}

void DNSPacket::setOpcode(uint16_t opcode)
{
  d.opcode=opcode;
}


void DNSPacket::clearRecords()
{
  rrs.clear();
}

void DNSPacket::addRecord(const DNSResourceRecord &rr)
{
  if(d_compress)
    for(vector<DNSResourceRecord>::const_iterator i=rrs.begin();i!=rrs.end();++i) 
      if(rr.qname==i->qname && rr.qtype==i->qtype && rr.content==i->content) {
	if(rr.qtype.getCode()!=QType::MX && rr.qtype.getCode()!=QType::SRV)
	  return;
	if(rr.priority==i->priority)
	  return;
      }

  rrs.push_back(rr);
}

// the functions below update the 'arcount' and 'ancount', plus they serialize themselves to the stringbuffer

string& attodot(string &str)
{
   if(str.find_first_of("@")==string::npos)
      return str;

   for (unsigned int i = 0; i < str.length(); i++)
   {
      if (str[i] == '@') {
         str[i] = '.';
         break;
      } else if (str[i] == '.') {
         str.insert(i++, "\\");
      }
   }

   return str;
}

void fillSOAData(const string &content, SOAData &data)
{
  // content consists of fields separated by spaces:
  //  nameservername hostmaster serial-number [refresh [retry [expire [ minimum] ] ] ]

  // fill out data with some plausible defaults:
  // 10800 3600 604800 3600
  data.serial=0;
  data.refresh=arg().asNum("soa-refresh-default");
  data.retry=arg().asNum("soa-retry-default");
  data.expire=arg().asNum("soa-expire-default");
  data.default_ttl=arg().asNum("soa-minimum-ttl");

  vector<string>parts;
  stringtok(parts,content);
  int pleft=parts.size();

  //  cout<<"'"<<content<<"'"<<endl;

  if(pleft)
    data.nameserver=parts[0];

  if(pleft>1) 
    data.hostmaster=attodot(parts[1]); // ahu@ds9a.nl -> ahu.ds9a.nl, piet.puk@ds9a.nl -> piet\.puk.ds9a.nl

  if(pleft>2)
    data.serial=strtoul(parts[2].c_str(), NULL, 10);

  if(pleft>3)
    data.refresh=atoi(parts[3].c_str());

  if(pleft>4)
    data.retry=atoi(parts[4].c_str());

  if(pleft>5)
    data.expire=atoi(parts[5].c_str());

  if(pleft>6)
    data.default_ttl=atoi(parts[6].c_str());

}

string serializeSOAData(const SOAData &d)
{
  ostringstream o;
  //  nameservername hostmaster serial-number [refresh [retry [expire [ minimum] ] ] ]
  o<<d.nameserver<<" "<< d.hostmaster <<" "<< d.serial <<" "<< d.refresh << " "<< d.retry << " "<< d.expire << " "<< d.default_ttl;

  return o.str();
}


static int rrcomp(const DNSResourceRecord &A, const DNSResourceRecord &B)
{
  if(A.d_place<B.d_place)
    return 1;

  return 0;
}

vector<DNSResourceRecord*> DNSPacket::getAPRecords()
{
  vector<DNSResourceRecord*> arrs;

  for(vector<DNSResourceRecord>::iterator i=rrs.begin();
      i!=rrs.end();
      ++i)
    {
      if(i->d_place!=DNSResourceRecord::ADDITIONAL && 
	 (i->qtype.getCode()==15 || 
	  i->qtype.getCode()==2 )) // CNAME or MX or NS
	{
	  arrs.push_back(&*i);
	}
    }

  return arrs;

}

void DNSPacket::setCompress(bool compress)
{
  d_compress=compress;
  stringbuffer.reserve(65000);
  rrs.reserve(200);
}

/** Must be called before attempting to access getData(). This function stuffs all resource
 *  records found in rrs into the data buffer. It also frees resource records queued for us.
 */
void DNSPacket::wrapup(void)
{
  if(d_wrapped) {
    return;
  }
  
  // do embedded-additional processing decapsulation
  DNSResourceRecord rr;
  vector<DNSResourceRecord>::iterator pos;

  vector<DNSResourceRecord> additional;

  int ipos=rrs.size();
  rrs.resize(rrs.size()+additional.size());
  copy(additional.begin(), additional.end(), rrs.begin()+ipos);

  // we now need to order rrs so that the different sections come at the right place
  // we want a stable sort, based on the d_place field

  stable_sort(rrs.begin(),rrs.end(),rrcomp);

  if(!d_tcp && !arg().mustDo("no-shuffle")) {
    shuffle(rrs);
  }
  d_wrapped=true;

  vector<uint8_t> packet;
  DNSPacketWriter pw(packet, qdomain, qtype.getCode(), 1);

  pw.getHeader()->rcode=d.rcode;
  pw.getHeader()->aa=d.aa;
  pw.getHeader()->ra=d.ra;
  pw.getHeader()->qr=d.qr;
  pw.getHeader()->id=d.id;
  pw.getHeader()->rd=d.rd;

  if(!rrs.empty()) {
    try {
      for(pos=rrs.begin(); pos < rrs.end(); ++pos) {
	// this needs to deal with the 'prio' mismatch!
	if(pos->qtype.getCode()==QType::MX || pos->qtype.getCode() == QType::SRV) {  
	  pos->content = lexical_cast<string>(pos->priority) + " " + pos->content;
	}
	pw.startRecord(pos->qname, pos->qtype.getCode(), pos->ttl, 1, (DNSPacketWriter::Place)pos->d_place); 
	if(!pos->content.empty() && pos->qtype.getCode()==QType::TXT && pos->content[0]!='"') {
	  pos->content="\""+pos->content+"\"";
	}
	if(pos->content.empty())  // empty contents confuse the MOADNS setup
	  pos->content=".";
	shared_ptr<DNSRecordContent> drc(DNSRecordContent::mastermake(pos->qtype.getCode(), 1, pos->content)); 
	drc->toPacket(pw);
      }
      pw.commit();
    }
    catch(exception& e) {
      L<<Logger::Error<<"Exception: "<<e.what()<<endl;
      throw;
    }
  }
  stringbuffer.assign((char*)&packet[0], packet.size());
  len=packet.size();
}


/** Truncates a packet that has already been wrapup()-ed, possibly via a call to getData(). Do not call this function
    before having done this - it will possibly break your packet, or crash your program. 

    This method sets the 'TC' bit in the stringbuffer, and caps the len attributed to new_length.
*/ 

void DNSPacket::truncate(int new_length)
{
  if(new_length>len || !d_wrapped)
    return;

  DLOG(L<<Logger::Warning<<"Truncating a packet to "<< remote.toString() <<endl);

  len=new_length;
  stringbuffer[2]|=2; // set TC
}


void DNSPacket::setQuestion(int op, const string &qd, int newqtype)
{
  memset(&d,0,sizeof(d));
  d.id=Utility::random();
  d.rd=d.tc=d.aa=false;
  d.qr=false;
  d.qdcount=1; // is htons'ed later on
  d.ancount=d.arcount=d.nscount=0;
  d.opcode=op;
  qdomain=qd;
  qtype=newqtype;
}

/** convenience function for creating a reply packet from a question packet. Do not forget to delete it after use! */
DNSPacket *DNSPacket::replyPacket() const
{
  DNSPacket *r=new DNSPacket;
  r->setSocket(d_socket);

  r->setRemote(&remote);
  r->setAnswer(true);  // this implies the allocation of the header
  r->setA(true); // and we are authoritative
  r->setRA(0); // no recursion available
  r->setRD(d.rd); // if you wanted to recurse, answer will say you wanted it (we don't do it)
  r->setID(d.id);
  r->setOpcode(d.opcode);

  r->d_dt=d_dt;
  r->d.qdcount=1;
  r->d_tcp = d_tcp;
  r->qdomain = qdomain;
  r->qtype = qtype;
  return r;
}

void DNSPacket::spoofQuestion(const string &qd)
{
  string label=simpleCompress(qd);
  for(string::size_type i=0;i<label.size();++i)
    stringbuffer[i+sizeof(d)]=label[i];
  d_wrapped=true; // if we do this, don't later on wrapup
}

/** This function takes data from the network, possibly received with recvfrom, and parses
    it into our class. Results of calling this function multiple times on one packet are
    unknown. Returns -1 if the packet cannot be parsed.
*/
int DNSPacket::parse(const char *mesg, int length)
try
{
  stringbuffer.assign(mesg,length); 

  len=length;
  if(length < 12) { 
    L << Logger::Warning << "Ignoring packet: too short from "
      << getRemote() << endl;
    return -1;
  }
  MOADNSParser mdp(string(mesg, length));

  memcpy((void *)&d,(const void *)stringbuffer.c_str(),12);
  qdomain=mdp.d_qname;
  if(!qdomain.empty()) // strip dot
    erase_tail(qdomain, 1);

  if(!ntohs(d.qdcount)) {
    if(!d_tcp) {
      L << Logger::Warning << "No question section in packet from " << getRemote() <<", rcode="<<(int)d.rcode<<endl;
      return -1;
    }
  }
  
  qtype=mdp.d_qtype;
  qclass=mdp.d_qclass;
  return 0;
}
catch(exception& e) {
  return -1;
}

//! Use this to set where this packet was received from or should be sent to
void DNSPacket::setRemote(const ComboAddress *s)
{
  remote=*s;
}

void DNSPacket::spoofID(uint16_t id)
{
  stringbuffer[1]=(id>>8)&0xff; 
  stringbuffer[0]=id&0xff;
  d.id=id;
}

void DNSPacket::setSocket(Utility::sock_t sock)
{
  d_socket=sock;
}

void DNSPacket::commitD()
{
  stringbuffer.replace(0,12,(char *)&d,12); // copy in d
}

