#ifndef ALG_H
#define ALG_H

#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <mutex>
#include "msgproto.h"
//#include "dspMemOps.h"

//extern "C"
//{
//    #include "../chelper/serialqueue.h"
//    #include "../chelper/pyhelper.h"
//}

#define   ALG_SG_DEBUG  1
#define   ALG_SG_SAMPLE_SIZE   64

//struct algConfig{
//    double speed;
//    double position_endstop;
//};


struct strainGaugeResult{
    int     flag;
    double  triggerLevel[8];
    double  position[8];
    int     index[8];
};


class Alg{

public:
   Alg();
    ~Alg();




    //func
    void AlgStart();


    int AlgFindStrainGageTrigger(void);
    int AlgCalcMeam(void);
    void setInitFlag(uint flag);
    uint getInitFlag(void);

    void AlgCalcZeroValue(std::vector<std::vector<double>> &sampleData,int size_0, int size_1,std::vector<double> &meanData);
    //double getMedianDate(double window[], int windowSize)

    int medianFilter(std::vector<double> signal, int size, std::vector<double> filteredSignal);

    int jumpSignalFilter(std::vector<double> &signal, int size, std::vector<double> &filteredSignal);

    int meanFilter(std::vector<double> &signal, int size, std::vector<double> &filteredSignal); 

    int AlgStrainGuageFilter(std::vector<std::vector<double>> &src, std::vector<std::vector<double>> &dst, int size,
                        std::vector<double> ch0,std::vector<double> ch1,std::vector<double> ch2, int type, int flag);

    int compositeSignal( std::vector<std::vector<double>> src, std::vector<double> dst, int size , std::vector<double> mean, int flag );                   

    int AlgSgPreCheck(std::vector<std::vector<double>> src, int src_size, int appendSize, std::vector<double> &meanData, int threshValue, int threshCount, int flag);

    int AlgCheckTrigger(std::vector<std::vector<double>> src, int size , std::vector<double> mean, std::vector<std::vector<int>>l_dwKdata, strainGaugeResult &result);   

    //var


    uint initFlag = 0;

    std::vector<double> dateOrgCh0;
    std::vector<double> dateOrgCh1;
    std::vector<double> dateOrgCh2;

    std::vector<double> dateFilterCh0;
    std::vector<double> dateFilterCh1;
    std::vector<double> dateFilterCh2;

    std::vector<double> dateDiffCh0;
    std::vector<double> dateDiffCh1;
    std::vector<double> dateDiffCh2;


    std::vector<double> dateCombine;



    double  zeroValueCh0;
    double  zeroValueCh1;
    double  zeroValueCh2;

    strainGaugeResult sgResult={0};
    


    //define for debug
    //-----------------------------------------------
    std::vector<double> DebugdateOrgCh0_init;
    std::vector<double> DebugdateOrgCh1_init;
    std::vector<double> DebugdateOrgCh2_init;

    std::vector<double>  DebugdateFilterCh0_init;
    std::vector<double>  DebugdateFilterCh1_init;
    std::vector<double>  DebugdateFilterCh2_init;

    std::vector<double> DebugdateOrgCh0;
    std::vector<double> DebugdateOrgCh1;
    std::vector<double> DebugdateOrgCh2;

    std::vector<double>  DebugdateFilterCh0;
    std::vector<double>  DebugdateFilterCh1;
    std::vector<double>  DebugdateFilterCh2;


    std::vector<double>  DebugdateCom;
    std::vector<double>  DebugdateComFilter;

    std::vector<int>     DebugDataDiff;
    std::vector<double>  DebugDataKValue;

  
private:
    
};



#endif