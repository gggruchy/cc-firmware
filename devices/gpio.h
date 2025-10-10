#ifndef HAL_GPIO_H
#define HAL_GPIO_H
#ifdef __cplusplus
extern "C"
{
#endif
#define R528_PA_BASE 0
#define R528_PB_BASE 32
#define R528_PC_BASE 64
#define R528_PD_BASE 96
#define R528_PE_BASE 128
#define R528_PF_BASE 160
#define R528_PG_BASE 192
#define R528_PH_BASE 224
#define R528_PI_BASE 256
#define R528_PJ_BASE 288
#define R528_PK_BASE 320
#define R528_PL_BASE 352
#define R528_PM_BASE 384
#define R528_PN_BASE 416
#define R528_PO_BASE 448

/* r528 gpio name space */
#define GPIOA(n) (R528_PA_BASE + (n))
#define GPIOB(n) (R528_PB_BASE + (n))
#define GPIOC(n) (R528_PC_BASE + (n))
#define GPIOD(n) (R528_PD_BASE + (n))
#define GPIOE(n) (R528_PE_BASE + (n))
#define GPIOF(n) (R528_PF_BASE + (n))
#define GPIOG(n) (R528_PG_BASE + (n))
#define GPIOH(n) (R528_PH_BASE + (n))
#define GPIOI(n) (R528_PI_BASE + (n))
#define GPIOJ(n) (R528_PJ_BASE + (n))
#define GPIOK(n) (R528_PK_BASE + (n))
#define GPIOL(n) (R528_PL_BASE + (n))
#define GPIOM(n) (R528_PM_BASE + (n))
#define GPION(n) (R528_PN_BASE + (n))
#define GPIOO(n) (R528_PO_BASE + (n))
    typedef enum
    {
        GPIO_LOW = 0,
        GPIO_HIGH,
    } gpio_state_t;

    typedef enum
    {
        GPIO_INPUT = 0,
        GPIO_OUTPUT,
    } gpio_direction_t;

    int gpio_init(int index);
    int gpio_deinit(int index);
    int gpio_set_direction(int index, gpio_direction_t direction);
    int gpio_get_direction(int index);
    int gpio_set_value(int index, gpio_state_t value);
    int gpio_get_value(int index);
    int gpio_is_init(int index);

#define MATERIAL_BREAK_DETECTION_GPIO GPIOC(0)
#define MATERIAL_BREAK_DETECTION_TRIGGER_LEVEL GPIO_LOW

#if ENABLE_MANUTEST
#define STEPPERZ_DETECTION_GPIO GPIOG(8)
#define STEPPERZ_DETECTION_TRIGGER_LEVEL GPIO_LOW
#endif

#define BREAK_DETECTION_GPIO GPIOG(0) 
#define BREAK_DETECTION_TRIGGER_LEVEL GPIO_HIGH

#define BREAK_CONTROL_GPIO GPIOG(11)  /* Power capacity output enable control: high->enable low->disable */
#define BREAK_CONTROL_TRIGGER_LEVEL GPIO_HIGH

#define LIGHT_BAR_CONTROL_GPIO GPIOG(15)  /* Power capacity output enable control: high->enable low->disable */
#define LIGHT_BAR_CONTROL_TRIGGER_LEVEL GPIO_HIGH

#define CC_EN_GPIO GPIOC(1) /* Power capacity charging enable control: high->enable low->disable */
#ifdef __cplusplus
}
#endif
#endif /* HAL_GPIO_H */
