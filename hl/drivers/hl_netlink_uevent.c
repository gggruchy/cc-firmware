#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include "hl_netlink_uevent.h"
#include "hl_tpool.h"
#include "hl_callback.h"
#include "hl_common.h"
#include "hl_assert.h"

static void netlink_uevent_thread(hl_tpool_thread_t thread, void *args);
static hl_tpool_thread_t thread;
static hl_callback_t callback;

void hl_netlink_uevent_init(void)
{
    HL_ASSERT(hl_callback_create(&callback) == 0);
    HL_ASSERT(hl_tpool_create_thread(&thread, netlink_uevent_thread, NULL, 0, 0, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(thread, 0) == 1);
}

void hl_netlink_uevent_register_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_register(callback, function, user_data);
}

void hl_netlink_uevent_unregister_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_unregister(callback, function, user_data);
}

static void netlink_uevent_thread(hl_tpool_thread_t thread, void *args)
{
    struct sockaddr_nl snl;
    struct pollfd pfd;
    snl.nl_family = AF_NETLINK;
    snl.nl_pad = 0;
    snl.nl_pid = getpid();
    snl.nl_groups = -1;
    pfd.events = POLLIN;
    HL_ASSERT((pfd.fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT)) != -1);
    HL_ASSERT((bind(pfd.fd, (void *)&snl, sizeof(snl))) != -1);

    char msg[4096];
    ssize_t msg_len;
    hl_netlink_uevent_msg_t uevent_msg = {0};

    while (poll(&pfd, 1, -1) > 0)
    {
        msg_len = recv(pfd.fd, msg, sizeof(msg), MSG_DONTWAIT);
        const char *pmsg = msg;

#define IDENTIFIER_ACTION "ACTION="
#define IDENTIFIER_DEVNAME "DEVNAME="
#define IDENTIFIER_DEVTYPE "DEVTYPE="
#define IDENTIFIER_PRODUCT "PRODUCT="
#define IDENTIFIER_INTERFACE "INTERFACE="
        memset(&uevent_msg, 0, sizeof(uevent_msg));

        while (pmsg != msg + msg_len)
        {
            // printf("pmsg: %s\n", pmsg);
            if (strncmp(IDENTIFIER_ACTION, pmsg, sizeof(IDENTIFIER_ACTION) - 1) == 0)
            {
                pmsg += sizeof(IDENTIFIER_ACTION) - 1;
                strncpy(uevent_msg.action, pmsg, sizeof(uevent_msg.action));
            }
            else if (strncmp(IDENTIFIER_DEVNAME, pmsg, sizeof(IDENTIFIER_DEVNAME) - 1) == 0)
            {
                pmsg += sizeof(IDENTIFIER_DEVNAME) - 1;
                strncpy(uevent_msg.devname, pmsg, sizeof(uevent_msg.devname));
            }
            else if (strncmp(IDENTIFIER_DEVTYPE, pmsg, sizeof(IDENTIFIER_DEVTYPE) - 1) == 0)
            {
                pmsg += sizeof(IDENTIFIER_DEVTYPE) - 1;
                strncpy(uevent_msg.devtype, pmsg, sizeof(uevent_msg.devtype));
            }
            else if (strncmp(IDENTIFIER_PRODUCT, pmsg, sizeof(IDENTIFIER_PRODUCT) - 1) == 0)
            {
                pmsg += sizeof(IDENTIFIER_PRODUCT) - 1;
                strncpy(uevent_msg.product, pmsg, sizeof(uevent_msg.product));
            }
            else if (strncmp(IDENTIFIER_INTERFACE, pmsg, sizeof(IDENTIFIER_INTERFACE) - 1) == 0)
            {
                pmsg += sizeof(IDENTIFIER_INTERFACE) - 1;
                strncpy(uevent_msg.interface, pmsg, sizeof(uevent_msg.interface));
            }
            pmsg += (strlen(pmsg) + 1);
        }
        hl_callback_call(callback, &uevent_msg);
        usleep(100000);
    }
}
