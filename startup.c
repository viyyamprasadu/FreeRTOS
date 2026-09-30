/* Minimal startup code and vector table for the LM3S6965 (Cortex-M3). */
#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);

/* FreeRTOS port handlers (FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c) */
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

void Reset_Handler(void);
void Default_Handler(void);
void HardFault_Handler(void);

__attribute__((section(".isr_vector"), used))
void (* const g_vectors[])(void) =
{
    (void (*)(void))&_estack,   /*  0: Initial stack pointer */
    Reset_Handler,              /*  1: Reset                 */
    Default_Handler,            /*  2: NMI                   */
    HardFault_Handler,          /*  3: HardFault             */
    Default_Handler,            /*  4: MemManage             */
    Default_Handler,            /*  5: BusFault              */
    Default_Handler,            /*  6: UsageFault            */
    0, 0, 0, 0,                 /*  7-10: Reserved           */
    vPortSVCHandler,            /* 11: SVCall  -> FreeRTOS   */
    Default_Handler,            /* 12: Debug monitor         */
    0,                          /* 13: Reserved              */
    xPortPendSVHandler,         /* 14: PendSV  -> FreeRTOS   */
    xPortSysTickHandler,        /* 15: SysTick -> FreeRTOS   */
    /* 16+: peripheral IRQs (GPIO, UART0, ...) - add here when needed */
};

void Reset_Handler(void)
{
    /* Copy initialised data from flash to RAM */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; )
        *dst++ = *src++;

    /* Zero the .bss section */
    for (uint32_t *dst = &_sbss; dst < &_ebss; )
        *dst++ = 0;

    main();
    for (;;) { }
}

void HardFault_Handler(void)
{
    for (;;) { }   /* put a breakpoint here when debugging with GDB */
}

void Default_Handler(void)
{
    for (;;) { }
}
