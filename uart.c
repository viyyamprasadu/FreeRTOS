/* Polled UART0 driver for the LM3S6965.
 * In QEMU, UART0 is connected to the terminal with "-serial stdio"
 * (or "-nographic").
 */
#include <stdint.h>
#include "uart.h"

#define UART0_BASE   0x4000C000UL
#define UART0_DR     (*(volatile uint32_t *)(UART0_BASE + 0x000))
#define UART0_FR     (*(volatile uint32_t *)(UART0_BASE + 0x018))
#define UART0_CTL    (*(volatile uint32_t *)(UART0_BASE + 0x030))
#define UART_FR_TXFF (1u << 5)   /* transmit FIFO full */

void uart_init(void)
{
    /* On real silicon you would also enable the UART0/GPIOA clocks in
     * SYSCTL_RCGC1/RCGC2 and set the baud rate. QEMU does not need this. */
    UART0_CTL |= (1u << 0) | (1u << 8);  /* UARTEN | TXE */
}

void uart_putc(char c)
{
    while (UART0_FR & UART_FR_TXFF) { }
    UART0_DR = (uint32_t)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_putu(unsigned int v)
{
    char buf[11];
    int i = 0;
    do { buf[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i--) uart_putc(buf[i]);
}
