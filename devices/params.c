#include "params.h"
#include "config.h"
#include "hl_disk.h"
#include "Define_config_path.h"
#include "jenkins.h"
#define LOG_TAG "params"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#if CONFIG_BOARD_E100 == BOARD_E100
   #define DEFAULT_MACHINE_NAME "Centauri Carbon"
   #define DEFAULT_MACHINE_MARKING "Centauri Carbon"
   #define MACHINE_INFO_VERSION 0x000002
#elif CONFIG_BOARD_E100 == BOARD_E100_LITE
   #define DEFAULT_MACHINE_NAME "Centauri"
   #define DEFAULT_MACHINE_MARKING "Centauri"
   #define MACHINE_INFO_VERSION 0x000003
#endif

#define DEFAULT_BOARD_NAME "CHITU Printer"
#define DEFAULT_BOARD_MARKING "REFERENCE"
#define DEFAULT_BRAND_NAME "ELEGOO"

machine_info_t machine_info;//避免多线程访问。
static char machine_info_filepath[1024] = {0};
int machine_info_save(void)
{
    return utils_params_write(&machine_info, sizeof(machine_info), machine_info_filepath);
}
int param_init(void)
{
    char param_directory[1024] = {0};
    // hl_disk_get_mountpoint(HL_DISK_TYPE_EMMC, 0, NULL, param_directory, sizeof(param_directory));
    snprintf(machine_info_filepath, sizeof(machine_info_filepath), HISTORY_PATH1);

    if (utils_params_read(&machine_info, sizeof(machine_info), machine_info_filepath) < 0 || machine_info.version != MACHINE_INFO_VERSION)
    {
        LOG_W("can't find machine info\n");
        memset(&machine_info, 0, sizeof(machine_info));
        strncpy(machine_info.machine_name, DEFAULT_MACHINE_NAME, sizeof(machine_info.machine_name));
        strncpy(machine_info.machine_marking, DEFAULT_MACHINE_MARKING, sizeof(machine_info.machine_marking));
        strncpy(machine_info.board_name, DEFAULT_BOARD_NAME, sizeof(machine_info.board_name));
        strncpy(machine_info.board_marking, DEFAULT_BOARD_MARKING, sizeof(machine_info.board_marking));
        strncpy(machine_info.brand_name, DEFAULT_BRAND_NAME, sizeof(machine_info.brand_name));
        machine_info.version = MACHINE_INFO_VERSION;
        machine_info_save();
    }
    // else
    // {
    //     for (int i = 0; i < machine_info.print_history_valid_numbers; i++) // 避免打印中断电重启后，打印记录状态不正确
    //     {
    //         if (machine_info.print_history_record[i].print_state == PRINT_RECORD_STATE_START)
    //         {
    //             machine_info.print_history_record[i].print_state = PRINT_RECORD_STATE_CANCEL;
    //         }
    //     }
    //     machine_info_save();
    // }
    return 0;
}