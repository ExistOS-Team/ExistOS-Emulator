
#include <stdint.h>
 
#include "main.h"
/*
void __attribute__((naked)) hyp_put_c(uint32_t c)
{
    asm volatile("push {r12}");

    asm volatile("mov r12,#1");
    asm volatile(".int 0xF7BBBBBB");

    asm volatile("pop {r12}");
    asm volatile("bx lr");

}

void osl_putc(char c)
{
	hyp_put_c(c);
}
*/

void osl_putc(char c);

static void put_s(const char* str){
	
	char c;
	
	while((c = *str++) != 0) osl_putc(c);
}




extern unsigned int __init_data;
extern unsigned int __data_start;
extern unsigned int __data_end;

extern unsigned int _sbss;
extern unsigned int _ebss;



volatile void _init() __attribute__((naked)) __attribute__((section(".init"))) __attribute__((naked));
volatile void _init()
{
	
	asm volatile("ldr pc,.Lreset");
	asm volatile("ldr pc,.Lund");
	asm volatile("ldr pc,.Lsvc");
	asm volatile("ldr pc,.Lpab");
	asm volatile("ldr pc,.Ldab");
	asm volatile("ldr pc,.Lreset");
	asm volatile("ldr pc,.Lirq");
	asm volatile("ldr pc,.Lfiq");

	asm volatile(".Lreset: 		.long 	 _boot");
	asm volatile(".Lund: 		.long 	 arm_vector_und");
	asm volatile(".Lsvc: 		.long 	 arm_vector_swi");
	asm volatile(".Lpab: 		.long 	 arm_vector_pab");
	asm volatile(".Ldab: 		.long 	 arm_vector_dab");
	asm volatile(".Lirq: 		.long 	 arm_vector_irq");
	asm volatile(".Lfiq: 		.long 	 arm_vector_fiq");


}


volatile void switch_mode(int mode) __attribute__((naked));
volatile void switch_mode(int mode) {
    asm volatile("and r0,r0,#0x1f");
    asm volatile("mrs r1,cpsr_all");
    asm volatile("bic r1,r1,#0x1f");
    asm volatile("orr r1,r1,r0");
    asm volatile("mov r0,lr"); // GET THE RETURN ADDRESS **BEFORE** MODE CHANGE
    asm volatile("msr cpsr_all,r1");
    asm volatile("bx r0");
}

volatile void set_stack(unsigned int newstackptr) __attribute__((naked));
volatile void set_stack(unsigned int newstackptr) {
    asm volatile("mov sp,r0");
    asm volatile("bx lr");
}

extern unsigned int _sbss;
extern unsigned int _ebss;


volatile void _boot() __attribute__((naked));
volatile void _boot(){
	


    asm volatile("mrs r1, cpsr_all");
    asm volatile("orr r1, r1, #0xc0");
    asm volatile("msr cpsr_all, r1");	//Disable interrupt

	asm volatile("ldr r13,=0x0080000");
	
	//asm volatile(".LstackInit: 		.long 	0x0080000");
/*
	asm volatile("mrc p15, 0, R0, c1, c0, 0");
	asm volatile("bic R0,R0,#0x2000");
	asm volatile("orr R0,R0,#0x1000");
	asm volatile("orr R0,R0,#0x4");
	asm volatile("bic R0,R0,#0x1");
	asm volatile("mcr p15, 0, R0, c1, c0, 0");
*/
	asm volatile("mrc p15, 0, R0, c1, c0, 0");
	asm volatile("bic R0,R0,#0x2000");
	asm volatile("bic R0,R0,#0x5");
	asm volatile("mcr p15, 0, R0, c1, c0, 0");

    switch_mode(ABT_MODE);
    set_stack(ABT_STACK_ADDR);

    switch_mode(UND_MODE);
    set_stack(UND_STACK_ADDR);

    switch_mode(FIQ_MODE);
    set_stack(FIQ_STACK_ADDR);

    switch_mode(IRQ_MODE);
    set_stack(IRQ_STACK_ADDR);


    switch_mode(SVC_MODE);
	set_stack(SVC_STACK_ADDR);

    switch_mode(SYS_MODE);
    set_stack(SYS_STACK_ADDR);


	for(char *i = (char *)&_sbss; i < (char *)&_ebss; i++){
		*i = 0;		//clear bss
	}




    put_s("Booting...\n");
	
	asm volatile("b main");
	
}


