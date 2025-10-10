#ifndef FILTER_H
#define FILTER_H

#include <iostream>
#include <vector>
#include <cmath>

class RCTFilter
{
private:
public:
    RCTFilter(){};
    ~RCTFilter(){};
    std::vector<double> ftr_val(std::vector<double> vals);
};

class RCHFilter
{
private:
public:
    RCHFilter(double cut_frq_hz, double acq_frq_hz) : cut_frq_hz(cut_frq_hz), acq_frq_hz(acq_frq_hz) {}
    std::vector<double> ftr_val(std::vector<double> vals);

public:
    double cut_frq_hz;
    double acq_frq_hz;
};

class RCLFilter
{
public:
    RCLFilter(double k1_new) : k1_new(k1_new) {}

    std::vector<double> ftr_val(std::vector<double> vals);

public:
    double k1_new;
};

typedef struct filter_result_t
{
    std::vector<double> out_vals;
    std::vector<std::vector<double>> tmp_vals;
}filter_result;

// typedef struct filter_result_t_new
// {
//     std::vector<double> out_vals;
//     std::vector<double> tmp_vals;
// }filter_result_new;

class Filter
{
public:
    Filter(std::string section_name);

    RCTFilter *get_tft();
    RCLFilter *get_lft(double k1);
    RCHFilter *get_hft(double cut_hz, double acq_hz);

    filter_result cal_offset_by_vals(int s_count, std::vector<std::vector<double>> new_valss, double lft_k1, int cut_len);
    filter_result cal_filter_by_vals(int s_count, std::vector<std::vector<double>> now_valss, double hft_hz, double lft_k1, int cut_len);
    std::vector<double> cal_filter_by_vals_new(std::vector<double> now_valss, double hft_hz, double lft_k1, int cut_len);

public:
    double hft_hz;
    double lft_k1;
    double lft_k1_oft;
    double lft_k1_cal;
};

std::vector<double> medianFilter(const std::vector<double> &input, int windowSize);

#endif