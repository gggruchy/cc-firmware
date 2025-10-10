
// #include "ADXL345Results.h"
#ifndef ADXL345RESULTS_H
#define ADXL345RESULTS_H
#include "NumCpp.hpp"
#include "bus.h"
#include <time.h>
#include "gcode.h"
#include "motion_report.h"
#include "adxl345_sensor.h"

// Helper class to obtain measurements

class ADXL345Results{
    private:

    public:
        ADXL345Results();
        ~ADXL345Results();
        nc::NdArray<double> m_samples_x;// std::vector<std::vector<double>> ;
        nc::NdArray<double> m_samples_y;//
        nc::NdArray<double> m_samples_z;//
        std::vector<std::vector<double>> m_axes_map;
        struct spi_data*  m_raw_samples;
        std::vector<int> m_raw_samples_seq;
        int m_samples_count;
        int m_overflows;
        double m_samp_time; 
        double m_start2_time;
        double m_start_range;
        double m_end_range;
        int m_total_count;
        double m_time_per_sample;
        double m_seq_to_time;
        int m_drops;
    public:
        void setup_data(std::vector<std::vector<double>> axes_map, std::vector<int> raw_samples_seq,struct spi_data * raw_samples, 
        int end_sequence, int overflows, double start1_time, double start2_time, double end1_time, double end2_time);
        void decode_samples();
        void write_to_file(std::string filename);

};

 
#endif