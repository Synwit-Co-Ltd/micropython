#include <stdint.h>
#include <string.h>

#include "py/runtime.h"

#include "lib/oofatfs/ff.h"
#include "lib/oofatfs/diskio.h"

#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"

#include "mods/pybflash.h"


/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
} pyb_flash_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static pyb_flash_obj_t pyb_flash_obj = { {&pyb_flash_type} };


/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
#define FLASH_BASE_ADDR             0x100000
#define FLASH_END_ADDR              0x800000    // Flash from FLASH_BASE_ADDR to FLASH_END_ADDR used by MicroPy

#define FLASH_BLOCK_SIZE            4096
#define FLASH_BLOCK_COUNT           ((FLASH_END_ADDR - FLASH_BASE_ADDR) / FLASH_BLOCK_SIZE)
#define FLASH_SECTOR_SIZE           512
#define FLASH_SECTOR_COUNT          ((FLASH_END_ADDR - FLASH_BASE_ADDR) / FLASH_SECTOR_SIZE)
#define FLASH_SECTORS_PER_BLOCK     (FLASH_BLOCK_SIZE / FLASH_SECTOR_SIZE)


static uint32_t FLASH_Block_Cache[FLASH_BLOCK_SIZE/4];

static uint32_t FLASH_Block_Index = 0;
static uint32_t FLASH_Block_IndexPre = 0xFFFFFFFF;

static uint32_t FLASH_Cache_Dirty = 0;

DRESULT flash_disk_flush(void);


DRESULT flash_disk_init(void)
{
    SFC_InitStructure SFC_initStruct;

    PORT_Init(PORTD, PIN5, PORTD_PIN5_FSPI_SCLK,  0);
	PORT_Init(PORTD, PIN6, PORTD_PIN6_FSPI_SSEL,  0);
	PORT_Init(PORTD, PIN8, PORTD_PIN8_FSPI_MOSI,  1);
	PORT_Init(PORTD, PIN7, PORTD_PIN7_FSPI_MISO,  1);
	PORT_Init(PORTD, PIN3, PORTD_PIN3_FSPI_DATA2, 1);
	PORT_Init(PORTD, PIN4, PORTD_PIN4_FSPI_DATA3, 1);

    SFC_initStruct.ClkDiv = SFC_CLKDIV_4;
    SFC_initStruct.Cmd_Read = 0xEB;
    SFC_initStruct.Width_Read = SFC_RDWIDTH_4;
    SFC_initStruct.Cmd_PageProgram = 0x32;
    SFC_initStruct.Width_PageProgram = SFC_PPWIDTH_4;
    SFC_Init(&SFC_initStruct);

    uint32_t id = SFC_ReadJEDEC();
    if((id == 0x00000000) || (id == 0xFFFFFFFF))
        return RES_ERROR;

    memset(FLASH_Block_Cache, 0, FLASH_BLOCK_SIZE);

    FLASH_Block_IndexPre = 0xFFFFFFFF;

    return RES_OK;
}


DRESULT flash_disk_read(BYTE *buff, DWORD sector, UINT count)
{
    if((sector + count > FLASH_SECTOR_COUNT) || (count == 0))
    {
        return RES_PARERR;
    }

    uint32_t index_in_block;
    for(uint32_t i = 0; i < count; i++)
    {
        index_in_block    = (sector + i) % FLASH_SECTORS_PER_BLOCK;
        FLASH_Block_Index = (sector + i) / FLASH_SECTORS_PER_BLOCK;

        if(FLASH_Block_Index != FLASH_Block_IndexPre)
        {
            flash_disk_flush();

            FLASH_Block_IndexPre = FLASH_Block_Index;

            SFC_Read(FLASH_BASE_ADDR + FLASH_BLOCK_SIZE * FLASH_Block_Index, FLASH_Block_Cache, FLASH_BLOCK_SIZE / 4);
        }

        // Copy the requested sector from the block cache
        memcpy(buff + FLASH_SECTOR_SIZE * i, &FLASH_Block_Cache[(index_in_block * FLASH_SECTOR_SIZE) / 4], FLASH_SECTOR_SIZE);
    }

    return RES_OK;
}


DRESULT flash_disk_write(const BYTE *buff, DWORD sector, UINT count)
{
    if((sector + count > FLASH_SECTOR_COUNT) || (count == 0))
    {
        return RES_PARERR;
    }

    uint32_t index_in_block;
    for(uint32_t i = 0; i < count; i++)
    {
        index_in_block    = (sector + i) % FLASH_SECTORS_PER_BLOCK;
        FLASH_Block_Index = (sector + i) / FLASH_SECTORS_PER_BLOCK;

        if(FLASH_Block_Index != FLASH_Block_IndexPre)
        {
            flash_disk_flush();

            FLASH_Block_IndexPre = FLASH_Block_Index;

            SFC_Read(FLASH_BASE_ADDR + FLASH_BLOCK_SIZE * FLASH_Block_Index, FLASH_Block_Cache, FLASH_BLOCK_SIZE / 4);
        }

        // Copy the input sector to the block cache
        memcpy(&FLASH_Block_Cache[(index_in_block * FLASH_SECTOR_SIZE) / 4], buff + FLASH_SECTOR_SIZE * i, FLASH_SECTOR_SIZE);

        FLASH_Cache_Dirty = 1;
    }

    return RES_OK;
}


DRESULT flash_disk_flush(void)
{
    // Write back the cache if it's dirty
    if(FLASH_Cache_Dirty)
    {
        SFC_Erase(FLASH_BASE_ADDR + FLASH_BLOCK_SIZE * FLASH_Block_IndexPre, 1);

        for(int i = 0; i < FLASH_BLOCK_SIZE; i += 256)
            SFC_Write(FLASH_BASE_ADDR + FLASH_BLOCK_SIZE * FLASH_Block_IndexPre + i, &FLASH_Block_Cache[i / 4], 256 / 4);

        FLASH_Cache_Dirty = 0;
    }
    return RES_OK;
}


/******************************************************************************/
// MicroPython bindings to expose the internal flash as an object with the
// block protocol.

static void flash_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    mp_printf(print, "<Flash>");
}


static mp_obj_t flash_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    return &pyb_flash_obj;
}


static mp_obj_t flash_readblocks(mp_obj_t self_in, mp_obj_t block_num, mp_obj_t buf)
{
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_WRITE);

    DRESULT res = flash_disk_read(bufinfo.buf, mp_obj_get_int(block_num), bufinfo.len / FLASH_SECTOR_SIZE);

    return MP_OBJ_NEW_SMALL_INT(res != RES_OK); // return of 0 means success
}
static MP_DEFINE_CONST_FUN_OBJ_3(flash_readblocks_obj, flash_readblocks);


static mp_obj_t flash_writeblocks(mp_obj_t self_in, mp_obj_t block_num, mp_obj_t buf)
{
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);

    DRESULT res = flash_disk_write(bufinfo.buf, mp_obj_get_int(block_num), bufinfo.len / FLASH_SECTOR_SIZE);

    return MP_OBJ_NEW_SMALL_INT(res != RES_OK); // return of 0 means success
}
static MP_DEFINE_CONST_FUN_OBJ_3(flash_writeblocks_obj, flash_writeblocks);


static mp_obj_t flash_ioctl(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t arg_in)
{
    uint cmd = mp_obj_get_int(cmd_in);
    switch(cmd)
    {
        case MP_BLOCKDEV_IOCTL_INIT:        return MP_OBJ_NEW_SMALL_INT(flash_disk_init() != RES_OK);
        case MP_BLOCKDEV_IOCTL_DEINIT:      flash_disk_flush();
                                            return MP_OBJ_NEW_SMALL_INT(0);
        case MP_BLOCKDEV_IOCTL_SYNC:        flash_disk_flush();
                                            return MP_OBJ_NEW_SMALL_INT(0);
        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT: return MP_OBJ_NEW_SMALL_INT(FLASH_SECTOR_COUNT);
        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:  return MP_OBJ_NEW_SMALL_INT(FLASH_SECTOR_SIZE);
        default: return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_3(flash_ioctl_obj, flash_ioctl);


static const mp_rom_map_elem_t flash_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_readblocks),  MP_ROM_PTR(&flash_readblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&flash_writeblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl),       MP_ROM_PTR(&flash_ioctl_obj) },
};
static MP_DEFINE_CONST_DICT(flash_locals_dict, flash_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    pyb_flash_type,
    MP_QSTR_Flash,
    MP_TYPE_FLAG_NONE,
    print, flash_print,
    make_new, flash_make_new,
    locals_dict, &flash_locals_dict
);


void pyb_flash_init_vfs(fs_user_mount_t *vfs)
{
    vfs->base.type = &mp_fat_vfs_type;
    vfs->blockdev.flags |= MP_BLOCKDEV_FLAG_NATIVE | MP_BLOCKDEV_FLAG_HAVE_IOCTL;
    vfs->fatfs.drv = vfs;
    vfs->blockdev.readblocks[0]  = (mp_obj_t)&flash_readblocks_obj;
    vfs->blockdev.readblocks[1]  = (mp_obj_t)&pyb_flash_obj;
    vfs->blockdev.readblocks[2]  = (mp_obj_t)flash_disk_read; // native version
    vfs->blockdev.writeblocks[0] = (mp_obj_t)&flash_writeblocks_obj;
    vfs->blockdev.writeblocks[1] = (mp_obj_t)&pyb_flash_obj;
    vfs->blockdev.writeblocks[2] = (mp_obj_t)flash_disk_write; // native version
    vfs->blockdev.u.ioctl[0]     = (mp_obj_t)&flash_ioctl_obj;
    vfs->blockdev.u.ioctl[1]     = (mp_obj_t)&pyb_flash_obj;
}
