#ifndef ADXL345_H
#define ADXL345_H
#include "bus.h"
#include <time.h>
#include "gcode.h"
#include "motion_report.h"
#include "adxl345_sensor.h"

// Helper class to obtain measurements
class ADXL345Results;
class AccelQueryHelper
{
private:
    
public:
    AccelQueryHelper(InternalDumpClient* cconn);
    ~AccelQueryHelper();
    void finish_measurements();
    std::vector<std::vector<uint8_t>> _get_raw_samples();
    bool has_valid_samples();
    std::vector<std::vector<double>> get_samples();
    void write_to_file(std::string filename);

public:
    InternalDumpClient* m_cconn;
    double m_request_start_time;
    double m_request_end_time;
    // std::vector<std::vector<double>> m_samples;
    struct spi_data m_raw_samples;
};




class ClockSyncRegression
{
private:
    
public:
    ClockSyncRegression();
    ~ClockSyncRegression();
};


class ADXL345{
    private:

    public:
        
        std::map<int, int> QUERY_RATES;
        struct adxl345_sensor adxl;
        // int adxl_type;
        int m_query_rate;
        double m_last_tx_time;
        std::vector<std::vector<double>> m_axes_map;
        std::vector<std::string>::iterator am_index_name_it;
       
        int m_data_rate;
        // Measurement storage (accessed from background thread)
        int m_last_sequence;
        int m_max_query_duration;
        int m_last_limit_count;
        int m_last_error_count;
         int use_mcu_spi;
        ClockSyncRegression* m_clock_sync;
        APIDumpHelper* m_api_dump;
        double m_samples_start1;
        double m_samples_start2;
        // Setup mcu sensor_adxl345 bulk query code
        MCU_SPI* m_spi;
        MCU* m_mcu;
        int m_oid;
        std::string m_query_adxl345_cmd;
        std::string m_query_adxl345_end_cmd;
        std::string m_query_adxl345_end_response_cmd;

        struct spi_data* m_raw_samples;
        std::vector<int> m_raw_samples_seq;
        // Register commands
        std::string m_cmd_ACCELEROMETER_MEASURE_help;
        std::string m_cmd_ACCELEROMETER_QUERY_help;
        std::string m_cmd_ADXL345_DEBUG_READ_help;
        std::string m_cmd_ADXL345_DEBUG_WRITE_help;
        std::string m_name;
        std::atomic<int> m_hal_exception_flag;
    

    public:
        ADXL345(std::string section_name);
        ~ADXL345();
        void _build_config(int para);    
        void reg_test();
        uint8_t read_reg(uint8_t reg);
        void set_reg(uint8_t reg, uint8_t val, uint64_t minclock=0);
        bool is_measuring();
        void _handle_adxl345_data(ParseResult params);
        void _extract_samples(std::vector<std::vector<uint8_t>> raw_samples);
        void _update_clock(uint64_t minclock = 0);
        int start_measurements(int rate=-1,int measure_time=10);
        ADXL345Results* finish_measurements();
        void _api_update();
        void _api_startstop(bool is_start);
        void _handle_dump_adxl345();
        AccelQueryHelper* start_internal_client();

        bool is_initialized();  
        int initialize();   
        int _convert_sequence(int sequence);
        double _clock_to_print_time(uint32_t clock);
        void _handle_adxl345_start(ParseResult params);
        void end_query(std::string name, GCodeCommand& gcmd);
        void cmd_ACCELEROMETER_MEASURE(GCodeCommand& gcmd);
        void cmd_ACCELEROMETER_QUERY(GCodeCommand& gcmd);
        void cmd_ADXL345_DEBUG_READ(GCodeCommand& gcmd);
        void cmd_ADXL345_DEBUG_WRITE(GCodeCommand& gcmd);
};

class AccelCommandHelper
{
private:
    
public:
    AccelCommandHelper(ADXL345* chip);
    ~AccelCommandHelper();
};

#endif