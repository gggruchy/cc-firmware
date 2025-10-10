#include "filter.h"
#include "klippy.h"

std::vector<double> RCTFilter::ftr_val(std::vector<double> vals)
{
    std::vector<double> out_vals;
    if (vals.size() < 3)
    {
        return vals;
    }
    for (int i = 0; i < vals.size() - 2; i++)
    {
        std::vector<double> tmp = {std::fabs(vals[i]), std::fabs(vals[i + 1]), std::fabs(vals[i + 2])};
        int index = std::distance(tmp.begin(), std::min_element(tmp.begin(), tmp.end()));
        out_vals.push_back(vals[index + i]);
    }
    out_vals.push_back(vals[vals.size() - 2]);
    out_vals.push_back(vals[vals.size() - 1]);
    return out_vals;
}

std::vector<double> RCHFilter::ftr_val(std::vector<double> vals)
{
    std::vector<double> out_vals = {0};
    double rc = 1. / 2. / M_PI / cut_frq_hz;
    double coff = rc / (rc + 1. / acq_frq_hz);
    for (int i = 1; i < vals.size(); i++)
    {
        out_vals.push_back((vals[i] - vals[i - 1] + out_vals[out_vals.size() - 1]) * coff);
    }
    return out_vals;
}

std::vector<double> RCLFilter::ftr_val(std::vector<double> vals)
{
    std::vector<double> out_vals = {vals[0]};
    for (int i = 1; i < vals.size(); i++)
    {
        out_vals.push_back(out_vals[out_vals.size() - 1] * (1 - k1_new) + vals[i] * k1_new);
    }
    return out_vals;
}

Filter::Filter(std::string section_name)
{
    hft_hz = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "hft_hz", 5);
    lft_k1 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "lft_k1", 0.8);
    lft_k1_oft = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "lft_k1_oft", 0.8);
    lft_k1_cal = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "lft_k1_cal", 0.8);
}

RCTFilter *Filter::get_tft()
{
    return new RCTFilter();
}

RCLFilter *Filter::get_lft(double k1)
{
    return new RCLFilter(k1);
}

RCHFilter *Filter::get_hft(double cut_hz, double acq_hz)
{
    return new RCHFilter(cut_hz, acq_hz);
}

filter_result Filter::cal_offset_by_vals(int s_count, std::vector<std::vector<double>> new_valss, double lft_k1, int cut_len)
{
    std::vector<double> out_vals;
    std::vector<std::vector<double>> tmp_vals(s_count, std::vector<double>());
    RCTFilter tft;
    RCLFilter lft(lft_k1);
    for (int i = 0; i < s_count; i++)
    {
        tmp_vals[i] = tft.ftr_val(new_valss[i]);
        tmp_vals[i] = lft.ftr_val(tmp_vals[i]);
    }
    // Implement the remaining logic for cal_offset_by_vals
    return filter_result{out_vals, tmp_vals};
}

filter_result Filter::cal_filter_by_vals(int s_count, std::vector<std::vector<double>> now_valss, double hft_hz, double lft_k1, int cut_len)
{
    std::vector<double> out_vals;
    std::vector<std::vector<double>> tmp_vals(s_count, std::vector<double>());
    RCTFilter tft;
    RCHFilter hft(hft_hz, 80);
    RCLFilter lft(lft_k1);

    for (int i = 0; i < s_count; i++)
    {
        tmp_vals[i] = tft.ftr_val(now_valss[i]);
        tmp_vals[i] = lft.ftr_val(tmp_vals[i]);
        tmp_vals[i] = hft.ftr_val(tmp_vals[i]);
    }

    if (out_vals.size() > cut_len)
    {
        out_vals.erase(out_vals.begin(), out_vals.begin() + (out_vals.size() - cut_len));
    }

    for (int i = 0; i < s_count; i++)
    {
        if (tmp_vals[i].size() > cut_len)
        {
            tmp_vals[i].erase(tmp_vals[i].begin(), tmp_vals[i].begin() + (tmp_vals[i].size() - cut_len));
        }
        for (int j = 0; j < tmp_vals[i].size(); j++)
        {
            tmp_vals[i][j] = std::abs(tmp_vals[i][j]);
        }
    }

    return filter_result{out_vals, tmp_vals};
}

std::vector<double> Filter::cal_filter_by_vals_new(std::vector<double> now_valss, double hft_hz, double lft_k1, int cut_len)
{
    std::vector<double> out_vals;
    std::vector<double> tmp_vals;
    RCTFilter tft;
    RCHFilter hft(hft_hz, 80);
    RCLFilter lft(lft_k1);


    tmp_vals = tft.ftr_val(now_valss);
    tmp_vals = lft.ftr_val(tmp_vals);
    tmp_vals = hft.ftr_val(tmp_vals);


    if (out_vals.size() > cut_len)
    {
        out_vals.erase(out_vals.begin(), out_vals.begin() + (out_vals.size() - cut_len));
    }

    if (tmp_vals.size() > cut_len)
    {
        tmp_vals.erase(tmp_vals.begin(), tmp_vals.begin() + (tmp_vals.size() - cut_len));
    }
    for (int j = 0; j < tmp_vals.size(); j++)
    {
        tmp_vals[j] = std::abs(tmp_vals[j]);
    }
    
    // return filter_result{out_vals, tmp_vals};
    return tmp_vals;
}

std::vector<double> medianFilter(const std::vector<double> &input, int windowSize)
{
    std::vector<double> output;

    int halfWindowSize = windowSize / 2;

    for (int i = 0; i < input.size(); i++)
    {
        std::vector<double> window;

        for (int j = i - halfWindowSize; j <= i + halfWindowSize; j++)
        {
            if (j >= 0 && j < input.size())
            {
                window.push_back(input[j]);
            }
        }

        std::sort(window.begin(), window.end());

        output.push_back(window[windowSize / 2]);
    }

    return output;
}