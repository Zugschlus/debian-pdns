
//
// SQLite backend for PowerDNS
// Copyright (C) 2003, Michel Stol <michel@powerdns.com>
//

#ifndef GSQLITEBACKEND_HH
#define GSQLITEBACKEND_HH

#include <string>
#include "pdns/backends/gsql/gsqlbackend.hh"

//! The gSQLiteBackend retrieves it's data from a SQLite database (http://www.sqlite.org/)
class gSQLiteBackend : public GSQLBackend
{
private:
protected:
public:
  //! Constructs the backend, throws an exception if it failed..
  gSQLiteBackend( const std::string & mode, const std::string & suffix );
  virtual string sqlEscape(const string &name);
};

#endif // GSQLITEBACKEND_HH
