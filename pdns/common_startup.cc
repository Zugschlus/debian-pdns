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
#include "common_startup.hh"

typedef Distributor<DNSPacket,DNSPacket,PacketHandler> DNSDistributor;


ArgvMap theArg;
StatBag S;  //!< Statistics are gathered accross PDNS via the StatBag class S
PacketCache PC; //!< This is the main PacketCache, shared accross all threads
DNSProxy *DP;
DynListener *dl;
CommunicatorClass Communicator;
UDPNameserver *N;
int avg_latency;
TCPNameserver *TN;

ArgvMap &arg()
{
  return theArg;
}


void declareArguments()
{
  arg().set("local-port","The port on which we listen")="53";
  arg().setSwitch("log-failed-updates","If PDNS should log failed update requests")="";
  arg().setSwitch("log-dns-details","If PDNS should log DNS non-erroneous details")="";
  arg().setSwitch("allow-recursion-override","Set this so that local data fully overrides the recursor")="no";
  arg().set("urlredirector","Where we send hosts to that need to be url redirected")="127.0.0.1";
  arg().set("smtpredirector","Our smtpredir MX host")="a.misconfigured.powerdns.smtp.server";
  arg().set("local-address","Local IP addresses to which we bind")="0.0.0.0";
  arg().set("local-ipv6","Local IP address to which we bind")="";
  arg().set("query-local-address","Source IP address for sending queries")="";
  arg().set("max-queue-length","Maximum queuelength before considering situation lost")="5000";
  arg().set("soa-serial-offset","Make sure that no SOA serial is less than this number")="0";

  arg().setCmd("help","Provide a helpful message");
  arg().setCmd("version","Output version and compilation date");
  arg().setCmd("config","Provide configuration file on standard output");
  arg().setCmd("list-modules","Lists all modules available");
  arg().setCmd("no-config","Don't parse configuration file");
  
  arg().set("version-string","PowerDNS version in packets - full, anonymous, powerdns or custom")="full"; 
  arg().set("control-console","Debugging switch - don't use")="no"; // but I know you will!
  arg().set("fancy-records","Process URL and MBOXFW records")="no";
  arg().set("wildcard-url","Process URL and MBOXFW records")="no";
  arg().set("wildcards","Honor wildcards in the database")="";
  arg().set("loglevel","Amount of logging. Higher is more. Do not set below 3")="4";
  arg().set("default-soa-name","name to insert in the SOA record if none set in the backend")="a.misconfigured.powerdns.server";
  arg().set("distributor-threads","Default number of Distributor (backend) threads to start")="3";
  arg().set("queue-limit","Maximum number of milliseconds to queue a query")="1500"; 
  arg().set("recursor","If recursion is desired, IP address of a recursing nameserver")="no"; 
  arg().set("lazy-recursion","Only recurse if question cannot be answered locally")="yes";
  arg().set("allow-recursion","List of subnets that are allowed to recurse")="0.0.0.0/0";
  
  arg().set("disable-tcp","Do not listen to TCP queries")="no";
  arg().set("disable-axfr","Do not allow zone transfers")="no";
  
  arg().set("config-name","Name of this virtual configuration - will rename the binary image")="";

  arg().set("load-modules","Load this module - supply absolute or relative path")="";
  arg().set("launch","Which backends to launch and order to query them in")="";
  arg().setSwitch("disable-axfr","Disable zonetransfers but do allow TCP queries")="no";
  arg().set("allow-axfr-ips","Allow zonetransfers only to these subnets")="0.0.0.0/0";
  arg().set("slave-cycle-interval","Reschedule failed SOA serial checks once every .. seconds")="60";
  
  arg().setSwitch("slave","Act as a slave")="no";
  arg().setSwitch("master","Act as a master")="no";
  arg().setSwitch("guardian","Run within a guardian process")="no";
  arg().setSwitch("skip-cname","Do not perform CNAME indirection for each query")="no";
  arg().setSwitch("strict-rfc-axfrs","Perform strictly rfc compliant axfrs (very slow)")="no";
  
  arg().setSwitch("webserver","Start a webserver for monitoring")="no"; 
  arg().setSwitch("webserver-print-arguments","If the webserver should print arguments")="no"; 
  arg().set("webserver-address","IP Address of webserver to listen on")="127.0.0.1";
  arg().set("webserver-port","Port of webserver to listen on")="8081";
  arg().set("webserver-password","Password required for accessing the webserver")="";

  arg().setSwitch("out-of-zone-additional-processing","Do out of zone additional processing")="no";
  arg().setSwitch("do-ipv6-additional-processing", "Do AAAA additional processing")="no";
  arg().setSwitch("query-logging","Hint backends that queries should be logged")="no";
  
  arg().set("cache-ttl","Seconds to store packets in the PacketCache")="20";
  arg().set("recursive-cache-ttl","Seconds to store packets in the PacketCache")="10";
  arg().set("negquery-cache-ttl","Seconds to store packets in the PacketCache")="60";
  arg().set("query-cache-ttl","Seconds to store packets in the PacketCache")="20";
  arg().set("soa-minimum-ttl","Default SOA mininum ttl")="3600";
  arg().set("default-ttl","Seconds a result is valid if not set otherwise")="3600";
  arg().set("max-tcp-connections","Maximum number of TCP connections")="10";

  arg().setSwitch( "use-logfile", "Use a log file" )= "no";
  arg().set( "logfile", "Logfile to use" )= "pdns.log";
  arg().set("setuid","If set, change user id to this uid for more security")="";
  arg().set("setgid","If set, change group id to this gid for more security")="";
}



void declareStats(void)
{
  S.declare("udp-queries","Number of UDP queries received");
  S.declare("udp-answers","Number of answers sent out over UDP");
  S.declare("recursing-answers","Number of recursive answers sent out");
  S.declare("recursing-questions","Number of questions sent to recursor");
  S.declare("corrupt-packets","Number of corrupt packets received");

  S.declare("tcp-queries","Number of TCP queries received");
  S.declare("tcp-answers","Number of answers sent out over TCP");

  S.declare("qsize-q","Number of questions waiting for database attention");

  S.declare("deferred-cache-inserts","Amount of cache inserts that were deferred because of maintenance");
  S.declare("deferred-cache-lookup","Amount of cache lookups that were deferred because of maintenance");

  S.declare("query-cache-hit","Number of hits on the query cache");
  S.declare("query-cache-miss","Number of misses on the query cache");


  S.declare("servfail-packets","Number of times a server-failed packet was sent out");
  S.declare("latency","Average number of microseconds needed to answer a question");
  S.declare("timedout-packets","Number of packets which weren't answered within timeout set");

  S.declareRing("queries","UDP Queries Received");
  S.declareRing("nxdomain-queries","Queries for non-existent records within existent domains");
  S.declareRing("noerror-queries","Queries for existing records, but for type we don't have");
  S.declareRing("servfail-queries","Queries that could not be answered due to backend errors");
  S.declareRing("unauth-queries","Queries for domains that we are not authoritative for");
  S.declareRing("logmessages","Log Messages");
  S.declareRing("remotes","Remote server IP addresses");
  S.declareRing("remotes-unauth","Remote hosts querying domains for which we are not auth");
  S.declareRing("remotes-corrupt","Remote hosts sending corrupt packets");

}


int isGuarded(char **argv)
{
  char *p=strstr(argv[0],"-instance");

  return !!p;
}


void sendout(const DNSDistributor::AnswerData &AD)
{
  static int &numanswered=*S.getPointer("udp-answers");
  if(!AD.A)
    return;
  
  N->send(AD.A);
  numanswered++;
  int diff=AD.A->d_dt.udiff();
  avg_latency=(int)(1023*avg_latency/1024+diff/1024);

  delete AD.A;  


}


//! The qthread receives questions over the internet via the Nameserver class, and hands them to the Distributor for futher processing
void *qthread(void *p)
{
  DNSDistributor *D=static_cast<DNSDistributor *>(p);

  DNSPacket *P;

  DNSPacket question;
  DNSPacket cached;

  int &numreceived=*S.getPointer("udp-queries");
  int &numanswered=*S.getPointer("udp-answers");
  numreceived=-1;
  int diff;

  for(;;) {
    if(!((numreceived++)%50)) { // maintenance tasks
      S.set("latency",(int)avg_latency);
      int qcount, acount;
      D->getQueueSizes(qcount, acount);
      S.set("qsize-q",qcount);
    }
    
    if(!(P=N->receive(&question))) { // receive a packet         inline
      continue;                    // packet was broken, try again
    }


    S.ringAccount("queries", P->qdomain+"/"+P->qtype.getName());
    S.ringAccount("remotes",P->getRemote());

    if(PC.get(P,&cached)) { // short circuit - does the PacketCache recognize this question?
      cached.setRemote((struct sockaddr *)(P->remote),P->d_socklen);  // inlined
      cached.setSocket(P->getSocket());                               // inlined
      cached.spoofID(P->d.id);                                        // inlined 
      cached.d.rd=P->d.rd; // copy in recursion desired bit 
      cached.commitD(); // commit d to the packet                        inlined

      N->send(&cached);   // answer it then                              inlined
      diff=P->d_dt.udiff();                                                    
      avg_latency=(int)(0.999*avg_latency+0.001*diff); // 'EWMA'
      
      numanswered++;
      continue;
    }

    D->question(P, &sendout); // otherwise, give to the distributor
  }
  return 0;
}


void mainthread()
{
  Utility::srandom(time(0));

   int newgid=0;      
   if(!arg()["setgid"].empty()) 
     newgid=Utility::makeGidNumeric(arg()["setgid"]);      
   int newuid=0;      
   if(!arg()["setuid"].empty())        
     newuid=Utility::makeUidNumeric(arg()["setuid"]); 
#ifndef WIN32

   if(!arg()["chroot"].empty()) {  
     if(arg().mustDo("master") || arg().mustDo("slave"))
	gethostbyname("a.root-servers.net"); // this forces all lookup libraries to be loaded
     if(chroot(arg()["chroot"].c_str())<0) {
       L<<Logger::Error<<"Unable to chroot to '"+arg()["chroot"]+"': "<<strerror(errno)<<", exiting"<<endl; 
       exit(1);
     }   
     else
       L<<Logger::Error<<"Chrooted to '"<<arg()["chroot"]<<"'"<<endl;      
   }  
#endif

   Utility::dropPrivs(newuid, newgid);

  if(arg().mustDo("recursor")){
    DP=new DNSProxy(arg()["recursor"]);
    DP->onlyFrom(arg()["allow-recursion"]);
    DP->go();
  }
  // NOW SAFE TO CREATE THREADS!
  dl->go();

  pthread_t qtid;
  StatWebServer sws;

  if(arg().mustDo("webserver"))
    sws.go();
  
  if(arg().mustDo("slave") || arg().mustDo("master"))
    Communicator.go(); 

  if(TN)
    TN->go(); // tcp nameserver launch
    
  //  fork(); (this worked :-))
  DNSDistributor *D= new DNSDistributor(arg().asNum("distributor-threads")); // the big dispatcher!
  pthread_create(&qtid,0,qthread,static_cast<void *>(D)); // receives packets

  void *p;
  pthread_join(qtid, &p);
  
  L<<Logger::Error<<"Mainthread exiting - should never happen"<<endl;
}




