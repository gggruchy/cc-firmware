/**
 * @file usb_uart.h
 * @author 
 * @brief 
 * @version 0.1
 * @date 2023-12-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef _USB_UART_H_
#define _USB_UART_H_

#include<stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void usb_uart_config(char* path, int baud_rate);
void usb_uart_recvdats(uint8_t *pBuf,int datalen);
void usb_uart_buf_clear();

#ifdef __cplusplus
}
#endif

#endif /*_USB_UART_H_*/

