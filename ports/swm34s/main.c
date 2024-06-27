#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/stackctrl.h"
#include "py/gc.h"
#include "misc/gccollect.h"

#include "shared/runtime/pyexec.h"
#include "shared/readline/readline.h"
#include "shared/timeutils/timeutils.h"

#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"

#include "mods/pybuart.h"
#include "mods/pybflash.h"
#include "mods/pybrtc.h"


static const char fresh_main_py[] = "# main.py -- put your code here!\r\n";
static const char fresh_boot_py[] = "# boot.py -- run on boot-up\r\n"
                                    "# can run arbitrary Python, but best to keep it minimal\r\n"
                                    "import os, machine\r\n"
                                    "os.dupterm(machine.UART(0, 115200, txd='PM1', rxd='PM0'))\r\n"
                                    ;

void SDRAM_config(void);
void init_flash_filesystem(void);

int main (void)
{
    SystemInit();

    SDRAM_config();

    SysTick_Config(SystemCoreClock / 1000);
    NVIC_SetPriority(SysTick_IRQn, 3);  // 优先级高于它的ISR中不能使用 mp_hal_delay_us() 等延时相关函数

soft_reset:

    gc_init((void *)0x80700000, (void *)0x80800000);    // 总共 8MB SDRAM，最后 1MB 用作 MircoPy 的堆

    mp_stack_set_top((char*)&__StackTop);
    mp_stack_set_limit((char*)&__StackTop - (char*)&__StackLimit - 1024);

    // MicroPython init
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_init(mp_sys_argv, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_)); // current dir (or base dir of the script)
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));

    uart_init0();
    readline_init0();

    init_flash_filesystem();    

    // run boot.py
    int ret = pyexec_file("boot.py");
    if(ret & PYEXEC_FORCED_EXIT)
    {
        goto soft_reset_exit;
    }

    // run main.py
    ret = pyexec_file("main.py");
    if(ret & PYEXEC_FORCED_EXIT)
    {
        goto soft_reset_exit;
    }

    // main script is finished, so now go into REPL mode.
    while(1)
    {
        if(pyexec_mode_kind == PYEXEC_MODE_RAW_REPL)
        {
            if(pyexec_raw_repl() != 0)
            {
                break;
            }
        }
        else
        {
            if(pyexec_friendly_repl() != 0)
            {
                break;
            }
        }
    }

soft_reset_exit:

    printf("MPY: soft reboot\n");

    // do some deinit

    goto soft_reset;
}


void SDRAM_config(void)
{
    PORT_Init(PORTM, PIN13, PORTM_PIN13_SDR_CLK, 0);
	PORT_Init(PORTM, PIN14, PORTM_PIN14_SDR_CKE, 0);
	PORT_Init(PORTB, PIN7,  PORTB_PIN7_SDR_WE,   0);
	PORT_Init(PORTB, PIN8,  PORTB_PIN8_SDR_CAS,  0);
	PORT_Init(PORTB, PIN9,  PORTB_PIN9_SDR_RAS,  0);
	PORT_Init(PORTB, PIN10, PORTB_PIN10_SDR_CS,  0);
	PORT_Init(PORTE, PIN15, PORTE_PIN15_SDR_BA0, 0);
	PORT_Init(PORTE, PIN14, PORTE_PIN14_SDR_BA1, 0);
	PORT_Init(PORTN, PIN14, PORTN_PIN14_SDR_A0,  0);
	PORT_Init(PORTN, PIN13, PORTN_PIN13_SDR_A1,  0);
	PORT_Init(PORTN, PIN12, PORTN_PIN12_SDR_A2,  0);
	PORT_Init(PORTN, PIN11, PORTN_PIN11_SDR_A3,  0);
	PORT_Init(PORTN, PIN10, PORTN_PIN10_SDR_A4,  0);
	PORT_Init(PORTN, PIN9,  PORTN_PIN9_SDR_A5,   0);
	PORT_Init(PORTN, PIN8,  PORTN_PIN8_SDR_A6,   0);
	PORT_Init(PORTN, PIN7,  PORTN_PIN7_SDR_A7,   0);
	PORT_Init(PORTN, PIN6,  PORTN_PIN6_SDR_A8,   0);
	PORT_Init(PORTN, PIN3,  PORTN_PIN3_SDR_A9,   0);
	PORT_Init(PORTN, PIN15, PORTN_PIN15_SDR_A10, 0);
	PORT_Init(PORTN, PIN2,  PORTN_PIN2_SDR_A11,  0);
	PORT_Init(PORTE, PIN7,  PORTE_PIN7_SDR_D0,   1);
	PORT_Init(PORTE, PIN6,  PORTE_PIN6_SDR_D1,   1);
	PORT_Init(PORTE, PIN5,  PORTE_PIN5_SDR_D2,   1);
	PORT_Init(PORTE, PIN4,  PORTE_PIN4_SDR_D3,   1);
	PORT_Init(PORTE, PIN3,  PORTE_PIN3_SDR_D4,   1);
	PORT_Init(PORTE, PIN2,  PORTE_PIN2_SDR_D5,   1);
	PORT_Init(PORTE, PIN1,  PORTE_PIN1_SDR_D6,   1);
	PORT_Init(PORTE, PIN0,  PORTE_PIN0_SDR_D7,   1);
	PORT_Init(PORTE, PIN8,  PORTE_PIN8_SDR_D8,   1);
	PORT_Init(PORTE, PIN9,  PORTE_PIN9_SDR_D9,   1);
	PORT_Init(PORTE, PIN10, PORTE_PIN10_SDR_D10, 1);
	PORT_Init(PORTE, PIN11, PORTE_PIN11_SDR_D11, 1);
	PORT_Init(PORTE, PIN12, PORTE_PIN12_SDR_D12, 1);
	PORT_Init(PORTE, PIN13, PORTE_PIN13_SDR_D13, 1);
	PORT_Init(PORTC, PIN14, PORTC_PIN14_SDR_D14, 1);
	PORT_Init(PORTC, PIN15, PORTC_PIN15_SDR_D15, 1);
	PORT_Init(PORTB, PIN6,  PORTB_PIN6_SDR_LDQM, 0);
	PORT_Init(PORTM, PIN12, PORTM_PIN12_SDR_UDQM,0);

    SDRAM_InitStructure SDRAM_InitStruct;

    SDRAM_InitStruct.Size = SDRAM_SIZE_8MB;
	SDRAM_InitStruct.ClkDiv = SDRAM_CLKDIV_1;
	SDRAM_InitStruct.CASLatency = SDRAM_CASLATENCY_2;
	SDRAM_InitStruct.TimeTRP  = SDRAM_TRP_2;
	SDRAM_InitStruct.TimeTRCD = SDRAM_TRCD_2;
	SDRAM_InitStruct.TimeTRC  = SDRAM_TRC_7;
	SDRAM_Init(&SDRAM_InitStruct);
}


void init_flash_filesystem(void)
{
    /* Initialise the local flash filesystem */
    static fs_user_mount_t vfs_fat;

    vfs_fat.blockdev.flags = 0;
    pyb_flash_init_vfs(&vfs_fat);

    FRESULT res = f_mount(&vfs_fat.fatfs);
    if(res == FR_NO_FILESYSTEM)
    {
        // no filesystem, so create a fresh one
        uint8_t working_buf[FF_MAX_SS];
        res = f_mkfs(&vfs_fat.fatfs, FM_FAT | FM_SFD, 0, working_buf, sizeof(working_buf));
        if(res != FR_OK)
        {
            printf("failed to create /");
            while(1) __NOP();
        }
    }

    /* mount the flash device */
    // we allocate this structure on the heap because vfs->next is a root pointer
    mp_vfs_mount_t *vfs = m_new_obj_maybe(mp_vfs_mount_t);

    vfs->str = "/";
    vfs->len = 1;
    vfs->obj = MP_OBJ_FROM_PTR(&vfs_fat);
    vfs->next = NULL;
    MP_STATE_VM(vfs_mount_table) = vfs;

    MP_STATE_PORT(vfs_cur) = vfs;

    f_chdir(&vfs_fat.fatfs, "/");

    /* create boot.py and main.py */
    FILINFO fno;
    FIL fp;
    UINT n;

    res = f_stat(&vfs_fat.fatfs, "/boot.py", &fno);
    if(res != FR_OK)
    {
        f_open(&vfs_fat.fatfs, &fp, "/boot.py", FA_WRITE | FA_CREATE_ALWAYS);
        f_write(&fp, fresh_boot_py, sizeof(fresh_boot_py) - 1 /* don't count null terminator */, &n);
        f_close(&fp);
    }

    res = f_stat(&vfs_fat.fatfs, "/main.py", &fno);
    if(res != FR_OK)
    {
        f_open(&vfs_fat.fatfs, &fp, "/main.py", FA_WRITE | FA_CREATE_ALWAYS);
        f_write(&fp, fresh_main_py, sizeof(fresh_main_py) - 1 /* don't count null terminator */, &n);
        f_close(&fp);
    }

    return;
}


DWORD get_fattime(void)
{
    timeutils_struct_time_t tm;
    timeutils_seconds_since_2000_to_struct_time(rtc_get_seconds(), &tm);

    return ((tm.tm_year - 1980) << 25) | ((tm.tm_mon) << 21)  | ((tm.tm_mday) << 16) |
            ((tm.tm_hour) << 11) | ((tm.tm_min) << 5) | (tm.tm_sec >> 1);
}


void NORETURN __fatal_error(const char *msg)
{
   while(1) __NOP();
}

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    printf("Assertion failed: %s, file %s, line %d\n", expr, file, line);
    __fatal_error(NULL);
}

void nlr_jump_fail(void *val)
{
    __fatal_error(NULL);
}
