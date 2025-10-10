
#pragma once

#include <vector>

#include "tiny_ekf_plus.h"

class StrainGaugeKalmanFilter : public TinyEKF {
 public:
  StrainGaugeKalmanFilter();
  ~StrainGaugeKalmanFilter(){};
  // Copy constructor and copy assignment operator
  StrainGaugeKalmanFilter(const StrainGaugeKalmanFilter &impl) = delete;
  StrainGaugeKalmanFilter &operator=(const StrainGaugeKalmanFilter &impl) =
      delete;

  // Move constructor and move assignment operator
  StrainGaugeKalmanFilter(const StrainGaugeKalmanFilter &&impl) = delete;
  StrainGaugeKalmanFilter &operator=(const StrainGaugeKalmanFilter &&impl) =
      delete;

 protected:
  void model(double fx[Nsta], double F[Nsta][Nsta], double hx[Mobs],
             double H[Mobs][Nsta]) override;

 private:
  int hx711s_count_;
  // 
  std::vector<double> kalma_q_;
  // 
  std::vector<double> kalma_r_;
};
