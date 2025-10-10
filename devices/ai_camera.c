/**
 * filename :       ai_camera
 *
 * descrition :     AI camera - commit base on uart
 * tips       :     null
 *                  +-----idle
 *                  |      |
 *                  |      | 收到命令
 *                  |      |
 *                  |      v
 *                  |     write
 *                  |      |
 *                  |      | 发送对应的命令
 *                  |      |
 *                  |      v
 *                  |     read
 *                  |      |
 *                  |      | 等待回复
 *                  |      |
 *                  +------+-----------> Error
 *                    P         N
 */
#include "config.h"

#if CONFIG_SUPPORT_AIC
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <dirent.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "utils.h"
#include "serial.h"
#include "hl_tpool.h"
#include "utils_ehsm.h"
#include "hl_assert.h"
#include <unistd.h>
#include "ai_camera.h"
#include "crc16.h"
#include "params.h"
#include "hl_ts_queue.h"

#define LOG_TAG "ai_camera"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_NUM
#include "log.h"

#define DEBUG_SERIAL_LOG 1

/* AI 摄像头 串口通讯配置 */
struct
{
    char *device; /* < device path > */
    int32_t baud; /* < . > */
    char parity;  /* < . > */
    int data_bit; /* < . > */
    int stop_bit; /* < . > */
} uart_config = {
    .device = "/dev/ttyS3",
    .baud = 115200,
    .parity = 'N',
    .data_bit = 8,
    .stop_bit = 1};

/******************************************************
 *  Command : M - > S
 *  +--------+-------+-----+-------------+
 *  | Header |  CRC  | LEN | DATA( ... ) |
 *  +--------+-------+-----+-------------+
 *
 *  Respone : S - > M
 *  +--------+-------+-----+-------------+
 *  | Header |  CRC  | LEN | DATA( ... ) |
 *  +--------+-------+-----+-------------+
 *
 * ***************************************************/

#define AI_CAMERA_CMD_SIZE 64

typedef struct aic_tag
{
    utils_ehsm_t ehsm;
    utils_ehsm_state_t idle;
    utils_ehsm_state_t write;
    utils_ehsm_state_t read;
    utils_ehsm_state_t error;
} ai_camera_t;

/**
 * AI 摄像头 实时运行参数
 */
struct
{
    bool online; /* AI 摄像头 上线&&重传状态 */
    int s;       /* AI 摄像头 套接字 */

    /* 在 init 中初始化 */
    pthread_mutex_t lock; /* 锁 */
    hl_ts_queue_t queue;  /* 队列 */

    uint8_t retry; /* 重传次数 */

    utils_ehsm_t ehsm;
    utils_ehsm_state_t idle;
    utils_ehsm_state_t write;
    utils_ehsm_state_t read;
    utils_ehsm_state_t error;

    aic_cmd_t cur_cmd;

    uint8_t s_buff[AI_CAMERA_S_BUFFER_SIZE];
    uint8_t r_buff[AI_CAMERA_R_BUFFER_SIZE];

    char aic_verison[AIC_VERSION_MAX];

} ctx = {
    .online = false,
    .s = -1,
    .retry = AIC_RETRY_MAX,
};

static hl_tpool_thread_t ai_camera_thread;
static hl_callback_t cb;

#if ENABLE_MANUTEST
extern int has_camera_uart;
extern int has_camera_usb;
#endif

static int idle_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event);
static int write_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event);
static int read_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event);

static int send_package(uint16_t cmd, uint8_t *body, uint16_t body_size);
static int recv_trycheck(uint8_t *buffer, uint16_t size);
static int recv_unpackage(uint8_t *buffer);
static int ai_camera_mjpeg_init();
static void camera_test_cb(const void *data, void *user_data);
static void aic_msg_handler_cb(const void *data, void *user_data);

void ai_camera_routine(hl_tpool_thread_t thread, void *args)
{
    while (1)
    {
        static uint64_t last_tick = 0;
        utils_ehsm_run(&ctx.ehsm);

        usleep(10000);
    }
}

static int idle_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event)
{
    switch (event->event)
    {
    case UTILS_EHSM_IDLE_EVENT:
    {
        if (hl_ts_queue_peek_try(ctx.queue, &ctx.cur_cmd, 1) == 1)
        {
            // 收到 cmd
            // LOG_D("recv cmd : %x\n", ctx.cur_cmd.cmdid);
            utils_ehsm_tran(&ctx.ehsm, &ctx.write);
            hl_ts_queue_dequeue(ctx.queue, NULL, 1);
        }
    }
    break;

    case UTILS_EHSM_INIT_EVENT:
    {
    }
    break;

    case UTILS_EHSM_EXIT_EVENT:
    {
    }
    break;
    }
    return 0;
}

static int write_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event)
{
    switch (event->event)
    {
    case UTILS_EHSM_IDLE_EVENT:
    {
    }
    break;

    case UTILS_EHSM_INIT_EVENT:
    {
        send_package(ctx.cur_cmd.cmdid, (uint8_t *)&ctx.cur_cmd.body, ctx.cur_cmd.body_size);
        utils_ehsm_tran(&ctx.ehsm, &ctx.read);
    }
    break;
    }
    return 0;
}

static int read_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event)
{
    switch (event->event)
    {
    case UTILS_EHSM_IDLE_EVENT:
    {
        fd_set rset;
        struct timeval tv;
        int s_rc = -1;
        static uint16_t r_offset = 0;
        uint8_t r_temp_buffer[AI_CAMERA_R_BUFFER_SIZE];

        FD_ZERO(&rset);
        FD_SET(ctx.s, &rset);

        /* Wait for a message , we don't know when the message will be received */
        if (ctx.online == true) // 在线等 5s , 不在线等 1s(防止队列阻塞)
        {
            if (ctx.cur_cmd.cmdid == AIC_CMD_NOT_CAPTURE ||
                ctx.cur_cmd.cmdid == AIC_CMD_FOREIGN_CAPTURE ||
                ctx.cur_cmd.cmdid == AIC_CMD_MAJOR_CAPTURE)
            {
                tv.tv_sec = 40; // AI摄像头识别特殊处理
                tv.tv_usec = 0;
            }
            else
            {
                tv.tv_sec = 5;
                tv.tv_usec = 0;
            }
        }
        else
        {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
        }

        while ((s_rc = select(ctx.s + 1, &rset, NULL, NULL, &tv)) < 0)
        {
            if (errno == EINTR)
            {
                LOG_W("A non blocked signal was caught\n");
                /* Necessary after an error */
                FD_ZERO(&rset);
                FD_SET(ctx.s, &rset);
            }
            else
            {
                LOG_I("errnor : %s", strerror(errno));
                break;
            }
        }

        if (s_rc == 0)
        {
            /* timeout */
            if (ctx.retry > 0)
                LOG_D("read timeout , need retry %d\n", ctx.retry);
            r_offset = 0;
            memset(ctx.r_buff, 0, AI_CAMERA_R_BUFFER_SIZE);

            if (ctx.cur_cmd.cmdid == AIC_CMD_NOT_CAPTURE ||
                ctx.cur_cmd.cmdid == AIC_CMD_FOREIGN_CAPTURE ||
                ctx.cur_cmd.cmdid == AIC_CMD_MAJOR_CAPTURE)
            {
                // 炒面/异物 超时处理
                aic_ack_t resp;
                resp.is_timeout = true;
                resp.cmdid = ctx.cur_cmd.cmdid;
                resp.body_size = ctx.cur_cmd.body_size;
                if (resp.body_size != 0)
                    memcpy(resp.body, ctx.cur_cmd.body, resp.body_size);
                hl_callback_call(cb, (void *)&resp);
            }

            if (ctx.retry > 0)
                ctx.retry--;

            if (ctx.retry == 0)
            {
                // 掉线了，取消重传机制
                {
                    aic_ack_t resp;
                    resp.is_timeout = true;
                    resp.cmdid = ctx.cur_cmd.cmdid;
                    resp.body_size = ctx.cur_cmd.body_size;
                    if (resp.body_size != 0)
                        memcpy(resp.body, ctx.cur_cmd.body, resp.body_size);
                    hl_callback_call(cb, (void *)&resp);
                }
                ctx.online = false;
                utils_ehsm_tran(&ctx.ehsm, &ctx.idle);
            }
            else
            {
                LOG_D("resend\n");
                utils_ehsm_tran(&ctx.ehsm, &ctx.write);
                break;
            }
        }

        s_rc = serial_read(ctx.s, r_temp_buffer, AI_CAMERA_R_BUFFER_SIZE);
#if DEBUG_SERIAL_LOG
        if (r_temp_buffer[6] != AIC_CMD_GET_STATUS && s_rc > 0)
        {
            char serial_log[128];
            char serial_str[64];
            memset(serial_log, 0, sizeof(serial_log));
            memset(serial_str, 0, sizeof(serial_str));
            // printf("\033[31mserial_rd: \033[0m");
            strcat(serial_log, "serial_rd: ");
            for (int i = 0; i < s_rc; i++)
            {
                if (i == 6)
                {
                    // printf("\033[31m%.2X \033[0m", r_temp_buffer[i]);
                    sprintf(serial_str, "[%.2X] ", r_temp_buffer[i]);
                    strcat(serial_log, serial_str);
                }
                else
                {
                    // printf("%.2X ", r_temp_buffer[i]);
                    sprintf(serial_str, "%.2X ", r_temp_buffer[i]);
                    strcat(serial_log, serial_str);
                }
            }
            // printf("\n");
            // printf("\n");
            strcat(serial_log, "\n\n");
            LOG_I(serial_log);
        }
#endif

        for (int i = 0; i < s_rc; i++)
        {
            if (r_offset >= AI_CAMERA_R_BUFFER_SIZE)
                break;
            ctx.r_buff[r_offset] = r_temp_buffer[i];
            r_offset++;
        }

        // try unpackage
        int ret = recv_trycheck(ctx.r_buff, r_offset);
        switch (ret)
        {
        case -1:
        case -2:
            // LOG_D("continue recv\n");
            break;
        case -3:
            // crc 校验出错
            r_offset = 0;
            break;

        case 0:
            // it's ok
            // for (int i = 0; i < r_offset; i++)
            // {
            //     printf("<%.2X>", ctx.r_buff[i]);
            // }
            // printf("\n");
            r_offset = 0;

            // success , unpackage
            recv_unpackage(ctx.r_buff);

            if (ctx.online == false)
            {
                ctx.online = true; // 上线处理
#if ENABLE_MANUTEST
                has_camera_uart = 1;
#endif
                LOG_I("ai camera online\n");
                ai_camera_send_cmd_handler(AIC_CMD_GET_VERSION, AIC_CMD_CARRY_NULL);
                ai_camera_send_cmd_handler(AIC_CMD_AI_FUNCTION, AIC_CMD_CARRY_ON_AI_FUNCTION); // 常开,使用flag控制炒面及异物
            }

            ctx.retry = AIC_RETRY_MAX; // 恢复重传次数

            utils_ehsm_tran(&ctx.ehsm, &ctx.idle);
            break;
        }
    }
    break;
    case UTILS_EHSM_INIT_EVENT:
    {
        // 发送完成 , 进入接收 ack 等待
        memset(ctx.r_buff, 0, AI_CAMERA_R_BUFFER_SIZE);
    }
    break;
    }
    return 0;
}

int ai_camera_init()
{
    /* open serial */
    pthread_mutex_init(&ctx.lock, NULL);
    ctx.s = serial_open(uart_config.device, uart_config.parity, uart_config.stop_bit, uart_config.data_bit, uart_config.baud);
    LOG_D("serial open sock : %d\n", ctx.s);
    HL_ASSERT(ctx.s != -1);

    hl_ts_queue_create(&ctx.queue, sizeof(aic_cmd_t), AI_CAMERA_CMD_SIZE);
    HL_ASSERT(hl_callback_create(&cb) == 0);

    utils_ehsm_create_state(&ctx.idle, NULL, idle_state_handler, "IDLE");
    utils_ehsm_create_state(&ctx.write, NULL, write_state_handler, "WRITE");
    utils_ehsm_create_state(&ctx.read, NULL, read_state_handler, "READ");
    utils_ehsm_create_state(&ctx.error, NULL, NULL, "ERROR");
    utils_ehsm_create(&ctx.ehsm, "ai_camera", &ctx.idle, NULL, 64, NULL);

    /* 状态机线程 */
    HL_ASSERT(hl_tpool_create_thread(&ai_camera_thread, ai_camera_routine, NULL, 0, 0, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(ai_camera_thread, 0) == 1);

    /* usb mjpeg */
    ai_camera_mjpeg_init();

    ai_camera_resp_cb_register(aic_msg_handler_cb, NULL);
    return 0;
}

/*******************************************************************************
 * @  	public pre-definition
 *******************************************************************************/
#define HEADER 0x2309

/**************************************************
 *
 * Send Class
 *
 **************************************************/

#define AIC_LEN_HEADER 2
#define AIC_LEN_CRC 2
#define AIC_LEN_LENGTH 2
#define AIC_LEN_BODY_MAX 128
/* 数据包最大字节数 */
#define AIC_LEN_TOTAL_MAX AIC_LEN_HEADER +     \
                              AIC_LEN_CRC +    \
                              AIC_LEN_LENGTH + \
                              AIC_LEN_BODY_MAX
/* 数据包最小字节数 */
#define AIC_LEN_MIN_SIZE AIC_LEN_HEADER +  \
                             AIC_LEN_CRC + \
                             AIC_LEN_LENGTH

/*******************************************************************************
 * @  	send/recv pre-definition
 *******************************************************************************/
typedef union
{
    struct __attribute__((packed))
    {
        uint16_t header;
        uint16_t crc;
        uint16_t len;
        uint8_t body[AIC_LEN_BODY_MAX];
    } name;

    struct __attribute__((packed))
    {
        uint8_t by[AIC_LEN_TOTAL_MAX];
    } byte;
} AIC_PAYLOAD_TypeDef;

static bool enforce_state = false; // 提高指令优先级,可中断其他指令操作

/**
 * @brief:  pack send buffer
 */
static int send_package(uint16_t cmd, uint8_t *body, uint16_t body_size)
{
    int len_ret;
    AIC_PAYLOAD_TypeDef send_buffer;

    uint16_t l_offset_ext = AIC_LEN_HEADER + AIC_LEN_CRC + AIC_LEN_LENGTH;

    send_buffer.name.header = HEADER;
    send_buffer.name.len = body_size;

    if (body_size != 0)
        memcpy(send_buffer.name.body, body, body_size);

    l_offset_ext += body_size;

    // crc
    send_buffer.name.crc = crc_16((u_int8_t *)&(send_buffer.byte.by[AIC_LEN_HEADER + AIC_LEN_CRC]), body_size + 2);

#if DEBUG_SERIAL_LOG
    if (send_buffer.byte.by[6] != AIC_CMD_GET_STATUS)
    {
        char serial_log[128];
        char serial_str[64];
        memset(serial_log, 0, sizeof(serial_log));
        memset(serial_str, 0, sizeof(serial_str));
        // printf("\033[31mserial_wr: \033[0m");
        strcat(serial_log, "serial_wr: ");
        for (int i = 0; i < l_offset_ext; i++)
        {
            if (i == 6)
            {
                // printf("\033[31m%.2X \033[0m", send_buffer.byte.by[i]);
                sprintf(serial_str, "[%.2X] ", send_buffer.byte.by[i]);
                strcat(serial_log, serial_str);
            }
            else
            {
                // printf("%.2X ", send_buffer.byte.by[i]);
                sprintf(serial_str, "%.2X ", send_buffer.byte.by[i]);
                strcat(serial_log, serial_str);
            }
        }

        // printf("\n");
        strcat(serial_log, "\n");
        LOG_I(serial_log);
    }
#endif
    len_ret = serial_write(ctx.s, send_buffer.byte.by, l_offset_ext);
    if (len_ret != l_offset_ext)
        LOG_W("write not match : %d[need : %d]\n", len_ret, l_offset_ext);
    return 0;
}

static int recv_trycheck(uint8_t *buffer, uint16_t size)
{
    uint16_t pack_size;
    uint16_t crc1;
    uint16_t crc2;

    if (size < (AIC_LEN_MIN_SIZE))
    {
        /* 可能没收全 , 理应继续收 */
        return -1;
    }

    pack_size = (buffer[4] << 0 | buffer[5] << 8);
    if (size != (pack_size + AIC_LEN_MIN_SIZE))
    {
        /* 包不完整 , 可能没收全 , 继续收 */
        return -2;
    }

    // if come here . maybe bytes are ok .
    crc1 = crc_16((u_int8_t *)&buffer[AIC_LEN_HEADER + AIC_LEN_CRC], pack_size + AIC_LEN_LENGTH);
    crc2 = buffer[2] << 8 | buffer[3] << 0;
    if (crc1 != crc2)
    {
        LOG_D("crc not match [%x]-[%x]\n", crc1, crc2);
        return -3;
    }

    // if come here . maybe it's complete package
    return 0;
}

static int recv_unpackage(uint8_t *buffer)
{
    // incoming data is a complete package
    AIC_PAYLOAD_TypeDef *data = (AIC_PAYLOAD_TypeDef *)buffer;
    uint8_t cmd_id = data->name.body[0];

    // hb && status
    aic_ack_t resp;
    resp.is_timeout = false;
    resp.cmdid = cmd_id;
    resp.body_size = data->name.len;
    if (resp.body_size != 0)
        memcpy(resp.body, data->name.body, resp.body_size);
    hl_callback_call(cb, (void *)&resp);

    return 0;
}

void aic_cmd_enforce_request(void)
{
    enforce_state = true;
}

/*      命令入队        */
static int aic_cmd_request(ai_camera_cmd_t cmd_id, uint8_t *body, uint16_t body_size)
{
    aic_cmd_t cmd;

    cmd.cmdid = cmd_id;
    if (body_size != 0)
    {
        memcpy(cmd.body, body, body_size);
    }
    cmd.body_size = body_size;

    if (enforce_state) // 优先执行
    {
        enforce_state = false;
        hl_ts_queue_reset(ctx.queue);
        hl_ts_queue_enqueue(ctx.queue, &cmd, 1);

        if (hl_ts_queue_peek_try(ctx.queue, &ctx.cur_cmd, 1) == 1)
        {
            // 收到 cmd
            // LOG_D("recv cmd : %x\n", ctx.cur_cmd.cmdid);
            utils_ehsm_tran(&ctx.ehsm, &ctx.write);
            hl_ts_queue_dequeue(ctx.queue, NULL, 1);
        }
    }
    else
    {
        // 若队列满,就不入队;避免队列锁住
        if (!hl_ts_queue_is_full(ctx.queue))
            hl_ts_queue_enqueue(ctx.queue, &cmd, 1);
    }
    return 0;
}
/*******************************************************************************
 * @  	public API
 *******************************************************************************/
void ai_camera_resp_cb_register(hl_callback_function_t callback, void *user_data)
{
    hl_callback_register(cb, callback, user_data);
}

void ai_camrea_resp_cb_unregister(hl_callback_function_t callback, void *user_data)
{
    hl_callback_unregister(cb, callback, user_data);
}

bool aic_get_online()
{
    return ctx.online;
}

int ai_camera_send_cmd_handler(uint8_t cmd, uint8_t state)
{
    uint8_t body[AI_CAMERA_S_BUFFER_SIZE];
    switch (cmd)
    {
    case AIC_CMD_GET_STATUS:
    case AIC_CMD_GET_VERSION:
        body[0] = cmd;
        aic_cmd_request(cmd, body, 0x01);
        break;
    case AIC_CMD_NOT_CAPTURE:
    case AIC_CMD_FOREIGN_CAPTURE:
    case AIC_CMD_CAMERA_LIGHT:
    case AIC_CMD_AI_FUNCTION:
    case AIC_CMD_MAJOR_CAPTURE:
        body[0] = cmd;
        body[1] = state;
        aic_cmd_request(cmd, body, 0x02);
        break;
    }
    return 0;
}

static void aic_msg_handler_cb(const void *data, void *user_data)
{
    aic_ack_t *resp = (aic_ack_t *)data;

    // LOG_D("resp is time out : %d\n", resp->is_timeout);
    // LOG_D("cmd = 0x%x\n", resp->cmdid);
    // LOG_D("body size = 0x%x\n", resp->body_size);

    // for (int i = 0; i < resp->body_size; i++)
    //     printf("<%.2x>", resp->body[i]);
    // printf("\r\n");
    switch (resp->cmdid)
    {
    case AIC_CMD_GET_STATUS:
        break;
    case AIC_CMD_GET_VERSION:
    {
        if (resp->body_size == 4)
        {
            // 内部要求,与云端交互转换格式修改为 AA.BB.CC
            sprintf(ctx.aic_verison, "%02d.%02d.%02d", resp->body[1], resp->body[2], resp->body[3]);
        }
    }
    break;
    }
}

char *aic_get_version()
{
    if (ctx.online == true)
    {
        if (strlen(ctx.aic_verison) > 0)
            return ctx.aic_verison;
        else
            return "-";
    }
    else
        return "-";
}

/***************************************************************************************
 * AI camera MJPEG
 * driver part
 ****************************************************************************************/
#include <linux/videodev2.h>
#include <dirent.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "hl_camera.h"

static hl_tpool_thread_t ai_camera_thread;
static hl_callback_t aic_cb;

static void camera_routine(hl_tpool_thread_t thread, void *args);

typedef struct
{
    void *start;
    size_t length;
} mmap_buffer_t;
typedef struct
{
    int fd;
    uint16_t width;
    uint16_t height;

    int capture_count;
    int mmap_buf_count;
    mmap_buffer_t *mmap_buf;

    struct v4l2_format format;

    hl_camera_state_t state;
    pthread_mutex_t lock;
} camera_t;

static camera_t ai_camera;
#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int xioctl(int fh, int request, void *arg)
{
    int r;
    do
    {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

int aic_probe(const char *devname)
{
    struct v4l2_capability capability;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format format;
    struct v4l2_queryctrl queryctrl;
    char devpath[PATH_MAX];

    snprintf(devpath, sizeof(devpath), "/dev/%s", devname);

    pthread_mutex_lock(&ai_camera.lock);
    if (ai_camera.state & HL_CAMERA_STATE_INITIALIZED)
    {
        return 0;
    }

    LOG_I("open camera devpath:%s\n", devpath);
    ai_camera.fd = open(devpath, O_RDWR, 0);
    if (ai_camera.fd == -1)
    {
        LOG_I("open camera failed %s %s\n", strerror(errno), devpath);
        pthread_mutex_unlock(&ai_camera.lock);
        return -1;
    }

    // 查看该设备是否为视频采集设备
    if (xioctl(ai_camera.fd, VIDIOC_QUERYCAP, &capability) == 0)
    {
        if (!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
            LOG_I("camera not supported :V4L2_CAP_VIDEO_CAPTURE\n");
            goto err;
        }
        if (!(capability.capabilities & V4L2_CAP_STREAMING))
        {
            LOG_I("camera not supported :V4L2_CAP_STREAMING\n");
            goto err;
        }
    }
    else
    {
        LOG_I("camera can't get capabilities\n");
        goto err;
    }

    CLEAR(fmtdesc);
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (xioctl(ai_camera.fd, VIDIOC_ENUM_FMT, &fmtdesc) >= 0)
    {
        LOG_I("index %d type %d flags %x description %s pixelformat %x\n", fmtdesc.index, fmtdesc.type, fmtdesc.flags, fmtdesc.description, fmtdesc.pixelformat);
        struct v4l2_frmsizeenum frmsize;
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;
        while (ioctl(ai_camera.fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0)
        {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                LOG_I("V4L2_FRMSIZE_TYPE_DISCRETE: cw %d ch %d\n", frmsize.discrete.width, frmsize.discrete.height);
            else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
                LOG_I("V4L2_FRMSIZE_TYPE_STEPWISE: cw %d ch %d\n", frmsize.discrete.width, frmsize.discrete.height);
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(ai_camera.fd, VIDIOC_CROPCAP, &cropcap) == 0)
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
        xioctl(ai_camera.fd, VIDIOC_S_CROP, &crop);
    }

    // 设置采集格式
    CLEAR(format);
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 1280;
    format.fmt.pix.height = 720;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(ai_camera.fd, VIDIOC_S_FMT, &format) == -1)
    {
        LOG_I("camera can't support V4L2_PIX_FMT_MJPEG %s\n", strerror(errno));
        goto err;
    }
    if (xioctl(ai_camera.fd, VIDIOC_G_FMT, &format) == -1)
    {
        LOG_I("camera can't get format\n");
        goto err;
    }
    LOG_I("width %d height %d pixelformat %x\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat);

    // 申请内核空间
    struct v4l2_requestbuffers requestbuffers;
    CLEAR(requestbuffers);
    requestbuffers.count = 2;
    requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    requestbuffers.memory = V4L2_MEMORY_MMAP;
    if (xioctl(ai_camera.fd, VIDIOC_REQBUFS, &requestbuffers) == -1)
    {
        LOG_I("camera can't get memory\n");
        goto err;
    }

    ai_camera.mmap_buf_count = requestbuffers.count;
    ai_camera.mmap_buf = calloc(ai_camera.mmap_buf_count, sizeof(mmap_buffer_t));
    if (ai_camera.mmap_buf == NULL)
    {
        LOG_I("camera can't allocate memory\n");
        goto err;
    }

    // 把内核缓冲区队列映射到用户地址空间
    int mmap_count = 0;
    for (int i = 0; i < ai_camera.mmap_buf_count; i++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(ai_camera.fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            LOG_I("ai_camera VIDIOC_QUERYBUF failed\n");
            goto err_free_mmap_buf;
        }

        ai_camera.mmap_buf[i].length = buf.length;
        ai_camera.mmap_buf[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, ai_camera.fd, buf.m.offset);
        if (ai_camera.mmap_buf[i].start == MAP_FAILED)
        {
            LOG_I("camera mmap failed\n");
            goto err_free_mmap_buf;
        }
        mmap_count++;
    }
    LOG_I("mmap_buf_count %d\n", ai_camera.mmap_buf_count);

    ai_camera.width = format.fmt.pix.width;
    ai_camera.height = format.fmt.pix.height;
    ai_camera.state |= HL_CAMERA_STATE_INITIALIZED;
    pthread_mutex_unlock(&ai_camera.lock);
    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_INITIALIZED;
    hl_callback_call(aic_cb, &event);

    LOG_I("############################################ai camera init %s############################################\n", devname);
    aic_capture_on();
    return 0;

err_free_mmap_buf:
    for (int i = 0; i < mmap_count; ++i)
        munmap(ai_camera.mmap_buf[i].start, ai_camera.mmap_buf[i].length);
    free(ai_camera.mmap_buf);
err:
    close(ai_camera.fd);
    pthread_mutex_unlock(&ai_camera.lock);
    return -1;
}

void aic_exit(void)
{
    pthread_mutex_lock(&ai_camera.lock);
    if (!(ai_camera.state & HL_CAMERA_STATE_INITIALIZED))
    {
        pthread_mutex_unlock(&ai_camera.lock);
        return;
    }

    // close capture
    if (ai_camera.state & HL_CAMERA_STATE_CAPTURED)
    {
        // camera_capture_off();
        ai_camera.capture_count = 0;
        ai_camera.state &= ~HL_CAMERA_STATE_CAPTURED;
    }

    if (ai_camera.state & HL_CAMERA_STATE_INITIALIZED)
    {
        for (int i = 0; i < ai_camera.mmap_buf_count; ++i)
            munmap(ai_camera.mmap_buf[i].start, ai_camera.mmap_buf[i].length);
    }
    free(ai_camera.mmap_buf);

    if (close(ai_camera.fd) != 0)
        LOG_I("close error %s\n", strerror(errno));

    ai_camera.state &= ~HL_CAMERA_STATE_INITIALIZED;
    pthread_mutex_unlock(&ai_camera.lock);
    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_DEINITIALIZED;
    hl_callback_call(aic_cb, &event);
    LOG_I("############################################ai camera deinit############################################\n");
    aic_capture_off();
}

static int camera_capture_on(void)
{
    for (int i = 0; i < ai_camera.mmap_buf_count; i++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(ai_camera.fd, VIDIOC_QBUF, &buf) == -1)
        {
            LOG_I("camera VIDIOC_QBUF failed\n");
            return -1;
        }
    }

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(ai_camera.fd, VIDIOC_STREAMON, &type) == -1)
    {
        LOG_I("camera VIDIOC_STREAMON failed\n");
        return -1;
    }

    LOG_I("camera capture on\n");
    hl_tpool_create_thread(&ai_camera_thread, camera_routine, NULL, 0, 0, 0, 0);
    hl_tpool_wait_started(ai_camera_thread, 0);
    return 0;
}
int aic_capture_on(void)
{
    pthread_mutex_lock(&ai_camera.lock);
    if (ai_camera.state & HL_CAMERA_STATE_CAPTURED)
    {
        ai_camera.capture_count++;
        pthread_mutex_unlock(&ai_camera.lock);
        return 0;
    }

    if (camera_capture_on() == -1)
        goto failed;

    ai_camera.capture_count++;
    ai_camera.state |= HL_CAMERA_STATE_CAPTURED;
    pthread_mutex_unlock(&ai_camera.lock);
    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_CAPTURE_ON;
    hl_callback_call(aic_cb, &event);
    return 0;
failed:
    LOG_E("aic_capture_on failed\n");
    pthread_mutex_unlock(&ai_camera.lock);
    return -1;
}

static void camera_capture_off(void)
{
    hl_tpool_cancel_thread(ai_camera_thread);
    hl_tpool_wait_completed(ai_camera_thread, 0);
    hl_tpool_destory_thread(&ai_camera_thread);
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(ai_camera.fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        LOG_I("camera VIDIOC_STREAMOFF failed %s\n", strerror(errno));
        pthread_mutex_unlock(&ai_camera.lock);
        return;
    }
    LOG_I("camera capture off\n");
}

void aic_capture_off(void)
{
    LOG_I("aic_capture_off\n");
    pthread_mutex_lock(&ai_camera.lock);

    if (!(ai_camera.state & HL_CAMERA_STATE_CAPTURED))
    {
        pthread_mutex_unlock(&ai_camera.lock);
        return;
    }

    ai_camera.capture_count--;
    if (ai_camera.capture_count != 0)
    {
        LOG_I("camera capture_count %d\n", ai_camera.capture_count);
        pthread_mutex_unlock(&ai_camera.lock);
        return;
    }

    camera_capture_off();
    ai_camera.state &= ~HL_CAMERA_STATE_CAPTURED;
    pthread_mutex_unlock(&ai_camera.lock);

    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_CAPTURE_OFF;
    hl_callback_call(aic_cb, &event);
    return;
}

static void camera_routine(hl_tpool_thread_t thread, void *args)
{
    LOG_I("camera thread start %d\n", ai_camera.fd);
    struct timeval timestamp = {0};
    int ignore = 4;
    while (!hl_tpool_thread_test_cancel(thread))
    {
        fd_set fds;
        struct timeval tv;
        int r;

        struct v4l2_buffer buf;
        FD_ZERO(&fds);
        FD_SET(ai_camera.fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        r = select(ai_camera.fd + 1, &fds, NULL, NULL, &tv);

        if (r == -1)
        {
            LOG_I("select error %s\n", strerror(errno));
            break;
        }
        else if (r == 0)
        {
            LOG_I("select timeout\n");
            continue;
        }

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(ai_camera.fd, VIDIOC_DQBUF, &buf) == -1)
        {
            if (errno == EAGAIN || errno == EIO)
            {
                continue;
            }
            else
            {
                LOG_I("VIDIOC_DQBUF error %s\n", strerror(errno));
                break;
            }
        }
        if (buf.index < ai_camera.mmap_buf_count)
        {
            hl_camera_event_t event;
            event.event = HL_CAMERA_EVENT_FRAME;
            event.data = ai_camera.mmap_buf[buf.index].start;
            event.data_length = buf.bytesused;
#if ENABLE_MANUTEST
            // usb 取流正常：
            has_camera_usb =1;
#endif

            if (ignore > 0)
                ignore--;
            else
                hl_callback_call(aic_cb, &event);

#if 0
            static int start_num = 0;
            static int hl_camera_index = 0;
            if (start_num <= 10 && hl_camera_index <= 100)
            {
                HL_ASS_LOG LOG_I("event.data_length:%d\n", event.data_length);

                FILE *fp = NULL;
                char path[128];
                start_num++;
                hl_camera_index++;
                sprintf(path, "/media/ubuntu/ai_camera%d.jpg", hl_camera_index);
                fp = fopen(path, "wb");
                fwrite(event.data, 1, event.data_length, fp);
                fflush(fp);
                fclose(fp);
            }
#endif
        }

        if (xioctl(ai_camera.fd, VIDIOC_QBUF, &buf) == -1)
        {
            LOG_I("QBUF failed\n");
            break;
        }
        usleep(10000);
    }
    LOG_I("camera thread stop\n");
}

void aic_register(hl_callback_function_t func, void *user_data)
{
    hl_callback_register(aic_cb, func, user_data);
}

void aic_unregister(hl_callback_function_t func, void *user_data)
{
    hl_callback_unregister(aic_cb, func, user_data);
}

static int ai_camera_mjpeg_init()
{
    // globa init
    pthread_mutex_init(&ai_camera.lock, NULL);
    hl_callback_create(&aic_cb);
    aic_register(camera_test_cb, NULL);
}

/***************************************************************************************
 * AI camera TLP
 *
 ****************************************************************************************/
static void camera_test_cb(const void *data, void *user_data)
{
    hl_camera_event_t *event = (hl_camera_event_t *)data;

    switch (event->event)
    {
    case HL_CAMERA_EVENT_FRAME:
        break;
    case HL_CAMERA_EVENT_INITIALIZED:
        LOG_D("aic init\n");
        break;
    case HL_CAMERA_EVENT_DEINITIALIZED:
        LOG_D("aic deinit\n");
        break;
    case HL_CAMERA_EVENT_CAPTURE_ON:
        LOG_D("capture on\n");
        break;
    case HL_CAMERA_EVENT_CAPTURE_OFF:
        LOG_D("capture off\n");
        break;
    }
}

/**
 * AI 摄像头升级文件
 */
bool aic_upgrade_file_identify(char *path)
{
    char aic_file_name[PATH_MAX_LEN];

    if (path == NULL)
        return false;

    strcpy(aic_file_name, utils_get_file_name(path));
    LOG_D("get filename : %s\n", aic_file_name);
    LOG_D("\tget suffix : %s\n", utils_get_suffix(aic_file_name));

    if ((strncmp(aic_file_name, "UC", 2) == 0) && (strcmp(utils_get_suffix(aic_file_name), "bin") == 0))
        return true;
    else
        return false;
}

#endif
