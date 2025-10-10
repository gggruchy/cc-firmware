#include "config.h"
#if CONFIG_SUPPORT_OTA
#ifndef __OTA_H__
#define __OTA_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "hl_net_tool.h"
#include "widget.h"
#include "hl_tpool.h"

    // enum
    // {
    //     STARTUP_DID_OTA = 0,
    //     STARTUP_DIDNT_OTA = 1,
    // };

    // 是否要进行ota标志
    // void set_ota_startup(void);
    // void reset_ota_startup(void);
    // int get_ota_startup(void);

    time_t get_current_timestamp(void);

    // 获取ota相关信息
    // int ota_get_upgrade_cmd(bool is_active);
    // void ota_get_upgrade_version(char *version, int len);
    // int ota_get_upgrade_strategy(void);
    // char *ota_get_upgrade_md5(void);

    // http获取ota信息
    // int get_ota_upgrade_info_pthread_create(void);
    // int get_ota_upgrade_info_pthread_destroy(void);
    // hl_tpool_thread_t get_ota_upgrade_info_thread_handle(void);
    // uint8_t get_thread_complete_status(void);

    // 下载ota 升级文件
    int ota_download_upgradefile_init(void);
    int ota_download_upgradefile_deinit(void);
    hl_curl_download_state_t get_ota_download_state(uint64_t *download_offset, uint64_t *download_size);
    void get_ota_upgrade_filepath(char *filepath, int len);

    /**
     * OTA
     *  +-----------------+
     *           |
     *  +-----------------+-----------------+
     *  |                 |                 |
     *  v                 v                 v
     * AIC              Chitu            anymore
     *
     * 当前统一做成异步接口，避免影响对UI显示
     */

    /**
     * OTA 升级通道
     */
    typedef enum
    {
        OTA_FIREMARE_CH_SYS = 0,
        OTA_FIREMARE_CH_AIC,
        OTA_FIREMARE_CH_MAX
    } OTA_CH_t;

    /**
     * OTA 请求状态
     */
    typedef enum
    {
        OTA_API_STAT_INIT = 0,   /* init */
        OTA_API_STAT_REQUESTING, /* requesting */
        OTA_API_STAT_SUCCESS,    /* get result */
        OTA_API_STAT_FAILED,     /* get result */
        OTA_API_STAT_TIMEOUT,    /* request timeout */
    } OTA_API_ST_t;

    /**
     * OTA升级进度
     */
    typedef enum
    {
        /* Burn firmware file failed */
        OTAP_BURN_FAILED = -4,

        /* Check firmware file failed */
        OTAP_CHECK_FALIED = -3,

        /* Fetch firmware file failed */
        OTAP_FETCH_FAILED = -2,

        /* Initialized failed */
        OTAP_GENERAL_FAILED = -1,

        /* [0, 100], percentage of fetch progress */

        /* The minimum percentage of fetch progress */
        OTAP_FETCH_PERCENTAGE_MIN = 0,

        /* The maximum percentage of fetch progress */
        OTAP_FETCH_PERCENTAGE_MAX = 100
    } OTA_Progress_t;

    /**
     * 项目时间原因，这里功能并未实现
    */
    typedef enum
    {
        OTAG_GET_VERSION,    /* 向云端获取最新版本号 */
        OTAG_SET_VERSION,    /* 设置固件版本号(特殊用途) */
        OTAG_FETCH_FIRMWARE, /* 拉取固件 */
        OTAG_BURN_FIRMWARE,  /* 固件烧录 */
        OTAG_GET_FETCH_PROCESS,    /* 获取下去进度 */
        OTAG_GET_BURN_PROCESS,     /* 获取烧录进度 */
        OTAG_FILE_SIZE,
        OTAG_CHECK_FIRMWARE, /* Check firmware is valid no not */
        OTAG_MD5SUM,         /* MD5 in string format */
        OTAG_MAX,
    } OTA_CmdType_t;

    /**
     * 云端定义的返回字段，获取固件后赋值
     */
    struct _ota_info
    {
        bool update;        /*< 更新计划 - true/false >*/
        char version[256];  /*< 更新后版本号 >*/
        char packUrl[256];  /*< 更新包地址 >*/
        char packHash[33];  /*< MD5码 >*/
        uint8_t updateStrateg;  /*< 更新策略 1:强制 2：授权 >*/
        char log[1024];
        /*< ... anymore >*/
    };

    // int OTA_Ioctl(OTA_CH_t ch, OTA_CmdType_t type);

    /**
     * 向云端发送 OTA 请求信息
    */
    int ota_get_info_request( OTA_CH_t ch );

    /**
     * 获取请求结果
    */
    OTA_API_ST_t ota_get_info_result( OTA_CH_t ch , struct _ota_info *info );

    /**
     * 一些 OTA 操作/配置
    */
    int OTA_Ioctl(OTA_CH_t ch, OTA_CmdType_t type,void *buf,int buf_len);

    /**
     * 
    */
    int ota_aic_fetch_start( struct _ota_info *info );
    void ota_aic_upgrade_task_destory(void);

    /**
     * 系统固件下载
    */
    int ota_sys_fetch_start(struct _ota_info *info);


    /**
     * AI摄像头开始烧录
    */
    int ota_aic_burn_start(char *ota_name);

    bool is_ota_version_greater(char* cur_version);
    int otalib_md5sum(char *filename, char out_string[32]);


#ifdef __cplusplus
}
#endif
#endif
#endif
