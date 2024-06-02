#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/binary.h"
#include "py/gc.h"
#include "py/mperrno.h"

#include "bufhelper.h"

#include "pybadc.h"
#include "pybpin.h"


/******************************************************************************
 DECLARE CONSTANTS
 ******************************************************************************/
#define ADC_NUM_CHANNELS   12
#define ADC_NUM_SEQS        4


/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    PYB_ADC_0  =  0,
    PYB_ADC_1  =  1,
    PYB_NUM_ADCS
} pyb_adc_id_t;

typedef struct {
    mp_obj_base_t base;
    pyb_adc_id_t adc_id;
    ADC_TypeDef *ADCx;

    struct {
        uint16_t channels;
        uint16_t trigger;
        uint16_t sampleTime;
        uint16_t convertTimes;
    } seqs[ADC_NUM_SEQS];
} pyb_adc_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static pyb_adc_obj_t pyb_adc_obj[PYB_NUM_ADCS] = {
    { {&pyb_adc_type}, .adc_id = PYB_ADC_0, .ADCx = ADC0 },
    { {&pyb_adc_type}, .adc_id = PYB_ADC_1, .ADCx = ADC1 },
};


/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/


/******************************************************************************/
/* MicroPython bindings : adc object                                          */

static void adc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    pyb_adc_obj_t *self = self_in;

    mp_printf(print, "ADC(%d", self->adc_id);

    for(uint i = 0, first_seq = 1;  i < ADC_NUM_SEQS; i++)
    {
        if(self->seqs[i].channels)
        {
            if(first_seq) mp_printf(print,         ", SEQ%d chns=[", i);
            else          mp_printf(print, ",\n       SEQ%d chns=[", i);
            first_seq = 0;

            for(uint j = 0, first_chn = 1; j < ADC_NUM_CHANNELS; j++)
            {
                if(self->seqs[i].channels & (1 << j))
                {
                    if(first_chn) mp_printf(print,   "%d", j);
                    else          mp_printf(print, ", %d", j);
                    first_chn = 0;
                }
            }

            mp_printf(print, "]");
        }
    }

    mp_printf(print, ")");
}


static mp_obj_t adc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_id };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} }
    };

    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // check the peripheral id
    uint adc_id = args[ARG_id].u_int;
    if(adc_id >= PYB_NUM_ADCS)
    {
        mp_raise_OSError(MP_ENODEV);
    }
    pyb_adc_obj_t *self = &pyb_adc_obj[adc_id];

    ADC_InitStructure ADC_initStruct;
    ADC_initStruct.clk_src = ADC_CLKSRC_HRC;
	ADC_initStruct.samplAvg = ADC_AVG_SAMPLE1;
	ADC_initStruct.EOC_IEn = 0;	
	ADC_initStruct.HalfIEn = 0;
    ADC_Init(self->ADCx, &ADC_initStruct);
    ADC_Open(self->ADCx);
    ADC_Calibrate(self->ADCx);

    return self;
}


static mp_obj_t adc_seq_config(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum { ARG_seq, ARG_chnn, ARG_trig, ARG_samp, ARG_conv };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_seq,          MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_channels,     MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_trigger,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = ADC_TRIGGER_SW} },
        { MP_QSTR_sampleTime,   MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_convertTimes, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 1} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    pyb_adc_obj_t *self = pos_args[0];

    uint seq_id = args[ARG_seq].u_int;
    if(seq_id >= ADC_NUM_SEQS)
        mp_raise_ValueError("seq valid value: 0 1 2 3");

    self->seqs[seq_id].trigger = args[ARG_trig].u_int;
    self->seqs[seq_id].sampleTime = args[ARG_samp].u_int;
    self->seqs[seq_id].convertTimes = args[ARG_conv].u_int;

    uint len;
    mp_obj_t *items;
    uint chns = 0x00;

    mp_obj_get_array(args[ARG_chnn].u_obj, &len, &items);

    for(uint i = 0; i < len; i++)
    {
        chns |= (1 << mp_obj_get_int(items[i]));
    }
    if(chns > 0xFFF)
        mp_raise_ValueError("chns invalid");

    self->seqs[seq_id].channels = chns;

    char pin_name[8] = {0};
    char af_name[16] = {0};

    for(uint i = 0; i < ADC_NUM_CHANNELS; i++)
    {
        if(self->seqs[seq_id].channels & (1 << i))
        {
            if(self->adc_id == PYB_ADC_0)
            {
                if(i < 7)
                {
                    snprintf(pin_name, 8, "PC%u", 6-i);
                    snprintf(af_name, 16, "PC%u_ADC0_CH%u", 6-i, i);
                }
                else if(i < 11)
                {
                    snprintf(pin_name, 8, "PA%u", 22-i);
                    snprintf(af_name, 16, "PA%u_ADC0_CH%u", 22-i, i);
                }
                else
                {
                    snprintf(pin_name, 8, "PA%u", 10);
                    snprintf(af_name, 16, "PA%u_ADC0_CH%u", 10, 11);
                }
            }
            else
            {
                if(i < 2)
                {
                    snprintf(pin_name, 8, "PD%u", 1-i);
                    snprintf(af_name, 16, "PD%u_ADC1_CH%u", 1-i, i);
                }
                else if(i < 7)
                {
                    snprintf(pin_name, 8, "PC%u", 15-i);
                    snprintf(af_name, 16, "PC%u_ADC1_CH%u", 15-i, i);
                }
                else
                {
                    mp_raise_ValueError("ADC1 has only 7 channels");
                }
            }

            pin_config_by_name(pin_name, af_name);
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(adc_seq_config_obj, 3, adc_seq_config);


static mp_obj_t adc_start(mp_obj_t self_in, mp_obj_t seq_id)
{
    pyb_adc_obj_t *self = self_in;

    ADC_Start(self->ADCx, (1 << mp_obj_get_int(seq_id)));

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(adc_start_obj, adc_start);


static mp_obj_t adc_any(mp_obj_t self_in, mp_obj_t seq_id)
{
    pyb_adc_obj_t *self = self_in;

    bool any = ADC_DataAvailable(self->ADCx, (1 << mp_obj_get_int(seq_id)));

    return mp_obj_new_bool(any);
}
static MP_DEFINE_CONST_FUN_OBJ_2(adc_any_obj, adc_any);


static mp_obj_t adc_read(mp_obj_t self_in, mp_obj_t seq_id)
{
    pyb_adc_obj_t *self = self_in;

    uint32_t chn;
    uint val = ADC_Read(self->ADCx, (1 << mp_obj_get_int(seq_id)), &chn);

    mp_obj_t chn_val = mp_obj_new_tuple(2, NULL);
    mp_obj_t *items = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(chn_val))->items;
    
    items[0] = mp_obj_new_int(chn);
    items[1] = mp_obj_new_int(val);
    
    return chn_val;
}
static MP_DEFINE_CONST_FUN_OBJ_2(adc_read_obj, adc_read);


static const mp_rom_map_elem_t adc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_seq_config),  MP_ROM_PTR(&adc_seq_config_obj) },
    { MP_ROM_QSTR(MP_QSTR_start),       MP_ROM_PTR(&adc_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_any),         MP_ROM_PTR(&adc_any_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&adc_read_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_TRIG_SW),   MP_ROM_INT(ADC_TRIGGER_SW) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_TIM2), MP_ROM_INT(ADC_TRIGGER_TIMER2) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_TIM3), MP_ROM_INT(ADC_TRIGGER_TIMER3) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_EXT0), MP_ROM_INT(ADC_TRIGGER_EXTRIG0) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_EXT1), MP_ROM_INT(ADC_TRIGGER_EXTRIG1) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_PWM0), MP_ROM_INT(ADC_TRIGGER_PWM0) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_PWM1), MP_ROM_INT(ADC_TRIGGER_PWM1) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_PWM2), MP_ROM_INT(ADC_TRIGGER_PWM2) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_PWM3), MP_ROM_INT(ADC_TRIGGER_PWM3) },
    { MP_ROM_QSTR(MP_QSTR_TRIG_PWM4), MP_ROM_INT(ADC_TRIGGER_PWM4) },
};
static MP_DEFINE_CONST_DICT(adc_locals_dict, adc_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    pyb_adc_type,
    MP_QSTR_ADC,
    MP_TYPE_FLAG_NONE,
    print, adc_print,
    make_new, adc_make_new,
    locals_dict, &adc_locals_dict
);
