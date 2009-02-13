#ifndef PDNS_LUA_PDNS_RECURSOR_HH
#define PDNS_LUA_PDNS_RECURSOR_HH
#include "dns.hh"
#include "iputils.hh"

struct lua_State;

class PowerDNSLua
{
public:
  explicit PowerDNSLua(const std::string& fname);
  ~PowerDNSLua();
  void reload();
  bool preresolve(const ComboAddress& remote,const ComboAddress& local, const string& query, const QType& qtype, vector<DNSResourceRecord>& res, int& ret);
  bool nxdomain(const ComboAddress& remote, const ComboAddress& local, const string& query, const QType& qtype, vector<DNSResourceRecord>& res, int& ret);
private:
  lua_State* d_lua;
  bool passthrough(const string& func, const ComboAddress& remote,const ComboAddress& local, const string& query, const QType& qtype, vector<DNSResourceRecord>& ret, int& res);
  bool getFromTable(const std::string& key, std::string& value);
  bool getFromTable(const std::string& key, uint32_t& value);
  bool d_failed;
};

#endif
