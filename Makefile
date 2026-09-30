# FreeRTOS on TI Stellaris LM3S6965 (Cortex-M3), run in QEMU
#   make          build build/firmware.elf
#   make run      run in QEMU (exit: Ctrl+A then X)
#   make debug    run in QEMU paused, waiting for GDB on port 1234
#   make gdb      connect gdb-multiarch to the QEMU started by 'make debug'
#   make clean

TARGET   = firmware
BUILD    = build
KERNEL   = FreeRTOS-Kernel
PORT     = $(KERNEL)/portable/GCC/ARM_CM3

CC       = arm-none-eabi-gcc
SIZE     = arm-none-eabi-size
GDB     ?= gdb-multiarch
QEMU     = qemu-system-arm

SRCS = main.c uart.c startup.c \
       $(KERNEL)/tasks.c $(KERNEL)/queue.c $(KERNEL)/list.c \
       $(KERNEL)/timers.c $(KERNEL)/event_groups.c $(KERNEL)/stream_buffer.c \
       $(PORT)/port.c \
       $(KERNEL)/portable/MemMang/heap_4.c

INCS = -I. -I$(KERNEL)/include -I$(PORT)

CFLAGS  = -mcpu=cortex-m3 -mthumb -O0 -g3 -Wall -Wextra \
          -ffunction-sections -fdata-sections -ffreestanding $(INCS)
LDFLAGS = -mcpu=cortex-m3 -mthumb -T lm3s6965.ld -nostartfiles \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
          --specs=nano.specs --specs=nosys.specs

OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o))

QEMU_FLAGS = -M lm3s6965evb -nographic -kernel $(BUILD)/$(TARGET).elf

all: $(BUILD)/$(TARGET).elf

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	$(SIZE) $@

$(BUILD)/%.o: %.c FreeRTOSConfig.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	$(QEMU) $(QEMU_FLAGS)

debug: all
	$(QEMU) $(QEMU_FLAGS) -S -gdb tcp::1234

gdb: all
	$(GDB) $(BUILD)/$(TARGET).elf -ex "target remote :1234" -ex "break main" -ex "continue"

clean:
	rm -rf $(BUILD)

.PHONY: all run debug gdb clean
