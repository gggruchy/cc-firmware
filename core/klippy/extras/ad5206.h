#ifndef AD5206_H
#define AD5206_H

#include "bus.h"

class ad5206{
    private:

    public:
        ad5206(std::string section_name);
        ~ad5206();

        MCU_SPI *m_spi;
        void set_register(uint8_t reg, uint8_t value);
};
#endif