#include <stdint.h>
#include <stdio.h>
#include "main.h"


hmem_t *h = (hmem_t *)HMEM_PHY_BASE;

void osl_putc(char c)
{
    *((char *)SERIAL_PORT)  = c;
}

int enable_irq()
{
    asm volatile("mrs r0, cpsr_all");
    asm volatile("bic r0, r0, #0xc0");  //enable IRQ FIQ
    asm volatile("msr cpsr_all, r0");	
    return 0;
}

int disable_irq()
{
    asm volatile("mrs r0, cpsr_all");
    asm volatile("orr r0, r0, #0xc0");  //enable IRQ FIQ
    asm volatile("msr cpsr_all, r0");	
    return 0;
}

void timer_set(bool enable, uint32_t period_ms)
{
    h->timer_en = enable;
    h->timer_period_ms = period_ms;
}



void irq_init()
{
    
}

uint32_t volatile get_ms()
{
    return (volatile uint32_t)h->time_ms_tick;
}

void delay_ms(uint32_t ms)
{
    uint32_t s = get_ms();
    while(get_ms() - s < ms )
    {
        ;
    }
}

void __attribute__((target("arm"))) thumbtest()
{

    uint32_t start = get_ms();
    for(;;)
    {
        delay_ms(900);
        printf("test loop\n");
    }
}
volatile void switch_mode(int mode) __attribute__((naked));
int main()
{

    switch_mode(USER_MODE);

    __asm volatile("mov r13,#0x02300000");
    __asm volatile("add r13,#0x000FA000");

    printf("test1 " __TIME__ "\n");
    //irq_init();
    //timer_init();


    //enable_irq();


    void (*f)();
    f = (void (*)())(0x00100000 + 4 * 4 );
    ((uint32_t *)0x00100000)[2] = 1;
    f();


/*
    h->timer_period_ms = 10;
    h->timer_en = 1;
    enable_irq();
*/


    //thumbtest();

    for(;;){
        
        //printf("%d, %d, %d\n", h->time_ms_tick, h->time_rtc_s, h->time_us_tick);
        //printf("%ld\n", PXA255_TIMR_OSCR);
    }
}




