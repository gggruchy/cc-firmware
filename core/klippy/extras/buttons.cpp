#include "buttons.h"
#include "klippy.h"
#include "my_string.h"


static const double QUERY_TIME = .002;
static const int RETRANSMIT_COUNT = 50;

MCU_buttons::MCU_buttons(MCU* mcu)
{
    m_reactor = Printer::GetInstance()->get_reactor();
    m_mcu = mcu;
    m_mcu->register_config_callback(std::bind(&MCU_buttons::build_config, this, std::placeholders::_1));
    m_invert = m_last_button = 0;
    m_ack_cmd = nullptr;
    m_ack_count = 0;
}

MCU_buttons::~MCU_buttons()
{
}

void MCU_buttons::setup_buttons(std::vector<pinParams*> pins, std::function<void(double, bool)> callback)
{
    int mask = 0;
    int shift = m_pin_list.size();
    for(auto pin_params : pins)
    {
        if(pin_params->invert)
        {
            m_invert |= 1 << m_pin_list.size();
        }
        mask |= 1 << m_pin_list.size();
        pin_list_item_t item{
            .pin = pin_params->pin,
            .pullup = pin_params->pullup
        };
        m_pin_list.push_back(item);
    }
    callbacks_item_t cb_item{
        .mask = mask,
        .shift = shift,
        .callback = callback
    };
    m_callbacks.push_back(cb_item);
}

void MCU_buttons::build_config(int para)
{
    if (m_pin_list.size() == 0)
        return;
    m_oid = m_mcu->create_oid();
    m_mcu->add_config_cmd("config_buttons oid=" + to_string(m_oid) + " button_count=" + to_string(m_pin_list.size()));
    for(int i = 0; i < m_pin_list.size(); i++)
    {
        m_mcu->add_config_cmd("buttons_add oid=" + to_string(m_oid) + " pos=" + to_string(i) +
                                 " pin=" + m_pin_list[i].pin + " pull_up=" + to_string(m_pin_list[i].pullup));
    }
    command_queue * cmd_queue = m_mcu->alloc_command_queue();
    m_ack_cmd = m_mcu->lookup_command("buttons_ack oid=%c count=%c", cmd_queue);
    double clock = m_mcu->get_query_slot(m_oid);
    double rest_ticks = m_mcu->seconds_to_clock(QUERY_TIME);
    m_mcu->add_config_cmd("buttons_query oid=" + to_string(m_oid) + " clock=" + to_string(clock) +
                            " rest_ticks=" + to_string(rest_ticks) + " retransmit_count=" + to_string(RETRANSMIT_COUNT) + 
                            " invert=" + to_string(m_invert), true);
    m_mcu->register_response(std::bind(&MCU_buttons::handle_buttons_state, this, std::placeholders::_1), "buttons_state", m_oid);
}

void MCU_buttons::handle_buttons_state(ParseResult &params)
{
    // Expand the message ack_count from 8-bit
    int ack_count = m_ack_count;
    int ack_diff = (ack_count - params.PT_uint32_outs["ack_count"]) & 0xff;
    if (ack_diff & 0x80)
        ack_diff -= 0x100;
    int msg_ack_count = ack_count - ack_diff;
    // Determine New buttons
    // buttons = bytearray(params['state']); //---????----
    // int new_count = msg_ack_count + buttons.size() - m_ack_count;
    // if (new_count <= 0)
    //     return;
    // new_buttons = buttons[-new_count:];
    // // Send ack to MCU
    // m_ack_cmd->send([m_oid, new_count]);
    // m_ack_count += new_count;
    // // Call self.handle_button() with this event in main thread
    // for (nb in new_buttons)
    //     self.reactor.register_async_callback(
    //         (lambda e, s=self, b=nb: s.handle_button(e, b)));
}

void MCU_buttons::handle_button(double eventtime, int button)
{
    button ^= m_invert;
    int changed = button ^ m_last_button;
    for(auto cb_item : m_callbacks)
    {
        if(changed & cb_item.mask)
        {
            cb_item.callback(eventtime, (button & cb_item.mask) >> cb_item.shift);
        }
        m_last_button = button;
    }
}


//######################################################################
// ADC button tracking
//######################################################################

static const double ADC_REPORT_TIME = 0.015;
static const double ADC_DEBOUNCE_TIME = 0.025;
static const double ADC_SAMPLE_TIME = 0.001;
static const int ADC_SAMPLE_COUNT = 6;

MCU_ADC_buttons::MCU_ADC_buttons(std::string pin, int pullup)
{
    m_reactor = Printer::GetInstance()->get_reactor();
    m_last_button = -1;
    m_last_pressed = -1;
    m_last_debouncetime = 0;
    m_pullup = pullup;
    m_pin = pin;
    m_min_value = 999999999999.9;
    m_max_value = 0.;
    m_mcu_adc = (MCU_adc*)Printer::GetInstance()->m_ppins->setup_pin("adc", m_pin);
    m_mcu_adc->setup_minmax(ADC_SAMPLE_TIME, ADC_SAMPLE_COUNT);
    m_mcu_adc->setup_adc_callback(ADC_REPORT_TIME, std::bind(&MCU_ADC_buttons::adc_callback, this, std::placeholders::_1, std::placeholders::_2));
    // query_adc = printer.lookup_object('query_adc'); //----????----
    // Printer::GetInstance()->m_query_adc->register_adc("adc_button:" + strip(pin), m_mcu_adc);//----????----
}

MCU_ADC_buttons::~MCU_ADC_buttons()
{
}

void MCU_ADC_buttons::setup_button(double min_value, double max_value, std::function<void(double, bool, std::function<void(double)>)> callback)
{
    m_min_value = std::min(m_min_value, min_value);
    m_max_value = std::max(m_max_value, max_value);
    buttons_item_t item{
        .min_value = min_value,
        .max_value = max_value,
        .callback = callback
    };
    m_buttons.push_back(item);
}

void MCU_ADC_buttons::adc_callback(double read_value, double read_time)
{
    double adc = std::max(.00001, std::min(.99999, read_value));
    double value = m_pullup * adc / (1.0 - adc);

    // Determine button pressed
    int btn = -1;
    if(m_min_value <= value && value <= m_max_value)
    {
        for(int i = 0; i < m_buttons.size(); i++)
        {
            if(m_buttons[i].min_value < value && value < m_buttons[i].max_value)
            {
                btn = i;
                break;
            }
        }
    }

    // If the button changed, due to noise or pressing:
    if (btn != m_last_button)
    {
        // reset the debouncing timer
        m_last_debouncetime = read_time;
    }

    // button debounce check & New button pressed
    if ((read_time - m_last_debouncetime) >= ADC_DEBOUNCE_TIME
        && m_last_button == btn && m_last_pressed != btn)
    {

        // release last_pressed
        if (m_last_pressed != -1)
        {
            call_button(m_last_pressed, false);
            m_last_pressed = -1;
        }
        if (btn != -1)
        {
            call_button(btn, true);
            m_last_pressed = btn;
        }
    }

    m_last_button = btn;
}

void MCU_ADC_buttons::call_button(int button, bool state)
{
    buttons_item_t bts = m_buttons[button];
    // m_reactor->register_async_callback(); //----????----
    // minval, maxval, callback = self.buttons[button]
    // self.reactor.register_async_callback(
    //     (lambda e, cb=callback, s=state: cb(e, s)))
}


//####################################################################
// Rotary Encoders
//####################################################################

// Rotary encoder handler https://github.com/brianlow/Rotary
// Copyright 2011 Ben Buxton (bb@cactii.net).
// Licenced under the GNU GPL Version 3.
static const int R_START     = 0x0;
static const int R_CW_FINAL  = 0x1;
static const int R_CW_BEGIN  = 0x2;
static const int R_CW_NEXT   = 0x3;
static const int R_CCW_BEGIN = 0x4;
static const int R_CCW_FINAL = 0x5;
static const int R_CCW_NEXT  = 0x6;
static const int R_DIR_CW    = 0x10;
static const int R_DIR_CCW   = 0x20;
static const int R_DIR_MSK   = 0x30;
// Use the full-step state table (emits a code at 00 only)
std::vector<std::vector<int>> ENCODER_STATES = {
  // R_START
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | R_DIR_CW},
  // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | R_DIR_CCW},
  // R_CCW_NEXT
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START}
};


RotaryEncoder::RotaryEncoder(std::function<void(double)> cw_callback, std::function<void(double)> ccw_callback)
{
    m_cw_callback = cw_callback;
    m_ccw_callback = ccw_callback;
    m_encoder_state = R_START;
}

RotaryEncoder::~RotaryEncoder()
{
}

void RotaryEncoder::encoder_callback(double eventtime, bool state)
{
    int es = ENCODER_STATES[m_encoder_state & 0xf][state & 0x3];
    m_encoder_state = es;
    if (es & R_DIR_MSK == R_DIR_CW)
        m_cw_callback(eventtime);
    else if( es & R_DIR_MSK == R_DIR_CCW)
        m_ccw_callback(eventtime);
}

//#####################################################################
// Button registration code
//#####################################################################

PrinterButtons::PrinterButtons()
{
    Printer::GetInstance()->load_object("query_adc");
}

PrinterButtons::~PrinterButtons()
{
}

void PrinterButtons::register_adc_button(std::string pin, double min_val, double max_val, int pullup, std::function<void(double, bool, std::function<void(double)>)> callback)
{
    auto iter = m_adc_buttons.find(pin);
    MCU_ADC_buttons* adc_buttons;
    if (iter == m_adc_buttons.end())
    {
        adc_buttons = new MCU_ADC_buttons(pin, pullup);       //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:MCU_ADC_buttons",0);
        m_adc_buttons[pin] = adc_buttons;
    }
     m_adc_buttons[pin]->setup_button(min_val, max_val, callback);
}

void PrinterButtons::helper(double eventtime, bool state, std::function<void(double)> callback)
{
    if(state)
        callback(eventtime);
}

void PrinterButtons::register_adc_button_push(std::string pin, double min_val, double max_val, int pullup, std::function<void(double)> callback)
{
    register_adc_button(pin, min_val, max_val, pullup, std::bind(&PrinterButtons::helper, this, std::placeholders::_1, std::placeholders::_2, callback));
}

void PrinterButtons::register_buttons(std::vector<std::string> pins, std::function<void(double, bool)> callback)
{
    // Parse pins
    MCU* mcu = nullptr; 
    std::string mcu_name = "";
    std::vector<pinParams*> pin_params_list;
    for (auto pin : pins)
    {
        pinParams *pin_params = Printer::GetInstance()->m_ppins->lookup_pin(pin, true, true);
        if (mcu != nullptr && pin_params->chip != mcu)
        {
            std::cout << "button pins must be on same mcu" << std::endl;
            return;
        }
        mcu =  (MCU *)pin_params->chip;
        mcu_name = pin_params->chip_name;
        pin_params_list.push_back(pin_params);
    }
    // Register pins and callback with the appropriate MCU
    auto iter = m_mcu_buttons.find(mcu_name);
    if (iter == m_mcu_buttons.end() || (iter->second->m_pin_list.size() + pin_params_list.size() > 8))
    {
        MCU_buttons* mcu_buttons = new MCU_buttons(mcu);       //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:MCU_buttons",0);
        m_mcu_buttons[mcu_name] = mcu_buttons;
    }
    // iter->second->setup_buttons(pin_params_list, callback);  
    m_mcu_buttons[mcu_name]->setup_buttons(pin_params_list, callback);      //---?----
}

void PrinterButtons::register_rotary_encoder(std::string pin1, std::string pin2, std::function<void(double)> cw_callback, std::function<void(double)> ccw_callback)
{
    RotaryEncoder* re = new RotaryEncoder(cw_callback, ccw_callback);       //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:RotaryEncoder",0);
    std::vector<std::string> pins = {pin1, pin2};
    register_buttons(pins, std::bind(&RotaryEncoder::encoder_callback, re, std::placeholders::_1, std::placeholders::_2));
}

void PrinterButtons::register_button_push(std::string pin, std::function<void(double)> callback)
{
    std::vector<std::string> pins = {pin};
    register_buttons(pins, std::bind(&PrinterButtons::helper, this, std::placeholders::_1, std::placeholders::_2, callback));
}
