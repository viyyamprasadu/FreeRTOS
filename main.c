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

static QueueHandle_t     xQueue;
static SemaphoreHandle_t xUartMutex;

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
        vTaskDelay(pdMS_TO_TICKS(500));
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
        vTaskDelayUntil(&last, pdMS_TO_TICKS(2000));
        print_line("Heartbeat #", ++beats);
    }
}

int main(void)
{
    uart_init();
    uart_puts("\nFreeRTOS " tskKERNEL_VERSION_NUMBER " on LM3S6965 (QEMU)\n");

    xQueue     = xQueueCreate(5, sizeof(unsigned int));
    xUartMutex = xSemaphoreCreateMutex();
    configASSERT(xQueue && xUartMutex);

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
