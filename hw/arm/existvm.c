

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-clock.h"
#include "qemu/error-report.h"
#include "hw/arm/boot.h"
#include "qemu/module.h"
#include "cpu.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/arm/pxa.h"
#include "sysemu/sysemu.h"
#include "hw/char/serial.h"
#include "hw/i2c/i2c.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/ssi/ssi.h"
#include "hw/sd/sd.h"
#include "chardev/char-fe.h"
#include "sysemu/blockdev.h"
#include "sysemu/qtest.h"
#include "sysemu/rtc.h"
#include "qemu/cutils.h"
#include "qemu/log.h"
#include "qom/object.h"
#include "target/arm/cpregs.h"
#include "exec/address-spaces.h"

#include "qemu/seqlock.h"
#include "softmmu/timers-state.h"

#include <pthread.h>
#include <wincodec.h>


void ctl_cHandler(int v);

typedef void(_stdcall *refUI)(void);
typedef uint32_t(_stdcall *funcUIchkKey)(void);
typedef int(_stdcall *ui_wincall_pf)(void *pvram, void *cb, void *dp);
ui_wincall_pf dec_fp;
refUI dll_refui;
funcUIchkKey uichkkey;
pthread_t pthread_refui;

#define TYPE_EVM_PIC "existvm_pic"
OBJECT_DECLARE_SIMPLE_TYPE(ExistVM_PICState, EVM_PIC)

struct ExistVM_PICState
{
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    ARMCPU *cpu;
    uint32_t int_pending;
};

DeviceState *pic;
static ARMCPU *cpu;
struct ExistVM_PICState *evmpic;
static qemu_irq tmr_irq;

static QEMUTimer *timer_ms;

#define SRAM_BASE (0x00000000)
#define SRAM_SIZE (512 * 1024)

#define HMEM_BASE (0x60000000)
#define HMEM_SIZE (4 * 7)

#define SYSRAM_BASE (0x02000000)
#define SYSRAM_SIZE (6 * 1048576)

#define SYSROM_BASE (0x00100000)
#define SYSROM_SIZE (4 * 1048576)

#define APPVROM_BASE (0x03000000)
#define APPVROM_SIZE (4 * 1048576)

#define FLASH_BASE (0x10000000)
#define FLASH_SIZE (64 * 1048576)

#define VRAM_BASE (0x05000000)
#define VRAM_SIZE (256 * 128)

#define DP_BASE (0x20000000)
#define DP_SIZE (48 * 1048576)

#define SERIAL_PORT (HMEM_BASE + 50 * 4)

FILE *fosloader, *fsystem, *fflash;

uint8_t *buf_sram;
uint8_t *buf_hmem;
uint8_t *buf_sysram;
uint8_t *buf_app_vrom;
uint8_t *buf_sysrom;
uint8_t *buf_flash;
uint8_t *buf_vram;
uint8_t *buf_dataport;

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

hmem_t *h;

static void *emu_uiref_thread(void *_)
{
    int64_t lasti = 0;

    for (;;)
    {
        Sleep(4000);

        // printf("cpu_ticks_prev:%lld\n",timers_state.cpu_ticks_prev);
        printf("Freq:~%lld MHz\n", ((timers_state.qemu_icount - lasti) / 1000000) / 4);

        lasti = timers_state.qemu_icount;
    }
    return NULL;
}

static uint64_t evm_h_mem_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    switch (offset)
    {
    case 0:
        h->time_us_tick = qemu_clock_get_us(QEMU_CLOCK_VIRTUAL);
        return h->time_us_tick;
    case 4:
        h->time_ms_tick = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
        return h->time_ms_tick;
    case 8:
        h->time_rtc_s = qemu_clock_get_ms(rtc_clock) / 1000;
        return h->time_rtc_s;
    case 12:
        return h->timer_en;
    case 16:
        return h->timer_period_ms;
    case 20:
    {
        uint32_t k = uichkkey();
        if (k)
        {
            h->key_status = k;
        }
        return h->key_status;
    }
    case 24:
        return h->irq_pending;
    default:
        break;
    }
    return 0;
}

static void evm_h_mem_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    // printf("wr:%d, %08x\n", (int)offset, (uint32_t)value);
    switch (offset)
    {
    case 0 * 4:
        h->time_us_tick = value;
        return;
    case 1 * 4:
        h->time_ms_tick = value;
        return;
    case 2 * 4:
        h->time_rtc_s = value;
        return;
    case 3 * 4:
        h->timer_en = value;
        return;
    case 4 * 4:
        h->timer_period_ms = value;
        timer_mod(timer_ms, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + value);
        return;
    case 5 * 4:
        h->key_status = value;
        return;
    case 6 * 4:
        h->irq_pending = value;
        qemu_irq_lower(tmr_irq);
        return;
    case 7 * 4:
        h->flash_sync = value;
        {
            fseek(fflash, 0, SEEK_SET);
            fwrite(buf_flash, 1, FLASH_SIZE, fflash);
            fsync(fileno(fflash));
        }
        h->flash_sync = 0;
        return;
    default:
        break;
    }
    return;
}

static const MemoryRegionOps evm_hmem_ops = {
    .read = evm_h_mem_read,
    .write = evm_h_mem_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static inline void evm_tmr_ms_tick(void *opaque)
{
    // printf("tick\n");
    if (h->timer_en)
    {
        // printf("qemu_irq_raise\n");
        h->irq_pending = 1;
        qemu_irq_raise(tmr_irq);
    }
}

static void evm_pic_update(void *opaque)
{
    // printf("update pic\n");
    if (h->irq_pending)
    {
        cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
    }
    else
    {
        cpu_reset_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
    }
}

static void evm_pic_set_irq(void *opaque, int irq, int level)
{
    evm_pic_update(opaque);
}
void ctl_cHandler(int v)
{
    printf("sig:%d\n", v);
    printf("saving...\n");
    fseek(fflash, 0, SEEK_SET);
    fwrite(buf_flash, 1, FLASH_SIZE, fflash);
    fclose(fflash);
    exit(0);
}

static void existvm_init(MachineState *machine)
{

    fosloader = fopen("OSLoader.bin", "rb");
    if (!fosloader)
    {
        printf("failed to open OSLoader.bin\n");
        exit(-1);
    }
    buf_sram = g_new0(uint8_t, SRAM_SIZE);
    fread(buf_sram, 1, SRAM_SIZE, fosloader);
    fsystem = fopen("ExistOS.sys", "rb");
    if (!fsystem)
    {
        printf("failed to open ExistOS.sys\n");
        exit(-1);
    }
    buf_sysrom = g_new0(uint8_t, SYSROM_SIZE);
    fread(buf_sysrom, 1, SYSROM_SIZE, fsystem);
    fflash = fopen("flash.img", "rb+");
    if (!fflash)
    {
        printf("failed to open flash.img\n Creating...");
        fflash = fopen("flash.img", "wb+");
        if (!fflash)
        {
            printf("failed to create flash.img\n");
            exit(-1);
        }
        fseek(fflash, FLASH_SIZE - 1, SEEK_SET);
        fwrite("", 1, 1, fflash);
    }
    buf_flash = g_new0(uint8_t, FLASH_SIZE);
    fseek(fflash, 0, SEEK_SET);
    fread(buf_flash, 1, FLASH_SIZE, fflash);

    buf_hmem = g_new0(uint8_t, HMEM_SIZE);
    buf_sysram = g_new0(uint8_t, SYSRAM_SIZE);
    buf_app_vrom = g_new0(uint8_t, APPVROM_SIZE);
    buf_vram = g_new0(uint8_t, VRAM_SIZE);
    buf_dataport = g_new0(uint8_t, DP_SIZE);
    memset(buf_dataport, 0, DP_SIZE);

    h = (hmem_t *)buf_hmem;

    cpu = g_new0(ARMCPU, 1);
    cpu = ARM_CPU(cpu_create(ARM_CPU_TYPE_NAME("arm926")));

    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *hmem = g_new(MemoryRegion, 1);
    MemoryRegion *sysram = g_new(MemoryRegion, 1);
    MemoryRegion *sysrom = g_new(MemoryRegion, 1);
    MemoryRegion *approm = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *dpram = g_new(MemoryRegion, 1);
    MemoryRegion *vram = g_new(MemoryRegion, 1);

    DeviceState *picdev = qdev_new(TYPE_EVM_PIC);
    evmpic = EVM_PIC(picdev);
    evmpic->cpu = cpu;
    evmpic->int_pending = 0;

    sysbus_realize_and_unref(SYS_BUS_DEVICE(picdev), &error_fatal);
    qdev_init_gpio_in(picdev, evm_pic_set_irq, 2);
    sysbus_init_mmio(SYS_BUS_DEVICE(picdev), &evmpic->iomem);

    printf("set tmr irq\n");
    timer_ms = timer_new_ms(QEMU_CLOCK_VIRTUAL, evm_tmr_ms_tick, NULL);
    tmr_irq = qdev_get_gpio_in(picdev, 0);

    memory_region_init_ram_ptr(sram, NULL, "OSLoader SRAM", SRAM_SIZE, buf_sram);
    memory_region_add_subregion(address_space_mem, SRAM_BASE, sram);

    memory_region_init_io(hmem, NULL, &evm_hmem_ops, NULL, "HMEM", sizeof(hmem_t));
    memory_region_add_subregion(address_space_mem, HMEM_BASE, hmem);

    memory_region_init_ram_ptr(sysram, NULL, "SYS RAM", SYSRAM_SIZE, buf_sysram);
    memory_region_add_subregion(address_space_mem, SYSRAM_BASE, sysram);

    memory_region_init_ram_device_ptr(approm, NULL, "APPVROM", APPVROM_SIZE, buf_app_vrom);
    memory_region_add_subregion(address_space_mem, APPVROM_BASE, approm);

    memory_region_init_ram_ptr(sysrom, NULL, "SYS ROM", SYSROM_SIZE, buf_sysrom);
    memory_region_add_subregion(address_space_mem, SYSROM_BASE, sysrom);

    memory_region_init_ram_device_ptr(flash, NULL, "Flash", FLASH_SIZE, buf_flash);
    memory_region_add_subregion(address_space_mem, FLASH_BASE, flash);

    memory_region_init_ram_device_ptr(dpram, NULL, "dpram", DP_SIZE, buf_dataport);
    memory_region_add_subregion(address_space_mem, DP_BASE, dpram);

    memory_region_init_ram_ptr(vram, NULL, "vram", VRAM_SIZE, buf_vram);
    memory_region_add_subregion(address_space_mem, VRAM_BASE, vram);

    if (serial_hd(0))
    {
        printf("Add serial.\n");
        serial_mm_init(address_space_mem, SERIAL_PORT, 0,
                       qdev_get_gpio_in(picdev, 1),
                       115200, serial_hd(0),
                       DEVICE_NATIVE_ENDIAN);
    }

    HINSTANCE hModule = LoadLibrary("ui.dll");
    if (!hModule)
    {
        printf("Can't load ui.dll!\n");
        exit(-1);
    }

    dec_fp = (ui_wincall_pf)GetProcAddress(hModule, "startWin");
    dll_refui = (refUI)GetProcAddress(hModule, "refreshUI");
    uichkkey = (funcUIchkKey)GetProcAddress(hModule, "UI_CheckKey");
    dec_fp((void *)buf_vram, NULL, (void *)buf_dataport);
    pthread_create(&pthread_refui, NULL, emu_uiref_thread, NULL);
    printf("Set PC:%08llx\n", cpu->env.pc);

    signal(SIGINT, &ctl_cHandler);
}

static int evm_pic_post_load(void *opaque, int version_id)
{
    evm_pic_update(opaque);
    return 0;
}

static const VMStateDescription vmstate_evm_pic_regs = {
    .name = "pxa2xx_pic",
    .version_id = 0,
    .minimum_version_id = 0,
    .post_load = evm_pic_post_load,
    .fields = (VMStateField[]){
        VMSTATE_UINT32(int_pending, ExistVM_PICState),
        VMSTATE_END_OF_LIST(),
    },
};

static void evm_pic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "ExistVM PIC";
    dc->vmsd = &vmstate_evm_pic_regs;
}

static const TypeInfo evm_pic_info = {
    .name = TYPE_EVM_PIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ExistVM_PICState),
    .class_init = evm_pic_class_init,
};

static void evm_pic_register_types(void)
{
    type_register_static(&evm_pic_info);
}

static void existvm_machine_init(MachineClass *mc)
{
    mc->desc = "ExistOS Sys VM";
    mc->init = existvm_init;
}

DEFINE_MACHINE("existvm", existvm_machine_init)

type_init(evm_pic_register_types)