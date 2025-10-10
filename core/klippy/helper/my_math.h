#pragma once
#include <complex.h>
#include <vector>
#include <iostream>
 
#define      MAX_VECTOR_SIZE          2048                       // 原始信号最大允许长度
#define      PI                                           3.141592653         // 圆周率π的近似值
//  #define      DFT_TYPE         float//double       

 template <typename DFT_TYPE>
class CDft1
{
public:
    CDft1( int n)
    {
        len = n;		// /2 +1
        if(n<=0)
            return ;

        m_dft_vector_size = (len / 2 + 1);
        m_has_dft_vector = true;
        m_dft_vector.resize(m_dft_vector_size);// 

        dft_cos.resize(len);// 
        dft_sin.resize(len);// 

        // 这是最关键的两个嵌套for循环，是离散傅里叶变换公式的代码实现，其中的运算操作和公式定义是完全一致的
        power = 0;
        fixed_factor = (-2 * PI) / n;
        for(int u=0; u<len; u++) 
        {
                dft_cos[u] =cos(power);
                dft_sin[u] =sin(power);
                power += fixed_factor;
        }
    }


    ~CDft1(void)
    {
        if(m_has_dft_vector && m_dft_vector.size() > 0)
        {
            std::vector<DFT_TYPE>().swap(m_dft_vector);
            std::vector<DFT_TYPE>().swap(dft_cos);
            std::vector<DFT_TYPE>().swap(dft_sin);
            
        }
        m_has_dft_vector = false;
    }
 
public:

    bool dft(const std::vector<DFT_TYPE> vec, int n)
    {
        // int len = n;// /2 +1
        // if((len<=0) || (0==vec.size()))
        // 	return false;

        // std::complex<DFT_TYPE> cplTemp(0, 0);			//复数
        //  power = 0;
        //  fixed_factor = (-2 * PI) / n;
        // // 这是最关键的两个嵌套for循环，是离散傅里叶变换公式的代码实现，其中的运算操作和公式定义是完全一致的
        // for(int u=0; u<(len / 2 + 1); u++) {
        // 	for (int x=0; x<len; x++) {
        // 		power =  u * x * fixed_factor;
        // 		cplTemp.real(vec[x] * cos(power));				//xn*cos(2πnm/N)
        // 		cplTemp.imag(vec[x] * sin(power));
        //         m_dft_vector[u] = m_dft_vector[u] + cplTemp;
        // 	}
        // }
        return true;
    }

    bool dft_mod_fast(const std::vector<DFT_TYPE> vec, int n,DFT_TYPE scale,DFT_TYPE fs)
    {
        if( (0==vec.size()) || (n != len) )
            return false;
        DFT_TYPE cplTemp_real ,cplTemp_imag ;			//复数
        DFT_TYPE sumTemp_real ,sumTemp_imag ;			//复数
        int power = 0;

        int len_sub =  len-1;
        // 这是最关键的两个嵌套for循环，是离散傅里叶变换公式的代码实现，其中的运算操作和公式定义是完全一致的

        for(int u=0; u<(len / 2 + 1); u++) 
        {
            sumTemp_real = 0;
            sumTemp_imag = 0;
            power =  0;
            for (int x=0; x<len; x++) 
            {
                cplTemp_real = (vec[x] * dft_cos[power]);				//xn*cos(2πnm/N)
                cplTemp_imag = (vec[x] * dft_sin[power]);
                sumTemp_real = sumTemp_real + cplTemp_real;
                sumTemp_imag = sumTemp_imag + cplTemp_imag;
                power += u;			//u * x * 1;
                // if (power >= len)
                {
                    power &= len_sub;
                }
            }
            
            m_dft_vector[u] = ((sumTemp_real * sumTemp_real + sumTemp_imag * sumTemp_imag  )) * scale / fs;
        }
        return true;
    }


    bool dft_resize( int n)
    {
        clear_dft_vector();
        len = n;
        if(len>0)
        {
            m_dft_vector_size = (len / 2 + 1);
            m_has_dft_vector = true;
            m_dft_vector.resize(m_dft_vector_size);// 
            dft_cos.resize(len);// 
            dft_sin.resize(len);// 
            power = 0;
            fixed_factor = (-2 * PI) / n;
            for(int u=0; u<len; u++) 
            {
                    dft_cos[u] =cos(power);
                    dft_sin[u] =sin(power);
                    power += fixed_factor;
            }

        }
    }

	void clear_dft_vector()                                                       // 清除已有的变换结果
    {
        if(m_has_dft_vector && m_dft_vector.size() > 0)
        {
            std::vector<DFT_TYPE>().swap(m_dft_vector);
            std::vector<DFT_TYPE>().swap(dft_cos);
            std::vector<DFT_TYPE>().swap(dft_sin);
        }
        m_has_dft_vector = false;
    }

public:
    std::vector<DFT_TYPE> m_dft_vector;                                     // 保存变换结果的容器
 
private:
    int len ;		// /2 +1
	bool      m_has_dft_vector;
	int         m_dft_vector_size;                                                 // 变换结果的长度
	DFT_TYPE  power ;
	DFT_TYPE   fixed_factor;
    std::vector<DFT_TYPE> dft_cos;
    std::vector<DFT_TYPE> dft_sin;
    std::vector<double> sin_vec;
	std::vector<double> cos_vec;
};

std::vector<double> rfftfreq(int n, double d);



template <typename T>
std::vector<T> range(T start, T end, T n) {
    size_t N = (int)floor(end - start) + n;
    std::vector<T> vec(N);
    iota(vec.begin(), vec.end(), start);
    return vec;
}

int bit_length(int n);



// extern "C" void interp(int N_x, int N_xp, const double* x, const double* xp, const double* yp,  double* y,double left,double right);
// extern "C" void interp_N(int N_x, int N_xp, int N_data, const double* x, const double* xp, const double* yp, double* y, const double* left, const double* right);

void draw_digital(std::vector<std::string> &script, double startx, double starty, char c);
