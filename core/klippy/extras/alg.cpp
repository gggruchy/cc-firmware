#include "alg.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "string.h"
#include <atomic>
#include "klippy.h"
#include "Define.h"

//#include "serial.h"
#include "debug.h"




Alg::Alg()
{
    ;
}

Alg::~Alg()
{
    ;
}

void Alg::AlgStart()
{
    printf("... ... ... ALG START 20231220 ... ... ... \n");
}


void Alg::setInitFlag(uint flag)
{
    initFlag = flag;
}

uint Alg::getInitFlag(void)
{
    return initFlag;
}



// calc mean value
void Alg::AlgCalcZeroValue(std::vector<std::vector<double>> &sampleData, int size_0, int size_1, std::vector<double> &meanData)
{
    int sum = 0;
    int i = 0;

    int l_dwSize = size_1;

    for (i=0;i<l_dwSize;i++ ) {
        sum += sampleData[0][i];
    }
    zeroValueCh0 = (sum) / l_dwSize;

    sum = 0;
    for (i=0;i<l_dwSize;i++ ) {
        sum += sampleData[1][i];
    }
    zeroValueCh1 = (sum) / l_dwSize;

    sum = 0;
    for (i=0;i<l_dwSize;i++ ) {
        sum += sampleData[2][i];
    }
    zeroValueCh2 = (sum) / l_dwSize;


    meanData[0] = zeroValueCh0;
    meanData[1] = zeroValueCh1;
    meanData[2] = zeroValueCh2;
}




//double getMedianDate(vector<double> window, int windowSize)
double getMedianDate(double window[], int windowSize)
{   
    double temp;
    std::cout << "A30" << std::endl; 
    for (int i = 0; i < windowSize - 1; i++) 
    {
        for (int j = i+1; j < windowSize; j++)
        {
            if (window[i] > window[j])
            {
                temp = window[i];
                window[i] = window[j]; 
                window[j] = temp;
            }
        }    
    }
    std::cout << "A31" << std::endl; 
    return window[windowSize / 2];
}   

 int Alg::medianFilter(std::vector<double> signal, int size, std::vector<double> filteredSignal)
 {
    int windowSize =3; // 窗口大小
    int l_dwSize =  size;
    std::cout << "A20" << std::endl;  
    for (int i = 0; i< l_dwSize; i++)
    {
        double window[windowSize];
        for (int j=0; j< windowSize; j++)
        { 
            int index = i - windowSize /2 + j; 
            if (index < 0)
            {index = 0;} 
            else if (index >= l_dwSize)
            {index = l_dwSize -1 ;}
            window[j] = signal[index];
        }
        filteredSignal[i] = getMedianDate(window, windowSize);
    }
    std::cout << "A21" << std::endl;  
 }


 int Alg::meanFilter(std::vector<double> &signal, int size, std::vector<double> &filteredSignal)
 {
		int m,n,i;
		int sum;
        int l_dwSize = size;
        std::cout << "A40" << std::endl; 
		for (n=0;n<l_dwSize;n++)
		{
			sum = 0;
			for (m=-2;m<=2;m++)
			{
				i=n+m;
				if(i<0)i=0;
				if(i>=l_dwSize)i=l_dwSize-1;
				sum =sum+signal[i];
			}
			filteredSignal[n]=(double)(((double)sum)/5.0+0.5);
		}
        std::cout << "A41" << std::endl; 
		return 0x00;
 }


 int Alg::jumpSignalFilter(std::vector<double> &signal, int size, std::vector<double> &filteredSignal)
 {
    int l_dwRetVal = 0;
    int  i = 0;
    int l_dwSize = size;

    int l_dwNoiseThresh = 20000;

    vector < int > diff_v;
	diff_v.resize(l_dwSize);

    vector < int > l_dwKdata;
	l_dwKdata.resize(l_dwSize);

    std::cout << "A50" << std::endl; 

    for (i = 0; i < l_dwSize - 1; i++)
    {
        if (signal[i+1] > signal[i])
        {
            diff_v[i] = 1;
            l_dwKdata[i] = (int)abs(signal[i+1] -  signal[i]); 
        }
        else if (signal[i+1] < signal[i])
        {
            diff_v[i] = -1;
            l_dwKdata[i] = (int)abs(signal[i] - signal[i+1]);
        }
        else
        {
            diff_v[i] = 0;
            l_dwKdata[i] = 0;
        }
    }

	for (i = l_dwSize - 2; i >= 0; i--)
	{
		if ((diff_v[i] == 0) && (i == (l_dwSize -2)))
		{
			diff_v[i] = 1;
		}
		else if (diff_v[i] == 0)
		{
			if (diff_v[i + 1] >= 0)
				diff_v[i] = 1;
			else
				diff_v[i] = -1; 
		}
	}

    int index   = 0;
    int tmp     = 0;
    int delta   = 5;
    double data = 0;
    for (i = 0; i < l_dwSize - 1; i++)
    {
        if(l_dwKdata[i] > l_dwNoiseThresh)//滤除N个连续点的突变噪声  //逻辑代码需简化
        {
            filteredSignal[i+1] =  signal[i]; 
            data = signal[i];
            //check next five
            //-----------------------------------------------------------------------------
            tmp = (i+1+delta) >= l_dwSize ? (l_dwSize - (i+1+delta)) : delta;
            while(tmp--)
            {       
                i++;
                if( abs(signal[i+1] - data) >  l_dwNoiseThresh) 
                {
                    filteredSignal[i+1] =  data;
                }    
                else
                {
                    filteredSignal[i+1] =  signal[i+1];
                    break;
                }    
            }
        }
        else
        {
            filteredSignal[i+1] = signal[i+1]; 
        }
    }
    filteredSignal[0] = signal[0];

    std::cout << "A51" << std::endl;

    return l_dwRetVal;
 }




//int medianFilter(int window[], int windowSize)
//{   
//    int temp;
//    for (int i = 0; i < windowSize - 1; i++) 
//    {
//        for (int j = i+ 1; j < windowSize; j++)
//        {
//            if (window[i] > window[j])
//            {
//                temp = window[i];
//                window[i] = window[j]; 
//                window[j] = temp;
//            }
//        }    
//    }
//    return window[windowSize / 2];
//}        



//
int Alg::AlgStrainGuageFilter(std::vector<std::vector<double>> &src, std::vector<std::vector<double>> &dst, int size,
                std::vector<double> ch0, std::vector<double> ch1, std::vector<double> ch2,  int type,  int flag)
{
    int dwRetVal = 0;
    int l_dwSize = size;

    if(0 == flag)
        return 0;
    else if(1 == flag) // filter  3  channal 
    {   
        medianFilter(src[0], l_dwSize, dst[0]);
        medianFilter(src[1], l_dwSize, dst[1]);
        medianFilter(src[2], l_dwSize, dst[2]);

    }
    else if(2 == flag)
    {
        jumpSignalFilter(src[0], l_dwSize, dst[0]);
        jumpSignalFilter(src[1], l_dwSize, dst[1]);
        jumpSignalFilter(src[2], l_dwSize, dst[2]);
    }
    else if(3 == flag)
    {
        meanFilter(src[0], l_dwSize, dst[0]);
        meanFilter(src[1], l_dwSize, dst[1]);
        meanFilter(src[2], l_dwSize, dst[2]);
    }

    return dwRetVal;
}




int Alg::compositeSignal( std::vector<std::vector<double>> src, std::vector<double> dst, int size , std::vector<double> mean, int flag )
{
    int l_dwRetVal = 0;
    int i = 0;
    int j = 0;
    int l_sigLength = size;

    std::cout << "A60" << std::endl;
    if(1 == flag)
    {
        for(i=0; i<src.size();i++)
        {
            for(j=0; j<l_sigLength; j++)
            {      
                dst[j] += abs(src[i][j] -mean[i]);
            }     
        }
    }
    else
    {
        ;    
    }
    std::cout << "A61" << std::endl;

    return l_dwRetVal;
}


//
int Alg::AlgSgPreCheck(std::vector<std::vector<double>> src, int src_size, int appendSize, std::vector<double> &meanData,
                                                                                    int threshValue, int threshCount, int flag)
{
    int l_dwRetVal = 1;

    int index = 0;
    int l_dwCheckSize = 0;

    int i, j;
    int cnt = 0;

    l_dwCheckSize = appendSize + 5;
    index = src_size - l_dwCheckSize;
    if(index < 0)
    {
        l_dwRetVal = 1;
        return l_dwRetVal;
    }

    if(1 == flag)
    {
        for(i=0;i<src.size();i++)
        {
            cnt = 0;
            while(index < src_size)
            {
                if( abs(src[i][index] - meanData[i]) >=  threshValue)//2000-8000
                {
                    cnt++;
                    if(cnt >=  threshCount)//5
                    {
                        return 0;//
                    }
                }
                else 
                {
                    cnt = 0;
                }
                index++;
            }
        }
    }

    return l_dwRetVal;
}



int FindKMaxData(vector<int>l_dwKdata, int sz, int level)
{
    int max = 0;
    int index = 0;
    int size = sz < l_dwKdata.size() ?  sz  :  l_dwKdata.size();
    int i = 0;

    if(size < level)// level set 100
        return 0;
    i = size - level;
    index = i;
    max = l_dwKdata[i];
    i++;
    while(i < size)
    {
        if( l_dwKdata[i] >  max   )
        {
            index = i;
            max = l_dwKdata[i];
        }
        i++;
    }
    return index;
}




int Alg::AlgCheckTrigger(std::vector<std::vector<double>> src, int size, std::vector<double> mean,  std::vector<std::vector<int>>l_dwKdata, strainGaugeResult &result)
{
    int l_dwRetValue = 0;
    std::vector<double> signal0;
    std::vector<double> signal;
    std::vector<double> buf;

    int  i = 0;
    int  j = 0;
    int l_dwSize = size;

    //vector < int > diff_v;
	//diff_v.resize(l_dwSize);

    std::vector<std::vector<int>>  diff_v(l_dwSize);
    diff_v[0].resize(2048,0);
    diff_v[1].resize(2048,0);
    diff_v[2].resize(2048,0);   

    //vector < int > l_dwKdata;
	//l_dwKdata.resize(l_dwSize);

    int winSize = 5;

    std::cout << "A70" << std::endl;

 //   Alg::compositeSignal( src, signal0, size , mean, 1 );

 //   meanFilter(signal0, size, buf);
 //   meanFilter(buf, size, signal);



    for(j=0; j< src.size(); j++)
    {
        for (i = 0; i < l_dwSize - 1; i++)
        {
            if (src[j][i+1] > src[j][i])
            {
                diff_v[j][i] = 1;
                l_dwKdata[j][i] = (int)abs(src[j][i+1] -  src[j][i]); 
            }
            else if (src[j][i+1] < src[j][i])
            {
                diff_v[j][i] = -1;
                l_dwKdata[j][i] = (int)abs(src[j][i] - src[j][i+1]);
            }
            else
            {
                diff_v[j][i] = 0;
                l_dwKdata[j][i] = 0;
            }
        }

        for (i = l_dwSize - 2; i >= 0; i--)
        {
            if ((diff_v[j][i] == 0) && (i == (l_dwSize -2)))
            {
                diff_v[j][i] = 1;
            }
            else if (diff_v[j][i] == 0)
            {
                if (diff_v[j][i + 1] >= 0)
                    diff_v[j][i] = 1;
                else
                    diff_v[j][i] = -1; 
            }
        }

    }

    result.index[0] = FindKMaxData(l_dwKdata[0], 512, 100);
    result.index[1] = FindKMaxData(l_dwKdata[1], 512, 100);
    result.index[2] = FindKMaxData(l_dwKdata[2], 512, 100);


    std::cout << "A71  RESULT"  << result.index[0]  << result.index[0]  <<  result.index[0]   << std::endl;

    return l_dwRetValue;
}