extern "C"
{
#include "adxl345_spi.h"
}
#define LOG_TAG "adx_lis2dw12"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#include "ADXL345Results.h"
#include "adxl345.h"
#include "klippy.h"
#include "Define.h"
#include <pthread.h>
#include "lis2dw12_reg.h"
#include "my_string.h"
#include "hl_common.h"
extern struct spi_data adxl_raw_samples;
extern std::vector<int> adxl_raw_samples_seq;
extern int adxl_last_sequence;

// ADXL345 registers
#define REG_DEVID_ADXL345 0x00
#define REG_BW_RATE_ADXL345 0x2C
#define REG_POWER_CTL_ADXL345 0x2D
#define REG_DATA_FORMAT_ADXL345 0x31
#define REG_FIFO_CTL_ADXL345 0x38
#define REG_MOD_READ_ADXL345 0x80
#define REG_MOD_MULTI_ADXL345 0x40
#define ADXL345_DEV_ID_ADXL345 0xe5
#define ADXL_DEV_CONF_ADXL345 0xB
#define FREEFALL_ACCEL_ADXL345 9.80665 * 1000.
#define SCALE_ADXL345 0.0039 * FREEFALL_ACCEL_ADXL345 // 3.9mg/LSB * Earth gravity in mm/s**2

#define REG_DEVID_LIS2DW12 LIS2DW12_WHO_AM_I
#define REG_BW_RATE_LIS2DW12 LIS2DW12_CTRL6   // 0x25U LIS2DW12_CTRL1 0x2C
#define REG_POWER_CTL_LIS2DW12 LIS2DW12_CTRL1 // 0x20U 0x2D
#define REG_DATA_FORMAT_LIS2DW12 LIS2DW12_CTRL6
#define REG_FIFO_CTL_LIS2DW12 LIS2DW12_FIFO_CTRL // 0x38
#define REG_MOD_READ_LIS2DW12 0x80
#define ADXL_DEV_CONF_LIS2DW12 0x30
#define ADXL345_DEV_ID_LIS2DW12 LIS2DW12_ID
// #define FREEFALL_ACCEL_LIS2DW12 9.80665 * 1000.
// #define SCALE_LIS2DW12 0.001952 * FREEFALL_ACCEL_LIS2DW12 // 1.952mg/LSB * Earth gravity in mm/s**2 历史遗留问题，这个值是错误的。。。。

#define SCALE_LIS2DW12 4.7884033203125 // 16bit/-16g-16g -> 65536/2/16g = 0.48828125mg/LSB * Earth gravity in mm/s**2 -------raw_data * 65536/2/16g/1000(g) * 9.80665(m/s^2) * 1000(转为mm/s^2)

#define ADXL_TYPE_ADXL345 1
#define ADXL_TYPE_LIS2DW12 2

#if ENABLE_MANUTEST
extern double acc_ex_x;
extern double acc_ex_y;
extern double acc_ex_z;
extern int acc_ex_flag;
#endif

AccelQueryHelper::AccelQueryHelper(InternalDumpClient *cconn)
{
    m_cconn = cconn;
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    m_request_start_time = m_request_end_time = print_time;
}

AccelQueryHelper::~AccelQueryHelper()
{
}

void AccelQueryHelper::finish_measurements()
{
    // m_request_end_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    // Printer::GetInstance()->m_tool_head->wait_moves();
    // m_cconn->finalize();
}

std::vector<std::vector<uint8_t>> AccelQueryHelper::_get_raw_samples()
{
}

bool AccelQueryHelper::has_valid_samples()
{
    std::vector<std::vector<uint8_t>> raw_samples = _get_raw_samples();
    for (auto msg : raw_samples)
    {
        // data = msg["params"]["data"];
        // first_sample_time = data[0][0];
        // last_sample_time = data[-1][0];
        // if(frist_sample_time > m_request_end_time || last_sample_time < m_request_start_time)
        //     continue;
        // The time intervals [first_sample_time, last_sample_time]
        // and [request_start_time, request_end_time] have non-zero
        // intersection. It is still theoretically possible that none
        // of the samples from raw_samples fall into the time interval
        // [request_start_time, request_end_time] if it is too narrow
        // or on very heavy data losses. In practice, that interval
        // is at least 1 second, so this possibility is negligible.
        return true;
    }
    return false;
}

std::vector<std::vector<double>> AccelQueryHelper::get_samples()
{
    // raw_samples = _get_raw_samples();
    // if(!raw_samples.size())
    //     return m_samples;
    // int total = 0;
    // for(auto m : raw_samples)
    // {
    //     total += m["params"]["data"];
    // }
    // int count = 0;
}

void AccelQueryHelper::write_to_file(std::string filename)
{
}

AccelCommandHelper::AccelCommandHelper(ADXL345 *chip)
{
}

AccelCommandHelper::~AccelCommandHelper()
{
}

ClockSyncRegression::ClockSyncRegression()
{
}

ClockSyncRegression::~ClockSyncRegression()
{
}

// Accel_Measurement = collections.namedtuple("Accel_Measurement", ("time", "accel_x", "accel_y", "accel_z"))

// Sample results
ADXL345Results::ADXL345Results()
{
    m_drops = 0;
    m_overflows = 0;
    m_time_per_sample = 0.;
    m_start_range = 0.;
    m_end_range = 0.;
}

ADXL345Results::~ADXL345Results()
{
}
void ADXL345Results::setup_data(std::vector<std::vector<double>> axes_map, std::vector<int> raw_samples_seq, struct spi_data *raw_samples,
                                int end_sequence, int overflows, double start1_time, double start2_time, double end1_time, double end2_time)
{ //-SC-ADXL345-G-G-2-4-1-0----
    if (raw_samples->size == 0 || !end_sequence)
        return;

    m_axes_map = axes_map;
    m_raw_samples_seq = raw_samples_seq;
    m_raw_samples = raw_samples;
    m_overflows = overflows;
    m_start2_time = start2_time;
    m_start_range = start2_time - start1_time;
    m_end_range = end2_time - end1_time;
    m_total_count = (end_sequence - 1) * raw_samples->size_per_pack / 6 + raw_samples->size_last_pack / 6; // 根据上传 计算总采样次数
    double total_time = end2_time - start2_time;
    m_time_per_sample = total_time / m_total_count;                                                                 //--ADXL345-G-G-3-----
    m_seq_to_time = m_time_per_sample * raw_samples->size_per_pack / 6.;                                            // 平均传输一次时间
    int actual_count = (raw_samples->size - 1) * raw_samples->size_per_pack / 6. + raw_samples->size_last_pack / 6; // 实际总采样次数
    // for (int i = 0; i < m_raw_samples->size; i++)
    // {
    //     actual_count += raw_samples->size_per_pack / 6;
    // }
    m_drops = m_total_count - actual_count;
    std::cout << "m_time_per_sample=" << m_time_per_sample << std::endl;
    std::cout << "start2_time=" << start2_time << " end2_time=" << end2_time << " end_sequence=" << end_sequence << " samples size=" << raw_samples->size << " samples error=" << raw_samples->error << std::endl;
    std::cout << "sample_hz=" << m_total_count / total_time << " total_time=" << total_time << " m_total_count=" << m_total_count << " actual_count=" << actual_count << std::endl;
}

void ADXL345Results::decode_samples()
{
    if (m_raw_samples->size == 0)
        return;
    double x_pos = m_axes_map[0][0];
    double x_scale = m_axes_map[0][1];
    double y_pos = m_axes_map[1][0];
    double y_scale = m_axes_map[1][1];
    double z_pos = m_axes_map[2][0];
    double z_scale = m_axes_map[2][1];
    m_samples_count = 0;
    std::cout << "x_pos= " << x_pos << " x_scale= " << x_scale << std::endl;
    std::cout << "y_pos= " << y_pos << " y_scale= " << y_scale << std::endl;
    std::cout << "z_pos= " << z_pos << " z_scale= " << z_scale << std::endl;

    system("echo 1 > /proc/sys/vm/compact_memory");
    system("echo 3 > /proc/sys/vm/drop_caches");
    double x_min = 10000000;
    double x_max = -10000000;
    double y_min = 10000000;
    double y_max = -10000000;
    double z_min = 10000000;
    double z_max = -10000000;

    int count_0 = 0;
    int count_i = 0;
    int j_last = 0;
    int i_last = 0;
    uint8_t *d;
    std::vector<double> sdata;
    if (m_raw_samples->size) // 上传次数
    {
        count_0 = m_raw_samples->size_per_pack; // 每次传输数据量
        sdata.resize(count_0 / 2);
        int count_d6 = count_0 / 6;

        m_samples_x.resizeFast(1, count_d6 * m_raw_samples->size); //
        m_samples_y.resizeFast(1, count_d6 * m_raw_samples->size); //
        m_samples_z.resizeFast(1, count_d6 * m_raw_samples->size); //
    }
    std::cout << "size_per_pack= " << m_raw_samples->size_per_pack << " m_raw_samples.size= " << m_raw_samples->size << std::endl;
    std::vector<double> sample = {0, 0, 0, 0};

    for (int i = 0; i < m_raw_samples->size; i++)
    {
        if ((i == m_raw_samples->size - 1) && m_raw_samples->size_last_pack)
        {
            count_i = m_raw_samples->size_last_pack;
        }
        else
        {
            count_i = count_0;
        }
        if (count_0 >= count_i) // 避免 sdata 越界
        {
            d = m_raw_samples->datas + i * m_raw_samples->size_per_pack;
            for (int j = 0; j < count_0 - 1; j += 2) //-32768=>32767
            {
                double value = (double)((d[j] | (d[j + 1] << 8)) - ((d[j + 1] & 0x80) << 9)); // 低位在前，高位带符号
                sdata[j / 2] = (value);
            }
            i_last = i; // double seq_time = m_start2_time +  i * m_seq_to_time;       //这组数据上传时间  m_seq_to_time是每组48字节数据上传平均间隔时间
            for (int j = 0; j < count_i / 6; j++)
            {
                j_last = j; // double samp_time = seq_time + j * m_time_per_sample;   //m_time_per_sample 是每次6字节数据采样平均间隔时间
                double x = sdata[j * 3 + x_pos] * x_scale;
                double y = sdata[j * 3 + y_pos] * y_scale;
                double z = sdata[j * 3 + z_pos] * z_scale; // 0.001952 * 9.80665 * 1000. *32767       SCALE   mm/s`2
                m_samples_x[m_samples_count] = x;
                m_samples_y[m_samples_count] = y;
                m_samples_z[m_samples_count] = z;
                m_samples_count += 1;

                x_min = (x < x_min) ? x : x_min;
                x_max = (x > x_max) ? x : x_max;
                y_min = (y < y_min) ? y : y_min;
                y_max = (y > y_max) ? y : y_max;
                z_min = (z < z_min) ? z : z_min;
                z_max = (z > z_max) ? z : z_max;
            }
        }
    }
    double seq_time = i_last * m_seq_to_time;
    m_samp_time = seq_time + j_last * m_time_per_sample;
    if (m_samples_count != m_samples_x.size())
    {
        m_samples_x.resizeSlow(1, m_samples_count); //
        m_samples_y.resizeSlow(1, m_samples_count); //
        m_samples_z.resizeSlow(1, m_samples_count); //
        std::cout << "m_samples_x.size= " << m_samples_x.size() << " m_samples_count= " << m_samples_count << " i_last= " << i_last << " j_last= " << j_last << std::endl;
    }
    free(m_raw_samples->datas);
    m_raw_samples->datas = NULL;
    system("echo 1 > /proc/sys/vm/compact_memory");
    system("echo 3 > /proc/sys/vm/drop_caches");
    std::cout << "x_min= " << x_min << " y_min= " << y_min << " z_min= " << z_min << std::endl;
    std::cout << "x_max= " << x_max << " y_max= " << y_max << " z_max= " << z_max << std::endl;
    return;
}

void ADXL345Results::write_to_file(std::string filename)
{
    FILE *f = fopen(filename.c_str(), "w");
    if (f == NULL)
    {
        LOG_E("open file %s failed\n", filename.c_str());
        return;
    }
    fprintf(f, "#time,accel_x,accel_y,accel_z\n");
    for (int i = 0; i < m_samples_count; i++)
    {
        fprintf(f, "%d,%.6f,%.6f,%.6f\n", i, m_samples_x[i], m_samples_y[i], m_samples_z[i]);
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
}

// Printer class that controls ADXL345 chip
ADXL345::ADXL345(std::string section_name)
{
    QUERY_RATES[25] = 0x8;   // 3
    QUERY_RATES[50] = 0x9;   // 4
    QUERY_RATES[100] = 0xa;  // 5
    QUERY_RATES[200] = 0xb;  // 6
    QUERY_RATES[400] = 0xc;  // 7
    QUERY_RATES[800] = 0xd;  // 8
    QUERY_RATES[1600] = 0xe; // 9
    QUERY_RATES[3200] = 0xf;
    AccelCommandHelper *ach = new AccelCommandHelper(this); //------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:AccelCommandHelper",0);
    m_query_rate = 0;
    m_last_tx_time = 0.;
    adxl.adxl_type = ADXL_TYPE_ADXL345;
    std::string adxl_type_str = Printer::GetInstance()->m_pconfig->GetString(section_name, "adxl_type", "adxl345");
    if (adxl_type_str == "lis2dw12")
        adxl.adxl_type = ADXL_TYPE_LIS2DW12;
    std::vector<std::vector<double>> am = std::vector<std::vector<double>>();
    if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
    {
        std::vector<std::vector<double>> am_adxl = {{0, SCALE_LIS2DW12}, {1, SCALE_LIS2DW12}, {2, SCALE_LIS2DW12}, {0, -SCALE_LIS2DW12}, {1, -SCALE_LIS2DW12}, {2, -SCALE_LIS2DW12}};
        am = am_adxl;
    }
    else
    {
        std::vector<std::vector<double>> am_adxl = {{0, SCALE_ADXL345}, {1, SCALE_ADXL345}, {2, SCALE_ADXL345}, {0, -SCALE_ADXL345}, {1, -SCALE_ADXL345}, {2, -SCALE_ADXL345}};
        am = am_adxl;
    }
    std::vector<std::string> am_index_name = {"x", "y", "z", "-x", "-y", "-z"};
    std::string axes_map_temp = Printer::GetInstance()->m_pconfig->GetString(section_name, "axes_map", "x,y,z");
    std::vector<std::string> axes_map;
    std::istringstream iss(axes_map_temp); // 输入流
    string token;                          // 接收缓冲区
    while (getline(iss, token, ','))       // 以split为分隔符
    {
        axes_map.push_back(token);
    }
    for (int i = 0; i < axes_map.size(); i++)
    {
        if (axes_map.size() != 3 || std::find(am_index_name.begin(), am_index_name.end(), axes_map[i]) == am_index_name.end())
        {
            std::cout << "Invalid adxl345 axes_map parameter" << std::endl;
        }
    }

    for (int i = 0; i < axes_map.size(); i++)
    {
        am_index_name_it = std::find(am_index_name.begin(), am_index_name.end(), axes_map[i]);
        int am_index = am_index_name_it - am_index_name.begin();
        if (am_index_name_it != am_index_name.end())
        {
            m_axes_map.push_back(am[am_index]);
        }
    }
    m_data_rate = Printer::GetInstance()->m_pconfig->GetInt(section_name, "rate", 3200, 0, 3200);
    if (QUERY_RATES.find(m_data_rate) == QUERY_RATES.end())
    {
        std::cout << "Invalid rate parameter:" << m_data_rate << std::endl;
    }
    adxl.m_data_rate = m_data_rate;

    // Measurement storage (accessed from background thread)

    m_last_sequence = 0;
    m_samples_start1 = 0.;
    m_samples_start2 = 0.;
    m_hal_exception_flag = 0;
    // Setup mcu sensor_adxl345 bulk query code

    std::string cs_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "cs_pin");
    pinParams *cs_pin_params = Printer::GetInstance()->m_ppins->lookup_pin(cs_pin, true, false);
    std::string pin = cs_pin_params->pin;
    m_mcu = (MCU *)cs_pin_params->chip;
    use_mcu_spi = m_mcu->use_mcu_spi;

    if (use_mcu_spi)
    {
        m_spi = MCU_SPI_from_config(section_name, 3, "cs_pin", 5000000);
        m_oid = m_mcu->create_oid();
        m_query_adxl345_cmd = "";
        m_query_adxl345_end_cmd = "";
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            std::stringstream config_lis2dw12;
            config_lis2dw12 << "config_lis2dw oid=" << m_oid << " spi_oid=" << m_spi->get_oid();
            m_mcu->add_config_cmd(config_lis2dw12.str());
            std::stringstream query_lis2dw12;
            query_lis2dw12 << "query_lis2dw oid=" << m_oid << " clock=0 rest_ticks=0";
            m_mcu->add_config_cmd(query_lis2dw12.str(), false, true);
        }
        else
        {
            std::stringstream config_adxl345;
            config_adxl345 << "config_adxl345 oid=" << m_oid << " spi_oid=" << m_spi->get_oid();
            m_mcu->add_config_cmd(config_adxl345.str());
            std::stringstream query_adxl345;
            query_adxl345 << "query_adxl345 oid=" << m_oid << " clock=0 rest_ticks=0";
            m_mcu->add_config_cmd(query_adxl345.str(), false, true);
        }
    }
    else
    {
        std::string bus = Printer::GetInstance()->m_pconfig->GetString(section_name, "spi_bus");
        adxl.speed = Printer::GetInstance()->m_pconfig->GetInt(section_name, "spi_speed", 2000000);
        adxl.spi_device_id = 0;
        if (bus == "spi1")
        {
            adxl.spi_device_id = 1;
        }
        init_spi(&adxl);
    }
    LOG_I("section_name = %s use_mcu_spi = %d adxl_type = %s\n", section_name.c_str(), use_mcu_spi, adxl_type_str.c_str());
    m_mcu->register_config_callback(std::bind(&ADXL345::_build_config, this, std::placeholders::_1));
    if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
    {
        m_mcu->register_response(std::bind(&ADXL345::_handle_adxl345_data, this, std::placeholders::_1), "lis2dw_data", m_oid);
    }
    else
    {
        m_mcu->register_response(std::bind(&ADXL345::_handle_adxl345_start, this, std::placeholders::_1), "adxl345_start", m_oid);
        m_mcu->register_response(std::bind(&ADXL345::_handle_adxl345_data, this, std::placeholders::_1), "adxl345_data", m_oid);
    }
    //  Clock tracking
    m_last_sequence = m_max_query_duration = 0;
    m_last_limit_count = m_last_error_count = 0;
    // m_clock_sync = APIDumpHelper(std::bind(&ADXL345::_api_update, this), std::bind(&ADXL345::_api_startstop, this), 0.100);
    m_name = section_name;
    // wh = self.printer.lookup_object('webhooks')
    // wh.register_mux_endpoint("adxl345/dump_adxl345", "sensor", self.name,
    //                         self._handle_dump_adxl345)

    // Register commands
    m_cmd_ACCELEROMETER_MEASURE_help = "Start/stop accelerometer";
    m_cmd_ACCELEROMETER_QUERY_help = "Query accelerometer for the current values";
    m_cmd_ADXL345_DEBUG_READ_help = "Query accelerometer register (for debugging)";
    m_cmd_ADXL345_DEBUG_WRITE_help = "Set accelerometer register (for debugging)";
    m_name = "default";
    if (section_name != "")
        m_name = section_name;
    string register_cmd_name = "_" + split(section_name, " ").back();
    LOG_I("register_cmd_name = %s\n", register_cmd_name.c_str());
    Printer::GetInstance()->m_gcode->register_mux_command("ACCELEROMETER_MEASURE" + register_cmd_name, "CHIP", m_name, std::bind(&ADXL345::cmd_ACCELEROMETER_MEASURE, this, std::placeholders::_1), m_cmd_ACCELEROMETER_MEASURE_help);
    Printer::GetInstance()->m_gcode->register_mux_command("ACCELEROMETER_QUERY" + register_cmd_name, "CHIP", m_name, std::bind(&ADXL345::cmd_ACCELEROMETER_QUERY, this, std::placeholders::_1), m_cmd_ACCELEROMETER_QUERY_help);
    Printer::GetInstance()->m_gcode->register_mux_command("ADXL345_DEBUG_READ" + register_cmd_name, "CHIP", m_name, std::bind(&ADXL345::cmd_ADXL345_DEBUG_READ, this, std::placeholders::_1), m_cmd_ADXL345_DEBUG_READ_help);
    Printer::GetInstance()->m_gcode->register_mux_command("ADXL345_DEBUG_WRITE" + register_cmd_name, "CHIP", m_name, std::bind(&ADXL345::cmd_ADXL345_DEBUG_WRITE, this, std::placeholders::_1), m_cmd_ADXL345_DEBUG_WRITE_help);
    if (m_name == "default")
    {
        Printer::GetInstance()->m_gcode->register_mux_command("ACCELEROMETER_MEASURE", "CHIP", "", std::bind(&ADXL345::cmd_ACCELEROMETER_MEASURE, this, std::placeholders::_1), m_cmd_ACCELEROMETER_MEASURE_help);
        Printer::GetInstance()->m_gcode->register_mux_command("ACCELEROMETER_QUERY", "CHIP", "", std::bind(&ADXL345::cmd_ACCELEROMETER_QUERY, this, std::placeholders::_1), m_cmd_ACCELEROMETER_QUERY_help);
        Printer::GetInstance()->m_gcode->register_mux_command("ADXL345_DEBUG_READ", "CHIP", "", std::bind(&ADXL345::cmd_ADXL345_DEBUG_READ, this, std::placeholders::_1), m_cmd_ADXL345_DEBUG_READ_help);
        Printer::GetInstance()->m_gcode->register_mux_command("ADXL345_DEBUG_WRITE", "CHIP", "", std::bind(&ADXL345::cmd_ADXL345_DEBUG_WRITE, this, std::placeholders::_1), m_cmd_ADXL345_DEBUG_WRITE_help);
    }
}

ADXL345::~ADXL345()
{
}

void ADXL345::_build_config(int para)
{
    if (use_mcu_spi)
    {
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            command_queue *cmdqueue = m_spi->get_command_queue();
            // m_query_adxl345_cmd = "query_lis2dw oid=%c clock=%u rest_ticks=%u";
            // m_query_adxl345_end_cmd = "query_adxl345 oid=%c clock=%u rest_ticks=%u";
            // m_query_adxl345_end_response_cmd = "adxl345_end oid=%c end1_clock=%u end2_clock=%u limit_count=%hu sequence=%hu";
        }
    }
}

void ADXL345::reg_test()
{
    uint64_t sum = 0;
    uint64_t error = 0;
    int i = 0;
    LOG_D("%d reg test \n", adxl.adxl_type);
    for (;;)
    {
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            set_reg(REG_POWER_CTL_LIS2DW12, 0x00); // 0x20U
            set_reg(REG_POWER_CTL_LIS2DW12, 0x00);
            int dev_id = read_reg(REG_DEVID_LIS2DW12);
            if (dev_id != ADXL345_DEV_ID_LIS2DW12)
            {
                m_hal_exception_flag = 1;
                std::cout << "Invalid lis2dw12 id (got " << dev_id << " vs " << ADXL345_DEV_ID_LIS2DW12 << " )" << std::endl;
            }
            set_reg(REG_DATA_FORMAT_LIS2DW12, 0x30);
        }
        else if (adxl.adxl_type == ADXL_TYPE_ADXL345)
        {
            int dev_id = read_reg(REG_DEVID_ADXL345);
            if (dev_id != ADXL345_DEV_ID_ADXL345)
            {
                m_hal_exception_flag = 1;
                error++;
                std::cout << "Invalid adxl345 id (got " << dev_id << " vs " << ADXL345_DEV_ID_ADXL345 << " )" << std::endl;
            }
            sum++;
            i++;
            if (i == 10000)
            {
                i = 0;
                std::cout << "sum = " << sum << " error = " << error << std::endl;
            }
        }
        usleep(1000 * 1000);
    }
}

uint8_t ADXL345::read_reg(uint8_t reg)
{
    if (use_mcu_spi)
    {
        std::vector<uint8_t> data = {(uint8_t)(reg | REG_MOD_READ_ADXL345), 0x00};
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            data[0] = (uint8_t)(reg | REG_MOD_READ_LIS2DW12);
        }
        ParseResult params = m_spi->spi_transfer(data);
        std::string response = params.PT_string_outs["response"];
        uint8_t *response_data = (uint8_t *)response.c_str();
        uint8_t response_count = response.length();
        LOG_D("read reg %02x\n", reg);
        LOG_D("tdata[0] = %02x data[1] = %02x\n", data[0], data[1]);
        LOG_D("read_reg response_count = %02x\n", response_count);
        for (int i = 0; i < response_count; i++)
        {
            LOG_D("read_reg response_data = %02x\n", response_data[i]);
        }
        return response_data[1];
    }
    else
    {
        uint8_t data[] = {(uint8_t)(reg | REG_MOD_READ_ADXL345), 0x00};
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            data[0] = (uint8_t)(reg | REG_MOD_READ_LIS2DW12);
        }
        LOG_D("read reg %02x\n", reg);
        LOG_D("tdata[0] = %02x data[1] = %02x\n", data[0], data[1]);
        spi_transfer(adxl.spi_fd, adxl.speed, data, data, sizeof(data), adxl.adxl345_name.c_str());
        LOG_D("rdata[0] = %02x rdata[1] = %02x\n", data[0], data[1]);
        return data[1];
    }
}

void ADXL345::set_reg(uint8_t reg, uint8_t val, uint64_t minclock)
{
    if (use_mcu_spi)
    {
        std::vector<uint8_t> data = {reg, (uint8_t)(val & 0xFF)};
        m_spi->spi_send(data, minclock);
    }
    else
    {
        uint8_t data[] = {reg, (uint8_t)(val & 0xFF)};
        // uint8_t rdata[2] = {0, };
        // printf("set_reg tdata[0] = %x data[1] = %x\n", data[0], data[1]);
        spi_transfer(adxl.spi_fd, adxl.speed, data, data, sizeof(data), adxl.adxl345_name.c_str());
        // printf("set_reg rdata[0] = %x rdata[1] = %x\n", data[0], data[1]);
    }
    uint8_t stored_val = read_reg(reg);
    if (stored_val != val)
    {
        // std::cout << "Failed to set ADXL345 register" << reg  << "val:" << val  << "stored_val:" << stored_val << std::endl;
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            LOG_E("Failed to set LIS2DW12 register:%x set_val:%x read_val:%x\n", reg, val, stored_val);
        }
        else
        {
            LOG_E("Failed to set ADXL345 register:%x set_val:%x read_val:%x\n", reg, val, stored_val); // 31 b 0    38 80 0
        }
    }
}

bool ADXL345::is_measuring()
{
    return m_query_rate > 0;
}

void ADXL345::_handle_adxl345_data(ParseResult params)
{
    static int cnt = 0;
    cnt++;
    if(cnt % 1000 == 0)
    {
        std::cout << "cnt : " << cnt << std::endl;
    }
    int last_sequence = m_last_sequence;
    int sequence = (last_sequence & ~0xffff) | params.PT_uint32_outs["sequence"];
    if (sequence < last_sequence)
        sequence += 0x10000;
    m_last_sequence = sequence;
    std::string data = params.PT_string_outs["data"];
    std::vector<uint8_t> datas;

    uint8_t *response_data = (uint8_t *)data.c_str();

    // printf("response_data len %d : ", data.length());
    // for (int i = 0; i < data.length(); i++)
    // {
    //     printf("%02x", response_data[i]);
    // }
    // printf("\n");
    // int rx = (((response_data[1] << 8) | response_data[0])) - ((response_data[1] & 0x80) << 9);
    // int ry = (((response_data[3] << 8) | response_data[2])) - ((response_data[3] & 0x80) << 9);
    // int rz = (((response_data[5] << 8) | response_data[4])) - ((response_data[5] & 0x80) << 9);
    // printf("rx = %d ry = %d rz = %d\n", rx, ry, rz);
    // printf("x = %f y = %f z = %f\n", rx * (32000.0f / 65536.0f), ry * (32000.0f / 65536.0f), rz * (32000.0f / 65536.0f));

    if (adxl_raw_samples.size_per_pack == 0)
        adxl_raw_samples.size_per_pack = data.length();
    else if (adxl_raw_samples.size_per_pack != data.length())
    {
        if (adxl_raw_samples.size_last_pack == 0)
        {
            adxl_raw_samples.size_last_pack = data.length();
        }
        else
        {
            int rx = (((response_data[1] << 8) | response_data[0])) - ((response_data[1] & 0x80) << 9);
            int ry = (((response_data[3] << 8) | response_data[2])) - ((response_data[3] & 0x80) << 9);
            int rz = (((response_data[5] << 8) | response_data[4])) - ((response_data[5] & 0x80) << 9);
            LOG_I("rx = %d ry = %d rz = %d\n", rx, ry, rz);
            LOG_I("x = %f mg/s^2 y = %f mg/s^2 z = %f mg/s^2\n", rx * (32000.0f / 65536.0f), ry * (32000.0f / 65536.0f), rz * (32000.0f / 65536.0f));
            LOG_E("size_per_pack error %d %d\n", adxl_raw_samples.size_per_pack, adxl_raw_samples.size_last_pack);
            adxl_raw_samples.error |= 0x100;
            return;
        }
    }
    if ((adxl_raw_samples.size * adxl_raw_samples.size_per_pack + adxl_raw_samples.size_per_pack) > adxl_raw_samples.max_size)
    {
        // LOG_E("size_per_pack error\n");
        adxl_raw_samples.error |= 0x200;
        return;
    }
    m_raw_samples_seq.push_back(sequence);
    memcpy(adxl_raw_samples.datas + adxl_raw_samples.size * adxl_raw_samples.size_per_pack, response_data, data.length());
    adxl_raw_samples.size++;
    // LOG_I("len = %d adxl_raw_samples.size = %d\n", data.length(), adxl_raw_samples.size);
}

void ADXL345::_extract_samples(std::vector<std::vector<uint8_t>> raw_samples)
{
    // Load variables to optimize inner loop below
    // double x_pos = m_axes_map[0][0];
    // double x_scale = m_axes_map[0][1];
    // double y_pos = m_axes_map[1][0];
    // double y_scale = m_axes_map[1][1];
    // double z_pos = m_axes_map[2][0];
    // double z_scale = m_axes_map[2][1];
    // int last_sequence = m_last_sequence;
}

void ADXL345::_update_clock(uint64_t minclock)
{
}

int ADXL345::start_measurements(int rate, int measure_time) //-SC-ADXL345-G-G-2-2-0----
{

    if (is_measuring())
        return -1;
    if (rate == -1)
    {
        rate = m_data_rate;
    }
    std::cout << "start measurements rate:" << rate << std::endl;
    if (!is_initialized())
    {
        int ret = initialize();
        if (ret)
        {
            return ret;
        }
    }
    // Setup chip in requested query rate
    uint64_t clock = 0;
    if (m_last_tx_time)
        clock = m_mcu->print_time_to_clock(m_last_tx_time);

    if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
    {
        set_reg(LIS2DW12_CTRL1, 0x00, clock);
        set_reg(LIS2DW12_FIFO_CTRL, 0x00); // LIS2DW12_BYPASS_MODE
        set_reg(LIS2DW12_CTRL6, 0x34);     // LIS2DW12_16g
        // set_reg(LIS2DW12_CTRL2, 0x4);
        set_reg(LIS2DW12_FIFO_CTRL, 0xC0); // LIS2DW12_STREAM_MODE

        set_reg(LIS2DW12_CTRL1, 0x94); // 0x94
    }
    else if (adxl.adxl_type == ADXL_TYPE_ADXL345)
    {
        set_reg(REG_POWER_CTL_ADXL345, 0x00, clock);
        set_reg(REG_FIFO_CTL_ADXL345, 0x00);
        set_reg(REG_BW_RATE_ADXL345, QUERY_RATES[rate]); //
        set_reg(REG_FIFO_CTL_ADXL345, 0x80);
    }

    // Setup samples
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    m_last_sequence = 0;
    m_samples_start1 = m_samples_start2 = print_time;
    // Start bulk reading

    m_last_tx_time = print_time;
    m_query_rate = rate;

    {
        adxl.data_count = 0;
        adxl.sequence = 0;
        adxl.limit_count = 0;
        adxl.m_data_rate = rate;
        adxl_last_sequence = 0;
        if (adxl_raw_samples.datas)
            free(adxl_raw_samples.datas);
        if (measure_time > 200)
        {
            std::cout << "start_measurements measure_time errer:" << measure_time << std::endl;
            measure_time = 200;
        }

        if (measure_time < 10)
        {
            //  std::cout << "start_measurements measure_time errer:" << measure_time<< std::endl;
            measure_time = 10;
        }

        adxl_raw_samples.max_size = 6 * adxl.m_data_rate * measure_time; // 250S避免出错
        adxl_raw_samples.datas = (uint8_t *)malloc(adxl_raw_samples.max_size);
        if (adxl_raw_samples.datas == NULL)
        {
            adxl_raw_samples.max_size = 0;
            std::cout << "start_measurements malloc  errer" << std::endl;
            return -1;
        }
        adxl_raw_samples.size = 0;

        adxl_raw_samples.size_last_pack = 0;
        adxl_raw_samples.error = 0;
        m_raw_samples = &adxl_raw_samples;
    }

    if (use_mcu_spi)
    {
        adxl_raw_samples.size_per_pack = 48;
        std::stringstream query_adxl345;
        uint64_t reqclock = m_mcu->print_time_to_clock(print_time);
        uint64_t rest_ticks = m_mcu->seconds_to_clock(4. / rate);
        std::cout << "rate : " << rate << std::endl;
        std::cout << "reset_ticks : " << rest_ticks << std::endl;
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            query_adxl345 << "query_lis2dw oid=" << m_oid << " clock=" << (uint32_t)reqclock << " rest_ticks=" << (uint32_t)rest_ticks;
        }
        else if (adxl.adxl_type == ADXL_TYPE_ADXL345)
        {
            query_adxl345 << "query_adxl345 oid=" << m_oid << " clock=" << (uint32_t)reqclock << " rest_ticks=" << (uint32_t)rest_ticks;
        }

        command_queue *cmd_queue = m_spi->get_command_queue();
        m_mcu->m_serial->send(query_adxl345.str(), 0, reqclock, cmd_queue); //
        adxl.start1_time = get_monotonic();                                 // 理论上这个是spi准备发送开始采样命令的时间
        adxl.start2_time = get_monotonic();                                 // 理论上这个是spi发送开始采样命令后的时间
    }
    else
    {
        adxl_raw_samples.size_per_pack = 48;
        adxl.rest_ticks = 1000 * 100 * 10 / rate; // usleep  至少延时时间能采样一个数据 也不能延时太久 通信速率满 会导致数据丢失
        command_query_adxl345(&adxl);
    }
    return 0;
}

ADXL345Results *ADXL345::finish_measurements() //-SC-ADXL345-G-G-2-4-0----
{
    if (!is_measuring())
    {
        std::cout << "is_measuring:false" << std::endl;
        //----------new---??-----
        // cbd_new_mem("------------------------------------------------new_mem test:ADXL345Results",0);
        return new ADXL345Results();
    }
    // Halt bulk reading
    double end1_time = 0;
    double end2_time = 0;
    int end_sequence = 0;
    int overflows = 0;
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();

    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    if (use_mcu_spi)
    {
        std::stringstream query_adxl345;
        if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
        {
            query_adxl345 << "query_lis2dw oid=" << m_oid << " clock=" << (uint32_t)0 << " rest_ticks=" << (uint32_t)0; // rest_ticks 0结束
            command_queue *cmd_queue = m_spi->get_command_queue();
            ParseResult params = m_mcu->m_serial->send_with_response(query_adxl345.str(), "lis2dw_status", cmd_queue, m_oid); // lis2dw 结束完成后会返回lis2dw_status
            end1_time = _clock_to_print_time(params.PT_uint32_outs["clock"]);                                                 // 只是结束关闭fifo所用的时间？？
            end2_time = _clock_to_print_time(params.PT_uint32_outs["query_ticks"]) + end1_time;                               // 只是结束关闭fifo所用的时间？？
            end_sequence = _convert_sequence(params.PT_uint32_outs["next_sequence"]);
            overflows = params.PT_uint32_outs["limit_count"];
            m_samples_start1 = adxl.start1_time;
            m_samples_start2 = adxl.start2_time;
            end1_time = end2_time = get_monotonic();
        }
        else if (adxl.adxl_type == ADXL_TYPE_ADXL345)
        {
            query_adxl345 << "query_adxl345 oid=" << m_oid << " clock=" << (uint32_t)0 << " rest_ticks=" << (uint32_t)0;
            command_queue *cmd_queue = m_spi->get_command_queue();
            ParseResult params = m_mcu->m_serial->send_with_response(query_adxl345.str(), "adxl345_end", cmd_queue, m_oid);
            end1_time = _clock_to_print_time(params.PT_uint32_outs["end1_clock"]);
            end2_time = _clock_to_print_time(params.PT_uint32_outs["end2_clock"]);
            end_sequence = _convert_sequence(params.PT_uint32_outs["sequence"]);
            overflows = params.PT_uint32_outs["limit_count"];
        }
    }
    else
    {
        adxl.rest_ticks = 0;
        command_query_adxl345(&adxl);
        end1_time = adxl.end1_time;
        end2_time = adxl.end2_time;
        end_sequence = adxl.sequence;
        overflows = adxl.limit_count;
        if (adxl_raw_samples.size != 0)
        {
            // m_raw_samples = &adxl_raw_samples;
            // m_raw_samples_seq = adxl_raw_samples_seq;
            // m_last_sequence = adxl_last_sequence;
            m_samples_start1 = adxl.start1_time;
            m_samples_start2 = adxl.start2_time;
            // adxl_raw_samples_seq = std::vector<int>();
        }
    }
    m_raw_samples = &adxl_raw_samples;
    m_raw_samples_seq = adxl_raw_samples_seq;
    m_last_sequence = adxl_last_sequence;
    adxl_raw_samples_seq = std::vector<int>();

    m_last_tx_time = print_time;
    m_query_rate = 0;

    std::cout << "finish measurements,raw_samples.size:" << m_raw_samples->size << " limit_count:" << overflows << std::endl;
    ADXL345Results *res = new ADXL345Results(); //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:ADXL345Results",0);

    res->setup_data(m_axes_map, m_raw_samples_seq, m_raw_samples, end_sequence, overflows,
                    m_samples_start1, m_samples_start2, end1_time, end2_time);

    m_raw_samples_seq = std::vector<int>();
    return res;
}

void _api_update()
{
    // m_update_clock();
}

void _api_startstop(bool is_start)
{
}

void _handle_dump_adxl345()
{
}

AccelQueryHelper *start_internal_client()
{
}

bool ADXL345::is_initialized()
{
    // int dev_id = read_reg(REG_DEVID_ADXL345);            //自动识别 设备
    // if (dev_id == ADXL345_DEV_ID_ADXL345)
    // {
    //     adxl.adxl_type = ADXL_TYPE_ADXL345;
    // }
    // else
    // {
    //     dev_id = read_reg(REG_DEVID_LIS2DW12);
    //     if (dev_id == ADXL345_DEV_ID_LIS2DW12)
    //     {
    //         adxl.adxl_type = ADXL_TYPE_LIS2DW12;
    //     }
    // }
    if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
    {
        return (read_reg(REG_DEVID_LIS2DW12) == ADXL345_DEV_ID_LIS2DW12 && (read_reg(REG_DATA_FORMAT_LIS2DW12) & ADXL_DEV_CONF_LIS2DW12) != 0);
    }
    else
    {
        return (read_reg(REG_DEVID_ADXL345) == ADXL345_DEV_ID_ADXL345 && (read_reg(REG_DATA_FORMAT_ADXL345) & ADXL_DEV_CONF_ADXL345) != 0);
    }
}


int ADXL345::initialize()
{
    // Setup ADXL345 parameters and verify chip connectivity

    if (adxl.adxl_type == ADXL_TYPE_LIS2DW12)
    {
        set_reg(REG_POWER_CTL_LIS2DW12, 0x00); // 0x20U
        set_reg(REG_POWER_CTL_LIS2DW12, 0x00);
        int dev_id = read_reg(REG_DEVID_LIS2DW12);
        if (dev_id != ADXL345_DEV_ID_LIS2DW12)
        {
            m_hal_exception_flag = 1;
            std::cout << "Invalid lis2dw12 id (got " << dev_id << " vs " << ADXL345_DEV_ID_LIS2DW12 << " )" << std::endl;
#if ENABLE_MANUTEST
            acc_ex_flag = 0;
#endif
            return -1;
        }
        set_reg(REG_DATA_FORMAT_LIS2DW12, 0x30);
    }
    else
    {
        set_reg(REG_POWER_CTL_ADXL345, 0x00); // 0x2D
        set_reg(REG_POWER_CTL_ADXL345, 0x00);
        int dev_id = read_reg(REG_DEVID_ADXL345);
        if (dev_id != ADXL345_DEV_ID_ADXL345)
        {
            m_hal_exception_flag = 1;
            std::cout << "Invalid adxl345 id (got " << dev_id << " vs " << ADXL345_DEV_ID_ADXL345 << " )" << std::endl;
            return -1;
        }
        set_reg(REG_DATA_FORMAT_ADXL345, 0x0B);
    }
    return 0;
}

double ADXL345::_clock_to_print_time(uint32_t clock)
{
    return m_mcu->clock_to_print_time(m_mcu->clock32_to_clock64(clock));
}

void ADXL345::_handle_adxl345_start(ParseResult params)
{
    m_samples_start1 = _clock_to_print_time(params.PT_uint32_outs["start1_clock"]);
    m_samples_start2 = _clock_to_print_time(params.PT_uint32_outs["start2_clock"]);
    std::cout << "start2_clock = " << params.PT_uint32_outs["start2_clock"] << " m_samples_start2 = " << m_samples_start2 << std::endl;
}

int ADXL345::_convert_sequence(int sequence)
{
    sequence = (m_last_sequence & ~0xffff) | sequence;
    if (sequence < m_last_sequence)
        sequence += 0x10000;
    return sequence;
}
void ADXL345::end_query(std::string name, GCodeCommand &gcmd)
{
    ADXL345Results *res = nullptr;
    if (!is_measuring())
    {
        return;
    }
    res = finish_measurements();
    // Write data to file
    std::stringstream filename;
    if (m_name == "default")
        filename << "/tmp/adxl345-" << name << ".csv";
    else
        filename << "/tmp/adxl345-" << m_name << "-" << name << ".csv";
    res->write_to_file(filename.str());
    if (res)
        delete res;
    gcmd.m_respond_info("Writing raw accelerometer data to %s file" + filename.str());  //---??---ADXL345
}

void ADXL345::cmd_ACCELEROMETER_MEASURE(GCodeCommand &gcmd)
{
    if (is_measuring())
    {
        time_t rawtime;
        struct tm *info;
        char buffer[80];
        time(&rawtime);
        info = localtime(&rawtime);
        strftime(buffer, 80, "%Y%m%d_%H:%M:%S", info);
        printf("格式化的日期 & 时间 : |%s|\n", buffer);
        std::stringstream time;
        time << buffer;
        std::string name = gcmd.get_string("NAME", time.str());
        // if not name.replace("-", "").replace("_", "").isalnum():
        //     raise gcmd.error("Invalid adxl345 NAME parameter")  //---??---ADXL345
        end_query(name, gcmd);
        gcmd.m_respond_info("adxl345 measurements stopped"); //---??---ADXL345
    }
    else
    {
        int rate = gcmd.get_int("RATE", m_data_rate);
        if (QUERY_RATES.find(rate) == QUERY_RATES.end())
        {
            std::cout << "Not a valid adxl345 query rate: " << rate << std::endl;
        }
        start_measurements(rate);
        // gcmd.mrespond_info("adxl345 measurements started"); //---??---ADXL345
    }
}

void ADXL345::cmd_ACCELEROMETER_QUERY(GCodeCommand &gcmd) //--ADXL345-G-G-0-----
{

    if (is_measuring())
    {
        std::cout << "adxl345 measurements in progress" << std::endl;
    }
    start_measurements();
    double eventtime = get_monotonic();
    double starttime = eventtime;
    usleep(100000);
    while (m_raw_samples->size == 0)
    {
        eventtime = get_monotonic();
        if (!use_mcu_spi)
        {

            if (adxl_raw_samples.size != 0)
                m_raw_samples = &adxl_raw_samples;
        }
        if (eventtime > starttime + 3.)
        {
            // Try to shutdown the measurements
            // finish_measurements();
            std::cout << "Timeout reading adxl345 data" << std::endl;
        }
        usleep(100000);
    }
    if (!use_mcu_spi)
    {
        if (adxl_raw_samples.size != 0)
            m_raw_samples = &adxl_raw_samples;
    }

    ADXL345Results *result = finish_measurements();
    result->decode_samples();
    int values_count = result->m_samples_x.size();
    double accel_x = 0;
    double accel_y = 0;
    double accel_z = 0;
    if (values_count != 0)
    {
        // std::vector<double> value = values[values_count - 1];
        accel_x = result->m_samples_x[values_count - 1];
        accel_y = result->m_samples_y[values_count - 1];
        accel_z = result->m_samples_z[values_count - 1];
#if ENABLE_MANUTEST
        acc_ex_x = accel_x;
        acc_ex_y = accel_y;
        acc_ex_z = accel_z;
        printf("[%s] update acc value\n", __FUNCTION__);
#endif
    }
    delete result;
    std::stringstream adxl345_values;
    adxl345_values << "adxl345 values (" << accel_x << ", " << accel_y << ", " << accel_z << ")";
    std::cout << "adxl345_values = " << adxl345_values.str() << std::endl;
    gcmd.m_respond_raw(adxl345_values.str());
}

void ADXL345::cmd_ADXL345_DEBUG_READ(GCodeCommand &gcmd)
{
    if (is_measuring())
    {
        std::cout << "adxl345 measurements in progress" << std::endl;
    }
    uint8_t reg = gcmd.get_int("REG", INT32_MIN, 29, 57);
    int val = read_reg(reg);
    gcmd.m_respond_info("ADXL345 REG = 0x%x" + std::to_string(reg) + std::to_string(val)); //---??---ADXL345
}

void ADXL345::cmd_ADXL345_DEBUG_WRITE(GCodeCommand &gcmd)
{
    if (is_measuring())
    {
        std::cout << "adxl345 measurements in progress" << std::endl;
    }
    int reg = gcmd.get_int("REG", INT32_MIN, 29, 57);
    int val = gcmd.get_int("VAL", INT32_MIN, 0, 255);
    set_reg(reg, val);
}
