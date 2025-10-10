#ifndef _AI_CAMERA_H_
#define _AI_CAMERA_H_

#include "config.h"
#if CONFIG_SUPPORT_AIC
#ifdef __cplusplus
extern "C"
{
#endif
#include "app.h"
#include "hl_callback.h"

#define AIC_RETRY_MAX 5    /* 重传次数 */
#define AIC_VERSION_MAX 32 /* AI摄像头版本号 */

#define AI_CAMERA_S_BUFFER_SIZE 128 // 发送数据包限制
#define AI_CAMERA_R_BUFFER_SIZE 128 // 接受数据包限制

    typedef enum
    {
        AIC_CMD_GET_STATUS = 0x00,      /* 获取 AI 识别状态*/
        AIC_CMD_TLP_CAPTURE = 0x01,     /* 延时摄影 抓拍 */
        AIC_CMD_TLP_CLEAN = 0x02,       /* 延时摄影 清除 */
        AIC_CMD_NOT_CAPTURE = 0x03,     /* 通知 AI 打印中(炒面识别指令) */
        AIC_CMD_GET_VERSION = 0x04,     /* 获取 AI 版本号 */
        AIC_CMD_FOREIGN_CAPTURE = 0x05, /* 通知 AI 打印前(异物识别指令) */
        AIC_CMD_CAMERA_LIGHT = 0x06,    /* 通知 AI 摄像头照明灯控制指令 */
        AIC_CMD_AI_FUNCTION = 0x07,     /* 通知 AI AI功能(开启/关闭)控制指令 */
        AIC_CMD_MAJOR_CAPTURE = 0x08,     /* 通知 AI 打印中(炒面识别指令) 专业模式 */
    } ai_camera_cmd_t;

    typedef enum
    {
        /* 打印中(炒面识别指令) 携带操作 */
        AIC_CMD_CARRY_ACTIVATE_CHOW_MEIN = 0x00, // 启动炒面监测
        AIC_CMD_CARRY_NORMAL_CHOW_MEIN = 0x01,   // 正常炒面监测
        AIC_CMD_CARRY_FINALLY_CHOW_MEIN = 0x02,  // 最后一次炒面监测

        /* 打印前(异物识别指令) 携带操作 */
        AIC_CMD_CARRY_ACTIVATE_AI_MONITOR = 0x00, // 启动AI监测
        AIC_CMD_CARRY_NORMAL_AI_MONITOR = 0x01,   // 正常AI监测
        AIC_CMD_CARRY_FINALLY_AI_MONITOR = 0x02,  // 最后一次AI监测

        /* 摄像头照明灯控制指令 携带操作 */
        AIC_CMD_CARRY_GET_LED_STATE = 0x00, // 获取LED灯状态
        AIC_CMD_CARRY_OFF_LED = 0x01,       // 关闭LED灯
        AIC_CMD_CARRY_ON_LED = 0x02,        // 开启LED灯

        /* AI功能(开启/关闭)控制指令 携带操作 */
        AIC_CMD_CARRY_GET_AI_FUNCTION_STATE = 0x00, // 获取AI功能状态
        AIC_CMD_CARRY_OFF_AI_FUNCTION = 0x01,       // 关闭AI功能
        AIC_CMD_CARRY_ON_AI_FUNCTION = 0x02,        // 开启AI功能

        /* 空携带 */
        AIC_CMD_CARRY_NULL = 0xff,
    } aic_cmd_carry_t;

    typedef enum
    {
        /* AI识别状态查询指令 摄像头返回状态 */
        AIC_GET_STATE_AI_FUNCTION_NORMAL = 0x00, // AI功能状态正常
        AIC_GET_STATE_AI_FUNCTION_OFF = 0x01,    // AI功能未开启

        /* 打印中(炒面识别指令) 摄像头返回状态 */
        AIC_GET_STATE_NOT_CHOW_MEIN = 0x00,            // 非炒面
        AIC_GET_STATE_CHOW_MEIN = 0x01,                // 炒面
        AIC_GET_STATE_CAMER_ABNORMAL = 0x02,           // 摄像头异常
        AIC_GET_STATE_PRINTING_AI_FUNCTION_OFF = 0x03, // AI功能未开启

        /* 打印前(异物识别指令) 摄像头返回状态 */
        AIC_GET_STATE_NO_FOREIGN_BODY = 0x00,             // 无异物(光板状态)
        AIC_GET_STATE_HAVE_FOREIGN_BODY = 0x01,           // 有异物(非光板)
        AIC_GET_STATE_PRINT_FRONT_CAMER_ABNORMAL = 0x02,  // 摄像头异常
        AIC_GET_STATE_PRINT_FRONT_AI_FUNCTION_OFF = 0x03, // AI功能未开启

        /* 摄像头照明灯控制指令 摄像头返回状态 */
        AIC_GET_STATE_LED_ABNORMAL = 0x00,           // LED灯异常
        AIC_GET_STATE_LED_OFF = 0x01,                // 关闭状态
        AIC_GET_STATE_LED_ON = 0x02,                 // 开启状态
        AIC_GET_STATE_OFF_LED_SECOND_CONFIRM = 0x03, // 关闭二次确认(关灯对AI识别有影响)

        /* AI功能(开启/关闭)控制指令 摄像头返回状态 */
        AIC_GET_STATE_AI_FUNCTION_ABNORMAL = 0x00,  // AI功能异常
        AIC_GET_STATE_AI_FUNCTION_OFF_STATE = 0x01, // AI关闭状态
        AIC_GET_STATE_AI_FUNCTION_ON_STATE = 0x02,  // AI开启状态

        /* 空状态 */
        AIC_GET_STATE_NULL = 0xff,
    } aic_get_state_t;

    /**
     * 命令格式 - CMD 部分
     */
    typedef struct aic_cmd
    {
        ai_camera_cmd_t cmdid;                 // 命令代码&&命令参数
        uint8_t body[AI_CAMERA_S_BUFFER_SIZE]; // 发送数据包
        uint16_t body_size;                    // 发送数据包大小
    } aic_cmd_t;

    /**
     * 命令格式 - ACK 部分
     */
    typedef struct aic_ack
    {
        bool is_timeout;                       // 答复超时标志
        uint16_t cmdid;                        // 命令代码&&命令参数
        uint8_t body[AI_CAMERA_R_BUFFER_SIZE]; // 接收数据包
        uint16_t body_size;                    // 接收数据包大小
    } aic_ack_t;

    /**
     * @brief : AI 摄像头通讯初始化
     * @param : null
     * @return: 0 -> OK
     */
    int ai_camera_init();
    /**
     * @brief : AI 摄像头回复注册
     * @param callback : 需要注册的回调
     * @param user_data : 参数
     */
    void ai_camera_resp_cb_register(hl_callback_function_t callback, void *user_data);

    /**
     * @brief : AI 摄像头回复注销
     * @param callback : 需要注销的回调
     * @param user_data : 参数
     */
    void ai_camrea_resp_cb_unregister(hl_callback_function_t callback, void *user_data);

    /**
     * @brief : 获取 AI 摄像头在线状态
     */
    bool aic_get_online();

    /**
     * @brief : AI摄像头 probe
     */
    int aic_probe(const char *devname);

    /**
     * @brief : AI摄像头 exit
     */
    void aic_exit(void);

    /**
     * @brief : AI 摄像头获取版本号
     */
    char *aic_get_version();

    int aic_capture_on(void);
    void aic_capture_off(void);

    void aic_register(hl_callback_function_t func, void *user_data);
    void aic_unregister(hl_callback_function_t func, void *user_data);

    /**
     * @brief : AI摄像头升级文件判断
     */
    bool aic_upgrade_file_identify(char *path);

    /**
     * @brief 向AI摄像头发送指令
     *
     * @param cmd 命令码
     * @param state 命令携带内容
     */
    int ai_camera_send_cmd_handler(uint8_t cmd, uint8_t state);
    void aic_cmd_enforce_request(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_SUPPORT_AIC */
#endif /* _AI_CAMERA_H_ */