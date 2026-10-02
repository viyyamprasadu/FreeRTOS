/* FreeRTOS demo for LM3S6965 on QEMU.
 *
 * - SenderTask   : every 500 ms puts an incrementing counter into a queue
 * - ReceiverTask : blocks on the queue and prints each value on UART0
 * - HeartbeatTask: prints a heartbeat every 2 s using vTaskDelayUntil
 * A mutex protects the UART so lines from different tasks never interleave.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "uart.h"

#define RX_LINE_MAX 64
static QueueHandle_t     xQueue;
static SemaphoreHandle_t xUartMutex;
QueueHandle_t xUartRxQueue;

static void print_line(const char *prefix, unsigned int value)
{
    xSemaphoreTake(xUartMutex, portMAX_DELAY);
    uart_puts("[");
    uart_putu((unsigned int)xTaskGetTickCount());
    uart_puts(" ms] ");
    uart_puts(prefix);
    uart_putu(value);
    uart_puts("\n");
    xSemaphoreGive(xUartMutex);
}

static void vSenderTask(void *pv)
{
    (void)pv;
    unsigned int count = 0;
    for (;;) {
        xQueueSend(xQueue, &count, portMAX_DELAY);
        count++;
        vTaskDelay(pdMS_TO_TICKS(5000));
	print_line("Sender sent: ", count);
    }
}

static void vReceiverTask(void *pv)
{
    (void)pv;
    unsigned int value;
    for (;;) {
        if (xQueueReceive(xQueue, &value, portMAX_DELAY) == pdPASS)
            print_line("Receiver got: ", value);
    }
}

static void vHeartbeatTask(void *pv)
{
    (void)pv;
    TickType_t last = xTaskGetTickCount();
    unsigned int beats = 0;
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10000));
        print_line("Heartbeat #", ++beats);
    }
}

//static void vUartRxTask(void *pv)
//{
//	(void)pv;
//	char c;
//	for(;;){
//		xQueueReceive(xUartRxQueue, &c, portMAX_DELAY);
//		xSemaphoreTake(xUartMutex, portMAX_DELAY);
//		uart_puts("RX task got '");
//		uart_putc(c);
//		uart_puts("'\n");
//		xSemaphoreGive(xUartMutex);
//	}
//}

/* Step 4: line editor. Blocks on the RX queue (0% CPU while waiting),
 * echoes each character, handles backspace, and prints the whole line
 * when Enter is pressed. */
static void vUartRxTask(void *pv)
{
    (void)pv;
    char c;
    char line[RX_LINE_MAX];
    unsigned int len = 0;

    xSemaphoreTake(xUartMutex, portMAX_DELAY);
    uart_puts("> ");
    xSemaphoreGive(xUartMutex);

    for (;;) {
        xQueueReceive(xUartRxQueue, &c, portMAX_DELAY);

        xSemaphoreTake(xUartMutex, portMAX_DELAY);
        if (c == '\r' || c == '\n') {                        /* Enter */
            line[len] = '\0';
            uart_puts("\nYou typed (");
            uart_putu(len);
            uart_puts(" chars): ");
            uart_puts(line);
            uart_puts("\n> ");
            len = 0;
        } else if (c == '\b' || c == 0x7F) {                 /* Backspace */
            if (len > 0) {
                len--;
                uart_puts("\b \b");                          /* erase on screen */
            }
        } else if (c >= ' ' && c < 0x7F && len < RX_LINE_MAX - 1) {
            line[len++] = c;                                  /* printable: store */
            uart_putc(c);                                     /* and echo         */
        }
        /* anything else (control chars, or line full) is ignored */
        xSemaphoreGive(xUartMutex);
    }
}

int main(void)
{
    uart_init();
    uart_puts("\nFreeRTOS " tskKERNEL_VERSION_NUMBER " on LM3S6965 (QEMU)\n");

    xQueue     = xQueueCreate(5, sizeof(unsigned int));
    xUartMutex = xSemaphoreCreateMutex();
    //configASSERT(xQueue && xUartMutex);
    //uart_rx_int_enable();

    xUartRxQueue = xQueueCreate(32,sizeof(char));
    configASSERT(xQueue && xUartMutex && xUartRxQueue);

    uart_rx_int_enable();   /* Step 2 - must come AFTER the queue exists */

    xTaskCreate(vUartRxTask, "UartRx", configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);  /* Step 3 */
    xTaskCreate(vSenderTask,    "Sender",    configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);
    xTaskCreate(vReceiverTask,  "Receiver",  configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
    xTaskCreate(vHeartbeatTask, "Heartbeat", configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);

    vTaskStartScheduler();   /* never returns unless out of heap */

    uart_puts("Scheduler failed to start!\n");
    for (;;) { }
}

/* ---- FreeRTOS hooks ---- */
void vApplicationMallocFailedHook(void)
{
    uart_puts("!! malloc failed\n");
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    uart_puts("!! stack overflow in task: ");
    uart_puts(pcTaskName);
    uart_puts("\n");
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
