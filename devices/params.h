#ifndef CORE_PARAM_H
#define CORE_PARAM_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "print_history.h"
#include "utils_params.h"
    typedef struct machine_info_tag
    {
// #define MACHINE_INFO_VERSION 0x000003
        utils_params_header_t header;
        uint32_t version;

        char machine_name[256];
        char machine_marking[256];
        char machine_sn[256];
        char machine_date_production[256];
        char board_name[256];
        char board_marking[256];
        char board_sn[256];
        char screen_marking[256];
        char screen_resolution[256];
        char brand_name[256];     // 品牌名称
        char guarantee_date[256]; // 保修年份
        char saleinfo_web[256];
        char saleinfo_tel[256];
        char saleinfo_mail[256];
        char saleinfo_facebook[256];
        char saleinfo_twitter[256];

        uint32_t print_history_current_index;
        uint32_t print_history_valid_numbers;
        print_history_record_t print_history_record[PRINT_HISTORY_SIZE];

    } machine_info_t;

    extern machine_info_t machine_info;
    int param_init(void);
    int machine_info_save(void);
    
#ifdef __cplusplus
}
#endif

#endif /* CORE_PARAM_H */