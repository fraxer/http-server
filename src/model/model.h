
#ifndef __MODEL__
#define __MODEL__

#include "../headers.h"
#include "../structs.h"

using namespace std;

namespace server {

class Model {
    
    protected:
        PGconn* pgconn;

        conn_st* conn_obj;

        long int tid;

    public:

        bool successQuery;

        Model();

        PGconn* loginDb();

        bool createConnection();

        bool isConnected();

        bool isConnected(long int);

        bool isConnected2();

        void setConnection();

        bool isInteger(const char*);

        bool isPositiveInteger(const char*);

        vector<vector<string>> query(string);

        vector<vector<string*>*>* queryNew(string);

        void freeRes(vector<vector<string*>*>*&);

        void freeConnection();

        string& shielding(string&);

        char* trim(char*&);

        string& trim(string&);

        char* shielding(char*&);

        ~Model();
};

} // namespace

#endif