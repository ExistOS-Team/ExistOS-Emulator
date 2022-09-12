#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "llapi_code.h"

uint32_t IRQ_STACK;
uint32_t IRQ_VECTOR;
uint32_t SVC_STACK;
uint32_t SVC_VECTOR;

uint32_t vm_temp_storage[16];
volatile uint32_t vm_saved_context[17 * 8]; //{CPSR, R0-R15}
volatile uint32_t context_ptr = 0;

volatile bool vm_in_exception = false;

bool softint = false;
extern hmem_t *h;
uint64_t pass_tick_ms = 0;

uint32_t timer_period_ms;

uint32_t
    __attribute__((naked))
    _get_sp(void)
{
    asm volatile("mov r0,sp");
    asm volatile("bx lr");
}

void vm_save_context(uint32_t *cur_context)
{
    if (context_ptr > 17 * 8)
    {
        printf("Context Stack Overflow!\n");
        for (;;)
            ;
    }
    memcpy((uint32_t *)(&vm_saved_context[context_ptr]), cur_context, 17 * 4);
    context_ptr += 17;
}

void vm_load_context(uint32_t *to, uint32_t *from, bool add_pc_4)
{

    {
        if ((from >= vm_saved_context) && (from <= &vm_saved_context[17 * 5 - 1]))
        {
            INFO("Context has been Loaded!\n");
        }
    }
    memcpy(to, from, 17 * 4);
    to[0] &= ~(0x1F);
    to[0] |= 0x10;
    if (add_pc_4)
    {
        to[2 + 15] += 4;
    }
}

void get_saved_context(uint32_t *to_addr)
{
    if (context_ptr < 17)
    {
        INFO("No saved context!\n");
        return;
    }
    context_ptr -= 17;
    memcpy(to_addr, (void *)&vm_saved_context[context_ptr], 17 * 4);
}

void vm_jump_irq(uint32_t *context)
{
    /*
    if(vm_in_exception && (curExp == 1))
    {
        INFO("Reenter IRQ Exception!\n");
        //return;
    }
    curExp = 1;*/
    vm_in_exception = true;
    context[0] |= (0xC0);

    context[0] = 0x10;
    context[15 + 1] = IRQ_VECTOR + 4;
    context[13 + 1] = IRQ_STACK;
}

static void vm_jump_svc(uint32_t *context)
{ /*
if(vm_in_exception  && (curExp == 2))
{
INFO("Reenter SVC Exception!\n");
}*/
    /*
    curExp = 2;*/
    vm_in_exception = true;

    context[0] |= (0xC0);

    context[0] = 0x10;
    context[15 + 1] = SVC_VECTOR + 4;
    context[13 + 1] = SVC_STACK;
}

void __attribute__((naked)) reset()
{
    asm volatile("mov r0,#0");
    asm volatile("bx r0");
}

void delay_ms(uint32_t ms);


unsigned int faultAddress;
unsigned int FSR;

void crash(uint32_t *context)
{
    printf("\n\n");
    printf("===================SYSTEM ERROR==================\n\n");

    __asm volatile("PUSH	{R0}");
    __asm volatile("mrc p15, 0, r0, c6, c0, 0");
    __asm volatile("str r0,%0"
                   : "=m"(faultAddress));
    __asm volatile("mrc p15, 0, r0, c5, c0, 0");
    __asm volatile("str r0,%0"
                   : "=m"(FSR));
    __asm volatile("POP	{R0}");

    printf("Fault address:%08x\n", faultAddress);
    printf("FSR:%08x ", FSR);
    switch (FSR & 0xF) {
    case 0x1:
    case 0x3:
        printf("Mem access unalign!\n");
        break;
    case 0x5:
    case 0x7:
        printf("Mem access unmap!\n");
        break;
    case 0xD:
    case 0xF:
        printf("Mem write read only segment!\n");
        break;
    default:
        printf("Unknown ERROR!\n");
        break;
    }

    printf("CPSR:%08x\n", context[0]);
    for (int i = 1; i < 17; i++)
    {
        printf("R%d = %08x\n", i-1, context[i]);
    }

    switch (context[0] & 0x1F)
    {
    case SYS_MODE:
        printf("CUR_MODE: SYS_MODE\n");
        break;
    case USER_MODE:
        printf("CUR_MODE: USER_MODE\n");
        break;
    case FIQ_MODE:
        printf("CUR_MODE: FIQ_MODE\n");
        break;
    case IRQ_MODE:
        printf("CUR_MODE: IRQ_MODE\n");
        break;
    case SVC_MODE:
        printf("CUR_MODE: SVC_MODE\n");
        break;
    case ABT_MODE:
        printf("CUR_MODE: ABT_MODE\n");
        break;
    case UND_MODE:
        printf("CUR_MODE: UND_MODE\n");
        break;

    default:
        break;
    }
    printf("=================================================\n\n");
    printf("Rebooting...\n\n");
    delay_ms(2000);
    reset();
}


void arm_vector_swi() __attribute__((naked));
void arm_vector_swi()
{
    __asm volatile("add     lr, lr, #4");
    __asm volatile("stmdb   sp, {R0-R14}^");
    __asm volatile("str     lr, [sp]");
    __asm volatile("mrs     r0, spsr");
    __asm volatile("str     r0, [sp, #-64]");
    __asm volatile("sub     sp, sp, #64");

    __asm volatile("push {r0-r12}");



    uint32_t *save_context_base = ((uint32_t *)_get_sp()) + 13;
    // printf("sp:%08x\n", save_context_base);
    /*
        printf("SPSR:%08lx\n", save_context_base[0]);
        for(int i = 1; i < 17; i++)
        {
            printf("context R%d:%08lx\n", i-1, save_context_base[i]);
        }*/

    uint32_t SWInum = (((uint32_t *)save_context_base[1 + 15])[-2]) & 0xFFFFFF;
    uint32_t par0 = (save_context_base[1 + 0]);
    uint32_t par1 = (save_context_base[1 + 1]);
    uint32_t par2 = (save_context_base[1 + 2]);
    uint32_t par3 = (save_context_base[1 + 3]);
    uint32_t *retval = &save_context_base[1 + 0];

    bool handle = false;

    switch (SWInum & 0xFF00)
    {
    case 0:
    {
        vm_save_context(save_context_base);
        vm_jump_svc(save_context_base);
        handle = true;
    }
    break;
    case LL_SWI_BASE:
    {
        switch (SWInum)
        {
        case LL_SWI_ENABLE_IRQ:
            // INFO("enable IRQ:%ld\n", par0);
            if (par0)
            {
                // enable_irq();
                save_context_base[0] &= (~0xC0);
                softint = true;
            }
            else
            {
                save_context_base[0] |= (0xC0);
                // disable_irq();
            }
            handle = true;
            break;

        case LL_SWI_SET_IRQ_STACK:
            IRQ_STACK = par0;
            printf("set irq stack:%08x\n", IRQ_STACK);
            handle = true;
            break;

        case LL_SWI_SET_IRQ_VECTOR:
            IRQ_VECTOR = par0;
            printf("set irq vector:%08x\n", IRQ_VECTOR);
            handle = true;
            break;

        case LL_SWI_SET_SVC_STACK:
            SVC_STACK = par0;
            printf("set svc stack:%08x\n", SVC_STACK);
            handle = true;
            break;

        case LL_SWI_SET_SVC_VECTOR:
            SVC_VECTOR = par0;
            printf("set svc vector:%08x\n", SVC_VECTOR);
            handle = true;
            break;

        case LL_SWI_SLOW_DOWN_ENABLE:
            handle = true;
            break;

        case LL_SWI_PUT_CH:
            printf("%c", par0 & 0xFF);
            handle = true;
            break;

        case LL_SWI_WRITE_STRING2:
            printf("%s", (char *)par0);
            handle = true;
            break;

        case LL_SWI_ENABLE_TIMER:
            printf("set tmr:%d,%d\n", par0, par1);
            if(par0)
            {
                timer_period_ms = par1;
            }else{
                timer_period_ms = 0;
            }
            timer_set(par0, par1);
            handle = true;
            break;

        case LL_SWI_SET_CONTEXT:
        {
            uint32_t *pCont = (uint32_t *)par0;
            /*
            for(int i = 0; i < 17; i++)
            {
                printf("getCont,%d:%08x\n", i, pCont[i]);
            }*/
            memcpy(save_context_base, pCont, 17 * 4);
            save_context_base[0] &= ~(0x1F);
            save_context_base[0] |= 0x10;
            if (par1)
            {
                save_context_base[0] &= (~0xC0);
                // enable_irq();
            }
            else
            {
                save_context_base[0] |= 0xC0;
                // disable_irq();
            }
            vm_in_exception = false;
            handle = true;
            break;
        }
        case LL_SWI_GET_CONTEXT:
        {
            // INFO("GET_CONTEXT:%08x\n", par0);
            get_saved_context((uint32_t *)par0);
            handle = true;
            break;
        }
        case LL_SWI_RESTORE_CONTEXT:
        {
            vm_load_context(save_context_base, (uint32_t *)par0, false);
            
            vm_in_exception = false;
            handle = true;
            break;
        }

        case LL_SWI_DISPLAY_SET_INDICATION:
        {
            //INFO("DISP:%d:%d\n", par0, par1);
            handle = true;
            break;
        }

        case LL_SWI_DISPLAY_FLUSH:
        {
            uint32_t par4 = ((uint32_t *)save_context_base[1 + 13])[0];
            if (
                (par0) && (par1 >= 0) && (par2 >= 0) && (par3 < 256) && (par4 < 128))
            {
                uint8_t *vs = (uint8_t *)par0;
                uint8_t *vram = (uint8_t *)VRAM_PHY_BASE;
                for (int y0 = par2, y1 = par4 + 1; y0 < y1; y0++)
                {
                    for (int x0 = par1, x1 = par3 + 1; x0 < x1; x0++)
                    {
                        vram[256 * y0 + x0] = *vs;
                        vs++;
                    }
                }
            }

            handle = true;
            break;
        }

        case LL_SWI_FLASH_PAGE_READ: // start page, pages, buf
        {
            //printf("fr:[%d,%d]\n", par0, par1);
            char *mtd_adr = (char *)MTD_PHY_BASE;
            memcpy((char*)par2, &mtd_adr[FLASH_PAGE_SIZE * par0], par1 * FLASH_PAGE_SIZE);
            handle = true;
            *retval = 0;
            break;
        }
        case LL_SWI_FLASH_PAGE_WRITE: // start page, pages, buf
        {
            
            //printf("wr:[%d,%d]\n", par0, par1);
            char *mtd_adr = (char *)MTD_PHY_BASE;
            memcpy(&mtd_adr[FLASH_PAGE_SIZE * par0], (char*)par2, par1 * FLASH_PAGE_SIZE);
            handle = true;
            *retval = 0;
            break;
        }
        case LL_SWI_FLASH_PAGE_TRIM: 
        {
            char *mtd_adr = (char *)MTD_PHY_BASE;
            memset(&mtd_adr[FLASH_PAGE_SIZE * par0], 0xFF, FLASH_PAGE_SIZE);
            *retval = 0;
            handle = true;
            break;
        }
        case LL_SWI_FLASH_SYNC:
        {
            h->flash_sync = 1;
            *retval = 0;
            handle = true;
            break;
        }
        case LL_SWI_FLASH_PAGE_NUM:
        {
            *retval = (64 * 1048576 / FLASH_PAGE_SIZE);
            handle = true;
            break;
        }
        case LL_SWI_FLASH_PAGE_SIZE_B:
        {
            *retval = FLASH_PAGE_SIZE;
            handle = true;
            break;
        }


        
        case LL_SWI_CLKCTL_GET_DIV:
        {
            *retval = 6;
            handle = true;
            break;
        }

        case LL_SWI_CLKCTL_SET_DIV:
        {
            
            handle = true;
            break;
        }

        case LL_SWI_CHARGE_ENABLE:
        {

            handle = true;
            break;
        }
        case LL_SWI_PWR_SPEED:
        {
            *retval = 123;
            
            handle = true;
            break;
        }

        case LL_SWI_SLOW_DOWN_MINFRAC:
        {

            handle = true;
            break;
        }

        default:
            break;
        }
    }
    break;
    case LL_FAST_SWI_BASE:
    {
        switch (SWInum)
        {
        case LL_FAST_SWI_GET_STVAL:
            if (par0 < 16)
            {
                *retval = vm_temp_storage[par0];
                handle = true;
            }
            break;
        case LL_FAST_SWI_SET_STVAL:
            if (par0 < 16)
            {
                vm_temp_storage[par0] = par1;
                handle = true;
            }
            break;

        case LL_FAST_SWI_SYSTEM_IDLE:
            handle = true;
            break;

        case LL_FAST_SWI_CHECK_KEY:
        {
            *retval = h->key_status;
            handle = true;

            break;
        }

        case LL_FAST_SWI_GET_TIME_MS:
        {
            if(h->timer_en)
            {
                *retval = pass_tick_ms;
            }else{
                *retval = h->time_ms_tick;
            }
            //
            handle = true;
            break;
        }

        case LL_FAST_SWI_GET_TIME_US:
        {
            *retval = h->time_us_tick;
            handle = true;
            break;
        }

        case LL_FAST_SWI_RTC_GET_SEC:
        {
            *retval = h->time_rtc_s;
            handle = true;
            break;   
        }

        case LL_FAST_SWI_RTC_SET_SEC:
        {
            //*retval = h->time_rtc_s;
            handle = true;
            break;   
        }

        case LL_FAST_SWI_GET_CHARGE_STATUS:
        {
            *retval = 0;
            handle = true;
            break;   
        }

        case LL_FAST_SWI_CORE_CUR_FREQ:
        {
            *retval = 114514;
            handle = true;
            break;   
        }

        case LL_FAST_SWI_PWR_VOLTAGE:
        {
            *retval = 220000;
            handle = true;
            break;   
        }
        case LL_FAST_SWI_CORE_TEMP:
        {
            *retval = 105;
            handle = true;
            break;   
        }



        default:
            break;
        }
    }
    break;

    case SYS_SWI_BASE:
    {
    }
    break;

    default:

        break;
    }

    if (!handle)
    {
        printf("ERROR! Unhandle swi:%08lx\n", SWInum);
        crash(save_context_base);
    }

    __asm volatile("pop {r0-r12}");

    __asm volatile("ldmfd	 sp!, {R0}");
    __asm volatile("msr      spsr, r0");
    __asm volatile("ldmfd	 sp, {R0-R14}^");
    __asm volatile("add     sp,sp,#60");
    __asm volatile("ldmfd	 sp, {lr}");
    __asm volatile("subs    pc,lr,#4");
}

void arm_vector_irq() __attribute__((naked));
void arm_vector_irq()
{
    __asm volatile("stmdb   sp, {R0-R14}^");
    __asm volatile("str     lr, [sp]");
    __asm volatile("mrs     r0, spsr");
    __asm volatile("str     r0, [sp, #-64]");
    __asm volatile("sub     sp, sp, #64");

    __asm volatile("push {r0-r12}");
    uint32_t *save_context_base = ((uint32_t *)_get_sp()) + 13;
    
    //printf("IRQS:%08x\n", save_context_base);


    h->timer_period_ms = timer_period_ms;
    pass_tick_ms += timer_period_ms;
    //h->timer_period_ms = 10;
    h->irq_pending = 0;
    
        if(!vm_in_exception)
        {
            vm_save_context(save_context_base);
            vm_jump_irq(save_context_base);
            save_context_base[1 + 0] = LL_IRQ_TIMER;
            save_context_base[1 + 1] = 0;
            save_context_base[1 + 2] = 0;
            save_context_base[1 + 3] = 0;
        }


    __asm volatile("pop {r0-r12}");

    __asm volatile("ldmfd	 sp!, {R0}");
    __asm volatile("msr      spsr, r0");
    __asm volatile("ldmfd	 sp, {R0-R14}^");
    __asm volatile("add     sp,sp,#60");
    __asm volatile("ldmfd	 sp, {lr}");
    __asm volatile("subs    pc,lr,#4");
}






void arm_vector_und() __attribute__((naked));
void arm_vector_und()
{
    __asm volatile("stmdb   sp, {R0-R14}^");
    __asm volatile("str     lr, [sp]");
    __asm volatile("mrs     r0, spsr");
    __asm volatile("str     r0, [sp, #-64]");
    __asm volatile("sub     sp, sp, #64");

    __asm volatile("push {r0-r12}");
    uint32_t *save_context_base = ((uint32_t *)_get_sp()) + 13;

    printf("UND\n");

    crash(save_context_base);
}

void arm_vector_dab() __attribute__((naked));
void arm_vector_dab()
{
    __asm volatile("stmdb   sp, {R0-R14}^");
    __asm volatile("str     lr, [sp]");
    __asm volatile("mrs     r0, spsr");
    __asm volatile("str     r0, [sp, #-64]");
    __asm volatile("sub     sp, sp, #64");

    __asm volatile("push {r0-r12}");
    uint32_t *save_context_base = ((uint32_t *)_get_sp()) + 13;

    printf("DAB\n");

    crash(save_context_base);
}

void arm_vector_pab() __attribute__((naked));
void arm_vector_pab()
{
    __asm volatile("stmdb   sp, {R0-R14}^");
    __asm volatile("str     lr, [sp]");
    __asm volatile("mrs     r0, spsr");
    __asm volatile("str     r0, [sp, #-64]");
    __asm volatile("sub     sp, sp, #64");

    __asm volatile("push {r0-r12}");
    uint32_t *save_context_base = ((uint32_t *)_get_sp()) + 13;

    printf("PAB\n");
    crash(save_context_base);
}

void arm_vector_fiq() __attribute__((naked));
void arm_vector_fiq()
{

    printf("FIQ\n");
    reset();
}
