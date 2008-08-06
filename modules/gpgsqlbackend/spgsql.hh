/* Copyright 2001 Netherlabs BV, bert.hubert@netherlabs.nl. See LICENSE 
   for more information.
   $Id: spgsql.hh,v 1.5 2004/01/17 13:18:22 ahu Exp $  */
#ifndef SPGSQL_HH
#define SPGSQL_HH
using namespace std;
#include "pdns/backends/gsql/ssql.hh"

#include <libpq-fe.h>
class SPgSQL : public SSql
{
public:
  SPgSQL(const string &database, const string &host="", 
	 const string &msocket="",const string &user="", 
	 const string &password="");

  ~SPgSQL();
  
  SSqlException sPerrorException(const string &reason);
  int doQuery(const string &query, result_t &result);
  int doQuery(const string &query);
  int doCommand(const string &query);
  bool getRow(row_t &row);
  string escape(const string &str);    
  void setLog(bool state);
private:
  PGconn* d_db; 
  PGresult* d_result;
  int d_count;
  static bool s_dolog;
};
      
#endif /* SPGSQL_HH */
