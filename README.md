# FreeRTOS on LM3S6965 (QEMU)

A minimal FreeRTOS V11.1.0 project for the TI Stellaris LM3S6965 (Cortex-M3), run on QEMU's `lm3s6965evb` machine.

## One-time setup (Ubuntu/Debian)

    sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi qemu-system-arm gdb-multiarch make git

(Fedora: `sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-newlib qemu-system-arm gdb make git`)

The kernel is included in `FreeRTOS-Kernel/`. To fetch it again:

    git clone --depth 1 --branch V11.1.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git

## Build and run

    make          # builds build/firmware.elf
    make run      # runs in QEMU; exit with Ctrl+A, then X

Expected output:

    FreeRTOS V11.1.0 on LM3S6965 (QEMU)
    [2 ms] Receiver got: 0
    [503 ms] Receiver got: 1
    ...
    [2002 ms] Heartbeat #1

QEMU also prints "Timer with period zero, disabling" at startup. That message comes from QEMU's Stellaris model and is harmless.

## Debug with GDB (two terminals)

    make debug    # terminal 1: QEMU starts paused, waiting on port 1234
    make gdb      # terminal 2: connects, breaks at main

## Files

| File | Purpose |
|---|---|
| `main.c` | Tasks: sender -> queue -> receiver, heartbeat, UART mutex |
| `FreeRTOSConfig.h` | Kernel config (12 MHz, 1 kHz tick, 32 KB heap, 3 priority bits) |
| `startup.c` | Vector table and Reset_Handler; SVC/PendSV/SysTick go to FreeRTOS |
| `lm3s6965.ld` | Linker script: 256 KB flash @ 0x0, 64 KB RAM @ 0x20000000 |
| `uart.c/.h` | Polled UART0 output (shown on your terminal by QEMU) |
| `Makefile` | Build, run, debug targets |

## Next steps

- To add a peripheral interrupt, put its handler at vector index 16 + IRQ number in `startup.c`. Give it a priority value of 5 or higher (numerically). It may only call `...FromISR` APIs.
- To try a software timer, use `xTimerCreate` (`configUSE_TIMERS` is already enabled).
