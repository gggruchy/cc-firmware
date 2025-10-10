
#include "klippy.h"
#include "tmc_uart.h"
#include "my_string.h"
#include "debug.h"
#define LOG_TAG "tmc_uart"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
MCU_analog_mux::MCU_analog_mux(MCU *mcu, command_queue *cmd_queue, std::string select_pins_desc) // select_pins  串口开关选择，用于分开配置串口
{
    m_mcu = mcu;
    m_cq = cmd_queue;
    PrinterPins *ppins = Printer::GetInstance()->m_ppins;
    std::istringstream iss(select_pins_desc); // 输入流
    std::string spd;                          // 接收缓冲区
    while (getline(iss, spd, ','))            // 以split为分隔符  for(auto spd : select_pins_desc)
    {
        pinParams *select_pin_params = ppins->lookup_pin(spd, true);
        m_pins.push_back(select_pin_params->pin);
        pin_values.push_back(-1);
        int oid = m_mcu->create_oid();
        m_oids.push_back(oid);
        // m_mcu->request_move_queue_slot();
        // m_oid = m_mcu->create_oid();
        // std::stringstream config_digital_out;
        // config_digital_out << "config_digital_out oid=" << m_oid << " pin=" << select_pin_params->pin << "  value=0 default_value=0 max_duration=0";
        // m_mcu->add_config_cmd(config_digital_out.str());
    }
    update_pin_cmd = nullptr;
    m_mcu->register_config_callback(std::bind(&MCU_analog_mux::build_config, this, std::placeholders::_1));
}

MCU_analog_mux::~MCU_analog_mux()
{
}
void MCU_analog_mux::build_config(int para)
{
    if (para & 1)
    {
        for (int i = 0; i < m_pins.size(); i++)
        {
            m_mcu->request_move_queue_slot();
            std::stringstream config_digital_out;
            config_digital_out << "config_digital_out oid=" << m_oids[i] << " pin=" << m_pins[i] << "  value=0 default_value=0 max_duration=0";
            m_mcu->add_config_cmd(config_digital_out.str());
        }
    }
    if (para & 4)
    {
        // std::stringstream update_digital_out;
        // update_digital_out << "update_digital_out oid=" << m_oid << " value=" << m_start_value;
        // m_mcu->add_config_cmd(update_digital_out.str(), false, true);
    }
}
std::vector<int> MCU_analog_mux::get_instance_id(std::string select_pins_desc)
{
    PrinterPins *ppins = Printer::GetInstance()->m_ppins;
    std::vector<int> instance_id;
    std::vector<std::string> parts = split(select_pins_desc, ",");
    for (int i = 0; i < parts.size(); i++)
    {
        pinParams *select_pin_params = ppins->lookup_pin(parts[i], true);
        if (select_pin_params->chip != m_mcu)
        {
            GAM_ERR_printf("TMC mux pins must be on the same mcu");
        }

        std::string pin = select_pin_params->pin;
        if ((m_pins.size() <= i) || (pin != m_pins[i]))
        {
            GAM_ERR_printf("All TMC mux instances must use identical pins");
        }
        instance_id.push_back(!select_pin_params->invert);
    }
    return instance_id;
}

void MCU_analog_mux::activate(std::vector<int> instance_id)
{
    for (int i = 0; i < instance_id.size(); i++)
    {
        if (pin_values[i] != instance_id[i])
        {
            std::stringstream update_digital_out;
            update_digital_out << "update_digital_out oid=" << m_oids[i] << " value=" << instance_id[i];
            m_mcu->add_config_cmd(update_digital_out.str(), false, true);
        }
    }
    pin_values = instance_id;
}

PrinterTMCUartMutexes::PrinterTMCUartMutexes()
{
    mcu_to_mutex = std::map<MCU *, ReactorMutex *>();
}

PrinterTMCUartMutexes::~PrinterTMCUartMutexes()
{
}

static ReactorMutex *lookup_tmc_uart_mutex(MCU *mcu)
{
    PrinterTMCUartMutexes *pmutexes = (PrinterTMCUartMutexes *)Printer::GetInstance()->lookup_object("tmc_uart");
    if (pmutexes == nullptr)
    {
        pmutexes = new PrinterTMCUartMutexes();
        Printer::GetInstance()->add_object("tmc_uart", pmutexes);
    }
    ReactorMutex *mutex = nullptr;
    if (pmutexes->mcu_to_mutex.find(mcu) != pmutexes->mcu_to_mutex.end())
        mutex = pmutexes->mcu_to_mutex[mcu];

    if (mutex == nullptr)
    {
        mutex = Printer::GetInstance()->get_reactor()->mutex();
        pmutexes->mcu_to_mutex[mcu] = mutex;
    }
    return mutex;
}

MCU_TMC_uart_bitbang::MCU_TMC_uart_bitbang(pinParams *rx_pin_params, pinParams *tx_pin_params, std::string select_pins_desc)
{
    m_mcu = (MCU *)rx_pin_params->chip;
    mutex = lookup_tmc_uart_mutex(m_mcu);
    pullup = rx_pin_params->pullup;
    rx_pin = rx_pin_params->pin;
    tx_pin = tx_pin_params->pin;
    m_oid = m_mcu->create_oid();
    m_cmd_queue = m_mcu->alloc_command_queue();
    analog_mux = nullptr;
    if (select_pins_desc != "")
        analog_mux = new MCU_analog_mux(m_mcu, m_cmd_queue, select_pins_desc);
    instances = std::map<std::vector<int>, bool>();
    m_mcu->register_config_callback(std::bind(&MCU_TMC_uart_bitbang::build_config, this, std::placeholders::_1));
}

MCU_TMC_uart_bitbang::~MCU_TMC_uart_bitbang()
{
}
#define TMC_BAUD_RATE 20000
void MCU_TMC_uart_bitbang::build_config(int para)
{
    uint64_t bit_ticks = m_mcu->seconds_to_clock(1. / TMC_BAUD_RATE);
    if (para & 1)
    {
        m_mcu->request_move_queue_slot();
        std::stringstream config_tmcuart;
        config_tmcuart << "config_tmcuart oid=" << m_oid << " rx_pin=" << rx_pin << " pull_up=" << pullup << " tx_pin=" << tx_pin << " bit_time=" << (uint64_t)bit_ticks;
        m_mcu->add_config_cmd(config_tmcuart.str());
    }
}
// ParseResult MCU_TMC_uart_bitbang:: tmcuart_send_cmd( std::vector<uint8_t> data, uint32_t read,uint64_t minclock, uint64_t reqclock)
//  {
//     ParseResult ret;
//     std::string data_msg;
//     for (int i = 0; i < data.size(); i++)
//     {
//         data_msg += data[i];
//     }
//     std::stringstream tmcuart_send;
//     tmcuart_send << "tmcuart_send oid=" << m_oid << " write=" << data_msg << " read=" << read;
//     if(read) ret = m_mcu->m_serial->send_with_response(tmcuart_send.str(), "tmcuart_response", m_cmd_queue, m_oid);
//     else m_mcu->m_serial->send(tmcuart_send.str(), minclock, reqclock, m_cmd_queue);
//     return ret;

//  }

std::vector<int> MCU_TMC_uart_bitbang::register_instance(pinParams *rx_pin_params, pinParams *tx_pin_params, std::string select_pins_desc, uint8_t addr)
{
    if ((rx_pin_params->pin != rx_pin) || (tx_pin_params->pin != tx_pin))
    {
        GAM_ERR_printf("Shared TMC uarts must use the same pins");
    }
    std::vector<int> instance_id = std::vector<int>();
    if (analog_mux != nullptr)
    {
        instance_id = analog_mux->get_instance_id(select_pins_desc);
    }
    instance_id.push_back((int)addr);
    if (instances.find(instance_id) != instances.end())
    {
        GAM_ERR_printf("Shared TMC uarts need unique address or select_pins polarity");
    }
    instances[instance_id] = true;
    instance_id.pop_back();
    return instance_id;
}
uint8_t MCU_TMC_uart_bitbang::_calc_crc8(uint16_t *data, uint8_t size)
{
    uint8_t crc = 0;
    for (int i = 0; i < size; i++)
    {
        uint8_t b = data[i];
        for (int j = 0; j < 8; j++)
        {
            if ((crc >> 7) ^ (b & 0x01))
                crc = (crc << 1) ^ 0x07;
            else
                crc = (crc << 1);
            crc &= 0xff;
            b >>= 1;
        }
    }
    return crc;
}
void MCU_TMC_uart_bitbang::_add_serial_bits(uint16_t *data, uint8_t size) // 8->10
{
    for (int i = 0; i < size; i++)
    {
        uint16_t d = data[i];
        data[i] = (d << 1) | 0x200;
    }
}
void MCU_TMC_uart_bitbang::_encode_read(uint8_t *data, uint8_t sync, uint8_t addr, uint8_t reg) // 1BYTE crc8  8->10
{
    uint16_t msg[4] = {sync, addr, reg, 0};

    msg[3] = _calc_crc8(msg, 3);
    // printf("_encode_read = %x %x %x %x\n",msg[0],msg[1],msg[2],msg[3] );
    _add_serial_bits(msg, 4);
    // for(int i=0; i<4; i+=4)
    {
        uint16_t a = msg[0];
        uint16_t b = msg[1];
        uint16_t c = msg[2];
        uint16_t d = msg[3];
        data[0] = (a) & 0xff;
        data[1] = ((b << 2) & 0xfc) | ((a >> 8) & 0x3);
        data[2] = ((c << 4) & 0xf0) | ((b >> 6) & 0xf);
        data[3] = ((d << 6) & 0xc0) | ((c >> 4) & 0x3f);
        data[4] = ((d >> 2) & 0xff);

        // data[0] =  0xaa;
        // data[1] = 0xaa;
        // data[2] = 0xaa;
        // data[3] = 0xaa;
        // data[4] =  0xaa;
    }
}
void MCU_TMC_uart_bitbang::_encode_write(uint8_t *data, uint8_t sync, uint8_t addr, uint8_t reg, uint32_t val)
{
    uint16_t msg[8] = {sync, addr, reg, (val >> 24) & 0xff, (val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff};

    msg[7] = _calc_crc8(msg, 7);
    _add_serial_bits(msg, 8);
    for (int j = 0; j < 2; j++)
    {
        int i = j * 4;
        uint16_t a = msg[i + 0];
        uint16_t b = msg[i + 1];
        uint16_t c = msg[i + 2];
        uint16_t d = msg[i + 3];
        i = j * 5;
        data[i + 0] = (a) & 0xff;
        data[i + 1] = ((b << 2) & 0xfc) | ((a >> 8) & 0x3);
        data[i + 2] = ((c << 4) & 0xf0) | ((b >> 6) & 0xf);
        data[i + 3] = ((d << 6) & 0xc0) | ((c >> 4) & 0x3f);
        data[i + 4] = ((d >> 2) & 0xff);
    }
}
uint32_t MCU_TMC_uart_bitbang::_decode_read(uint8_t reg, uint8_t *data, uint8_t size)
{
    uint32_t val = 0;
    uint8_t data1[10];
    if (size != 10)
    {
        return 0;
    }
    {
        uint32_t a = data[3]; // 24-31
        val = (a >> 7) << 24;
        a = data[4]; // 32-39
        val |= (a & 0x7f) << 25;
        a = data[5]; // 40-47
        val |= ((a >> 1) & 0x7f) << 16;
        a = data[6]; // 48-55
        val |= ((a) & 1) << 23;
        val |= ((a >> 3) & 0x1f) << 8;
        a = data[7]; // 56-63
        val |= ((a) & 0x7) << 13;
        val |= ((a >> 5) & 0x7) << 0;
        a = data[8]; // 64-71
        val |= ((a) & 0x1f) << 3;
    }
    _encode_write(data1, 0x05, 0xff, reg, val);
    // printf("_decode_read data=%x   3:%x 4:%x 5:%x 6:%x 7:%x 8:%x 9:%x\n",val,data[3],data[4],data[5],data[6],data[7],data[8],data[9]);
    for (int i = 0; i < 10; i++)
    {
        // printf("_decode_read data=%x   %x %x \n",i,data1[i],data[i]);
        if (data1[i] != data[i])
        {
            GAM_ERR_printf("_decode_read data=%x   2:%x 3:%x 4:%x 5:%x 6:%x 7:%x 8:%x 9:%x\n", val, data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9]);
            return UINT32_MAX;
        }
    }
    return val;
}
uint32_t MCU_TMC_uart_bitbang::reg_read(std::vector<int> instance_id, uint8_t addr, uint8_t reg)
{
    uint8_t data[5];
    if (analog_mux != nullptr)
    {
        analog_mux->activate(instance_id);
    }
    _encode_read(data, 0xf5, addr, reg); // 1BYTE crc8  8->10

    std::string data_msg;
    for (int i = 0; i < 5; i++)
    {
        // printf("data = %x\n", data[i]);
        data_msg += data[i];
    }
    std::stringstream tmcuart_send;
    tmcuart_send << "tmcuart_send oid=" << m_oid << " write=" << data_msg << " read=" << 10;
    ParseResult params = m_mcu->m_serial->send_with_response(tmcuart_send.str(), "tmcuart_response", m_cmd_queue, m_oid);

    std::string response = params.PT_string_outs["read"];
    uint8_t *response_data = (uint8_t *)response.c_str();
    uint8_t response_count = response.length();

    return _decode_read(reg, response_data, response_count);
}

void MCU_TMC_uart_bitbang::reg_write(std::vector<int> instance_id, uint8_t addr, uint8_t reg, uint32_t val, double print_time)
{
    uint8_t data[10];
    uint64_t minclock = 0;
    if (print_time != DBL_MAX)
        minclock = m_mcu->print_time_to_clock(print_time);
    if (analog_mux != nullptr)
        analog_mux->activate(instance_id);
    _encode_write(data, 0xf5, addr, reg | 0x80, val);
    std::string data_msg;
    for (int i = 0; i < 10; i++)
    {
        data_msg += data[i];
    }
    std::stringstream tmcuart_send;
    tmcuart_send << "tmcuart_send oid=" << m_oid << " write=" << data_msg << " read=" << 0;
    // m_mcu->m_serial->send(tmcuart_send.str(), minclock, minclock, m_cmd_queue);
    ParseResult params = m_mcu->m_serial->send_with_response(tmcuart_send.str(), "tmcuart_response", m_cmd_queue, m_oid, minclock, minclock); // 写完才能下一次读写
}
//  Lookup a (possibly shared) tmc uart

std::vector<int> lookup_tmc_uart_bitbang(MCU_TMC_uart_bitbang **m_mcu_uart, std::string section_name, uint8_t addr, uint8_t max_addr)
{
    std::string uart_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "uart_pin", "");
    PrinterPins *ppins = Printer::GetInstance()->m_ppins;
    pinParams *rx_pin_params = ppins->lookup_pin(uart_pin, false, true, "tmc_uart_rx");
    std::string tx_pin_desc = Printer::GetInstance()->m_pconfig->GetString(section_name, "tx_pin", "");
    pinParams *tx_pin_params;
    if (tx_pin_desc == "")
        tx_pin_params = rx_pin_params;
    else
        tx_pin_params = ppins->lookup_pin(tx_pin_desc, false, false, "tmc_uart_tx");
    if (rx_pin_params->chip != tx_pin_params->chip)
    {
        GAM_ERR_printf("TMC uart rx and tx pins must be on the same mcu\n");
    }
    std::string select_pins_desc = Printer::GetInstance()->m_pconfig->GetString(section_name, "select_pins", "");
    *m_mcu_uart = (MCU_TMC_uart_bitbang *)rx_pin_params->pclass;
    if (*m_mcu_uart == nullptr)
    {
        *m_mcu_uart = new MCU_TMC_uart_bitbang(rx_pin_params, tx_pin_params, select_pins_desc);
        rx_pin_params->pclass = *m_mcu_uart;
    }

    std::vector<int> instance_id = (*m_mcu_uart)->register_instance(rx_pin_params, tx_pin_params, select_pins_desc, addr);
    return instance_id;
}
MCU_TMC_uart::MCU_TMC_uart(std::string section_name, std::map<std::string, uint8_t> name_to_reg_i, FieldHelper *fields_i, uint8_t max_addr) : MCU_TMC()
{
    m_mcu_uart = nullptr;
    name = split(section_name, " ").back();
    name_to_reg = name_to_reg_i;
    fields = fields_i;
    ifcnt = UINT32_MAX;
    addr = Printer::GetInstance()->m_pconfig->GetInt(section_name, "uart_address", 0, 0, max_addr);
    instance_id = lookup_tmc_uart_bitbang(&m_mcu_uart, section_name, addr, max_addr);
    mutex = m_mcu_uart->mutex;
}

// MCU_TMC_uart::~MCU_TMC_uart()
// {

// }

uint32_t MCU_TMC_uart::_do_get_register(std::string reg_name)
{
    uint8_t reg = name_to_reg[reg_name];
    if (Printer::GetInstance()->get_start_args("debugoutput") != "")
        return 0;
    for (int i = 0; i < 5; i++)
    {
        uint32_t val = m_mcu_uart->reg_read(instance_id, addr, reg);
        // printf(" =instance_id:%x  addr:%x reg:%x _do_get_register val %s:%x\n",instance_id,addr,reg,reg_name.c_str(),val);
        // printf(" _do_get_register:addr:%x reg:%x register_val %s:%x\n",addr,reg,reg_name.c_str(),val);
        if (val != UINT32_MAX)
            return val;
    }
    LOG_E("Unable to read tmc uart '%s' register %s", name, reg_name);
}
uint32_t MCU_TMC_uart::get_register(std::string reg_name)
{
    // if(mutex != nullptr)
    return _do_get_register(reg_name);
}

#if ENABLE_MANUTEST
extern int stepperx_com;
extern int steppery_com;
extern int stepperz_com;
extern int steppere_com;
#endif

void MCU_TMC_uart::set_register(std::string reg_name, uint32_t val, double print_time)
{
    uint8_t reg = name_to_reg[reg_name];
    if (Printer::GetInstance()->get_start_args("debugoutput") != "")
        return;
    if (mutex != nullptr) // with self.mutex:
    {
    }
    // printf(" _do_get_register:addr:%x reg:%x register_val %s:%x\n",addr,reg,reg_name.c_str(),_do_get_register(reg_name));
    for (int i = 0; i < 5; i++)
    {
        uint32_t ifcnt_old = ifcnt;
        if (ifcnt == UINT32_MAX)
        {
            ifcnt_old = ifcnt = _do_get_register("IFCNT");
        }
        m_mcu_uart->reg_write(instance_id, addr, reg, val, print_time);
        if (ifcnt == 0)
        {
            LOG_I(" set_register:addr:%x reg:%x set_register_val %s:%x\n", addr, reg, reg_name.c_str(), val);
            LOG_I(" _do_get_register:addr:%x reg:%x register_val %s:%x\n", addr, reg, reg_name.c_str(), _do_get_register(reg_name));
        }
        ifcnt = _do_get_register("IFCNT");
        if (ifcnt == (ifcnt_old + 1) & 0xff)
            return;
    }
#if ENABLE_MANUTEST
    stepperx_com = 0;
    steppery_com = 0;
    stepperz_com = 0;
    steppere_com = 0;
#endif
    LOG_E("Unable to write tmc uart : %s register : %s\n", name.c_str(), reg_name.c_str());
}
