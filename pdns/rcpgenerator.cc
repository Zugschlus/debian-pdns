/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2005  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as 
    published by the Free Software Foundation

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "rcpgenerator.hh"
#include "dnsparser.hh"
#include "misc.hh"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "base64.hh"
using namespace boost;

RecordTextReader::RecordTextReader(const string& str, const string& zone) : d_string(str), d_zone(zone), d_pos(0), d_end(str.size())
{
}

void RecordTextReader::xfr32BitInt(uint32_t &val)
{
  skipSpaces();

  if(!isdigit(d_string.at(d_pos)))
    throw RecordTextException("expected digits at position "+lexical_cast<string>(d_pos)+" in '"+d_string+"'");

  char *endptr;
  unsigned long ret=strtoul(d_string.c_str() + d_pos, &endptr, 10);
  val=ret;
  
  d_pos = endptr - d_string.c_str();
}

void RecordTextReader::xfrTime(uint32_t &val)
{
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  
  string tmp;
  xfrLabel(tmp); // ends on number, so this works 

  sscanf(tmp.c_str(), "%04d%02d%02d" "%02d%02d%02d", 
	 &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
	 &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

  tm.tm_year-=1900;
  tm.tm_min-=1;
  
  val=mktime(&tm);
}

void RecordTextReader::xfrIP(uint32_t &val)
{
  skipSpaces();

  if(!isdigit(d_string.at(d_pos)))
    throw RecordTextException("expected digits at position "+lexical_cast<string>(d_pos)+" in '"+d_string+"'");
  
  string ip;
  xfrLabel(ip);
  if(!IpToU32(ip, &val))
    throw RecordTextException("unable to parse IP address '"+ip+"'");
}


bool RecordTextReader::eof()
{
  return d_pos==d_end;
}

void RecordTextReader::xfr16BitInt(uint16_t &val)
{
  uint32_t tmp;
  xfr32BitInt(tmp);
  val=tmp;
  if(val!=tmp)
    throw RecordTextException("Overflow reading 16 bit integer from record content"); // fixme improve
}

void RecordTextReader::xfr8BitInt(uint8_t &val)
{
  uint32_t tmp;
  xfr32BitInt(tmp);
  val=tmp;
  if(val!=tmp)
    throw RecordTextException("Overflow reading 8 bit integer from record content"); // fixme improve
}


void RecordTextReader::xfrLabel(string& val, bool)
{
  skipSpaces();

  val.clear();
  val.reserve(d_end - d_pos);

  while(d_pos < d_end) {
    if(dns_isspace(d_string[d_pos]))
      break;

    if(d_string[d_pos]=='\\' && d_pos < d_end - 1) 
      d_pos++;

    val.append(1, d_string[d_pos]);      
    d_pos++;
  }

  if(val.empty())
    val=d_zone;
  else if(!d_zone.empty()) {
    char last=val[val.size()-1];
   
    if(last =='.')
      val.resize(val.size()-1);
    else if(last != '.' && !isdigit(last)) // don't add zone to IP address
      val+="."+d_zone;
  }
}

void RecordTextReader::xfrBlob(string& val)
{
  skipSpaces();
  int pos=d_pos;
  while(d_pos < d_end && !dns_isspace(d_string[d_pos]))
    d_pos++;

  string tmp;
  tmp.assign(d_string.c_str()+pos, d_string.c_str() + d_pos);
  val.clear();
  B64Decode(tmp, val);
}

void RecordTextReader::xfrText(string& val)
{
  skipSpaces();
  if(d_string[d_pos]!='"')
    throw RecordTextException("Data field in DNS should start with quote (\") at position "+lexical_cast<string>(d_pos)+" of '"+d_string+"'");

  val.clear();
  val.reserve(d_end - d_pos);
  
  while(++d_pos < d_end && d_string[d_pos]!='"') {
    if(d_string[d_pos]=='\\' && d_pos+1!=d_end) {
      ++d_pos;
    }
    val.append(1, d_string[d_pos]);
  }
  if(d_pos == d_end)
    throw RecordTextException("Data field in DNS should end on a quote (\") in '"+d_string+"'");
  d_pos++;
}

void RecordTextReader::xfrType(uint16_t& val)
{
  skipSpaces();
  int pos=d_pos;
  while(d_pos < d_end && !dns_isspace(d_string[d_pos]))
    d_pos++;

  string tmp;
  tmp.assign(d_string.c_str()+pos, d_string.c_str() + d_pos);

  val=DNSRecordContent::TypeToNumber(tmp);
}


void RecordTextReader::skipSpaces()
{
  while(d_pos < d_end && dns_isspace(d_string[d_pos]))
    d_pos++;

  if(d_pos == d_end)
    throw RecordTextException("missing field at the end of record content '"+d_string+"'");
}


RecordTextWriter::RecordTextWriter(string& str) : d_string(str)
{
  d_string.clear();
}

void RecordTextWriter::xfr32BitInt(const uint32_t& val)
{
  if(!d_string.empty())
    d_string.append(1,' ');
  d_string+=lexical_cast<string>(val);
}



void RecordTextWriter::xfrType(const uint16_t& val)
{
  if(!d_string.empty())
    d_string.append(1,' ');
  d_string+=DNSRecordContent::NumberToType(val);
}

// this function is on the fast path for the pdns_recursor
void RecordTextWriter::xfrIP(const uint32_t& val)
{
  if(!d_string.empty())
    d_string.append(1,' ');

  char tmp[17];
  snprintf(tmp, sizeof(tmp)-1, "%u.%u.%u.%u", 
	   (val >> 24)&0xff,
	   (val >> 16)&0xff,
	   (val >>  8)&0xff,
	   (val      )&0xff);
  
  d_string+=tmp;
}


void RecordTextWriter::xfrTime(const uint32_t& val)
{
  if(!d_string.empty())
    d_string.append(1,' ');
  
  struct tm tm;
  time_t time=val; // Y2038 bug!
  gmtime_r(&time, &tm);
  
  char tmp[16];
  snprintf(tmp,sizeof(tmp)-1, "%04d%02d%02d" "%02d%02d%02d", 
	   tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, 
	   tm.tm_hour, tm.tm_min, tm.tm_sec);
  
  d_string += tmp;
}


void RecordTextWriter::xfr16BitInt(const uint16_t& val)
{
  xfr32BitInt(val);
}

void RecordTextWriter::xfr8BitInt(const uint8_t& val)
{
  xfr32BitInt(val);
}


void RecordTextWriter::xfrLabel(const string& val, bool)
{
  if(!d_string.empty())
    d_string.append(1,' ');
  if(val.find(' ')==string::npos) 
    d_string+=val;
  else {
    d_string.reserve(d_string.size()+val.size()+3);
    for(string::size_type pos=0; pos < val.size() ; ++pos)
      if(dns_isspace(val[pos]))
	d_string+="\\ ";
      else if(val[pos]=='\\')
	d_string.append(1,'\\');
      else
	d_string.append(1,val[pos]);
  }
  d_string.append(1,'.');
}

void RecordTextWriter::xfrBlob(const string& val)
{
  if(!d_string.empty())
    d_string.append(1,' ');

  d_string+=Base64Encode(val);
}

void RecordTextWriter::xfrText(const string& val)
{
  if(!d_string.empty())
    d_string.append(1,' ');
  d_string.append(1,'"');

  if(val.find_first_of("\\\"") == string::npos)
    d_string+=val;
  else {
    string::size_type end=val.size();
    
    for(string::size_type pos=0; pos < end; ++pos) {
      if(val[pos]=='\'' || val[pos]=='"')
	d_string.append(1,'\\');
      d_string.append(1, val[pos]);
    }
  }

  d_string.append(1,'"');
}


#ifdef TESTING

int main(int argc, char**argv)
try
{
  RecordTextReader rtr(argv[1], argv[2]);
  
  unsigned int order, pref;
  string flags, services, regexp, replacement;
  string mx;

  rtr.xfrInt(order);
  rtr.xfrInt(pref);
  rtr.xfrText(flags);
  rtr.xfrText(services);
  rtr.xfrText(regexp);
  rtr.xfrLabel(replacement);

  cout<<"order: "<<order<<", pref: "<<pref<<"\n";
  cout<<"flags: \""<<flags<<"\", services: \""<<services<<"\", regexp: \""<<regexp<<"\", replacement: "<<replacement<<"\n";

  string out;
  RecordTextWriter rtw(out);

  rtw.xfrInt(order);
  rtw.xfrInt(pref);
  rtw.xfrText(flags);
  rtw.xfrText(services);
  rtw.xfrText(regexp);
  rtw.xfrLabel(replacement);

  cout<<"Regenerated: '"<<out<<"'\n";
  
}
catch(exception& e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}

#endif
