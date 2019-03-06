#ifndef __VIEW__
#define __VIEW__

#include "../headers.h"
#include "../structs.h"
#include "../functions/functions.h"

using namespace std;

namespace server {

class View {
    public:
        View();

        virtual char* tpl(const char*, unsigned int& content_length);

        virtual ~View();
};

} // namespace

#endif