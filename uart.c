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

/* ---- Step 2: RX interrupt registers ---- */
#define UART0_IM     (*(volatile uint32_t *)(UART0_BASE + 0x038))  /* interrupt mask          */
#define UART0_MIS    (*(volatile uint32_t *)(UART0_BASE + 0x040))  /* masked interrupt status */
#define UART0_ICR    (*(volatile uint32_t *)(UART0_BASE + 0x044))  /* interrupt clear         */
#define UART_FR_RXFE (1u << 4)   /* receive FIFO empty         */
#define UART_INT_RX  (1u << 4)   /* receive interrupt          */
#define UART_INT_RT  (1u << 6)   /* receive timeout interrupt  */

/* NVIC registers (Cortex-M3 core, same on every Cortex-M3 chip) */
#define NVIC_ISER0   (*(volatile uint32_t *)0xE000E100UL)  /* set-enable, IRQ 0-31      */
#define NVIC_IPR     ((volatile uint8_t  *)0xE000E400UL)   /* one priority byte per IRQ */
#define UART0_IRQn   5           /* LM3S6965 datasheet: UART0 = interrupt 5 */

/* Must be numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5),
 * because the ISR will call FreeRTOS "FromISR" APIs in Step 3. */
#define UART0_IRQ_PRIORITY  6
//#define UART0_IRQ_PRIORITY  0

#include "FreeRTOS.h"   /* for configPRIO_BITS */
#include "queue.h"      /* Step 3: xQueueSendFromISR */

extern QueueHandle_t xUartRxQueue;   /* Step 3: created in main.c */

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

void uart_rx_int_enable(void)
{
    UART0_ICR  = UART_INT_RX | UART_INT_RT;            /* 1. clear anything stale    */
    UART0_IM  |= UART_INT_RX | UART_INT_RT;            /* 2. unmask RX in the UART   */
    NVIC_IPR[UART0_IRQn] =                             /* 3. set priority in NVIC    */
        (uint8_t)(UART0_IRQ_PRIORITY << (8 - configPRIO_BITS));
    NVIC_ISER0 = (1u << UART0_IRQn);                   /* 4. enable IRQ in the NVIC  */
}

/* TEMPORARY test ISR for Step 2 - replaced by the real one in Step 3.
 * Echoes each received char wrapped in [ ] so you can see the ISR ran. */
void UART0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    UART0_ICR = UART0_MIS;                     /* acknowledge, or it re-fires forever */
    
    while (!(UART0_FR & UART_FR_RXFE)) {       /* drain everything received */
        char c = (char)(UART0_DR & 0xFF);
	xQueueSendFromISR(xUartRxQueue, &c, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
