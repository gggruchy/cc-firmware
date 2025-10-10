#ifndef ADXL345_SENSOR
#define ADXL345_SENSOR
#include <string.h> // memcpy
#include <pthread.h>
#include <string>
#include <atomic>
#include <mutex>
extern "C"{
    #include "sunxi_gpio.h"
}


struct spi_data {
    uint8_t  *datas;
    uint32_t max_size;      //datas 数据最大长度 字节
    // uint32_t pos; 
    uint32_t size;      //上传次数
    uint32_t size_per_pack;     //每次上传字节数 48
    uint32_t size_last_pack;     //每次上传字节数 48
    uint32_t error;             //记录错误位置 
    
};

struct adxl345_sensor {
    double start1_time;
    double start2_time;
    double end1_time;
    double end2_time;
    int rest_ticks;
    std::atomic<long> wakeup;
    int state;
    int spi_fd;
    uint32_t speed;     //spi 速度 1-5M
    int m_data_rate;        //采样速率 3200
    int spi_device_id;
    int adxl_type;
    uint16_t sequence, limit_count;         //数据包编号  出错包数
    std::atomic<long> flags; // volatile uint8_t flags;
    uint8_t data_count;
    uint8_t data[48];
    std::string adxl345_name;
};

enum {
    AX_HAVE_START = 1<<0, AX_RUNNING = 1<<1, AX_PENDING = 1<<2, AX_HAVE_END = 1<<3,
};

void command_query_adxl345(struct adxl345_sensor *ax);
void* adxl345_task(void *arg);
void init_spi(struct adxl345_sensor *adxl);

#endif