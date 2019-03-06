
#ifndef __CONTROLLER__
#define __CONTROLLER__

#include "../headers.h"
#include "../structs.h"
#include "../functions/functions.h"

using namespace std;

namespace server {

class Controller {

    private:

        bool isCacheable;

        bool findedInCache;

        unsigned int headersPos;

        char* allowMethods;

        double hashHandlerPath;

    protected:

        char* method;

        obj* p_obj;

        char* (*pointerOnFunc)(void*);

    public:

        char* pageMemory;

        int socketfd;

        int epollfd;

        unsigned int contentLength;

        unordered_map<float, pair_chars> query;

        unordered_map<float, pair_chars> cookie;

        void setCache(const unsigned int&, const unsigned int&, char*, char*);

        Controller(obj*);

        Controller(obj*, char*(*)(void*));

        Controller(obj*, char*(*)(void*), const int, const int&);

        virtual ~Controller();

        virtual void setHandler(char*(*)(void*));

        void setSocketFd(int);

        void setEpollFd(int);

        static char* methodNotAllwed(Controller*);

        char* allocMemory();

        char* createThread(void*(*)(void*), Controller*);

        void toggleThreadOnWrite();

        void setHeader(const char*, const char*, size_t = 0, size_t = 0);

        void parseQuery(char*);

        void parseCookie(char*);

        // void initContent();

        bool parseHeaders(t_headers&, char*, int&, const unsigned int&);

        bool parseHeaderFields(char*, unordered_map<double, char*>&);

        int parseData(char*, const char*, const unsigned int&, unordered_map<double, t_funvalues&>&);

        // void fillRights();

        void setAllowMethods(char*);

        // bool hasRight(const string&, const string&);

        virtual char* getContentWrapper(bool in_thread = false);

        // virtual char* getThreadContent();

        void runHandler();

        bool runPollingHandler();

        char* getData();

        char* result(string*);

        char* makeCacheable(const char*);

        char* getToken();

        char* getQuery(const char*);

        char* getCookie(const char*);

        // void freePageMemory();

        char* hexDecode(char*&);

        void* resultThread(char*);

        void* resultThread(string*);
};

} // namespace

#endif