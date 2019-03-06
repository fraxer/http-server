
#ifndef __FUNCTIONS__
#define __FUNCTIONS__

#include "../headers.h"
#include "../structs.h"

using namespace std;

namespace server {

size_t mb_strlen(const char *);

char* itoa(int64_t, char*);

double fastPow(double, double);

double simpleHash(const char*);

char* toLowerCase(char*);

void loadMimeTypes();

char* setMethod(obj*, char*, int&);

void setRedirect(const char*, const char*, const char*, bool, short int);

char* redirects(char*, obj*, char*&, const int&);

char* setURI(obj*, char*, int&);

char* setHttpVersion(obj*, char*, int&);

void setHeader(char*, unsigned int&, const char*, const char*, size_t = 0, size_t = 0);

char* standardResponse(obj*);

char* standardResponseAsync(obj*);

bool setHeaders(obj*, char*, int&);

char* getHeader(const char*, obj*);

/*
 * return short
 *  1 - success
 * -1 - bad alloc
 * -2 - header parse error
 */
short processingHeaderRange(char*, unsigned int&, unsigned int&, char*, obj*, unsigned int&);

void writeHeadersInHead(obj*, const pair_char_int&, unsigned int&, unsigned int);

bool mkdirTree(const char*, const char*);

} // namespace

#endif