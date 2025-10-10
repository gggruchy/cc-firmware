#include "srv_state.h"
#include "simplebus.h"
#include "klippy.h"

#include "hl_disk.h"

#define LOG_TAG "srv_state"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

static srv_state_t __srv_state[2];
static srv_state_t *srv_state;
static pthread_rwlock_t rwlock;

static void srv_state_home_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len);
static void srv_state_heater_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len);
static void srv_state_fan_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len);

static void srv_state_service_callback(const char *name, void *context, int request_id, void *args, void *response);

void srv_state_init(void)
{
    memset(__srv_state, 0, sizeof(__srv_state));
    srv_state = &__srv_state[0];

    pthread_rwlock_init(&rwlock, NULL);

    simple_bus_register_service("srv_state", &srv_state, srv_state_service_callback);

    simple_bus_subscribe("home", NULL, srv_state_home_subscribe_callback);
    simple_bus_subscribe("heater", NULL, srv_state_heater_subscribe_callback);
    simple_bus_subscribe("fan", NULL, srv_state_fan_subscribe_callback);
}

static void srv_state_service_callback(const char *name, void *context, int request_id, void *args, void *response)
{
    switch (request_id)
    {
    case SRV_STATE_SRV_ID_STATE:
    {
        srv_state_res_t *res = (srv_state_res_t *)response;
        pthread_rwlock_rdlock(&rwlock);
        memcpy(&res->state, srv_state, sizeof(res->state));
        pthread_rwlock_unlock(&rwlock);
    }
    break;
    default:
        break;
    }
}

static void srv_state_home_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len)
{
    switch (msg_id)
    {
    case SRV_HOME_MSG_ID_STATE:
    {
        srv_state_home_msg_t *home_msg = (srv_state_home_msg_t *)msg;
        if (home_msg->axis > 2 || home_msg->axis < 0)
            return;
        if (srv_state->home_state[home_msg->axis] != home_msg->st)
        {
            srv_state->home_state[home_msg->axis] = home_msg->st;
            simple_bus_publish_async("srv_state", SRV_STATE_MSG_ID_STATE, NULL, 0);
        }
    }
    break;
    default:
        break;
    }
}

static void srv_state_heater_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len)
{
    switch (msg_id)
    {
    case SRV_HEATER_MSG_ID_STATE:
    {
        srv_state_heater_msg_t *heater_msg = (srv_state_heater_msg_t *)msg;
        if (heater_msg->heater_id < 0 || heater_msg->heater_id > HEATER_NUMBERS)
            return;

        if (fabs(srv_state->heater_state[heater_msg->heater_id].target_temperature - heater_msg->target_temperature) > 1e-6 ||
            fabs(srv_state->heater_state[heater_msg->heater_id].current_temperature - heater_msg->current_temperature) > 1e-6)
        {
            srv_state->heater_state[heater_msg->heater_id].target_temperature = heater_msg->target_temperature;
            srv_state->heater_state[heater_msg->heater_id].current_temperature = heater_msg->current_temperature;
            simple_bus_publish_async("srv_state", SRV_STATE_MSG_ID_STATE, NULL, 0);
        }
    }
    break;
    default:
        break;
    }
}

static void srv_state_fan_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len)
{
    switch (msg_id)
    {
    case SRV_FAN_MSG_ID_STATE:
    {
        srv_state_fan_msg_t *fan_msg = (srv_state_fan_msg_t *)msg;
        if (fan_msg->fan_id < 0 || fan_msg->fan_id > FAN_NUMBERS)
            return;

        if (fabs(srv_state->fan_state[fan_msg->fan_id].value - fan_msg->value) > 1e-6)
        {
            srv_state->fan_state[fan_msg->fan_id].value = fan_msg->value;
            simple_bus_publish_async("srv_state", SRV_STATE_MSG_ID_STATE, NULL, 0);
        }
    }
    break;
    default:
        break;
    }
}