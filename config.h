#ifndef CONFIG_H
#define CONFIG_H

// 项目
#define PROJECT_ELEGO_E100 1
// 主板
#define PROJECT_BOARD_E100 0
// UI
#define PROJECT_UI_E100 0

#define CONFIG_PROJECT PROJECT_ELEGO_E100

// clang-format off
#if CONFIG_PROJECT == PROJECT_ELEGO_E100
    #define CONFIG_BOARD PROJECT_BOARD_E100
    #define CONFIG_UI PROJECT_UI_E100
    #define CONFIG_TEST_SCREEN_UPDATE_TIME 0 //测试屏幕刷新率
    #define CONFIG_PRINT_HISTORY 1
    #define CONFIG_SUPPORT_TLP 1            // 延时摄影
    #define CONFIG_SUPPORT_AIC 0            // AI摄像头(AI监测)
    #define MCU_UPDATE 1
    #define STRAIN_GAUGE_MCU_UPDATE_PATH "/lib/firmware/upgrade_sg.bin"
    #define EXTRUDER_MCU_UPDATE_PATH "/lib/firmware/upgrade_extruder.bin"
    #define CONFIG_SUPPORT_OTA 1
    #define CONFIG_SUPPORT_CAMERA 1          // 摄像头功能
    #define CONFIG_SUPPORT_NETWORK 1         // 网络功能
#endif
// clang-format on

#endif