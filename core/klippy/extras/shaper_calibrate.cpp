#include "shaper_calibrate.h"
#include "my_math.h"
#include "adxl345.h"
#include "klippy.h"
#include "Define_config_path.h"

std::vector<std::vector<double>> get_zv_shaper(double shaper_freq, double damping_ratio)
{
    double df = sqrt(1. - pow(damping_ratio, 2));
    double K = exp(-damping_ratio * M_PI / df);
    double t_d = 1. / (shaper_freq * df);

    return std::vector<std::vector<double>>{{1., K}, {0., 0.5 * t_d}};
}

std::vector<std::vector<double>> get_zvd_shaper(double shaper_freq, double damping_ratio)
{
    double df = sqrt(1. - pow(damping_ratio, 2));
    double K = exp(-damping_ratio * M_PI / df);
    double t_d = 1. / (shaper_freq * df);

    return std::vector<std::vector<double>>{{0., 2. * K, pow(K, 2)}, {0., 0.5 * t_d, t_d}};
}

std::vector<std::vector<double>> get_mzv_shaper(double shaper_freq, double damping_ratio)
{
    double df = sqrt(1. - pow(damping_ratio, 2));
    double K = exp(-0.75 * damping_ratio * M_PI / df);
    double t_d = 1. / (shaper_freq * df);

    double a1 = 1. - 1. / sqrt(2.);
    double a2 = (sqrt(2.) - 1.) * K;
    double a3 = a1 * K * K;

    return std::vector<std::vector<double>>{{a1, a2, a3}, {0., 0.375 * t_d, 0.75 * t_d}};
}

std::vector<std::vector<double>> get_ei_shaper(double shaper_freq, double damping_ratio)
{
    double v_tol = 1. / SHAPER_VIBRATION_REDUCTION; // vibration tolerance
    double df = sqrt(1. - pow(damping_ratio, 2));
    double K = exp(-damping_ratio * M_PI / df);
    double t_d = 1. / (shaper_freq * df);

    double a1 = 0.25 * (1. + v_tol);
    double a2 = 0.5 * (1. - v_tol) * K;
    double a3 = a1 * K * K;

    return std::vector<std::vector<double>>{{a1, a2, a3}, {0., 0.5 * t_d, t_d}};
}

std::vector<std::vector<double>> get_2hump_ei_shaper(double shaper_freq, double damping_ratio)
{
    double v_tol = 1. / SHAPER_VIBRATION_REDUCTION; // vibration tolerance
    double df = sqrt(1. - pow(damping_ratio, 2));
    double K = exp(-damping_ratio * M_PI / df);
    double t_d = 1. / (shaper_freq * df);

    double V2 = pow(v_tol, 2);
    double X = pow(V2 * (sqrt(1. - V2) + 1.), 1. / 3.);
    double a1 = (3. * X * X + 2. * X + 3. * V2) / (16. * X);
    double a2 = (0.5 - a1) * K;
    double a3 = a2 * K;
    double a4 = a1 * K * K * K;

    return std::vector<std::vector<double>>{{a1, a2, a3, a4}, {0., 0.5 * t_d, t_d, 1.5 * t_d}};
}

std::vector<std::vector<double>> get_3hump_ei_shaper(double shaper_freq, double damping_ratio)
{
    double v_tol = 1. / SHAPER_VIBRATION_REDUCTION; // vibration tolerance
    double df = sqrt(1. - pow(damping_ratio, 2));
    double K = exp(-damping_ratio * M_PI / df);
    double t_d = 1. / (shaper_freq * df);

    double K2 = K * K;
    double a1 = 0.0625 * (1. + 3. * v_tol + 2. * sqrt(2. * (v_tol + 1.) * v_tol));
    double a2 = 0.25 * (1. - v_tol) * K;
    double a3 = (0.5 * (1. + v_tol) - 2. * a1) * K2;
    double a4 = a2 * K2;
    double a5 = a1 * K2 * K2;

    return std::vector<std::vector<double>>{{a1, a2, a3, a4, a5}, {0., 0.5 * t_d, t_d, 1.5 * t_d, 2. * t_d}};
}

double get_shaper_smoothing(std::vector<std::vector<double>> &shaper, double accel, double scv)
{
    double half_accel = accel * 0.5;

    std::vector<double> A = shaper[0]; // 权重
    std::vector<double> T = shaper[1]; // 时间值
    double inv_D = 1. / std::accumulate(A.begin(), A.end(), 0.0);
    int n = T.size();
    // Calculate input shaper shift
    double ts = 0;
    for (auto i : range(0, n - 1, 1)) // 求加权平均数
    {
        ts += A[i] * T[i];
    }
    ts *= inv_D;
    // Calculate offset for 90 and 180 degrees turn
    double offset_90 = 0., offset_180 = 0.;
    for (auto i : range(0, n - 1, 1))
    {
        if (T[i] >= ts) // 大于加权平均值
        {
            // Calculate offset for one of the axes
            offset_90 += A[i] * (scv + half_accel * (T[i] - ts)) * (T[i] - ts);
        }
        offset_180 += A[i] * half_accel * pow((T[i] - ts), 2);
    }
    offset_90 *= inv_D * sqrt(2);
    offset_180 *= inv_D;

    return std::max(offset_90, offset_180);
}

const double MIN_FREQ = 5.;
const double MAX_FREQ = 200.;
const double WINDOW_T_SEC = 0.5;
const double MAX_SHAPER_FREQ = 150.;
const double SHAPER_DAMPING_RATIO = 0.1;
const std::vector<double> TEST_DAMPING_RATIOS = {0.075, 0.1, 0.15};
const double TARGET_SMOOTHING = 0.12;
const double TARGET_ACCEL = 10000.; // 设置加速度取工作时的加速度

static std::vector<InputShaperCfg> INPUT_SHAPERS =
{
    // InputShaperCfg("zv", std::bind(get_zv_shaper, std::placeholders::_1, std::placeholders::_2), 21),
    InputShaperCfg("mzv", std::bind(get_mzv_shaper, std::placeholders::_1, std::placeholders::_2), 23),
    // InputShaperCfg("zvd", std::bind(get_zvd_shaper, std::placeholders::_1, std::placeholders::_2), 29),
    // InputShaperCfg("ei", std::bind(get_ei_shaper, std::placeholders::_1, std::placeholders::_2), 29),
    // InputShaperCfg("2hump_ei", std::bind(get_2hump_ei_shaper, std::placeholders::_1, std::placeholders::_2), 39),
    // InputShaperCfg("3hump_ei", std::bind(get_3hump_ei_shaper, std::placeholders::_1, std::placeholders::_2), 48),
};

CalibrationData::CalibrationData(nc::NdArray<double> &freq_bins, nc::NdArray<double> &psd_sum, nc::NdArray<double> &psd_x, nc::NdArray<double> &psd_y, nc::NdArray<double> &psd_z)
{                            //---SC-ADXL345-G-G-2-5-2-2-0--
    m_freq_bins = freq_bins; // 1行1025列
    m_psd_sum = psd_sum;
    m_psd_x = psd_x; // 1行1025列
    m_psd_y = psd_y;
    m_psd_z = psd_z;
    m_psd_list = {m_psd_sum, m_psd_x, m_psd_y, m_psd_z};
    m_psd_map["x"] = m_psd_x;
    m_psd_map["y"] = m_psd_y;
    m_psd_map["z"] = m_psd_z;
    m_psd_map["all"] = m_psd_sum;
    m_data_sets = 1;
}

CalibrationData::~CalibrationData()
{
}

void CalibrationData::add_data(CalibrationData *other) // 有多个加速度计采样多份数据  多份数据求算术平均数
{
    int joined_data_sets = m_data_sets + other->m_data_sets;
    for (int i = 0; i < m_psd_list.size(); i++)
    {
        // `other` data may be defined at different frequency bins,
        // interpolating to fix that.
        nc::NdArray<double> other_normalized = nc::interp(m_freq_bins, other->m_freq_bins, other->m_psd_list[i]);
        for (auto x : other_normalized)
            x *= other->m_data_sets;
        for (auto x : m_psd_list[i])
            x *= m_data_sets;
        nc::NdArray<double> temp = m_psd_list[i] + other_normalized;
        for (auto x : temp)
            x *= (1 / joined_data_sets);
    }
    m_data_sets = joined_data_sets;
}

void CalibrationData::normalize_to_frequencies() //-SC-ADXL345-G-G-3-----功率普密度除以频率
{
    for (auto &psd : m_psd_list)
    {
        // std::cout << "psd min=" << psd.min()<< "psd max=" << psd.max()  << std::endl;
        // Avoid division by zero errors
        nc::NdArray<double> temp_bins = m_freq_bins;
        for (auto &freq : temp_bins) // 0->1528.7=3200/2
        {
            freq += 0.1;
        }
        // Remove low-frequency noise
        psd /= temp_bins;
        for (int i = 0; i < m_freq_bins.size(); i++) //
        {
            if (m_freq_bins[i] < MIN_FREQ)
            {
                psd[i] = 0;
            }
        }
        //  std::cout << "psd=" << psd  << std::endl;
        // std::cout << "psd min=" << psd.min()<< "psd max=" << psd.max()  << std::endl;
        // std::cout << "m_freq_bins.size=" << m_freq_bins.size() << " numCols=" << psd.numCols() << " numRows=" <<  psd.numRows() << std::endl;
    }
}

nc::NdArray<double> CalibrationData::get_psd(std::string axis) // 用来MATLAB显示的
{
    return m_psd_map[axis];
}

ShaperCalibrate::ShaperCalibrate()
{
}

ShaperCalibrate::~ShaperCalibrate()
{
}
// d0         d1024    d2048 ............... d216064
// d1         d1025    d2049................ d216065
//...............................................
// d2047  d3071    d4095.................d218111
nc::NdArray<double> as_strided(nc::NdArray<double> &x, int rows, int cols, int strides)
{
    nc::NdArray<double> ret(rows, cols);
    for (int i = 0; i < cols; i++)
    {
        for (int j = 0; j < rows; j++)
        {
            ret[j * cols + i] = x[i * strides + j];
        }
    }
    return ret;
}

nc::NdArray<double> ShaperCalibrate::split_into_windows(nc::NdArray<double> &x, int window_size, int overlap) // 二维数组 2048行 212 列
{                                                                                                             //---SC-ADXL345-G-G-2-5-2-1-2-0-----
    // Memory-efficient algorithm to split an input 'x' into a series
    // of overlapping windows 将输入'x'分割为一系列重叠窗口的内存高效算
    int step_between_window = window_size - overlap;                  // 2048-1024
    int n_windows = (x.shape().cols - overlap) / step_between_window; // 窗口个数
    nc::NdArray<double> ret(window_size, n_windows);                  // 二维数组 2048行 212 列
    ret = as_strided(x, window_size, n_windows, step_between_window);
    return ret;
}

nc::NdArray<double> ShaperCalibrate::psd(nc::NdArray<double> &x, double fs, int nfft) //---SC-ADXL345-G-G-2-5-2-1-0-计算功率普密度
{
    // 用韦尔奇算法计算功率普密度 Calculate power spectral density (PSD) using Welch's algorithm
    nc::NdArray<double> window = nc::kaiser(nfft, 6.); // 产生一个 1行 2048列 锥形数组   0->1->0   0.0148733->0.999997->0.0148733

    // 补偿窗口损失 Compensation for windowing loss
    double sum = 0;
    for (auto w : window)
    {
        sum += pow(w, 2);
    }
    double scale = 2.0 / sum; // pow(window.sum().front(), 2);
    // std::cout << "window_numCols=" << window.numCols() << " window_numRows=" << window.numRows() << " sum="<< sum << std::endl;
    // 分割成大小为 2048 的重叠窗口  Split into overlapping windows of size nfft
    int overlap = nfft / 2;
    x = split_into_windows(x, nfft, overlap); //---SC-ADXL345-G-G-2-5-2-1-2--生成一个转置的二维数组 2048行 n 列
    // First detrend, then apply windowing function

    std::cout << "x_numCols=" << x.numCols() << " x_numRows=" << x.numRows() << " window_numCols=" << window.numCols() << std::endl;
    nc::Slice slice(x.numRows());
    for (int i = 0; i < x.numCols(); i++) // 所有列减去各列的平均值乘以kaiser数组
    {
        nc::NdArray<double> mean = nc::mean(x(slice, i)); // 1行1列 取data的各个列 计算平均值
        // std::cout << "mean.front()=" << mean.front() << " mean_numRows=" << mean.numRows() << " mean_numCols="<<  mean.numCols() << std::endl;
        for (int j = 0; j < x.numRows(); j++) // 每个值减去所在列的平均值 乘以 锥形数组对应的值
        {
            x[j * x.numCols() + i] -= mean.front();
            x[j * x.numCols() + i] *= window[j];
        }
    }

#define DFT_TYPE_F double // float//double
    // Calculate frequency response for each window using FFT
    std::vector<std::vector<DFT_TYPE_F>> result;
    std::cout << "x.numCols() = " << x.numCols() << std::endl;

    CDft1<DFT_TYPE_F> dft(nfft);
    int size = x.numRows(); // 2048行
    std::vector<DFT_TYPE_F> signal(size);

    for (int i = 0; i < x.numCols(); i++) // 对每一列求傅立叶变换
    {
        nc::NdArray<double> points = x(slice, i);
        for (int j = 0; j < size; j++) // 矩阵一列转换为向量
        {
            signal[j] = (DFT_TYPE_F)(points[j]);
        }
        dft.dft_mod_fast(signal, nfft, scale, fs);              // 对列求离散傅立叶变换
        std::vector<DFT_TYPE_F> temp_result = dft.m_dft_vector; // 1025 长
        // For one-sided FFT output the response must be doubled, except
        // the last point for unpaired Nyquist frequency (assuming even nfft)
        // and the 'DC' term (0 Hz)
        // for(int i = 1; i < temp_result.size() - 1; i++)
        if (temp_result.size())
        {
            temp_result[0] /= 2;        // 第一个值除以2
            if (temp_result.size() > 1) // 最后一个值除以2
            {
                temp_result[temp_result.size() - 1] /= 2;
            }
        }
        result.push_back(temp_result); // n个 1025长的向量
    }
    dft.dft_resize(0);
    std::cout << "result.size() = " << result.size() << " result[0].size() = " << result[0].size() << std::endl;
    // Welch's algorithm: average response over windows
    nc::NdArray<double> psd(1, result[0].size());
    // std::cout << "result[0].size() = " << result[0].size() << std::endl;
    for (int i = 0; i < result[0].size(); i++) // 求所有行的平均值
    {
        double sum = 0;
        for (int j = 0; j < result.size(); j++) //
        {
            sum += (double)result[j][i];
        }
        psd[i] = sum / result.size();
    }

    // // Calculate the frequency bins
    // std::vector<double> freqs = rfftfreq(nfft, 1 / fs);         //1025 长的
    // psd_result ret;
    // ret.freqs = nc::NdArray<double>(freqs);
    // ret.psd = psd;
    return psd; // 1025行 1列
}

CalibrationData *ShaperCalibrate::calc_freq_response(ADXL345Results *res) //---SC-ADXL345-G-G-2-5-2-0--
{
#if 1
    res->decode_samples();
    system("echo 1 > /proc/sys/vm/compact_memory");
    system("echo 3 > /proc/sys/vm/drop_caches");
    // usleep(10*1000*1000);
    double N = res->m_samples_x.size(); // 总采样数
    double T = res->m_samp_time;        // 总采样时间
    double SAMPLING_FREQ = N / T;       // 平均采样频率 1600 3200

#else
    std::vector<std::vector<double>> get_accelerometer_data();
    std::vector<std::vector<double>> samples = get_accelerometer_data();
    nc::NdArray<double> data(samples);
    double N = data.shape().rows;        // 总采样数
    double T = data(-1, 0) - data(0, 0); // 总采样时间
    double SAMPLING_FREQ = N / T;        // 平均采样频率 1600 3200
    nc::Slice slice(data.shape().rows);
#endif

    // Round up to the nearest power of 2 for faster FFT
    int M = 1 << bit_length(int(SAMPLING_FREQ * WINDOW_T_SEC - 1)); // 1024 2048
    std::cout << "N=" << N << " T=" << T << " M=" << M << " SAMPLING_FREQ=" << SAMPLING_FREQ << std::endl;
    if (N <= M)
    {

        return nullptr;
    }
    // Calculate PSD (power spectral density) of vibrations per
    // std::cout << "numCols=" << res->m_samples_x.numCols() << " numRows=" << res->m_samples_x.numRows()  << std::endl;
    nc::NdArray<double> psd_x = psd(res->m_samples_x, SAMPLING_FREQ, M); //---SC-ADXL345-G-G-2-5-2-1------3200----2048-------
    res->m_samples_x = nc::NdArray<double>();

    nc::NdArray<double> psd_y = psd(res->m_samples_y, SAMPLING_FREQ, M);
    res->m_samples_y = nc::NdArray<double>();

    nc::NdArray<double> psd_z = psd(res->m_samples_z, SAMPLING_FREQ, M);
    res->m_samples_z = nc::NdArray<double>(); // deleteArray
    res->m_samples_count = 0;

    // std::cout << "psd_x numCols=" << psd_x.numCols()  << " psd_x numRows=" << psd_x.numRows()  << std::endl;

    // Calculate the frequency bins
    std::vector<double> freqsV = rfftfreq(M, 1 / SAMPLING_FREQ); // 1025 长的
    nc::NdArray<double> freqs = nc::NdArray<double>(freqsV);     // 1行1025列

    nc::NdArray<double> psd_sum = psd_x + psd_y + psd_z;                                         // 矩阵加法，矩阵大小不变  1行1025列
    CalibrationData *calibrationdata = new CalibrationData(freqs, psd_sum, psd_x, psd_y, psd_z); //---SC-ADXL345-G-G-2-5-2-2--
    system("echo 1 > /proc/sys/vm/compact_memory");
    system("echo 3 > /proc/sys/vm/drop_caches");
    return calibrationdata;
}

CalibrationData *ShaperCalibrate::process_accelerometer_data(ADXL345Results *data) //-SC-ADXL345-G-G-2-5-0---
{
    CalibrationData *calibration_data = calc_freq_response(data); // ---SC-ADXL345-G-G-2-5-2--
    return calibration_data;
}

nc::NdArray<double> outer(const nc::NdArray<double> &a, const nc::NdArray<double> &b)
{
    nc::NdArray<double> ret(a.size(), b.size());
    for (int i = 0; i < a.size(); i++)
    {
        for (int j = 0; j < b.size(); j++)
        {
            ret[i * b.size() + j] = a[i] * b[j];
        }
    }
    return ret;
}

nc::NdArray<double> ShaperCalibrate::estimate_shaper(std::vector<std::vector<double>> &shaper, double test_damping_ratio, nc::NdArray<double> &test_freqs) // 估计整形器
{

    nc::NdArray<double> A(shaper[0]);
    nc::NdArray<double> T(shaper[1]);
    double inv_D = 1. / A.sum().front();
    nc::NdArray<double> omega(test_freqs.numRows(), test_freqs.numCols());
    for (int i = 0; i < test_freqs.size(); i++)
    {
        omega[i] = 2 * PI * test_freqs[i];
    }
    nc::NdArray<double> damping(omega.numRows(), omega.numCols());
    for (int i = 0; i < omega.size(); i++)
    {
        damping[i] = test_damping_ratio * omega[i];
    }
    nc::NdArray<double> omega_d(omega.numRows(), omega.numCols());
    for (int i = 0; i < omega.size(); i++)
    {
        omega_d[i] = omega[i] * sqrt(1 - pow(test_damping_ratio, 2));
    }
    nc::NdArray<double> T_temp = T;
    // nc::NdArray<double>
    for (int i = 0; i < T_temp.size(); i++)
    {
        T_temp[i] = T[T_temp.size() - i - 1];
    }
    nc::NdArray<double> exp1 = nc::exp(outer(-damping, T_temp));
    // nc::NdArray<double> W = A * nc::exp(outer(-damping, T_temp));
    nc::NdArray<double> W(exp1.numRows(), A.numCols());
    for (int i = 0; i < W.numRows(); i++)
    {
        for (int j = 0; j < W.numCols(); j++)
        {
            W[i * W.numCols() + j] = exp1[i * W.numCols() + j] * A[j];
        }
    }
    nc::NdArray<double> sin1 = nc::sin(outer(omega_d, T));
    // nc::NdArray<double> S = W * nc::exp(outer(omega_d, T));
    nc::NdArray<double> S(sin1.numRows(), W.numCols());
    for (int i = 0; i < S.numRows(); i++)
    {
        for (int j = 0; j < S.numCols(); j++)
        {
            S[i * S.numCols() + j] = W[i * S.numCols() + j] * sin1[i * S.numCols() + j];
        }
    }
    nc::NdArray<double> cos1 = nc::cos(outer(omega_d, T));
    // nc::NdArray<double> C = W * nc::exp(outer(omega_d, T));
    nc::NdArray<double> C(cos1.numRows(), W.numCols());
    for (int i = 0; i < C.numRows(); i++)
    {
        for (int j = 0; j < C.numCols(); j++)
        {
            C[i * C.numCols() + j] = W[i * C.numCols() + j] * cos1[i * S.numCols() + j];
        }
    }
    nc::NdArray<double> S_sum = S.sum(nc::Axis::COL);
    for (auto &sum : S_sum)
    {
        sum = pow(sum, 2);
    }
    nc::NdArray<double> C_sum = C.sum(nc::Axis::COL);
    for (auto &sum : C_sum)
    {
        sum = pow(sum, 2);
    }
    return nc::sqrt(S_sum + C_sum) * inv_D;
}

vir_val ShaperCalibrate::estimate_remaining_vibrations(std::vector<std::vector<double>> &shaper, double test_damping_ratio, nc::NdArray<double> &freq_bins, nc::NdArray<double> &psd)
{
    nc::NdArray<double> vals = estimate_shaper(shaper, test_damping_ratio, freq_bins); // 估计整形器  整形器在各个频率 freq_bins 上的振动幅度降低比例
    // The input shaper can only reduce the amplitude of vibrations by  SHAPER_VIBRATION_REDUCTION times,   输入整形器只能将振动幅度降低SHAPER_VIBRATION_REDUCTION倍
    // so all vibrations below that threshold can be igonred  所以所有低于这个阈值的振动都可以忽略
    //
    double vibrations_threshold = (double)psd.max().front() / (double)SHAPER_VIBRATION_REDUCTION;
    double remaining_vibrations = 0;
    for (int i = 0; i < vals.size(); i++)
    {
        remaining_vibrations += std::max(vals[i] * psd[i] - vibrations_threshold, 0.); // 各个频率点输入整形后幅度衰减不够 累加
    }
    double all_vibrations = 0;
    for (int i = 0; i < vals.size(); i++)
    {
        all_vibrations += std::max(psd[i] - vibrations_threshold, 0.);
    }
    vir_val ret = {remaining_vibrations / all_vibrations, vals};
    return ret;
}

shaper_t ShaperCalibrate::fit_shaper(InputShaperCfg shaper_cfg, CalibrationData *calibration_data, double max_smoothing) //-SC-ADXL345-G-G-4-1-0---
{
    std::vector<double> test_freqs; // = range(shaper_cfg.min_freq, MAX_SHAPER_FREQ, 0.2);
    for (double i = shaper_cfg.min_freq; i < MAX_SHAPER_FREQ; i += 0.2)
    {
        test_freqs.push_back(i);
    }

    std::vector<double> f;
    std::vector<double> p;
    for (int i = 0; i < calibration_data->m_freq_bins.size(); i++)
    {
        if (calibration_data->m_freq_bins[i] <= MAX_FREQ)
        {
            f.push_back(calibration_data->m_freq_bins[i]);
            p.push_back(calibration_data->m_psd_sum[i]);
        }
    }
    nc::NdArray<double> psd(p);
    nc::NdArray<double> freq_bins(f);

    shaper_t best_res = {"", 0, {}, 0, 0, 0, 0};
    std::vector<shaper_t> results;
    for (int i = test_freqs.size() - 1; i >= 0; i--)
    {
        double shaper_vibrations = 0.; // 估算剩余振动 最大值
        double test_freq = test_freqs[i];
        nc::NdArray<double> shaper_vals(1, freq_bins.size()); // 0矩阵        //保存各个频率点上，振动幅度降低比例的最大值
        for (auto &val : shaper_vals)
        {
            val = 0;
        }
        std::vector<std::vector<double>> shaper = shaper_cfg.func(test_freq, SHAPER_DAMPING_RATIO);
        double shaper_smoothing = get_shaper_smoothing(shaper, TARGET_ACCEL);
        if (max_smoothing && shaper_smoothing > max_smoothing && best_res.name != "")
        {
            std::cout << "max_smoothing = " << max_smoothing << std::endl;
            std::cout << "shaper_smoothing = " << shaper_smoothing << std::endl;
            std::cout << "best_res.name = " << best_res.name << std::endl;
            return best_res;
        }
        // Exact damping ratio of the printer is unknown, pessimizing
        // remaining vibrations over possible damping values
        // 打印机的确切阻尼比是未知的，悲观的情况是剩余振动超过可能的阻尼值
        for (auto dr : TEST_DAMPING_RATIOS)
        {
            vir_val virs_vals = estimate_remaining_vibrations(shaper, dr, freq_bins, psd); // 估算剩余振动
            double vibrations = virs_vals.vibrations;
            nc::NdArray<double> vals = virs_vals.vals;
            for (int j = 0; j < shaper_vals.size(); j++) // 各个频率点上，振动幅度降低比例的最大值
            {
                shaper_vals[j] = std::max(shaper_vals[j], vals[j]);
            }
            if (vibrations > shaper_vibrations)
            {
                shaper_vibrations = vibrations;
            }
        }
        double max_accel = find_shaper_max_accel(shaper); // 二分法查找整形器最大加速度
        // The score trying to minimize vibrations, but also accounting
        // the growth of smoothing. The formula itself does not have any
        // special meaning, it simply shows good results on real user data
        // 分数试图最小化振动，但也考虑了平滑的增长。这个公式本身没有任何特殊的意义，它只是在真实的用户数据上显示了良好的结果
        double shaper_score = shaper_smoothing * (pow(shaper_vibrations, 1.5) + shaper_vibrations * 0.2 + 0.01);
        shaper_t result = {shaper_cfg.name, test_freq, shaper_vals, shaper_vibrations, shaper_smoothing, shaper_score, max_accel};
        results.push_back(result);
        if (best_res.name == "" || best_res.vibrs > results.back().vibrs) // 剩余振动能量最小的 整形器 是最好的
        {
            // The current frequency is better for the shaper.
            best_res = results.back();
        }
    }
    // Try to find an 'optimal' shapper configuration: the one that is not
    // much worse than the 'best' one, but gives much less smoothing
    shaper_t selected = best_res;
    // std::cout << "best_res.name = " << best_res.name << " best_res.freq = " << best_res.freq <<std::endl;
    // std::cout << "best_res.score = " << best_res.score << " best_res.max_accel = " << best_res.max_accel << std::endl;
    // std::cout << "best_res.smoothing = " << best_res.smoothing << " best_res.vibrs = " << best_res.vibrs << std::endl;
    for (int i = results.size() - 1; i >= 0; i--)
    {
        if (results[i].vibrs < best_res.vibrs * 1.1 && results[i].score < selected.score)
            selected = results[i];
    }
    return selected;
}

double ShaperCalibrate::bisect(std::function<bool(std::vector<std::vector<double>> &, double)> func, std::vector<std::vector<double>> &shaper) // 二分法查找
{
    // double left = 0.1, right = 0.1;
    double left = 1.0, right = 1.0;
    while (!func(shaper, left))
    {
        right = left;
        left *= 0.5;
    }
    if (right == left) // 默认值太小
    {
        while (func(shaper, right))
        {
            right *= 2;
        }
    }
    while (right - left > 1e-8)
    {
        double middle = (left + right) * 0.5;
        if (func(shaper, middle))
        {
            left = middle;
        }
        else
        {
            right = middle;
        }
    }
    return left;
}

bool bisect_func(std::vector<std::vector<double>> &shaper, double test_accel)
{
    return get_shaper_smoothing(shaper, test_accel) <= TARGET_SMOOTHING;
}

double ShaperCalibrate::find_shaper_max_accel(std::vector<std::vector<double>> &shaper)
{
    // Just some empirically chosen value which produces good projections  只是一些经验值，可以对 max_accel 产生良好的预测
    // for max_accel without much smoothing
    double max_accel = bisect(std::bind(bisect_func, std::placeholders::_1, std::placeholders::_2), shaper);
    return max_accel;
}

std::tuple<shaper_t, std::vector<shaper_t>> ShaperCalibrate::find_best_shaper(CalibrationData *calibration_data, double max_smoothing) //-SC-ADXL345-G-G-4-0----
{
    std::vector<shaper_t> all_shapers;
    shaper_t best_shaper = {"", 0, {}, 0, 0, 0, 0};
    for (auto shaper_cfg : INPUT_SHAPERS) // 对所有整形器类型 查找各自最佳整形器频率
    {
        shaper_t shaper = fit_shaper(shaper_cfg, calibration_data, max_smoothing); //-----SC-ADXL345-G-G-4-1----
        all_shapers.emplace_back(shaper);
        // std::cout << shaper.name << "  " << shaper.freq << std::endl;
        // std::cout << "score = " << shaper.score << std::endl;
        if (best_shaper.name == "" || shaper.score * 1.2 < best_shaper.score || (shaper.score * 1.05 < best_shaper.score && shaper.smoothing * 1.1 < best_shaper.smoothing))
        {
            // Either the shaper significantly improves the score (by 20%),
            // or it improves the score and smoothing (by 5% and 10% resp.)
            best_shaper = shaper;
        }
        // std::cout << shaper.name << "  " << shaper.freq << "  vibrations:" << shaper.vibrs << "  smoothing:" << shaper.smoothing << "  score:" << shaper.score << "  max_accel:" << shaper.max_accel << "  vals numCols:" << shaper.vals.numCols() << "  vals numRows:" << shaper.vals.numRows() << std::endl;
    }
    return std::make_tuple(best_shaper, all_shapers);
}

void ShaperCalibrate::save_params(std::string axis, std::string shaper_name, double shaper_freq)
{
    if (axis == "xy")
    {
        save_params("x", shaper_name, shaper_freq);
        save_params("y", shaper_name, shaper_freq);
    }
    else
    {
        Printer::GetInstance()->m_pconfig->SetValue("input_shaper", "shaper_type_" + axis, shaper_name);
        Printer::GetInstance()->m_pconfig->SetDouble("input_shaper", "shaper_freq_" + axis, shaper_freq);
        Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
        Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "input_shaper", {});
        if (axis == "x")
        {
            Printer::GetInstance()->m_gcode_io->single_command("SET_INPUT_SHAPER SHAPER_TYPE_X=%s SHAPER_FREQ_X=%.2f", shaper_name, shaper_freq);
        }
        else if (axis == "y")
        {
            Printer::GetInstance()->m_gcode_io->single_command("SET_INPUT_SHAPER SHAPER_TYPE_Y=%s SHAPER_FREQ_Y=%.2f", shaper_name, shaper_freq);
        }
        Printer::GetInstance()->m_gcode->respond_info("shaper name = " + shaper_name);
        Printer::GetInstance()->m_gcode->respond_info("shaper freq = " + std::to_string(shaper_freq));
        // std::cout << "shaper name = " << shaper_name << std::endl;
        // std::cout << "shaper freq = " << shaper_freq << std::endl;
    }
}

bool ShaperCalibrate::save_calibration_data(std::string file_name, CalibrationData *calibration_data, std::vector<shaper_t> all_shapers, double max_freq)
{
    if (max_freq <= 0)
        max_freq = MAX_FREQ;

    if (all_shapers.empty())
        return false;

    std::ofstream csvfile(file_name);
    if (!csvfile.is_open())
        return false;

    csvfile << "freq,psd_x,psd_y,psd_z,psd_xyz";

    for (auto &shaper : all_shapers)
        csvfile << "," << shaper.name << "(" << std::fixed << std::setprecision(1) << shaper.freq << ")";
    csvfile << "\n";

    size_t num_freqs = calibration_data->m_freq_bins.size();
    for (size_t i = 0; i < num_freqs; ++i)
    {
        if (calibration_data->m_freq_bins[i] >= max_freq)
            break;

        csvfile << std::fixed << std::setprecision(1) << calibration_data->m_freq_bins[i] << ",";
        csvfile << std::fixed << std::setprecision(3) << calibration_data->m_psd_x[i] << ",";
        csvfile << std::fixed << std::setprecision(3) << calibration_data->m_psd_y[i] << ",";
        csvfile << std::fixed << std::setprecision(3) << calibration_data->m_psd_z[i] << ",";
        csvfile << std::fixed << std::setprecision(3) << calibration_data->m_psd_sum[i];

        for (auto &shaper : all_shapers)
            csvfile << "," << std::fixed << std::setprecision(3) << shaper.vals[i];
        csvfile << "\n";
    }

    csvfile.close();
    return true;
}

bool ShaperCalibrate::save_calibration_data(std::string axis, CalibrationData *calibration_data, double max_freq)
{
    if (max_freq <= 0)
        max_freq = MAX_FREQ;

    std::string file = "/shaper_calibration_" + axis + ".csv";
    std::ofstream csvfile(file);
    if (!csvfile.is_open())
        return false;

    csvfile << "freq,psd\n";

    std::cout << "save_calibration_data" << std::endl;
    nc::NdArray<double> *ptr;
    if (axis == "x")
        ptr = &calibration_data->m_psd_x;
    else
        ptr = &calibration_data->m_psd_y;

    size_t num_freqs = calibration_data->m_freq_bins.size();
    for (size_t i = 0; i < num_freqs; ++i)
    {
        if (calibration_data->m_freq_bins[i] >= max_freq)
            break;

        csvfile << std::fixed << std::setprecision(1) << calibration_data->m_freq_bins[i] << ",";
        csvfile << std::fixed << std::setprecision(3) << (*ptr)[i] << "\n";
    }

    csvfile.close();
    delete ptr;

    return true;
}