#include "service.h"
#include "simplebus.h"
#include "klippy.h"

#include <string>

void service_init(void)
{
    srv_control_init();
    srv_state_init();
}
