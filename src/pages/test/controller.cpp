#include "controller.h"

namespace server {

char* TestController::actionIndex(TestController* c) {
     return c->result(new string("hello world"));
}

} // namespace