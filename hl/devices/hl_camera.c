#include "hl_camera.h"
#include "hl_netlink_uevent.h"
#include "hl_common.h"
#include "hl_tpool.h"
#include "hl_assert.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <stdbool.h>
#include "ai_camera.h"

#define LOG_TAG "hl_camera"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

#define V4L2_BUF_COUNT 2
#define H264_CAMERA_WIDTH 1280
#define H264_CAMERA_HEIGHT 720
#define YUYV422_CAMERA_WIDTH 640
#define YUYV422_CAMERA_HEIGHT 480
#define MJPEG_CAMERA_WIDTH 640
#define MJPEG_CAMERA_HEIGHT 360

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define VIDEO_STREAM_DEBUG 0
#define VIDEO_CAPTURE_MJPEG 1
static void camera_interface_scan(void);
static void uevent_callback(const void *data, void *user_data);
static int camera_init(const char *devname);
static void camera_deinit(void);
static int camera_capture_on(void);
static void camera_capture_off(void);
static void camera_routine(hl_tpool_thread_t id, void *args);
static void camera_interface_scan_routine(hl_tpool_thread_t thread, void *args);
static int xioctl(int fh, int request, void *arg);

#if ENABLE_MANUTEST
extern int has_camera_uart;
extern int has_camera_usb;
#endif
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
    int mmap_buf_count;      // 帧缓存数量
    mmap_buffer_t *mmap_buf; // 帧缓存

    struct v4l2_format format;

    hl_camera_state_t state;
    pthread_mutex_t lock;
    char devname[256];
} camera_t;

static camera_t camera;
static hl_callback_t camera_cb;
static hl_tpool_thread_t camera_thread;
static hl_tpool_thread_t camera_interface_scan_thread;
static int camera_scan_enable = 1;
static bool video_exist_state = false;

void hl_camera_init(void)
{
    memset(&camera, 0, sizeof(camera));
    hl_callback_create(&camera_cb);
    pthread_mutex_init(&camera.lock, NULL);
    hl_netlink_uevent_register_callback(uevent_callback, NULL);
    HL_ASSERT(hl_tpool_create_thread(&camera_interface_scan_thread, camera_interface_scan_routine, NULL, 0, 0, 0, 0) == 0);
}

void hl_camera_register(hl_callback_function_t func, void *user_data)
{
    hl_callback_register(camera_cb, func, user_data);
}

void hl_camera_unregister(hl_callback_function_t func, void *user_data)
{
    hl_callback_unregister(camera_cb, func, user_data);
}

hl_camera_state_t hl_camera_get_state(void)
{
    hl_camera_state_t st;
    pthread_mutex_lock(&camera.lock);
    st = camera.state;
    pthread_mutex_unlock(&camera.lock);
    return st;
}

uint32_t hl_camera_get_format(void)
{
    return camera.format.fmt.pix.pixelformat;
}

void hl_camera_get_size(int *width, int *height)
{
    *width = camera.width;
    *height = camera.height;
}

int hl_camera_capture_on(void)
{
    pthread_mutex_lock(&camera.lock);
    if (camera.state & HL_CAMERA_STATE_CAPTURED)
    {
        // camera.capture_count++;
        LOG_D("hl_camera_capture_on:: capture_count : %d\n", camera.capture_count);
        pthread_mutex_unlock(&camera.lock);
        return 0;
    }

    if (camera_capture_on() == -1)
        goto failed;

    camera.capture_count++;
    LOG_D("hl_camera_capture_on :: %d\n", camera.capture_count);
    camera.state |= HL_CAMERA_STATE_CAPTURED;
    pthread_mutex_unlock(&camera.lock);
    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_CAPTURE_ON;
    hl_callback_call(camera_cb, &event);
    return 0;
failed:
    pthread_mutex_unlock(&camera.lock);
    return -1;
}

void hl_camera_capture_off(void)
{
    pthread_mutex_lock(&camera.lock);

    if (!(camera.state & HL_CAMERA_STATE_CAPTURED))
    {
        LOG_D("hl_camera_capture_off:: capture_count : %d\n", camera.capture_count);
        pthread_mutex_unlock(&camera.lock);
        return;
    }

    camera.capture_count--;
    if (camera.capture_count != 0)
    {
        LOG_I("camera capture_count %d\n", camera.capture_count);
        pthread_mutex_unlock(&camera.lock);
        return;
    }

    camera_capture_off();
    camera.state &= ~HL_CAMERA_STATE_CAPTURED;
    pthread_mutex_unlock(&camera.lock);

    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_CAPTURE_OFF;
    hl_callback_call(camera_cb, &event);
}

static void uevent_callback(const void *data, void *user_data)
{
    hl_netlink_uevent_msg_t *uevent_msg = (hl_netlink_uevent_msg_t *)data;
    int simple_retry = 0;
    int ret;
    if (strstr(uevent_msg->devname, "video"))
    {
        // if (strcmp(uevent_msg->action, "add") == 0)
        // {
        //     LOG_I("add camera %s\n", uevent_msg->devname);
        //     camera_init(uevent_msg->devname);
        // }
        // else
        // LOG_I("uevent action %s devname %s\n", uevent_msg->action, uevent_msg->devname);
        if (strcmp(uevent_msg->action, "remove") == 0)
        {
            video_exist_state = false;
            LOG_I("remove camera %s\n", uevent_msg->devname);
            pthread_mutex_lock(&camera.lock);
            camera.state &= ~HL_CAMERA_STATE_FOUND_VIDEO;
            memset(&camera.devname, 0, sizeof(camera.devname));
            pthread_mutex_unlock(&camera.lock);
            camera_deinit();
        }
    }
    else
        return;
}

static void camera_interface_scan_routine(hl_tpool_thread_t thread, void *args)
{
    uint64_t ticks = hl_get_tick_ms();
    for (;;)
    {
        if ((hl_camera_get_state() & HL_CAMERA_STATE_FOUND_VIDEO) == 0 && camera_scan_enable == 1)
        {
            if (hl_tick_is_overtime(ticks, hl_get_tick_ms(), 1000))
            {
                ticks = hl_get_tick_ms();
                camera_interface_scan();
            }
        }
        usleep(500000);
    }
}

void hl_camera_scan_enable(int enable)
{
    pthread_mutex_lock(&camera.lock);
    camera_scan_enable = enable;
    pthread_mutex_unlock(&camera.lock);
    return;
}

int hl_camera_get_exist_state(void)
{
    return video_exist_state;
}

static void camera_interface_scan(void)
{
    DIR *dirp;
    struct dirent *dp;
    char syspath[PATH_MAX];
    const char add[] = "add";
    dirp = opendir("/sys/class/video4linux");
    int mindevnum = INT32_MAX;
    int sec_min_num = INT32_MAX;
    char *pdevnum = NULL;
    int devnum_index = 0;
    int devnum[10]; // E100摄像头有四路UVC 序号不确定

    if (dirp == NULL)
    {
        LOG_I("open video4linux failed\n");
        video_exist_state = false;
        return;
    }
    // while ((dp = readdir(dirp)) != NULL)
    // {
    //     if ((pdevnum = strstr(dp->d_name, "video")) != NULL)
    //     {
    //         pdevnum += sizeof("video") - 1;
    //         devnum[devnum_index] = atoi(pdevnum);
    //         LOG_I("found camera devnum[%d] %d\n", devnum_index, devnum[devnum_index]);
    //         devnum_index++;
    //     }
    // }
    while ((dp = readdir(dirp)) != NULL)
    {
        char *pdevnum = NULL;
        int devnum;
        if ((pdevnum = strstr(dp->d_name, "video")) != NULL)
        {
            pdevnum += sizeof("video") - 1;
            devnum = atoi(pdevnum);
            LOG_I("found camera %d\n", devnum);
            if (devnum < mindevnum)
            {
                sec_min_num = mindevnum;
                mindevnum = devnum;
            }
            else
            {
                if( sec_min_num >= devnum )
                    sec_min_num = devnum;
            }
            LOG_D("min : %d , sec : %d\n", mindevnum, sec_min_num);
            // if (devnum < mindevnum)
            //     mindevnum = devnum;
            // snprintf(syspath, sizeof(syspath), "/sys/class/video4linux/%s/uevent", dp->d_name);
            // hl_echo(syspath, add, sizeof(add));
        }
    }
    closedir(dirp);

    /*在FDM E100中, 两个video_index是同一路UVC;另外两个video_index又是同一路UVC */
    // if (devnum_index >= 4)
    // {
    //     for (int i = 0; i < devnum_index; i++)
    //     {
    //         for (int j = i + 1; j < devnum_index; j++)
    //         {
    //             if (devnum[j] < devnum[i])
    //             {
    //                 devnum[i] = devnum[i] + devnum[j];
    //                 devnum[j] = devnum[i] - devnum[j];
    //                 devnum[i] = devnum[i] - devnum[j];
    //             }
    //         }
    //     }

    //     // utils_vfork_system("ls -l /dev/video*");
    //     // for (int i = 0; i < devnum_index; i++)
    //     //     LOG_I("found camera devnum[%d] %d\n", i, devnum[i]);
    //     // LOG_D("\n");

    //     mindevnum = devnum[0];
    //     sec_min_num = devnum[2];
    //     LOG_I("AIC_canera /dev/video%d\n", mindevnum);
    //     LOG_I("hl_canera  /dev/video%d\n", sec_min_num);
    // }
    // else if (devnum_index >= 2)
    // {
    //     mindevnum = devnum[0];
    //     sec_min_num = devnum[1];
    //     LOG_I("AIC_canera /dev/video%d\n", mindevnum);
    //     LOG_I("hl_canera  /dev/video%d\n", sec_min_num);
    // }

#if CONFIG_SUPPORT_AIC
    if (mindevnum != INT32_MAX && sec_min_num != INT32_MAX) // AI 摄像头有2路 uvc
    {
        char devname[256];
        snprintf(devname, sizeof(devname), "video%d", mindevnum);
        LOG_I("aic_probe final found devname %s\n", devname);
        aic_probe(devname);

        if (sec_min_num != mindevnum)
        {
            pthread_mutex_lock(&camera.lock);
            snprintf(camera.devname, sizeof(camera.devname), "video%d", sec_min_num);
            camera.state |= HL_CAMERA_STATE_FOUND_VIDEO;
            pthread_mutex_unlock(&camera.lock);
            LOG_I("camera_init final found devname %s\n", camera.devname);
            camera_init(camera.devname);
        }
        video_exist_state = true;
    }
    else
    {
        video_exist_state = false;
    }
#else
    if (mindevnum != INT32_MAX)
    {
        pthread_mutex_lock(&camera.lock);
        snprintf(camera.devname, sizeof(camera.devname), "video%d", mindevnum);
        camera.state |= HL_CAMERA_STATE_FOUND_VIDEO;
        pthread_mutex_unlock(&camera.lock);
        LOG_I("camera_init final found devname %s\n", camera.devname);
        camera_init(camera.devname);
        video_exist_state = true;
    }
#endif
}

static int camera_init(const char *devname)
{
    struct v4l2_capability capability;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format format;
    struct v4l2_queryctrl queryctrl;
    char devpath[PATH_MAX];
    int h264 = 0;
    snprintf(devpath, sizeof(devpath), "/dev/%s", devname);

    pthread_mutex_lock(&camera.lock);
    if (camera.state & HL_CAMERA_STATE_INITIALIZED)
    {
        pthread_mutex_unlock(&camera.lock);
        return 0;
    }
    if (strlen(camera.devname) == 0)
    {
        pthread_mutex_unlock(&camera.lock);
        return -1;
    }

    snprintf(devpath, sizeof(devpath), "/dev/%s", camera.devname);
    LOG_I("open camera devpath:%s\n", devpath);

    camera.fd = open(devpath, O_RDWR, 0); // 打开摄像头设备
    if (camera.fd == -1)
    {
        LOG_I("open camera failed %s %s\n", strerror(errno), devpath);
        pthread_mutex_unlock(&camera.lock);
        return -1;
    }

    if (xioctl(camera.fd, VIDIOC_QUERYCAP, &capability) == 0) // 查询设备属性，capabilities
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
        if ((capability.capabilities & V4L2_CAP_EXT_PIX_FORMAT))
        {
            LOG_I("camera supported :V4L2_CAP_EXT_PIX_FORMAT\n");
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
    while (xioctl(camera.fd, VIDIOC_ENUM_FMT, &fmtdesc) >= 0) // 枚举所有支持的格式
    {
        LOG_I("index %d type %d flags %x description %s pixelformat %x\n", fmtdesc.index, fmtdesc.type, fmtdesc.flags, fmtdesc.description, fmtdesc.pixelformat);
#if VIDEO_CAPTURE_MJPEG == 0
        if (fmtdesc.pixelformat == V4L2_PIX_FMT_H264)
            h264 = 1;
#endif
        struct v4l2_frmsizeenum frmsize;
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;
        while (ioctl(camera.fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) // 枚举所有支持的分辨率
        {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                LOG_I("V4L2_FRMSIZE_TYPE_DISCRETE: cw %d ch %d\n", frmsize.discrete.width, frmsize.discrete.height);
            }
            else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            {
                LOG_I("V4L2_FRMSIZE_TYPE_STEPWISE: cw %d ch %d\n", frmsize.discrete.width, frmsize.discrete.height);
            }
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera.fd, VIDIOC_CROPCAP, &cropcap) == 0) // 查询裁剪属性
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
        xioctl(camera.fd, VIDIOC_S_CROP, &crop);
    }

    CLEAR(format);
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 设置格式
#if VIDEO_CAPTURE_MJPEG
    format.fmt.pix.width = MJPEG_CAMERA_WIDTH;
    format.fmt.pix.height = MJPEG_CAMERA_HEIGHT;
#else
    if (h264)
    {
        format.fmt.pix.width = H264_CAMERA_WIDTH;
        format.fmt.pix.height = H264_CAMERA_HEIGHT;
    }
    else
    {
        format.fmt.pix.width = YUYV422_CAMERA_WIDTH;
        format.fmt.pix.height = YUYV422_CAMERA_HEIGHT;
    }
#endif

// 有H264使用H264，否则使用YUYV------------V4L2_PIX_FMT_MJPEG
#if VIDEO_CAPTURE_MJPEG
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
#else
    if (h264 == 0)
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    else
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
#endif

    format.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(camera.fd, VIDIOC_S_FMT, &format) == -1) // 设置格式
    {
        if (h264 == 0)
            LOG_I("camera can't support V4L2_PIX_FMT_YUYV %s\n", strerror(errno));
        else
            LOG_I("camera can't support V4L2_PIX_FMT_H264 %s\n", strerror(errno));
        goto err;
    }

    if (xioctl(camera.fd, VIDIOC_G_FMT, &format) == -1) // 获取格式
    {
        LOG_I("camera can't get format\n");
        goto err;
    }

    LOG_I("width %d height %d pixelformat %x\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat);
    camera.format = format;

    // 申请内存
    struct v4l2_requestbuffers requestbuffers;
    CLEAR(requestbuffers);
    requestbuffers.count = V4L2_BUF_COUNT;                        // 设置缓存数量
    requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;            // 设置缓存类型
    requestbuffers.memory = V4L2_MEMORY_MMAP;                     // 设置缓存类型
    if (xioctl(camera.fd, VIDIOC_REQBUFS, &requestbuffers) == -1) // 申请内存
    {
        LOG_I("camera can't get memory\n");
        goto err;
    }

    camera.mmap_buf_count = requestbuffers.count;
    camera.mmap_buf = calloc(camera.mmap_buf_count, sizeof(mmap_buffer_t));
    if (camera.mmap_buf == NULL)
    {
        LOG_I("camera can't allocate memory\n");
        goto err;
    }

    int mmap_count = 0;
    for (int i = 0; i < camera.mmap_buf_count; i++) // 映射内存到用户空间
    {
        struct v4l2_buffer buf; // 查询缓存
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera.fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            LOG_I("camera VIDIOC_QUERYBUF failed\n");
            goto err_free_mmap_buf;
        }

        camera.mmap_buf[i].length = buf.length;
        camera.mmap_buf[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera.fd, buf.m.offset);
        if (camera.mmap_buf[i].start == MAP_FAILED)
        {
            LOG_I("camera mmap failed\n");
            goto err_free_mmap_buf;
        }
        mmap_count++;
    }
    LOG_I("mmap_buf_count %d\n", camera.mmap_buf_count);

    struct v4l2_streamparm frameint;

    memset(&frameint, 0, sizeof(frameint));

    frameint.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frameint.parm.capture.timeperframe.numerator = 1;
    frameint.parm.capture.timeperframe.denominator = 10; // 设置为10帧每秒

    if (xioctl(camera.fd, VIDIOC_S_PARM, &frameint) == -1)
    {
        LOG_E("Setting frame rate");
        goto err_free_mmap_buf;
    }

    if (xioctl(camera.fd, VIDIOC_G_PARM, &frameint) == -1)
    {
        LOG_E("Getting frame rate");
        goto err_free_mmap_buf;
    }

    LOG_I("Current frame rate: %u/%u\n", frameint.parm.capture.timeperframe.numerator, frameint.parm.capture.timeperframe.denominator);

    camera.width = format.fmt.pix.width;
    camera.height = format.fmt.pix.height;
    camera.state |= HL_CAMERA_STATE_INITIALIZED;
    pthread_mutex_unlock(&camera.lock);
    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_INITIALIZED;
    hl_callback_call(camera_cb, &event);

    LOG_I("############################################camera init %s############################################\n", camera.devname);

// #if VIDEO_STREAM_DEBUG
#if 1
    hl_camera_capture_on();
#endif

    return 0;

err_free_mmap_buf:
    for (int i = 0; i < mmap_count; ++i)
        munmap(camera.mmap_buf[i].start, camera.mmap_buf[i].length);
    free(camera.mmap_buf);
err:
    close(camera.fd);
    pthread_mutex_unlock(&camera.lock);
    return -1;
}

/* 摄像头厂商定义使用BACKLIGHT_COMPENSATION参数控灯，1开0关 */
int camera_control_light(int light_state)
{
    int camera_fd;
    camera_fd = open("/dev/video0", O_RDWR, 0); // 打开摄像头设备
    if (camera_fd == -1)
    {
        LOG_I("open camera failed %s\n", strerror(errno));
        return -1;
    }

    struct v4l2_control control;
    memset(&control, 0, sizeof(control));
    control.id = V4L2_CID_BACKLIGHT_COMPENSATION;
    control.value = light_state;
    if (xioctl(camera_fd, VIDIOC_S_CTRL, &control) < 0)
    {
        LOG_E("camera control light error!!!\n");
    }

    close(camera_fd);
    return 0;
}

static void camera_deinit(void)
{
    pthread_mutex_lock(&camera.lock);
    if (!(camera.state & HL_CAMERA_STATE_INITIALIZED))
    {
        pthread_mutex_unlock(&camera.lock);
        return;
    }

    // close capture
    if (camera.state & HL_CAMERA_STATE_CAPTURED)
    {
        camera_capture_off();
        camera.capture_count = 0;
        camera.state &= ~HL_CAMERA_STATE_CAPTURED;
    }

    if (camera.state & HL_CAMERA_STATE_INITIALIZED)
    {
        for (int i = 0; i < camera.mmap_buf_count; ++i)
            munmap(camera.mmap_buf[i].start, camera.mmap_buf[i].length);
    }
    free(camera.mmap_buf);

    if (close(camera.fd) != 0)
        LOG_I("close error %s\n", strerror(errno));

    camera.state &= ~HL_CAMERA_STATE_INITIALIZED;
    pthread_mutex_unlock(&camera.lock);
    hl_camera_event_t event;
    event.event = HL_CAMERA_EVENT_DEINITIALIZED;
    hl_callback_call(camera_cb, &event);
    LOG_I("############################################camera deinit############################################\n");
#if VIDEO_STREAM_DEBUG
    hl_camera_capture_off();
#endif
}

static int camera_capture_on(void)
{
    for (int i = 0; i < camera.mmap_buf_count; i++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera.fd, VIDIOC_QBUF, &buf) == -1)
        {
            LOG_I("camera VIDIOC_QBUF failed\n");
            return -1;
        }
    }

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera.fd, VIDIOC_STREAMON, &type) == -1)
    {
        LOG_I("camera VIDIOC_STREAMON failed\n");
        return -1;
    }

    LOG_I("camera capture on\n");
    hl_tpool_create_thread(&camera_thread, camera_routine, NULL, 0, 0, 0, 0);
    hl_tpool_wait_started(camera_thread, 0);
    return 0;
}

static void camera_capture_off(void)
{
    hl_tpool_cancel_thread(camera_thread);
    hl_tpool_wait_completed(camera_thread, 0);
    hl_tpool_destory_thread(&camera_thread);
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera.fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        LOG_I("camera VIDIOC_STREAMOFF failed %s\n", strerror(errno));
        pthread_mutex_unlock(&camera.lock);
        return;
    }
    LOG_I("camera capture off\n");
}

static void camera_routine(hl_tpool_thread_t thread, void *args)
{
    LOG_I("camera thread start %d\n", camera.fd);
    struct timeval timestamp = {0};
    int ignore = 4;

    uint64_t ticks = 0;
    uint64_t gop_size = 0;
#if VIDEO_STREAM_DEBUG
    FILE *fp = NULL;
    fp = fopen("/mnt/nfs/FMD/test.video", "wb");
#endif
    while (!hl_tpool_thread_test_cancel(thread))
    {
        fd_set fds;
        struct timeval tv;
        int r;

        struct v4l2_buffer buf;
        FD_ZERO(&fds);
        FD_SET(camera.fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        r = select(camera.fd + 1, &fds, NULL, NULL, &tv);

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
        if (xioctl(camera.fd, VIDIOC_DQBUF, &buf) == -1) // 取出缓存
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

        if (buf.index < camera.mmap_buf_count)
        {
            hl_camera_event_t event;
            event.event = HL_CAMERA_EVENT_FRAME;
            event.data = camera.mmap_buf[buf.index].start;
            event.data_length = buf.bytesused;
            event.format = camera.format.fmt.pix.pixelformat;

#if ENABLE_MANUTEST
        // usb 取流正常：
        has_camera_usb =1;
#endif

            if (ignore > 0)
                ignore--;
            else
            {
#if VIDEO_STREAM_DEBUG
                fwrite(event.data, 1, event.data_length, fp);
                fflush(fp);
#endif
                hl_callback_call(camera_cb, &event);
            }

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
                sprintf(path, "/media/ubuntu/hl_camera%d.jpg", hl_camera_index);
                fp = fopen(path, "wb");
                fwrite(event.data, 1, event.data_length, fp);
                fflush(fp);
                fclose(fp);
            }
#endif
        }

        if (xioctl(camera.fd, VIDIOC_QBUF, &buf) == -1) // 重新放入缓存
        {
            LOG_I("QBUF failed\n");
            break;
        }
        usleep(100000);
    }
#if VIDEO_STREAM_DEBUG
    fclose(fp);
#endif
    LOG_I("camera thread stop\n");
}

static int xioctl(int fh, int request, void *arg)
{
    int r;
    do
    {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}
