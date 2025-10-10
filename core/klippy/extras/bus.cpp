#include "bus.h"
#include "klippy.h"
#include "Define.h"

std::string resolve_bus_name(MCU *mcu, std::string param, std::string bus)
{
    if (bus.find("spi0") != std::string::npos)
    {
        return "0";
    }
    else if (bus.find("spi1") != std::string::npos)
    {
        return "1";
    }
    else if (bus.find("spi2") != std::string::npos)
    {
        return "2";
    }
    else if (bus.find("spi3") != std::string::npos)
    {
        return "3";
    }
    else
    {
        return "";
    }
}

//#####################################################################
// SPI
//#####################################################################

// Helper code for working with devices connected to an MCU via an SPI bus
MCU_SPI::MCU_SPI(MCU* mcu, std::string bus, std::string pin, int mode, double speed, std::vector<std::string> sw_pins)
{
    m_mcu = mcu;
    m_bus = bus;
    m_pin = m_mcu->m_serial->m_msgparser->m_pinMap[pin];
    // Config SPI object (set all CS pins high before spi_set_bus commands)
    m_oid = m_mcu->create_oid();
    if(m_mcu->use_mcu_spi)
    {
        if (pin == "")
        {
            m_mcu->add_config_cmd("config_spi_without_cs oid=" + std::to_string(m_oid));
        }
        else
        {
            m_mcu->add_config_cmd("config_spi oid=" + std::to_string(m_oid) + " pin=" + std::to_string(m_pin));
        }
    }
    // Generate SPI bus config message
    if (sw_pins.size() > 0)
    {
        int miso_pin = m_mcu->m_serial->m_msgparser->m_pinMap[sw_pins[0]];
        int mosi_pin = m_mcu->m_serial->m_msgparser->m_pinMap[sw_pins[1]];
        int sclk_pin = m_mcu->m_serial->m_msgparser->m_pinMap[sw_pins[2]];
        m_config_fmt << "spi_set_software_bus oid=" << m_oid << " miso_pin=" << miso_pin << " mosi_pin=" << mosi_pin << " sclk_pin=" << sclk_pin <<  " mode=" << mode << " rate=" << (uint32_t)speed;
    }
    else
    {
        std::string bus_id = resolve_bus_name(m_mcu, "spi_bus", m_bus);
        m_config_fmt << "spi_set_bus oid=" << m_oid << " spi_bus=" << bus_id << " mode=" << mode << " rate=" << (uint32_t)speed;
    }
    m_cmd_queue = m_mcu->alloc_command_queue();
    if (m_mcu->use_mcu_spi)
    {
        m_mcu->register_config_callback(std::bind(&MCU_SPI::build_config, this, std::placeholders::_1));
    }

    m_spi_send_cmd = "";
    m_spi_transfer_cmd = "";
}

MCU_SPI::~MCU_SPI()
{

}
        
void MCU_SPI::setup_shutdown_msg(int shutdown_seq)
{
    std::string shutdown_msg = std::to_string(shutdown_seq);
    std::stringstream config_spi_shutdown;
    config_spi_shutdown << "config_spi_shutdown oid=" << m_mcu->create_oid() << " spi_oid=" << m_oid << " shutdown_msg=" << shutdown_msg;
    m_mcu->add_config_cmd(config_spi_shutdown.str());
}
        
int MCU_SPI::get_oid()
{
    return m_oid;
}
        
MCU* MCU_SPI::get_mcu()
{
    return m_mcu;
}
        
command_queue * MCU_SPI::get_command_queue()
{
    return m_cmd_queue;
}
        
void MCU_SPI::build_config(int para)
{
    if( para & 1 )
    {
        m_mcu->add_config_cmd(m_config_fmt.str());
    }
    m_spi_send_cmd = "spi_send oid=%c data=%*s";
    m_spi_transfer_cmd = "spi_transfer oid=%c data=%*s";
    m_spi_transfer_response_cmd = "spi_transfer_response oid=%c response=%*s";
}
        
void MCU_SPI::spi_send(std::vector<uint8_t> data, uint64_t minclock, uint64_t reqclock)
{
    std::stringstream spi_send;
    std::string data_msg;
    for (int i = 0; i < data.size(); i++)
    {
        // printf("data = %x\n", data[i]);
        data_msg += data[i];
    }
    // std::cout << "send data_msg = " << data_msg << std::endl;
    if (m_spi_send_cmd == "")
    {
        // Send setup message via mcu initialization
        
        // data_msg = "".join(["%02x" % (x,) for x in data]);  //---??---bus  data_msg输入数据需要对齐
        spi_send << "spi_send oid=" << m_oid << " data=" << data_msg;
        m_mcu->add_config_cmd(spi_send.str(), true);
        return;
    }
    spi_send << "spi_send oid=" << m_oid << " data=" << data_msg;
    m_mcu->m_serial->send(spi_send.str(), minclock, reqclock, m_cmd_queue);
}
        
ParseResult MCU_SPI::spi_transfer(std::vector<uint8_t> data, uint64_t minclock, uint64_t reqclock)
{
    std::string data_msg;
    for (int i = 0; i < data.size(); i++)
    {
        // printf("data = %x\n", data[i]);
        data_msg += data[i];
    }
    // std::cout << "transfer data_msg =  " << data_msg << std::endl;
    std::stringstream spi_transfer;
    spi_transfer << "spi_transfer oid=" << m_oid << " data=" << data_msg;  //"data=%*s"  //---??---bus data输入数据需要对齐0x00
    ParseResult ret = m_mcu->m_serial->send_with_response(spi_transfer.str(), "spi_transfer_response", m_cmd_queue, m_oid);
    return ret;
    
}
        
void MCU_SPI::spi_transfer_with_preface(std::string preface_data, std::string data, uint64_t minclock, uint64_t reqclock)
{
    // return m_spi_transfer_cmd.send_with_preface(
    //         m_spi_send_cmd, [m_oid, preface_data], [m_oid, data],
    //         minclock=minclock, reqclock=reqclock)  //---??---bus
}  

// Helper to setup an spi bus from settings in a config section
MCU_SPI * MCU_SPI_from_config(std::string sectoin_name, int mode, std::string pin_option, int default_speed, std::string share_type)
{
    // Determine pin from config
    std::string cs_pin = Printer::GetInstance()->m_pconfig->GetString(sectoin_name, pin_option);
    pinParams *cs_pin_params = Printer::GetInstance()->m_ppins->lookup_pin(cs_pin, false, false, share_type);
    std::string pin = cs_pin_params->pin;
    if (pin == "")
    {
        Printer::GetInstance()->m_ppins->reset_pin_sharing(cs_pin_params);
        pin = "";
    }
        
    // Load bus parameters
    MCU *mcu =  (MCU *)cs_pin_params->chip;
    double speed = Printer::GetInstance()->m_pconfig->GetInt(sectoin_name, "spi_speed", default_speed, 100000);
    std::vector<std::string> sw_pins;
    std::string bus;
    if (Printer::GetInstance()->m_pconfig->GetString(sectoin_name, "spi_software_sclk_pin") != "")
    {
        std::vector<std::string> sw_pin_names = {"spi_software_miso_pin", "spi_software_mosi_pin", "spi_software_sclk_pin"};
        std::vector<pinParams*> sw_pin_params;
        for (int i = 0; i < sw_pin_names.size(); i++)
        {
            sw_pin_params.push_back(Printer::GetInstance()->m_ppins->lookup_pin(Printer::GetInstance()->m_pconfig->GetString(sectoin_name, sw_pin_names[i]), false, false, sw_pin_names[i]));
        }
        for (int i = 0; i < sw_pin_params.size(); i++)
        {
            if(sw_pin_params[i]->chip == mcu)
            {
                std::cout << sectoin_name << ": spi pins must be on same mcu" << std::endl;
            }
            sw_pins.push_back(sw_pin_params[i]->pin);
        }
        bus = "";
    }
    else
    {
        bus = Printer::GetInstance()->m_pconfig->GetString(sectoin_name, "spi_bus");
        sw_pins = std::vector<std::string>();
    }
    // Create MCU_SPI object
    MCU_SPI *ret = new MCU_SPI(mcu, bus, pin, mode, speed, sw_pins);
    return ret;
}

//#####################################################################
// I2C
//#####################################################################

// Helper code for working with devices connected to an MCU via an I2C bus
MCU_I2C::MCU_I2C(MCU *mcu, std::string bus, int addr, int speed)
{
    m_mcu = mcu;
    m_bus = bus;
    m_i2c_address = addr;
    m_oid = m_mcu->create_oid();
    std::string bus_id = resolve_bus_name(m_mcu, "i2c_bus", m_bus);
    m_config_fmt << "config_i2c oid=" << m_oid << " i2c_bus=" << bus_id << " rate=" << speed << " address=" << addr;
    m_cmd_queue = m_mcu->alloc_command_queue();
    m_mcu->register_config_callback(std::bind(&MCU_I2C::build_config, this, std::placeholders::_1));

    m_i2c_write_cmd = "";
    m_i2c_read_cmd = "";
    m_i2c_modify_bits_cmd = "";
}

MCU_I2C::~MCU_I2C()
{

}
        
int MCU_I2C::get_oid()
{
    return m_oid;
}
        
MCU* MCU_I2C::get_mcu()
{
    return m_mcu;
}
        
int MCU_I2C::get_i2c_address()
{
    return m_i2c_address;
}
        
command_queue* MCU_I2C::get_command_queue()
{
    return m_cmd_queue;
}
        
void MCU_I2C::build_config(int para)
{
    m_mcu->add_config_cmd(m_config_fmt.str());
    m_i2c_write_cmd = "i2c_write oid=%c data=%*s";
    m_i2c_read_cmd = "i2c_read oid=%c reg=%*s read_len=%u";
    m_i2c_read_response_cmd = "i2c_read_response oid=%c response=%*s";
    m_i2c_modify_bits_cmd = "i2c_modify_bits oid=%c reg=%*s clear_set_bits=%*s";
}
        
void MCU_I2C::i2c_write(std::vector<uint8_t> data, uint64_t minclock, uint64_t reqclock)
{
    std::stringstream data_msg;
    for (int i = 0; i < data.size(); i++)
    {
        data_msg << data[i];
    }
    std::stringstream i2c_write;
    if (m_i2c_write_cmd != "")
    {
        // Send setup message via mcu initialization
        // data_msg = "".join(["%02x" % (x,) for x in data])  //---??---bus  data_msg输入数据需要对齐
        i2c_write << "i2c_write oid=" << m_oid << " data=" << data_msg.str();  //输入数据需要对齐
        m_mcu->add_config_cmd(i2c_write.str(), true);
        return;
    }
    i2c_write << "i2c_write oid=" << m_oid << " data=" << data_msg.str();
    m_mcu->m_serial->send(i2c_write.str(), minclock, reqclock, m_cmd_queue);
}
        
ParseResult MCU_I2C::i2c_read(std::string write, int read_len)
{
    std::stringstream i2c_read;
    i2c_read << "i2c_read oid=" << m_oid << " reg=" << write << " read_len" << read_len;
    ParseResult ret = m_mcu->m_serial->send_with_response(i2c_read.str(), "i2c_read_response", m_cmd_queue);
}
        
void MCU_I2C::i2c_modify_bits(std::string reg, std::string clear_bits, std::string set_bits, uint64_t minclock, uint64_t reqclock)
{
    std::string clearset = clear_bits + set_bits;
    std::stringstream i2c_modify_bits;
    if (m_i2c_modify_bits_cmd != "")
    {
        // Send setup message via mcu initialization
        std::string reg_msg;
        // reg_msg = "".join(["%02x" % (x,) for x in reg]); //---??---bus  data_msg输入数据需要对齐
        std::string clearset_msg;
        // clearset_msg = "".join(["%02x" % (x,) for x in clearset]) //---??---bus  data_msg输入数据需要对齐
        i2c_modify_bits << "i2c_modify_bits oid=" << m_oid << " reg=" << reg_msg << " clear_set_bits=" << clearset_msg;
        m_mcu->add_config_cmd(i2c_modify_bits.str(), true);
        return;
    }
    i2c_modify_bits << "i2c_modify_bits oid=" << m_oid << " reg=" << reg << " clear_set_bits=" << clearset;  
    m_mcu->m_serial->send(i2c_modify_bits.str(), minclock, reqclock, m_cmd_queue);
} 


// [mcu host]
// serial: /tmp/klipper_host_mcu

// # Using the i2c bus of the RPi to read a sensor
// [temperature_sensor enclosure_temp]
// sensor_type: HTU21D
// i2c_mcu: host
// i2c_bus: i2c.1
// htu21d_hold_master: False

MCU_I2C* MCU_I2C_from_config(std::string section_name, int default_addr, int default_speed)
{
    // Load bus parameters
    // MCU *i2c_mcu = new MCU(Printer::GetInstance()->m_pconfig->GetString(section_name, "i2c_mcu", "mcu"));  //---??---
    MCU *i2c_mcu = get_printer_mcu(Printer::GetInstance(),Printer::GetInstance()->m_pconfig->GetString(section_name, "i2c_mcu", "mcu"));   
    double speed = Printer::GetInstance()->m_pconfig->GetInt(section_name, "i2c_speed", default_speed, 100000);
    std::string bus = Printer::GetInstance()->m_pconfig->GetString(section_name, "i2c_bus");
    int addr = INT32_MIN;
    if (default_addr == INT32_MIN)
    {
        addr = Printer::GetInstance()->m_pconfig->GetInt(section_name, "i2c_address", INT32_MIN, 0, 127);
    }
    else
    {
        addr = Printer::GetInstance()->m_pconfig->GetInt(section_name, "i2c_address", default_addr, 0, 127);
    }
    // Create MCU_I2C object
    MCU_I2C *ret = new MCU_I2C(i2c_mcu, bus, addr, speed); 
    return ret;
}
    


//#####################################################################
// Bus synchronized digital outputs
//#####################################################################

// Helper code for a gpio that updates on a cmd_queue
MCU_bus_digital_out::MCU_bus_digital_out(MCU* mcu, std::string pin_desc, command_queue*cmd_queue, int value)
{
    MCU* m_mcu = mcu;
    int m_oid = mcu->create_oid();
    pinParams* pin_params = Printer::GetInstance()->m_ppins->lookup_pin(pin_desc);
    if (pin_params->chip != mcu)
    {
        std::cout << "Pin " << pin_desc << " must be on mcu " << std::endl;
    }
    std::stringstream config_digital_out;
    config_digital_out << "config_digital_out oid=" << m_oid << " pin=" << pin_params->pin << " value=" << value << " default_value=" << value << " max_duration=" << 0;
    mcu->add_config_cmd(config_digital_out.str());
    m_mcu->register_config_callback(std::bind(&MCU_bus_digital_out::build_config, this, std::placeholders::_1));
    if (cmd_queue != NULL)
    {
        cmd_queue = mcu->alloc_command_queue();
    }
    command_queue* m_cmd_queue = cmd_queue;
    std::string m_update_pin_cmd = "";

}

MCU_bus_digital_out::~MCU_bus_digital_out()
{

}

int MCU_bus_digital_out::get_oid()
{
    return m_oid;
}
        
MCU* MCU_bus_digital_out::get_mcu()
{
    return m_mcu;
}
        
command_queue* MCU_bus_digital_out::get_command_queue()
{
    return m_cmd_queue;
}
        
void MCU_bus_digital_out::build_config(int para)
{
    m_update_pin_cmd = "update_digital_out oid=%c value=%c";
}
        
void MCU_bus_digital_out::update_digital_out(int value, uint64_t minclock, uint64_t reqclock)
{
    std::stringstream update_digital_out;
    if (m_update_pin_cmd != "")
    {
        //Send setup message via mcu initialization
        update_digital_out << "update_digital_out oid=" << m_oid << " value=" << !!value;
        m_mcu->add_config_cmd(update_digital_out.str());
        return;
    }
    update_digital_out << "update_digital_out oid=" << m_oid << " value=" << !!value;   
    m_mcu->m_serial->send(update_digital_out.str(), minclock, reqclock, m_cmd_queue);
}
        
