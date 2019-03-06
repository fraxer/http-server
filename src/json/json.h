#ifndef __JSON__
#define __JSON__

#include "../headers.h"
#include "../structs.h"

using namespace std;

namespace server {

enum JsonTypes {
    STRING,
    ARRAY,
    OBJECT
};

class Json {

    private:

        JsonTypes tag;

        void* p = nullptr;

    public:

        Json();

        ~Json();

        Json(const Json&);

        Json(string&&);

        Json(const string&);

        Json(vector< unordered_map<string, Json*>* >);

        Json(vector< unordered_map<string, Json*>* >*);

        Json(unordered_map<string, Json*>);

        Json(unordered_map<string, Json*>*);

        JsonTypes type();

        Json& operator=(const Json&);

        string& getString();

        vector< unordered_map<string, Json*>* >& getArray();

        unordered_map<string, Json*>& getObject();

        void deletePointer();

        static Json* parse(char*, ssize_t);

        static unordered_map<string, Json*>* deserializeObject(char*, ssize_t);

        static vector< unordered_map<string, Json*>* >* deserializeArray(char*, ssize_t);
};
    
} // namespace

#endif