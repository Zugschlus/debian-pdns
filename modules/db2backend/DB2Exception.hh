// $Id: DB2Exception.hh,v 1.1 2002/11/29 11:44:21 ahu Exp $

#ifndef DB2EXCEPTION_HH
#define DB2EXCEPTION_HH

#include <string>

using namespace std;

#include <sqlcli1.h>

class DB2Exception
{
   public:

      DB2Exception(SQLRETURN inError);
      DB2Exception(SQLRETURN inError, SQLSMALLINT inHandleType, SQLHANDLE inHandle);
      virtual ~DB2Exception();

      SQLRETURN GetError();      
      bool GetNextSqlError(int& outNativeError, string& outSqlState, string& outSqlMessage);

   private:
            
      SQLRETURN   mError;
      SQLHANDLE   mHandle;
      SQLSMALLINT mHandleType;
      SQLSMALLINT mErrorIndex;
};
      
#endif // DB2EXCEPTION_HH
