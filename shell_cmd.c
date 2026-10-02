/* Stage 2: a small command shell.
 *
 *   line  ->  split into words (argc/argv)  ->  look up argv[0] in a table
 *         ->  call that command's function
 */
#include <string.h>
#include "shell_cmd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"

#define SHELL_MAX_ARGS 8

typedef void (*cmd_fn_t)(int argc, char *argv[]);

typedef struct {
    const char *name;     /* what the user types          */
    const char *help;     /* one-line description         */
    cmd_fn_t    fn;       /* function that does the work  */
} shell_cmd_t;

/* ---- command functions (defined below the table) ---- */
static void cmd_help(int argc, char *argv[]);
static void cmd_echo(int argc, char *argv[]);
static void cmd_uptime(int argc, char *argv[]);
static void cmd_heap(int argc, char *argv[]);
static void cmd_tasks(int argc, char *argv[]);
static void cmd_suspend(int argc, char *argv[]);
static void cmd_resume(int argc, char *argv[]);

/* ---- the command table: to add a command, add one line here ---- */
static const shell_cmd_t commands[] = {
    { "help", "list all commands",          cmd_help },
    { "echo", "echo <words>: print them back", cmd_echo },
    { "uptime", "time since boot", cmd_uptime },
    { "heap", "FreeRTOS heap usage", cmd_heap },
    { "tasks", "list tasks : state,priority, free stack", cmd_tasks },
    { "suspend", "suspend <task>: stop a task", cmd_suspend },
    { "resume", "resume <task>: let it run again", cmd_resume },
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* Split "echo  hello world" into argv[0]="echo", argv[1]="hello", argv[2]="world".
 * It does this in place: each space after a word is overwritten with '\0',
 * and argv[] just points into the original buffer. No copying, no malloc. */
static int split_args(char *line, char *argv[], int max_args)
{
    int argc = 0;

    while (argc < max_args) {
        while (*line == ' ') line++;            /* skip spaces before a word */
        if (*line == '\0') break;               /* end of line               */

        argv[argc++] = line;                    /* a word starts here        */

        while (*line != '\0' && *line != ' ')   /* walk to the end of it     */
            line++;
        if (*line == ' ')
            *line++ = '\0';                     /* cut the word off          */
    }
    return argc;
}

void shell_execute(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = split_args(line, argv, SHELL_MAX_ARGS);

    if (argc == 0)
        return;                                 /* empty line: do nothing */

    for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);         /* call through the pointer */
            return;
        }
    }

    uart_puts("Unknown command: ");
    uart_puts(argv[0]);
    uart_puts("  (type 'help')\n");
}

/* ------------------------------------------------------------------ */

static void cmd_help(int argc, char *argv[])
{
    (void)argc; (void)argv;
    for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
        uart_puts("  ");
        uart_puts(commands[i].name);
        uart_puts(" - ");
        uart_puts(commands[i].help);
        uart_puts("\n");
    }
}

static void cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {            /* argv[0] is "echo" itself */
        uart_puts(argv[i]);
        uart_puts(i < argc - 1 ? " " : "");
    }
    uart_puts("\n");
}

static void cmd_uptime(int argc, char *argv[])
{
    (void)argc; (void)argv;
    TickType_t   ticks = xTaskGetTickCount();
    unsigned int ms    = (unsigned int)(ticks * portTICK_PERIOD_MS);

    uart_puts("Up ");
    uart_putu(ms / 1000);
    uart_puts(" s ");
    uart_putu(ms % 1000);
    uart_puts(" ms  (");
    uart_putu((unsigned int)ticks);
    uart_puts(" ticks)\n");
}

static void cmd_heap(int argc, char *argv[])
{
    (void)argc; (void)argv;
    uart_puts("Heap total   : ");
    uart_putu(configTOTAL_HEAP_SIZE);
    uart_puts(" bytes\nFree now     : ");
    uart_putu((unsigned int)xPortGetFreeHeapSize());
    uart_puts(" bytes\nLowest ever  : ");
    uart_putu((unsigned int)xPortGetMinimumEverFreeHeapSize());
    uart_puts(" bytes\n");
}

/* Print a string, then spaces up to 'width' columns, so tables line up. */
static void put_padded(const char *s, unsigned int width)
{
    unsigned int n = 0;
    while (*s) { uart_putc(*s++); n++; }
    while (n++ < width) uart_putc(' ');
}

#define SHELL_MAX_TASKS 10

static void cmd_tasks(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* static: about 360 bytes, too big to put on this task's 1 KB stack.
     * Safe here because only the shell task ever calls this function. */
    static TaskStatus_t status[SHELL_MAX_TASKS];
    static const char * const state_name[] = {
        "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"
    };

    UBaseType_t n = uxTaskGetSystemState(status, SHELL_MAX_TASKS, NULL);

    put_padded("Name", 12); put_padded("State", 11);
    put_padded("Prio", 6);  uart_puts("Stack free (words)\n");

    for (UBaseType_t i = 0; i < n; i++) {
        put_padded(status[i].pcTaskName, 12);
        put_padded(state_name[status[i].eCurrentState], 11);
        uart_putu((unsigned int)status[i].uxCurrentPriority);
        put_padded("", 5);
        uart_putu((unsigned int)status[i].usStackHighWaterMark);
        uart_puts("\n");
    }
}

/* Shared by suspend/resume: check the argument and look the task up by name.
 * Returns NULL (after printing why) if the command should not go ahead. */
static TaskHandle_t find_task(int argc, char *argv[])
{
    if (argc != 2) {
        uart_puts("Usage: ");
        uart_puts(argv[0]);
        uart_puts(" <task name>\n");
        return NULL;
    }

    TaskHandle_t h = xTaskGetHandle(argv[1]);
    if (h == NULL) {
        uart_puts("No task called '");
        uart_puts(argv[1]);
        uart_puts("'  (names are case-sensitive, see 'tasks')\n");
    }
    return h;
}

static void cmd_suspend(int argc, char *argv[])
{
    TaskHandle_t h = find_task(argc, argv);
    if (h == NULL)
        return;

    /* Two tasks we must never suspend:
     * - ourselves: we hold the UART mutex, so every other task that prints
     *   would wait for it forever, and nobody is left to type 'resume'.
     * - IDLE: the scheduler needs one task that is always able to run. */
    if (h == xTaskGetCurrentTaskHandle() || strcmp(argv[1], "IDLE") == 0) {
        uart_puts("Refusing to suspend '");
        uart_puts(argv[1]);
        uart_puts("'\n");
        return;
    }

    vTaskSuspend(h);
    uart_puts("Suspended ");
    uart_puts(argv[1]);
    uart_puts("\n");
}

static void cmd_resume(int argc, char *argv[])
{
    TaskHandle_t h = find_task(argc, argv);
    if (h == NULL)
        return;

    vTaskResume(h);
    uart_puts("Resumed ");
    uart_puts(argv[1]);
    uart_puts("\n");
}
