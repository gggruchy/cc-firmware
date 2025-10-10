#include "ad5206.h"
#include "klippy.h"

ad5206::ad5206(std::string section_name)
{
    m_spi = MCU_SPI_from_config(section_name, 0, "enable_pin", 25000000);
    double scale = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "scale" , 1., DBL_MIN, DBL_MAX, 0.);
    for (int i = 0; i < 6; i++)
    {
        double val = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "channel_" + std::to_string(i+1), DBL_MIN, 0., scale);
        if (val != DBL_MIN)
            set_register(i, (int)(val * 256. / scale + .5));
    }
}

ad5206::~ad5206()
{

}
        
void ad5206::set_register(uint8_t reg, uint8_t value)
{
    std::vector<uint8_t> data = {reg, value};
    m_spi->spi_send(data);
}
        
