// 系统 SPI 直接驱动加速度计 ADXL345
#include "adxl345_sensor.h"
#include "klippy.h"
extern "C"
{
#include "adxl345_spi.h"
}
// Chip registers
#define AR_POWER_CTL_ADXL345 0x2D
#define AR_DATAX0_ADXL345 0x32
#define AR_FIFO_STATUS_ADXL345 0x39
#define AM_READ_ADXL345 0x80
#define AM_MULTI_ADXL345 0x40
#define ADXL_START_ADXL345 0x08

#define AR_POWER_CTL_LIS2DW12 0x20
#define AR_DATAX0_LIS2DW12 0x28
#define AR_FIFO_STATUS_LIS2DW12 0x2F
#define AM_READ_LIS2DW12 0x80
#define AM_MULTI_LIS2DW12 0x00
#define ADXL_START_LIS2DW12 0x94

#define ADXL_TYPE_ADXL345 1
#define ADXL_TYPE_LIS2DW12 2

// static
struct adxl345_sensor *gp_adxl345 = NULL;
struct adxl345_sensor *gp_adxl345_back = NULL;
mutex adxl_mutex;

struct spi_data adxl_raw_samples = {0};//全局变量到处飞
std::vector<int> adxl_raw_samples_seq;
int adxl_last_sequence = 0;

void report_adxl345_data(struct adxl345_sensor *adxl)
{
    int last_sequence = adxl_last_sequence;
    int sequence = (last_sequence & ~0xffff) | adxl->sequence;
    if (sequence < last_sequence)
        sequence += 0x10000;
    adxl_last_sequence = sequence;

    if (adxl_raw_samples.size_per_pack != (uint32_t)(adxl->data_count))
    {
        if (adxl_raw_samples.size_last_pack == 0)
        {
            adxl_raw_samples.size_last_pack = (uint32_t)(adxl->data_count);
        }
        else
        {
            adxl_raw_samples.error |= 0x1;
            return;
        }
    }
    if ((adxl_raw_samples.size * adxl_raw_samples.size_per_pack + adxl_raw_samples.size_per_pack) > adxl_raw_samples.max_size)
    {
        adxl_raw_samples.error |= 0x2;
        return;
    }
    // std::vector<uint8_t> datas;
    // for (int i = 0; i < (int)(adxl->data_count); i++)
    // {
    //     datas.push_back(adxl->data[i]);
    // }
    memcpy(adxl_raw_samples.datas + adxl_raw_samples.size * adxl_raw_samples.size_per_pack, adxl->data, (uint32_t)(adxl->data_count));
    adxl_raw_samples.size++;
    adxl_raw_samples_seq.push_back(sequence); // 数据包序列号数组
}
// Report local measurement buffer
static void adxl_report(struct adxl345_sensor *ax)
{
    report_adxl345_data(ax);
    ax->data_count = 0;
    ax->sequence++;
}

static uint_fast8_t adxl_query_fifo_status(struct adxl345_sensor *ax)
{
    uint8_t msg[2] = {AR_FIFO_STATUS_ADXL345 | AM_READ_ADXL345, 0x00};
    if (ax->adxl_type == ADXL_TYPE_LIS2DW12)
    {
        msg[0] = AR_FIFO_STATUS_LIS2DW12 | AM_READ_LIS2DW12;
    }
    msg[1] = 0;
    spi_transfer(ax->spi_fd, ax->speed, msg, msg, sizeof(msg), ax->adxl345_name.c_str());

    {
        return (msg[1] & 0x3f);
    }
}

static void adxl_query_adxl345(struct adxl345_sensor *ax)
{
    static double last_eventtime = 0;
    double eventtime = get_monotonic();

    adxl_mutex.lock();

    uint8_t msg[9] = {AR_DATAX0_ADXL345 | AM_READ_ADXL345 | AM_MULTI_ADXL345, 0, 0, 0, 0, 0, 0, 0, 0};
    int ret = spi_transfer(ax->spi_fd, ax->speed, msg, msg, sizeof(msg), ax->adxl345_name.c_str());
    uint_fast8_t fifo_status = msg[8] & ~0x80; // Ignore trigger bit
    // if(ret >= 0)
    {
        memcpy(&ax->data[ax->data_count], &msg[1], 6);
        ax->data_count += 6;

        if (ax->data_count + 6 > ARRAY_SIZE(ax->data))
            adxl_report(ax);
    }
    // else{
    //     fifo_status = 2;
    // }

    if (fifo_status >= 31 && ax->limit_count != 0xffff)
    {
        ax->limit_count++;
        adxl_raw_samples.error |= 0x4;
        // printf(" limit_count: %x fifo_status %x\n",ax->limit_count,fifo_status);
        // if (eventtime - last_eventtime > 0.008)
        // {
        //     printf(" eventtime: %f %f\n", last_eventtime ,eventtime);
        // }
    }
    if (fifo_status > 1 && fifo_status <= 32)
    { // More data in fifo - wake this task again
    }
    else if (ax->flags & AX_RUNNING)
    {
        if (ax->rest_ticks) // 避免无法停止
        {
            usleep(ax->rest_ticks); // usleep(300 );
        }
    }
    adxl_mutex.unlock();

    last_eventtime = eventtime;
}
static void adxl_query_lis2dw12(struct adxl345_sensor *ax)
{
    adxl_mutex.lock();
    uint8_t msg[7] = {AR_DATAX0_LIS2DW12 | AM_READ_LIS2DW12 | AM_MULTI_LIS2DW12, 0, 0, 0, 0, 0, 0};
    spi_transfer(ax->spi_fd, ax->speed, msg, msg, sizeof(msg), ax->adxl345_name.c_str());
    memcpy(&ax->data[ax->data_count], &msg[1], 6);
    ax->data_count += 6;
    if (ax->data_count + 6 > ARRAY_SIZE(ax->data))
        adxl_report(ax);

    msg[0] = AR_FIFO_STATUS_LIS2DW12 | AM_READ_LIS2DW12;
    msg[1] = 0;
    spi_transfer(ax->spi_fd, ax->speed, msg, msg, 2, ax->adxl345_name.c_str());

    uint_fast8_t fifo_status = msg[1] & 0x3f; // Ignore trigger bit
    if (fifo_status >= 31 && ax->limit_count != 0xffff)
        ax->limit_count++;
    if (fifo_status > 1 && fifo_status <= 32)
    {
        // More data in fifo - wake this task again
    }
    else if (ax->flags & AX_RUNNING)
    {
        // Sleep until next check time
        usleep(ax->rest_ticks);
    }
    adxl_mutex.unlock();
}
// Startup measurements

// End measurements
static void adxl_stop(struct adxl345_sensor *ax)
{
    // Disable measurements
    uint8_t msg[2] = {AR_POWER_CTL_ADXL345, 0x00};
    if (ax->adxl_type == ADXL_TYPE_LIS2DW12)
    {
        msg[0] = AR_POWER_CTL_LIS2DW12;
    }
    double end1_time = get_monotonic();
    spi_transfer(ax->spi_fd, ax->speed, msg, msg, sizeof(msg), ax->adxl345_name.c_str());
    double end2_time = get_monotonic();

    // Drain any measurements still in fifo
    uint_fast8_t i;
    for (i = 0; i < 33; i++)
    {
        msg[0] = AR_FIFO_STATUS_ADXL345 | AM_READ_ADXL345;
        if (ax->adxl_type == ADXL_TYPE_LIS2DW12)
        {
            msg[0] = AR_FIFO_STATUS_LIS2DW12 | AM_READ_LIS2DW12;
        }

        msg[1] = 0;
        spi_transfer(ax->spi_fd, ax->speed, msg, msg, sizeof(msg), ax->adxl345_name.c_str());
        if (!(msg[1] & 0x3f))
            break;
        if (ax->adxl_type == ADXL_TYPE_LIS2DW12)
            adxl_query_lis2dw12(ax);
        else
            adxl_query_adxl345(ax);
    }
    // Report final data
    if (ax->data_count)
        adxl_report(ax);
    ax->end1_time = end1_time;
    ax->end2_time = end2_time;
    ax->flags = 0;
}
static void adxl_start(struct adxl345_sensor *ax)
{
    adxl_raw_samples.size_per_pack = 48;

    ax->flags = AX_RUNNING;
    ax->data_count = 0;
    ax->sequence = ax->limit_count = 0;
    uint8_t msg[2] = {AR_POWER_CTL_ADXL345, ADXL_START_ADXL345};
    if (ax->adxl_type == ADXL_TYPE_LIS2DW12)
    {
        msg[0] = AR_POWER_CTL_LIS2DW12;
        msg[1] = ADXL_START_LIS2DW12;
    }
    double start1_time = get_monotonic();
    spi_transfer(ax->spi_fd, ax->speed, msg, msg, sizeof(msg), ax->adxl345_name.c_str());
    double start2_time = get_monotonic();
    usleep(ax->rest_ticks);
    ax->flags |= AX_PENDING;
    ax->start1_time = start1_time;
    ax->start2_time = start2_time;
}
void command_query_adxl345(struct adxl345_sensor *ax)
{
    if (ax->rest_ticks == 0)
    {
        if (gp_adxl345 != ax)
        {
            std::cout << "finish_measurements errer" << std::endl;
            return;
        }
        // End measurements
        ax->flags = AX_HAVE_END;
        ax->flags |= AX_PENDING;
        ax->wakeup = 1;
        // adxl_stop(ax);
        while (ax->flags) // 等待结束
        {
            usleep(100);
        }
        ax->wakeup = 0;
        gp_adxl345_back = NULL;
        return;
    }
    // Start New measurements query

    ax->flags = AX_HAVE_START;
    ax->flags |= AX_PENDING;
    gp_adxl345_back = ax;
    ax->wakeup = 1;
}

void init_spi(struct adxl345_sensor *adxl)
{
    adxl->spi_fd = spi_init(adxl->spi_device_id, adxl->speed);
#if SPI_GPIO_CS
    spi_cs_init();
#endif
}

void *adxl345_task(void *arg)
{
    while (1)
    {
        if (gp_adxl345 && gp_adxl345->wakeup == 1)
        {
        }
        else
        {
            gp_adxl345 = gp_adxl345_back;
            usleep(1000);
            continue;
        }
        if (!(gp_adxl345->flags & AX_PENDING))
            continue;
        if (gp_adxl345->flags & AX_HAVE_START)
            adxl_start(gp_adxl345);
        else if (gp_adxl345->flags & AX_HAVE_END)
            adxl_stop(gp_adxl345);
        else
        {
            if (gp_adxl345->adxl_type == ADXL_TYPE_LIS2DW12)
                adxl_query_lis2dw12(gp_adxl345);
            else
                adxl_query_adxl345(gp_adxl345);
        }
    }
}
