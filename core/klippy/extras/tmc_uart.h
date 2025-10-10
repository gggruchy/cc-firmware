#ifndef TMC_UART_H
#define TMC_UART_H
#include "mcu.h"
#include "tmc.h"
class MCU_analog_mux
{
private:
    
public:
    MCU_analog_mux(MCU *mcu,command_queue * cmd_queue, std::string select_pins_desc);
    ~MCU_analog_mux();
    void build_config(int para);
    std::vector<int> get_instance_id(std::string select_pins_desc);
    void activate(  std::vector<int> instance_id);

    MCU *m_mcu;
    struct command_queue *m_cq;
    CommandWrapper*update_pin_cmd;
    std::vector<std::string> m_pins ;
    std::vector<int> m_oids ;
     std::vector<int> pin_values ;
};

class PrinterTMCUartMutexes
{
private:
    
public:
    PrinterTMCUartMutexes();
    ~PrinterTMCUartMutexes();
     std::map<MCU *, ReactorMutex*> mcu_to_mutex;
};

class MCU_TMC_uart_bitbang
{
private:
public:
    MCU_TMC_uart_bitbang( pinParams* rx_pin_params,pinParams* tx_pin_params, std::string select_pins_desc);
    ~MCU_TMC_uart_bitbang();
    void build_config(int para);

    std::vector<int>  register_instance( pinParams* rx_pin_params, pinParams* tx_pin_params,std::string select_pins_desc, uint8_t addr);
    uint8_t _calc_crc8(  uint16_t* data,uint8_t size );
    void _add_serial_bits(  uint16_t* data,uint8_t size);
    void _encode_read(uint8_t* data, uint8_t sync, uint8_t addr, uint8_t reg);
    void _encode_write(   uint8_t* data,uint8_t sync, uint8_t addr, uint8_t reg,uint32_t val);
    uint32_t _decode_read( uint8_t reg, uint8_t* data, uint8_t size);
    uint32_t reg_read( std::vector<int> instance_id, uint8_t addr, uint8_t reg);
    void reg_write(std::vector<int> instance_id,uint8_t addr,uint8_t reg, uint32_t val, double print_time = DBL_MAX);

    ReactorMutex*mutex;
    MCU *m_mcu;
    std::string rx_pin;
    std::string tx_pin;
    int pullup;
    int m_oid;
    struct command_queue *m_cmd_queue;
    MCU_analog_mux *analog_mux;
    std::map<std::vector<int>, bool> instances ;
};
class MCU_TMC_uart : public MCU_TMC
{
    private:
    public:
    MCU_TMC_uart( std::string section_name, std::map<std::string, uint8_t> name_to_reg_i, FieldHelper * fields, uint8_t max_addr=0);
    ~MCU_TMC_uart(){};

    uint32_t _do_get_register(std::string reg_name);
    uint32_t get_register(std::string reg_name);
    void set_register(std::string reg_name, uint32_t val, double print_time = DBL_MAX);
 
    MCU_TMC_uart_bitbang * m_mcu_uart ;
};




#endif
