#ifndef BUS_H
#define BUS_H

#include <string>
#include "string.h"
#include <functional>

#include "mcu.h"
#include "pins.h"

extern "C"
{
    #include "../chelper/stepcompress.h"
    #include "../chelper/serialqueue.h"
}


class MCU_SPI{
    private:

    public:
        MCU_SPI(MCU* mcu, std::string bus, std::string pin, int mode, double speed, std::vector<std::string> sw_pins = std::vector<std::string>());
        ~MCU_SPI();

        MCU*m_mcu;
        std::string m_bus;
        int m_pin;
        // Config SPI object (set all CS pins high before spi_set_bus commands)
        int m_oid;
        std::stringstream m_config_fmt;
        command_queue* m_cmd_queue;
        std::string m_spi_send_cmd;
        std::string m_spi_transfer_cmd;
        std::string m_spi_transfer_response_cmd;
        

    public:
        void setup_shutdown_msg(int shutdown_seq);
        int get_oid();
        MCU* get_mcu();
        command_queue *get_command_queue();
        void build_config(int para);
        void spi_send(std::vector<uint8_t> data, uint64_t minclock=0, uint64_t reqclock=0);
        ParseResult spi_transfer(std::vector<uint8_t> data, uint64_t minclock=0, uint64_t reqclock=0);
        void spi_transfer_with_preface(std::string preface_data, std::string data, uint64_t minclock=0, uint64_t reqclock=0);
};

class MCU_I2C{
    private:

    public:
        MCU_I2C(MCU *mcu, std::string bus, int addr, int speed);
        ~MCU_I2C();

        MCU* m_mcu;
        std::string m_bus;
        int m_i2c_address;
        int m_oid;
        std::stringstream m_config_fmt;
        command_queue* m_cmd_queue;
        std::string m_i2c_write_cmd;
        std::string m_i2c_read_cmd;
        std::string m_i2c_read_response_cmd;
        std::string m_i2c_modify_bits_cmd;

    public:

        int get_oid();
        MCU* get_mcu();   
        int get_i2c_address();  
        command_queue* get_command_queue(); 
        void build_config(int para);
        void i2c_write(std::vector<uint8_t> data, uint64_t minclock=0, uint64_t reqclock=0); 
        ParseResult i2c_read(std::string write, int read_len);
        void i2c_modify_bits(std::string reg, std::string clear_bits, std::string set_bits, uint64_t minclock=0, uint64_t reqclock=0);
};

class MCU_bus_digital_out{
    private:

    public:
        MCU_bus_digital_out(MCU* mcu, std::string pin_desc, command_queue* cmd_queue=NULL, int value=0);
        ~MCU_bus_digital_out();

        MCU* m_mcu;
        int m_oid;
        command_queue* m_cmd_queue;
        std::string m_update_pin_cmd;

    public:

        int get_oid();  
        MCU* get_mcu();    
        command_queue* get_command_queue();      
        void build_config(int para);    
        void update_digital_out(int value, uint64_t minclock=0, uint64_t reqclock=0);
};

std::string resolve_bus_name(MCU *mcu, std::string param, std::string bus);
MCU_SPI * MCU_SPI_from_config(std::string sectoin_name, int mode, std::string pin_option="cs_pin", int default_speed=100000, std::string share_type="");
MCU_I2C* MCU_I2C_from_config(std::string section_name, int default_add = INT32_MIN, int default_speed=100000);
#endif