#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_putu(unsigned int v);
void uart_rx_int_enable(void);
void UART0_IRQHandler(void);
#endif
