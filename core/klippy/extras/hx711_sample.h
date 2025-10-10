
#ifndef _HX711_SAMPLE_H
#define _HX711_SAMPLE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#include "gcode.h"
#include "mcu.h"
#include "msgproto.h"
#include "usb_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

class HX711Sample {
 public:
  HX711Sample(std::string section_name);
  ~HX711Sample();

  void _build_config(int para);
  inline void _handle_calibration_sample(ParseResult &params) {
    std::cout << "HX711Sample calibration ok \r\n " << std::endl;
    m_is_calibration = true;
  }
  inline void _handle_debug_hx711_sample(ParseResult &params) {}
  inline void _handle_mcu_identify() {}
  inline void _handle_shutdown() { m_is_shutdown = true; }
  inline void _handle_disconnect() { m_is_timeout = true; }
  inline std::vector<ParseResult> get_params() {
    m_mutex.lock();
    std::vector<ParseResult> tmps = m_all_params;
    m_mutex.unlock();
    return tmps;
  }
  void delay_s(double delay_s);

  void calibration_start(int times_read);

  inline void cmd_CALIBRATION_HX711_SAMPLE(GCodeCommand &gcmd) {
    int times_read = gcmd.get_int("C", 1, 1, 9999);
    calibration_start(times_read);
  }

  int m_s_count;
  int kalman_q_;
  int kalman_r_;
  MCU *m_mcu;
  command_queue *m_queue;
  std::vector<std::string> m_s_clk_pin;
  std::vector<std::string> m_s_sdo_pin;
  std::vector<ParseResult> m_all_params;
  bool m_is_shutdown;
  bool m_is_timeout;
  bool m_is_calibration;
  int m_oid;
  std::mutex m_mutex;

 private:
};

#ifdef __cplusplus
}
#endif

#endif /*_HX711_SAMPLE_H*/
