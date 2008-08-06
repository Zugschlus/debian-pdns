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
#ifndef MISC_HH
#define MISC_HH
/* (C) 2002 POWERDNS.COM BV */


#include "utility.hh"

#ifndef WIN32
# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <time.h>
#else
# pragma warning ( disable: 4786 )
# define WINDOWS_LEAN_AND_MEAN
# include <windows.h>
# include "utility.hh"


#endif // WIN32

#include <deque>
#include <string>
#include <ctype.h>
using namespace std;
bool chopOff(string &domain);
bool endsOn(const string &domain, const string &suffix);
string nowTime();
const string unquotify(const string &item);
string humanDuration(time_t passed);
void chomp(string &line, const string &delim);
bool stripDomainSuffix(string *qname, const string &domain);
void stripLine(string &line);
string getHostname();
string urlEncode(const string &text);
int waitForData(int fd, int seconds);
u_int16_t getShort(const unsigned char *p);
u_int16_t getShort(const char *p);
u_int32_t getLong(const unsigned char *p);
u_int32_t getLong(const char *p);

inline void putLong(unsigned char* p, u_int32_t val)
{
  *p++=(val>>24)&0xff;
  *p++=(val>>16)&0xff;
  *p++=(val>>8)&0xff;
  *p++=(val   )&0xff;

}
inline void putLong(char* p, u_int32_t val)
{
  putLong((unsigned char *)p,val);
}


inline u_int32_t getLong(unsigned char *p)
{
  return (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
}


struct ServiceTuple
{
  string host;
  u_int16_t port;
};
void parseService(const string &descr, ServiceTuple &st);

template <typename Container>
void
stringtok (Container &container, string const &in,
           const char * const delimiters = " \t\n")
{
  const string::size_type len = in.length();
  string::size_type i = 0;
  
  while (i<len) {
    // eat leading whitespace
    i = in.find_first_not_of (delimiters, i);
    if (i == string::npos)
      return;   // nothing left but white space
    
    // find the end of the token
    string::size_type j = in.find_first_of (delimiters, i);
    
    // push token
    if (j == string::npos) {
      container.push_back (in.substr(i));
      return;
    } else
      container.push_back (in.substr(i, j-i));
    
    // set up for next loop
    i = j + 1;
  }
}
const string toLower(const string &upper);
bool IpToU32(const string &str, u_int32_t *ip);
string stringerror();
string itoa(int i);
string uitoa(unsigned int i);

void dropPrivs(int uid, int gid);
int makeGidNumeric(const string &group);
int makeUidNumeric(const string &user);
void cleanSlashes(string &str);

/** The DTime class can be used for timing statistics with microsecond resolution. 
On 32 bits systems this means that 2147 seconds is the longest time that can be measured. */
class DTime 
{
public:
  DTime(); //!< Does not set the timer for you! Saves lots of gettimeofday() calls
  DTime(const DTime &dt);
  time_t time();
  inline void set();  //!< Reset the timer
  inline int udiff(); //!< Return the number of microseconds since the timer was last set.
private:
  struct timeval d_set;
};
const string sockAddrToString(struct sockaddr_in *remote, Utility::socklen_t socklen);
int sendData(const char *buffer, int replen, int outsock);


inline void DTime::set()
{
  Utility::gettimeofday(&d_set,0);
}

inline int DTime::udiff()
{
  struct timeval now;
  Utility::gettimeofday(&now,0);

  return 1000000*(now.tv_sec-d_set.tv_sec)+(now.tv_usec-d_set.tv_usec);
}

inline const string toLower(const string &upper)
{
  string reply(upper);
  for(unsigned int i = 0; i < reply.length(); i++)
    reply[i] = tolower(reply[i]);
  return reply;
}


// Make s uppercase:
inline string toUpper( const string& s )
{
	string r(s);
	for( unsigned int i = 0; i < s.length(); i++ ) {
		r[i] = toupper( r[i] );
	}
	return r;
}

inline double getTime()
{
  struct timeval now;
  Utility::gettimeofday(&now,0);
  
  return now.tv_sec+now.tv_usec/1000000.0;
}


inline void chomp( string& line, const string& delim )
{
	string::size_type pos;

	if( ( pos = line.find_last_not_of( delim ) ) != string::npos )
	{
		line.resize( pos + 1 );
	}
}


#endif
