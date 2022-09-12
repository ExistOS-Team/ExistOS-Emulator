#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define INFO(...) do{printf(__VA_ARGS__);}while(0)

#define MEMORY_BASE     (0)
#define MEMORY_SIZE     (512*1024)

#define TIMER_TICK_PERIOD   (100)

#define ABT_STACK_ADDR      (MEMORY_BASE + MEMORY_SIZE - 4)
#define UND_STACK_ADDR      (ABT_STACK_ADDR - 0x1000)
#define FIQ_STACK_ADDR      (UND_STACK_ADDR - 0x1000)
#define IRQ_STACK_ADDR      (FIQ_STACK_ADDR - 0x1000)
#define SVC_STACK_ADDR      (IRQ_STACK_ADDR - 0x1000)
#define SYS_STACK_ADDR      (SVC_STACK_ADDR - 0x1000)

#define HEAP_END   (SYS_STACK_ADDR - 0x1000)

#define USER_MODE 0x10
#define FIQ_MODE 0x11
#define IRQ_MODE 0x12
#define SVC_MODE 0x13
#define ABT_MODE 0x17
#define UND_MODE 0x1b
#define SYS_MODE 0x1f



typedef struct hmem_t
{
    volatile uint32_t time_us_tick;
    volatile uint32_t time_ms_tick;
    volatile uint32_t time_rtc_s;
    volatile uint32_t timer_en;
    volatile uint32_t timer_period_ms;
    volatile uint32_t key_status;
    volatile uint32_t irq_pending;
    volatile uint32_t flash_sync;
} hmem_t;


#define HMEM_PHY_BASE (0x60000000)

#define SERIAL_PORT     (HMEM_PHY_BASE + 50 * 4)

#define VRAM_PHY_BASE (0x05000000)
#define MTD_PHY_BASE (0x10000000)

#define FLASH_PAGE_SIZE     (512)

int enable_irq();
int disable_irq();
void timer_set(bool enable, uint32_t period_ms);

