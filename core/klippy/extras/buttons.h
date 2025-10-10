#ifndef BUTTONS_H
#define BUTTONS_H
#include "mcu.h"
#include "pins.h"
#include "mcu_io.h"

typedef struct pin_list_item_tag
{
    std::string pin;
    int pullup;
}pin_list_item_t;

typedef struct callbacks_item_tag
{
    int mask;
    int shift;
    std::function<void(double, bool)> callback;
}callbacks_item_t;


class MCU_buttons
{
private:
    
public:
    SelectReactor* m_reactor;
    MCU* m_mcu;
    std::vector<pin_list_item_t> m_pin_list;
    std::vector<callbacks_item_t> m_callbacks;
    int m_invert;
    int m_last_button;
    CommandWrapper* m_ack_cmd;
    int m_ack_count;
    int m_oid = 0;
public:
    MCU_buttons(MCU* mcu);
    ~MCU_buttons();
    void setup_buttons(std::vector<pinParams*> pins, std::function<void(double, bool)> callback);
    void build_config(int para);
    void handle_buttons_state(ParseResult &params);
    void handle_button(double eventtimem, int button);

};

typedef struct buttons_item_tag
{
    double min_value;
    double max_value;
    std::function<void(double, bool, std::function<void(double)>)> callback;
}buttons_item_t;


class MCU_ADC_buttons
{
private:
    
public:
    SelectReactor* m_reactor;
    int m_last_debouncetime;
    int m_pullup;
    std::string m_pin;
    double m_min_value;
    double m_max_value;
    MCU_adc* m_mcu_adc;
    int m_last_button;
    int m_last_pressed;
    std::vector<buttons_item_t> m_buttons;
public:
    MCU_ADC_buttons(std::string pin, int pullup);
    ~MCU_ADC_buttons();
    void setup_button(double min_value, double max_value, std::function<void(double, bool, std::function<void(double)>)> callback);
    void adc_callback(double read_value, double read_time);
    void call_button(int button, bool state);
};


class RotaryEncoder
{
private:
public:
    std::function<void(double)> m_cw_callback;
    std::function<void(double)> m_ccw_callback;
    int m_encoder_state;
public:
    RotaryEncoder(std::function<void(double)> cw_callback, std::function<void(double)> ccw_callback);
    ~RotaryEncoder();
    void encoder_callback(double eventtime, bool state);
};

class PrinterButtons
{
private:
    
public:
    std::map<std::string, MCU_buttons*> m_mcu_buttons;
    std::map<std::string, MCU_ADC_buttons*> m_adc_buttons;
public:
    PrinterButtons();
    ~PrinterButtons();
    void register_adc_button(std::string pin, double min_val, double max_val, int pullup, std::function<void(double, bool, std::function<void(double)>)> callback);
    void helper(double eventtime, bool state, std::function<void(double)> callback);
    void register_adc_button_push(std::string pin, double min_val, double max_val, int pullup, std::function<void(double)> callback);
    void register_buttons(std::vector<std::string> pins, std::function<void(double, bool)> callback);
    void register_rotary_encoder(std::string pin1, std::string pin2, std::function<void(double)> cw_callback, std::function<void(double)> ccw_callback);
    void register_button_push(std::string pin, std::function<void(double)> callback);
};







#endif