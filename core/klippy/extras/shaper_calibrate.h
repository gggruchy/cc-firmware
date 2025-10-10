
// shaper_calibrate.h

#ifndef SHAPER_CALIBRATE_H
#define SHAPER_CALIBRATE_H
#include "ADXL345Results.h"
#include <vector>
#include <map>

#include "shaper_defs.h"
#include "adxl345.h"


#define SHAPER_VIBRATION_REDUCTION   20.
#define DEFAULT_DAMPING_RATIO     0.1

struct InputShaperCfg
{
    std::string name;
    std::function<std::vector<std::vector<double>>(double, double)> func;
    double min_freq;
    InputShaperCfg(std::string _name,
                    std::function<std::vector<std::vector<double>>(double, double)> _func,
                    double _min_freq) : name(_name), func(_func) , min_freq(_min_freq){};
};

std::vector<std::vector<double>> get_zv_shaper(double shaper_freq, double damping_ratio);
std::vector<std::vector<double>> get_zvd_shaper(double shaper_freq, double damping_ratio);
std::vector<std::vector<double>> get_mzv_shaper(double shaper_freq, double damping_ratio);
std::vector<std::vector<double>> get_ei_shaper(double shaper_freq, double damping_ratio);
std::vector<std::vector<double>> get_2hump_ei_shaper(double shaper_freq, double damping_ratio);
std::vector<std::vector<double>> get_3hump_ei_shaper(double shaper_freq, double damping_ratio);
double get_shaper_smoothing(std::vector<std::vector<double>> &shaper, double accel = 5000, double scv = 5);


class CalibrationData
{
private:
    
public:
    nc::NdArray<double> m_freq_bins;
    nc::NdArray<double> m_psd_sum;
    nc::NdArray<double> m_psd_x;
    nc::NdArray<double> m_psd_y;
    nc::NdArray<double> m_psd_z;
    std::vector<nc::NdArray<double>> m_psd_list;
    std::map<std::string, nc::NdArray<double>> m_psd_map;
    int m_data_sets = 1;

    CalibrationData(nc::NdArray<double>& freq_bins, nc::NdArray<double>& psd_sum, nc::NdArray<double>& psd_x, nc::NdArray<double>& psd_y, nc::NdArray<double>& psd_z);
    ~CalibrationData();
    void add_data(CalibrationData* other);
    void normalize_to_frequencies();
    nc::NdArray<double> get_psd(std::string axis = "all");

};

struct psd_result{
    nc::NdArray<double> psd;
    nc::NdArray<double> freqs;
};

typedef struct shaper_tag{
    std::string name;       //名陈
    double freq;                //测试频率
    nc::NdArray<double> vals;        //各个频率点上，振动幅度降低比例的最大值  越小 整形效果越好
    double vibrs;               //剩余振动能量最大值
    double smoothing;       //默认 5000 加速度下的平滑度
    double score;               //得分值
    double max_accel;       //该整形器下的最大加速度
}shaper_t;

struct vir_val{
    double vibrations;
    nc::NdArray<double> vals;
};

class ShaperCalibrate
{
private:
    
public:
    ShaperCalibrate();
    ~ShaperCalibrate();
    nc::NdArray<double> split_into_windows(nc::NdArray<double>&x, int window_size, int overlap);
    nc::NdArray<double> psd(nc::NdArray<double> &x, double fs, int nfft);
    CalibrationData* calc_freq_response(ADXL345Results* data);
    CalibrationData* process_accelerometer_data(ADXL345Results* data);
    nc::NdArray<double> estimate_shaper(std::vector<std::vector<double>>& shaper, double test_damping_ratio, nc::NdArray<double>& test_freqs);
    vir_val estimate_remaining_vibrations(std::vector<std::vector<double>> &shaper, double test_damping_ratio, nc::NdArray<double>& freq_bins, nc::NdArray<double>& psd);
    shaper_t fit_shaper(InputShaperCfg shaper_cfg, CalibrationData* calibration_data, double max_smoothing);
    double bisect(std::function<bool(std::vector<std::vector<double>>&, double)> func, std::vector<std::vector<double>> &shaper);
    double find_shaper_max_accel(std::vector<std::vector<double>> &shaper);
    std::tuple<shaper_t, std::vector<shaper_t>> find_best_shaper(CalibrationData* calibration_data, double max_smoothing);
    void save_params(std::string axis, std::string shaper_name, double shaper_freq);
    bool save_calibration_data(std::string file_name, CalibrationData *calibration_data, std::vector<shaper_t> all_shapers, double max_freq);
    bool save_calibration_data(std::string file_name, CalibrationData *calibration_data, double max_freq);
};



#endif