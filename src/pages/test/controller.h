#ifndef __ROOMCONTROLLER__
#define __ROOMCONTROLLER__

#include "../../controller/controller.h"

using namespace std;

namespace server {

class TestController : public Controller {

    public:

        TestController(obj* ob): Controller(ob) {}

        TestController(obj* ob, char*(*f)(TestController*)): Controller(ob, (char*(*)(void*))f) {}

		static char* actionIndex(TestController* c);

};

} // namespace

#endif