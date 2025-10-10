#include "strain_gauge_kalman_filter.h"

#include "klippy.h"
#define LOG_TAG "strain_gauge_kalman_filter"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

StrainGaugeKalmanFilter::StrainGaugeKalmanFilter() {
  hx711s_count_ =
      Printer::GetInstance()->m_pconfig->GetInt("hx711s", "count", 3, 1, 4);
  kalma_q_ =
      Printer::GetInstance()->m_pconfig->GetDoubleVector("filter", "kalma_q_");
  kalma_r_ =
      Printer::GetInstance()->m_pconfig->GetDoubleVector("filter", "kalma_q_");
  if (kalma_q_.size() < hx711s_count_) {
    // TODO: exception process
    LOG_E("[filter] kalma_q_ size error! size(%d) < hx711s_count_(%d)\n",
          kalma_q_.size(), hx711s_count_);
  }
  if (kalma_r_.size() < hx711s_count_) {
    // TODO: exception process
    LOG_E("[filter] kalma_r_ size error! size(%d) < hx711s_count_(%d)\n",
          kalma_r_.size(), hx711s_count_);
  }

  // We approximate the process noise using a small constant
  for (int i = 0; i < kalma_q_.size(); i++) {
    LOG_I("[filter] kalma_q_[%d]:%f \n", i, kalma_q_.at(i));
    this->setQ(i, i, kalma_q_.at(i));
  }

  // Same for measurement noise
  for (int i = 0; i < kalma_r_.size(); i++) {
    LOG_I("[filter] kalma_r_[%d]:%f \n", i, kalma_r_.at(i));
    this->setR(i, i, kalma_r_.at(i));
  }
}

void StrainGaugeKalmanFilter::model(double fx[Nsta], double F[Nsta][Nsta],
                                    double hx[Mobs], double H[Mobs][Nsta]) {
  // Process model is f(x) = x
  for (int i = 0; i < hx711s_count_; i++) {
    LOG_I("Process value:%f \n", this->x[i]);
    fx[i] = this->x[i];
  }

  // So process model Jacobian is identity matrix
  for (int i = 0; i < hx711s_count_; i++) {
    F[i][i] = 1;
  }

  // Measurement function simplifies the relationship between state and sensor
  // readings for convenience. A more realistic measurement function would
  // distinguish between state value and measured value; e.g.:
  //   hx[0] = pow(this->x[0], 1.03);
  //   hx[1] = 1.005 * this->x[1];
  //   hx[2] = .9987 * this->x[1] + .001;
  // Sensor acquisition value from previous state
  for (int i = 0; i < hx711s_count_; i++) {
    LOG_I("Measure value:%f \n", this->x[i]);
    hx[i] = this->x[i];
  }

  // Jacobian of measurement function
  for (int i = 0; i < hx711s_count_; i++) {
    H[i][i] = 1;  // Sensor acquisition value from previous state
  }
}
