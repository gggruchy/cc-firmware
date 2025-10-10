#ifndef _CBD_ERROR_CODE_H
#define _CBD_ERROR_CODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    OPRT_OK = 0,
    /* general */
    OPRT_PARAM_INVALID = -500,              /* 参数非法 */
    OPRT_NOT_FOUND,                         /* 没找到 */
    OPRT_NOT_SUPPORTED,                     /* 不支持 */
    OPRT_NOT_INIT,                          /* 没有初始化 */
    OPRT_EXCEED_UPPER_LIMIT,                /* 超上限 */
    OPRT_COM_ERROR,                         /* 通用错误 */
    OPRT_SOCK_ERR,                          /* 套接字错误 */
    OPRT_RESOURCE_NOT_READY,                /* 资源没准备好 */
    OPRT_NO_SPACE,                          /* 没空间了 */
    OPRT_NOR_NOT_MATCH,                     /* 匹配不上 */
    OPRT_VER_NOT_MATCH,                     /* 版本号不匹配 */
    OPRT_MAG_NOT_MATCH,                     /* magic 匹配补上 */
    OPRT_VERIFY_FAILED,                     /* 校验失败 */
    OPRT_TIMEOUT,                           /* 超时 */
    OPRT_BUSY,                              /* 忙碌 */
    OPRT_ALREADY_EXIST,                     /* 已存在 */
    OPRT_NOT_EXIST,                         /* 不存在 */
    OPRT_SEND_ERROR,                        /* 发送失败 */
    OPRT_RECV_ERROR,                        /* 接收失败 */

    /***************************
     * 
     * Base OS
     *  
    ***************************/
    /* adapt general */
    OPRT_BASE_OS_ADAPTER_REG_NULL_ERROR = -1000,
    OPRT_BASE_UTILITIES_PARTITION_EMPTY,
    OPRT_BASE_UTILITIES_PARTITION_NOT_FOUND,
    OPRT_BASE_UTILITIES_PARTITION_FULL,

    /* filesystem */
    OPRT_OPEN_FILE_FAILED = -1800,
    OPRT_WRITE_FILE_FAILED,
    OPRT_READ_FILE_FAILED,
    OPRT_KVS_RD_FAIL,
    OPRT_OPEN_DIR_FAILED,
    OPRT_SEEK_FILE_FAILED,
    
    /* memory */
    OPRT_MALLOC_FAILED = -2000,

    /* mutex */
    OPRT_BASE_INIT_MUTEX_ATTR_FAILED = -2500,
    OPRT_BASE_SET_MUTEX_ATTR_FAILED,
    OPRT_BASE_INIT_MUTEX_FAILED,
    OPRT_BASE_DESTORY_MUTEX_ATTR_FAILED,
    OPRT_BASE_MUTEX_LOCK_FAILED,
    OPRT_BASE_MUTEX_UNLOCK_FAILED,
    OPRT_BASE_MUTEX_RELEASE_FAILED,

    OPRT_CR_MUTEX_ERR,

    /* pthread */
    OPRT_BASE_OS_ADAPTER_THRD_CR_FAILED,

    /* semaphore */
    OPRT_BASE_SEM_WAIT_SEM_FAILED = -3000,
    OPRT_BASE_SEM_WAIT_TIME_FAILED,
    OPRT_BASE_SEM_INVALID_PARAM,
    OPRT_BASE_SEM_POST_FAILED,
    OPRT_BASE_SEM_RELEASE_FAILED,
    OPRT_BASE_OS_ADAPTER_INIT_SEM_FAILED,

    /* event */
    OPRT_BASE_EVENT_INVALID_EVENT_NAME = -3500,
    OPRT_BASE_EVENT_INVALID_EVENT_DESC,

    /* workq */


    /* JSON */
    OPRT_CJSON_PARSE_ERR = -10000,                  /* JSON解析对象错误 */
    OPRT_CJSON_GET_ERR,                             /* JSON获取对象错误 */

    /***************************
     * 
     * Application
     *  
    ***************************/
    /* devices */
    OPRT_DEV_REGISTER_FAILED = -11000,               /* 注册失败 */
    OPRT_DEV_UNREGISTER_FAILED,                      /* 注销失败 */
    OPRT_DEV_DUPLICATE_REGISTER,                     /* 重复注册 */
    OPRT_DEV_CB_NOT_FOUND,                           /* 回调函数不存在 */

    /* feed */
    OPRT_FEED_INIT_FAILED = -12000,                  /* 自动进出料初始化错误 */
    OPRT_FEED_DEVICE_ERROR,                          /* 设备不存在或故障 */
    OPRT_FEED_BOTTLE_EMPTY,                          /* 料瓶为空 */
    OPRT_FEED_SG_NOT_CAL,                            /* 应变片未校准 */
    OPRT_FEED_IN_TIMEOUT,                            /* 树脂进料超时 */
    OPRT_FEED_ABORT,                                 /* 进出料终止 */

    /* tank */
    OPRT_TANK_DEVICE_ERROR = -12500,                 /* 设备不存在或故障 */
    OPRT_TANK_TEMP_SENSOR_ERROR,                     /* 料槽温度传感器异常 */
    OPRT_TANK_TEMP_ERROR,                            /* 料槽温度异常（过高或不足） */
    OPRT_TANK_PREHEAT_ERROR,                         /* 预热异常 */
    OPRT_TANK_PREHEAT_PAUSE,                         /* 暂停预热 */
    OPRT_TANK_PREHEAT_ABORT,                         /* 终止预热 */

}CBD_ERROR_CODE;

#ifdef __cplusplus
}
#endif

#endif