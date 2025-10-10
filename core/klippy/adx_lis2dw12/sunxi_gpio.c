#include "sunxi_gpio.h"

void cs0_Init()
{
  system(CS0_EXPORT_GPIO);
  system(CS0_GPIO_OUTPUT);
}

void cs1_Init()
{
  system(CS1_EXPORT_GPIO);
  system(CS1_GPIO_OUTPUT);
}

void cs0_h()
{
  system(CS0_GPIO_VAL_H);
}

void cs0_l()
{
  system(CS0_GPIO_VAL_L);
}

void cs1_h()
{
  system(CS1_GPIO_VAL_H);
}

void cs1_l()
{
  system(CS1_GPIO_VAL_L);
}

int spi_cs_init() 
{ 
    cs0_Init();
    cs1_Init();
    // while(1)
    // {
    //     write(fd, SYSFS_GPIO_VAL_H, sizeof(SYSFS_GPIO_VAL_H));
    //     usleep(1000000);
    //     write(fd, SYSFS_GPIO_VAL_H, sizeof(SYSFS_GPIO_VAL_H));
    //     usleep(1000000);
    // }

    return 0;

}  