#ifndef __POLLING__
#define __POLLING__

#include "../headers.h"
#include "../structs.h"
#include "../functions/functions.h"

using namespace std;

namespace server {

class Polling {

    public:

        static void create(obj*, int, int, int, char*);

        static void addInQueue(int, int, char*, vector<string*>*);

        static void joinConnection(int, int);

        static void freePoll(polling_st*);

        static void makeResult(polling_st*);

        static void setHeader(char*, int&, const char*, const char*, size_t = 0, size_t = 0);

        static char* allocMemory(int&);

        static char* setContent(obj*, polling_st*, char*, int&);
};
    
} // namespace

#endif