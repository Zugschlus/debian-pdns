#include "lua-pdns-recursor.hh"

#if !defined(PDNS_ENABLE_LUA) && !defined(LIBDIR)

// stub implementation

PowerDNSLua::PowerDNSLua(const std::string& fname)
{
  throw runtime_error("Lua support disabled");
}

bool PowerDNSLua::nxdomain(const ComboAddress& remote,const ComboAddress& local, const string& query, const QType& qtype, vector<DNSResourceRecord>& ret, int& res, bool* variable)
{
  return false;
}

bool PowerDNSLua::preresolve(const ComboAddress& remote, const ComboAddress& local, const string& query, const QType& qtype, vector<DNSResourceRecord>& ret, int& res, bool* variable)
{
  return false;
}

bool PowerDNSLua::axfrfilter(const ComboAddress& remote, const string& zone, const DNSResourceRecord& in, vector<DNSResourceRecord>& out)
{
  return false;
}


PowerDNSLua::~PowerDNSLua()
{

}

#else

extern "C" {
#undef L
/* Include the Lua API header files. */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <stdexcept>
#include "logger.hh"
#include "namespaces.hh"

bool netmaskMatchTable(lua_State* lua, const std::string& ip)
{
  lua_pushnil(lua);  /* first key */
  while (lua_next(lua, 2) != 0) {
    string netmask=lua_tostring(lua, -1);
    Netmask nm(netmask);
    ComboAddress ca(ip);
    lua_pop(lua, 1);
    
    if(nm.match(ip)) 
      return true;
  }
  return false;
}

extern "C" {

int netmaskMatchLua(lua_State *lua)
{
  bool result=false;
  if(lua_gettop(lua) >= 2) {
    string ip=lua_tostring(lua, 1);
    if(lua_istable(lua, 2)) {
      result = netmaskMatchTable(lua, ip);
    }
    else {
      for(int n=2 ; n <= lua_gettop(lua); ++n) { 
        string netmask=lua_tostring(lua, n);
        Netmask nm(netmask);
        ComboAddress ca(ip);
        
        result = nm.match(ip);
        if(result)
          break;
      }
    }
  }
  
  lua_pushboolean(lua, result);
  return 1;
}

int getLocalAddressLua(lua_State* lua)
{
  lua_getfield(lua, LUA_REGISTRYINDEX, "__PowerDNSLua"); 
  PowerDNSLua* pl = (PowerDNSLua*)lua_touserdata(lua, -1);
  
  lua_pushstring(lua, pl->getLocal().toString().c_str());
  return 1;
}

// called by lua to indicate that this answer is 'variable' and should not be cached
int setVariableLua(lua_State* lua)
{
  lua_getfield(lua, LUA_REGISTRYINDEX, "__PowerDNSLua"); 
  PowerDNSLua* pl = (PowerDNSLua*)lua_touserdata(lua, -1);
  pl->setVariable();
  return 0;
}

int logLua(lua_State *lua)
{
  if(lua_gettop(lua) >= 1) {
    string message=lua_tostring(lua, 1);
    theL()<<Logger::Error<<"From Lua script: "<<message<<endl;
  }
  return 0;
}
}

PowerDNSLua::PowerDNSLua(const std::string& fname)
{
  d_lua = lua_open();

#ifndef LUA_VERSION_NUM
  luaopen_base(d_lua);
  luaopen_string(d_lua);

  if(lua_dofile(d_lua,  fname.c_str())) 
#else
  luaL_openlibs(d_lua);
  if(luaL_dofile(d_lua,  fname.c_str())) 
#endif
    throw runtime_error(string("Error loading LUA file '")+fname+"': "+ string(lua_isstring(d_lua, -1) ? lua_tostring(d_lua, -1) : "unknown error"));

  lua_settop(d_lua, 0);
  
  lua_pushcfunction(d_lua, netmaskMatchLua);
  lua_setglobal(d_lua, "matchnetmask");

  lua_pushcfunction(d_lua, logLua);
  lua_setglobal(d_lua, "pdnslog");

  lua_pushcfunction(d_lua, setVariableLua);
  lua_setglobal(d_lua, "setvariable");


  lua_pushcfunction(d_lua, getLocalAddressLua);
  lua_setglobal(d_lua, "getlocaladdress");

  lua_newtable(d_lua);

  for(vector<QType::namenum>::const_iterator iter = QType::names.begin(); iter != QType::names.end(); ++iter) {
    lua_pushnumber(d_lua, iter->second);
    lua_setfield(d_lua, -2, iter->first.c_str());
  }
  lua_pushnumber(d_lua, 3);
  lua_setfield(d_lua, -2, "NXDOMAIN");
  lua_setglobal(d_lua, "pdns");
  
  lua_pushlightuserdata(d_lua, (void*)this); 
  lua_setfield(d_lua, LUA_REGISTRYINDEX, "__PowerDNSLua");
}

bool PowerDNSLua::nxdomain(const ComboAddress& remote, const ComboAddress& local,const string& query, const QType& qtype, vector<DNSResourceRecord>& ret, int& res, bool* variable)
{
  return passthrough("nxdomain", remote, local, query, qtype, ret, res, variable);
}

bool PowerDNSLua::preresolve(const ComboAddress& remote, const ComboAddress& local,const string& query, const QType& qtype, vector<DNSResourceRecord>& ret, int& res, bool* variable)
{
  return passthrough("preresolve", remote, local, query, qtype, ret, res, variable);
}

bool PowerDNSLua::axfrfilter(const ComboAddress& remote, const string& zone, const DNSResourceRecord& in, vector<DNSResourceRecord>& out)
{
  lua_getglobal(d_lua,  "axfrfilter");
  if(!lua_isfunction(d_lua, -1)) {
    cerr<<"No such function 'axfrfilter'\n";
    lua_pop(d_lua, 1);
    return false;
  }
  
  lua_pushstring(d_lua,  remote.toString().c_str() );
  lua_pushstring(d_lua,  zone.c_str() );
  lua_pushstring(d_lua,  in.qname.c_str() );
  lua_pushnumber(d_lua,  in.qtype.getCode() );
  lua_pushnumber(d_lua,  in.ttl );
  lua_pushnumber(d_lua,  in.priority );
  lua_pushstring(d_lua,  in.content.c_str() );

  if(lua_pcall(d_lua,  7, 2, 0)) { // error 
    string error=string("lua error in axfrfilter: ")+lua_tostring(d_lua, -1);
    lua_pop(d_lua, 1);
    throw runtime_error(error);
    return false;
  }
  
  int newres = (int)lua_tonumber(d_lua, 1); // did we handle it?
  if(newres < 0) {
    //cerr << "handler did not handle"<<endl;
    lua_pop(d_lua, 2);
    return false;
  }

  /* get the result */
  DNSResourceRecord rr;
  rr.d_place = DNSResourceRecord::ANSWER;
  rr.ttl = 3600;
  rr.domain_id = in.domain_id;

  out.clear();

  /*           1       2   3   4   */
  /* stack:  boolean table key row */

#ifndef LUA_VERSION_NUM
  int tableLen = luaL_getn(d_lua, 2);
#else
  int tableLen = lua_objlen(d_lua, 2);
#endif
  cerr<<"Returned "<<tableLen<<" rows"<<endl;
  for(int n=1; n < tableLen + 1; ++n) {
    lua_pushnumber(d_lua, n);
    lua_gettable(d_lua, 2);

    uint32_t tmpnum=0;
    if(!getFromTable("qtype", tmpnum)) 
      rr.qtype=QType::A;
    else
      rr.qtype=tmpnum;

    getFromTable("content", rr.content);
    if(!getFromTable("ttl", rr.ttl))
      rr.ttl=3600;

    if(!getFromTable("qname", rr.qname))
      rr.qname = zone;

    if(!getFromTable("place", tmpnum))
      rr.d_place = DNSResourceRecord::ANSWER;
    else
      rr.d_place = (DNSResourceRecord::Place) tmpnum;

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(d_lua, 1); // table

    //    cerr<<"Adding content '"<<rr.content<<"' with place "<<(int)rr.d_place<<" \n";
    out.push_back(rr);
  }
  lua_pop(d_lua, 2); // c
  return true;
}

bool PowerDNSLua::getFromTable(const std::string& key, std::string& value)
{
  lua_pushstring(d_lua, key.c_str()); // 4 is now '1'
  lua_gettable(d_lua, -2);  // replace by the first entry of our table we hope

  bool ret=false;
  if(!lua_isnil(d_lua, -1)) {
    value = lua_tostring(d_lua, -1);
    ret=true;
  }
  lua_pop(d_lua, 1);
  return ret;
}


bool PowerDNSLua::getFromTable(const std::string& key, uint32_t& value)
{
  lua_pushstring(d_lua, key.c_str()); // 4 is now '1'
  lua_gettable(d_lua, -2);  // replace by the first entry of our table we hope

  bool ret=false;
  if(!lua_isnil(d_lua, -1)) {
    value = (uint32_t)lua_tonumber(d_lua, -1);
    ret=true;
  }
  lua_pop(d_lua, 1);
  return ret;
}


bool PowerDNSLua::passthrough(const string& func, const ComboAddress& remote, const ComboAddress& local, const string& query, const QType& qtype, vector<DNSResourceRecord>& ret, 
  int& res, bool* variable)
{
  d_variable = false;
  lua_getglobal(d_lua,  func.c_str());
  if(!lua_isfunction(d_lua, -1)) {
    //  cerr<<"No such function '"<<func<<"'\n";
    lua_pop(d_lua, 1);
    return false;
  }
  
  d_local = local; 
  /* the first argument */
  lua_pushstring(d_lua,  remote.toString().c_str() );
  lua_pushstring(d_lua,  query.c_str() );
  lua_pushnumber(d_lua,  qtype.getCode() );

  if(lua_pcall(d_lua,  3, 2, 0)) { // error 
    string error=string("lua error in '"+func+"': ")+lua_tostring(d_lua, -1);
    lua_pop(d_lua, 1);
    throw runtime_error(error);
    return false;
  }
  
  *variable |= d_variable;
  
  int newres = (int)lua_tonumber(d_lua, 1); // new rcode
  if(newres < 0) {
    //    cerr << "handler did not handle"<<endl;
    lua_pop(d_lua, 2);
    return false;
  }
  res=newres;

  /* get the result */
  DNSResourceRecord rr;
  rr.qname = query;
  rr.d_place = DNSResourceRecord::ANSWER;
  rr.ttl = 3600;

  ret.clear();

  /*           1       2   3   4   */
  /* stack:  boolean table key row */

#ifndef LUA_VERSION_NUM
  int tableLen = luaL_getn(d_lua, 2);
#else
  int tableLen = lua_objlen(d_lua, 2);
#endif

  for(int n=1; n < tableLen + 1; ++n) {
    lua_pushnumber(d_lua, n);
    lua_gettable(d_lua, 2);

    uint32_t tmpnum=0;
    if(!getFromTable("qtype", tmpnum)) 
      rr.qtype=QType::A;
    else
      rr.qtype=tmpnum;

    getFromTable("content", rr.content);
    if(!getFromTable("ttl", rr.ttl))
      rr.ttl=3600;

    if(!getFromTable("qname", rr.qname))
      rr.qname = query;

    if(!getFromTable("place", tmpnum))
      rr.d_place = DNSResourceRecord::ANSWER;
    else
      rr.d_place = (DNSResourceRecord::Place) tmpnum;

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(d_lua, 1); // table

    // cerr<<"Adding content '"<<rr.content<<"' with place "<<(int)rr.d_place<<" \n";
    ret.push_back(rr);
  }

  lua_pop(d_lua, 2);

  return true;
}

PowerDNSLua::~PowerDNSLua()
{
  lua_close(d_lua);
}
#endif
