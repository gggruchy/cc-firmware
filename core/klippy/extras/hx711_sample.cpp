#include "hx711_sample.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hl_assert.h"
#include "hl_tpool.h"
#include "klippy.h"
#include "usb_uart.h"
#define LOG_TAG "HX711SAMPLE"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

HX711Sample::HX711Sample(std::string section_name)
    : m_is_shutdown(false), m_is_timeout(false), m_is_calibration(false) {
  m_s_count =
      Printer::GetInstance()->m_pconfig->GetInt(section_name, "count", 3, 1, 4);
  kalman_q_ = Printer::GetInstance()->m_pconfig->GetInt(
      section_name, "kalman_q", 1, 0, 10000);
  kalman_r_ = Printer::GetInstance()->m_pconfig->GetInt(
      section_name, "kalman_r", 10, 0, 10000);
  for (int i = 0; i < m_s_count; i++) {
    stringstream clk_pin;
    stringstream sdo_pin;
    clk_pin << "sensor" << i << "_clk_pin";
    sdo_pin << "sensor" << i << "_sdo_pin";
    m_s_clk_pin.push_back(Printer::GetInstance()->m_pconfig->GetString(
        section_name, clk_pin.str()));
    m_s_sdo_pin.push_back(Printer::GetInstance()->m_pconfig->GetString(
        section_name, sdo_pin.str()));
  }
  m_mcu = get_printer_mcu(
      Printer::GetInstance(),
      Printer::GetInstance()->m_pconfig->GetString(section_name, "use_mcu"));
  m_oid = m_mcu->create_oid();

  m_mcu->register_config_callback(
      std::bind(&HX711Sample::_build_config, this, std::placeholders::_1));


  Printer::GetInstance()->register_event_handler(
      "klippy:connect" + section_name,
      std::bind(&HX711Sample::_handle_mcu_identify, this));

  Printer::GetInstance()->register_event_handler(
      "klippy:shutdown" + section_name,
      std::bind(&HX711Sample::_handle_shutdown, this));

  Printer::GetInstance()->register_event_handler(
      "klippy:disconnect" + section_name,
      std::bind(&HX711Sample::_handle_disconnect, this));

  std::string cmd_CALIBRATION_HX711_SAMPLE_help = "Read hx711s vals";
  Printer::GetInstance()->m_gcode->register_command(
      "CALIBRATION_HX711_SAMPLE",
      std::bind(&HX711Sample::cmd_CALIBRATION_HX711_SAMPLE, this,
                std::placeholders::_1),
      false, cmd_CALIBRATION_HX711_SAMPLE_help);
  m_queue = m_mcu->alloc_command_queue();
}

HX711Sample::~HX711Sample() {}

void HX711Sample::_build_config(int para) {
  if (para & 1) {
    stringstream config_hx711_sample;
    config_hx711_sample << "config_hx711_sample oid=" << m_oid
                        << " hx711_count=" << m_s_count
                        << " kalman_q=" << kalman_q_
                        << " kalman_r=" << kalman_r_;
    m_mcu->add_config_cmd(config_hx711_sample.str());

    std::cout << "**************config_hx711_sample:"
              << config_hx711_sample.str() << std::endl;

    for (int i = 0; i < m_s_count; i++) {
      pinParams *clk_pin_params =
          Printer::GetInstance()->m_ppins->lookup_pin(m_s_clk_pin[i]);
      pinParams *sdo_pin_params =
          Printer::GetInstance()->m_ppins->lookup_pin(m_s_sdo_pin[i]);
      stringstream add_hx711_sample;
      add_hx711_sample << "add_hx711_sample oid=" << m_oid << " index=" << i
                       << " clk_pin=" << clk_pin_params->pin
                       << " sdo_pin=" << sdo_pin_params->pin;
      m_mcu->add_config_cmd(add_hx711_sample.str());

      std::cout << "i:" << i << " clk_pin_params->pin:" << clk_pin_params->pin
                << " sdo_pin_params->pin:" << sdo_pin_params->pin << std::endl;
    }
  }
}

void HX711Sample::delay_s(double delay_s) {
  double eventtime = get_monotonic();
  if (!Printer::GetInstance()->is_shutdown()) {
    Printer::GetInstance()->m_tool_head->get_last_move_time();
    eventtime = Printer::GetInstance()->m_reactor->pause(eventtime + delay_s);
  }
}

void HX711Sample::calibration_start(int times_read) {
  if (m_is_shutdown || m_is_timeout) {
    return;
  }
  stringstream calibration_sample;
  calibration_sample << "calibration_sample oid=" << m_oid
                           << " times_read=" << times_read;
  m_mcu->m_serial->send(calibration_sample.str(), 0, 0, m_queue);
}
