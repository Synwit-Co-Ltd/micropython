#include <stdint.h>

#include "py/obj.h"
#include "py/runtime.h"

#include "mods/pyblcd.h"


/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
} pyb_lcd_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static pyb_lcd_obj_t pyb_lcd_obj = { {&pyb_lcd_type} };


static uint16_t *LCD_Buffer = (uint16_t *)SDRAMM_BASE;

static uint16_t LCD_HnPixel = 480;
static uint16_t LCD_VnPixel = 272;


/******************************************************************************/
/* MicroPython bindings                                                       */

static void lcd_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    pyb_lcd_obj_t *self = self_in;

    mp_printf(print, "LCD()");
}


static mp_obj_t lcd_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    PORT_Init(PORTB, PIN1,  PORTB_PIN1_LCD_B0,  0);
	PORT_Init(PORTB, PIN11, PORTB_PIN11_LCD_B1, 0);
	PORT_Init(PORTB, PIN13, PORTB_PIN13_LCD_B2, 0);
	PORT_Init(PORTB, PIN15, PORTB_PIN15_LCD_B3, 0);
	PORT_Init(PORTA, PIN2,  PORTA_PIN2_LCD_B4,  0);
	PORT_Init(PORTA, PIN9,  PORTA_PIN9_LCD_B5,  0);
	PORT_Init(PORTA, PIN10, PORTA_PIN10_LCD_B6, 0);
	PORT_Init(PORTA, PIN11, PORTA_PIN11_LCD_B7, 0);
	PORT_Init(PORTA, PIN12, PORTA_PIN12_LCD_G0, 0);
	PORT_Init(PORTA, PIN13, PORTA_PIN13_LCD_G1, 0);
	PORT_Init(PORTA, PIN14, PORTA_PIN14_LCD_G2, 0);
	PORT_Init(PORTA, PIN15, PORTA_PIN15_LCD_G3, 0);
	PORT_Init(PORTC, PIN0,  PORTC_PIN0_LCD_G4,  0);
	PORT_Init(PORTC, PIN1,  PORTC_PIN1_LCD_G5,  0);
	PORT_Init(PORTC, PIN2,  PORTC_PIN2_LCD_G6,  0);
	PORT_Init(PORTC, PIN3,  PORTC_PIN3_LCD_G7,  0);
	PORT_Init(PORTC, PIN4,  PORTC_PIN4_LCD_R0,  0);
	PORT_Init(PORTC, PIN5,  PORTC_PIN5_LCD_R1,  0);
	PORT_Init(PORTC, PIN8,  PORTC_PIN8_LCD_R2,  0);
	PORT_Init(PORTC, PIN9,  PORTC_PIN9_LCD_R3,  0);
	PORT_Init(PORTC, PIN10, PORTC_PIN10_LCD_R4, 0);
	PORT_Init(PORTC, PIN11, PORTC_PIN11_LCD_R5, 0);
	PORT_Init(PORTC, PIN12, PORTC_PIN12_LCD_R6, 0);
	PORT_Init(PORTC, PIN13, PORTC_PIN13_LCD_R7, 0);
	PORT_Init(PORTB, PIN2,  PORTB_PIN2_LCD_VSYNC, 0);
	PORT_Init(PORTB, PIN3,  PORTB_PIN3_LCD_HSYNC, 0);
	PORT_Init(PORTB, PIN4,  PORTB_PIN4_LCD_DEN,   0);
	PORT_Init(PORTB, PIN5,  PORTB_PIN5_LCD_DCLK,  0);
	
    LCD_InitStructure LCD_initStruct;
	LCD_initStruct.ClkDiv = 6;
	LCD_initStruct.Format = LCD_FMT_RGB565;
	LCD_initStruct.HnPixel = LCD_HnPixel;
	LCD_initStruct.VnPixel = LCD_VnPixel;
	LCD_initStruct.Hfp = 5;
	LCD_initStruct.Hbp = 40;
	LCD_initStruct.Vfp = 8;
	LCD_initStruct.Vbp = 8;
	LCD_initStruct.HsyncWidth = 5;
	LCD_initStruct.VsyncWidth = 5;
	LCD_initStruct.DataSource = (uint32_t)LCD_Buffer;
	LCD_initStruct.Background = 0xFFFF;
	LCD_initStruct.SampleEdge = LCD_SAMPLE_FALL;	// ATK-4342 samples data on falling edge
	LCD_initStruct.IntEOTEn = 0;
	LCD_Init(LCD, &LCD_initStruct);

    LCD_Start(LCD);

    return &pyb_lcd_obj;
}


static mp_obj_t lcd_fill(mp_obj_t self_in, mp_obj_t rgb_in)
{
    uint rgb = mp_obj_get_int(rgb_in);

    for(int i = 0; i < LCD_VnPixel; i++)
    {
        for(int j = 0; j < LCD_HnPixel; j++)
        {
            LCD_Buffer[i * LCD_HnPixel + j] = rgb;
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(lcd_fill_obj, lcd_fill);


static mp_obj_t lcd_pixel(size_t n_args, const mp_obj_t *args)
{
    uint x = mp_obj_get_int(args[1]);
    uint y = mp_obj_get_int(args[2]);

    if(n_args == 3)     // get
    {
        return mp_obj_new_int(LCD_Buffer[y * LCD_HnPixel + x]);
    }
    else                // set
    {
        uint rgb = mp_obj_get_int(args[3]);

        LCD_Buffer[y * LCD_HnPixel + x] = rgb;

        return mp_const_none;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lcd_pixel_obj, 3, 4, lcd_pixel);


static const mp_rom_map_elem_t lcd_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_fill),  MP_ROM_PTR(&lcd_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&lcd_pixel_obj) },
};
static MP_DEFINE_CONST_DICT(lcd_locals_dict, lcd_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    pyb_lcd_type,
    MP_QSTR_LCD,
    MP_TYPE_FLAG_NONE,
    print, lcd_print,
    make_new, lcd_make_new,
    locals_dict, &lcd_locals_dict
);
