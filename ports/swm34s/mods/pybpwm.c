#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#include "bufhelper.h"

#include "pybpin.h"
#include "pybpwm.h"


/// \moduleref pyb
/// \class PWM - Pulse-Width Modulation


/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    PYB_PWM_0   =  0,
    PYB_PWM_1   =  1,
    PYB_PWM_2   =  2,
    PYB_PWM_3   =  3,
    PYB_PWM_4   =  4,
    PYB_NUM_PWMS
} pyb_pwm_id_t;

typedef struct _pyb_pwm_obj_t {
    mp_obj_base_t base;
    pyb_pwm_id_t pwm_id;
    PWM_TypeDef *PWMx;
    uint16_t clkdiv;
    uint16_t period;
    uint16_t dutyA;
    uint16_t dutyB;
    uint16_t deadzoneA;
    uint16_t deadzoneB;

    uint8_t mode;
    uint8_t trigger;    // TO_ADC
} pyb_pwm_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static pyb_pwm_obj_t pyb_pwm_obj[PYB_NUM_PWMS] = {
    { {&pyb_pwm_type}, .pwm_id = PYB_PWM_0, .PWMx = PWM0 },
    { {&pyb_pwm_type}, .pwm_id = PYB_PWM_1, .PWMx = PWM1 },
    { {&pyb_pwm_type}, .pwm_id = PYB_PWM_2, .PWMx = PWM2 },
    { {&pyb_pwm_type}, .pwm_id = PYB_PWM_3, .PWMx = PWM3 },
    { {&pyb_pwm_type}, .pwm_id = PYB_PWM_4, .PWMx = PWM4 },
};


/******************************************************************************/
/* MicroPython bindings                                                      */
/******************************************************************************/
static void pwm_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    pyb_pwm_obj_t *self = self_in;

    mp_printf(print, "PWM(%d, clkdiv=%u, period=%u, dutyA=%u, dutyB=%u)", self->pwm_id, self->clkdiv, self->period, self->dutyA, self->dutyB);
}


static mp_obj_t pwm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_id, ARG_clkdiv, ARG_period, ARG_mode, ARG_dutyA, ARG_dutyB, ARG_deadzoneA, ARG_deadzoneB, ARG_pinA, ARG_pinAN, ARG_pinB, ARG_pinBN };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,        MP_ARG_REQUIRED | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_clkdiv,    MP_ARG_REQUIRED | MP_ARG_INT,  {.u_int = 1} },
        { MP_QSTR_period,    MP_ARG_REQUIRED | MP_ARG_INT,  {.u_int = 10000} },
        { MP_QSTR_mode,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = PWM_EDGE_ALIGNED} },
        { MP_QSTR_dutyA,     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int =  2500} },
        { MP_QSTR_dutyB,     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int =  7500} },
        { MP_QSTR_deadzoneA, MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_deadzoneB, MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_pinA,      MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_pinAN,     MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_pinB,      MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_pinBN,     MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    };

    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint pwm_id = args[ARG_id].u_int;
    if(pwm_id >= PYB_NUM_PWMS)
    {
        mp_raise_OSError(MP_ENODEV);
    }
    pyb_pwm_obj_t *self = &pyb_pwm_obj[pwm_id];

    self->clkdiv    = args[ARG_clkdiv].u_int;
    self->period    = args[ARG_period].u_int;
    self->mode      = args[ARG_mode].u_int;
    self->dutyA     = args[ARG_dutyA].u_int;
    self->dutyB     = args[ARG_dutyB].u_int;
    self->deadzoneA = args[ARG_deadzoneA].u_int;
    self->deadzoneB = args[ARG_deadzoneB].u_int;

    if(args[ARG_pinA].u_obj != mp_const_none)
    {
        pin_config_by_func(args[ARG_pinA].u_obj, "%s_PWM%dA", self->pwm_id);
    }

    if(args[ARG_pinAN].u_obj != mp_const_none)
    {
        pin_config_by_func(args[ARG_pinAN].u_obj, "%s_PWM%dAN", self->pwm_id);
    }

    if(args[ARG_pinB].u_obj != mp_const_none)
    {
        pin_config_by_func(args[ARG_pinB].u_obj, "%s_PWM%dB", self->pwm_id);
    }

    if(args[ARG_pinBN].u_obj != mp_const_none)
    {
        pin_config_by_func(args[ARG_pinBN].u_obj, "%s_PWM%dBN", self->pwm_id);
    }

    PWM_InitStructure  PWM_initStruct;
    PWM_initStruct.Mode = self->mode;
	PWM_initStruct.Clkdiv = self->clkdiv;
	PWM_initStruct.Period = self->period;
	PWM_initStruct.HdutyA =  self->dutyA;
	PWM_initStruct.DeadzoneA = self->deadzoneA;
	PWM_initStruct.IdleLevelA = 0;
	PWM_initStruct.IdleLevelAN= 0;
	PWM_initStruct.OutputInvA = 0;
	PWM_initStruct.OutputInvAN= 0;
	PWM_initStruct.HdutyB =  self->dutyB;
	PWM_initStruct.DeadzoneB = self->deadzoneB;
	PWM_initStruct.IdleLevelB = 0;
	PWM_initStruct.IdleLevelBN= 0;
	PWM_initStruct.OutputInvB = 0;
	PWM_initStruct.OutputInvBN= 0;
	PWM_initStruct.UpOvfIE    = 0;
	PWM_initStruct.DownOvfIE  = 0;
	PWM_initStruct.UpCmpAIE   = 0;
	PWM_initStruct.DownCmpAIE = 0;
	PWM_initStruct.UpCmpBIE   = 0;
	PWM_initStruct.DownCmpBIE = 0;
	PWM_Init(self->PWMx, &PWM_initStruct);

    return self;
}

static mp_obj_t pwm_start(mp_obj_t self_in)
{
    pyb_pwm_obj_t *self = self_in;

    PWM_Start(1 << self->pwm_id);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pwm_start_obj, pwm_start);


static mp_obj_t pwm_stop(mp_obj_t self_in)
{
    pyb_pwm_obj_t *self = self_in;

    PWM_Stop(1 << self->pwm_id);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pwm_stop_obj, pwm_stop);


static mp_obj_t pwm_period(size_t n_args, const mp_obj_t *args)
{
    pyb_pwm_obj_t *self = args[0];

    if(n_args == 1)
    {
        return MP_OBJ_NEW_SMALL_INT(self->period);
    }
    else
    {
        self->period = mp_obj_get_int(args[1]);
        PWM_SetPeriod(self->PWMx, self->period);

        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pwm_period_obj, 1, 2, pwm_period);


static mp_obj_t pwm_duty(size_t n_args, const mp_obj_t *args)
{
    pyb_pwm_obj_t *self = args[0];

    uint ch = mp_obj_get_int(args[1]);

    if(n_args == 2)
    {
        return MP_OBJ_NEW_SMALL_INT(ch == PWM_CH_A ? self->dutyA : self->dutyB);
    }
    else
    {
        if(ch == PWM_CH_A)
        {
            self->dutyA = mp_obj_get_int(args[2]);
            PWM_SetHDuty(self->PWMx, PWM_CH_A, self->dutyA);
        }
        else
        {
            self->dutyB = mp_obj_get_int(args[2]);
            PWM_SetHDuty(self->PWMx, PWM_CH_B, self->dutyB);
        }
        

        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pwm_duty_obj, 2, 3, pwm_duty);


static mp_obj_t pwm_deadzone(size_t n_args, const mp_obj_t *args)
{
    pyb_pwm_obj_t *self = args[0];

    uint ch = mp_obj_get_int(args[1]);

    if(n_args == 2)
    {
        return MP_OBJ_NEW_SMALL_INT(ch == PWM_CH_A ? self->deadzoneA : self->deadzoneB);
    }
    else
    {
        if(ch == PWM_CH_A)
        {
            self->deadzoneA = mp_obj_get_int(args[2]);
            PWM_SetDeadzone(self->PWMx, PWM_CH_A, self->deadzoneA);
        }
        else
        {
            self->deadzoneB = mp_obj_get_int(args[2]);
            PWM_SetDeadzone(self->PWMx, PWM_CH_B, self->deadzoneB);
        }
        
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pwm_deadzone_obj, 2, 3, pwm_deadzone);


static const mp_rom_map_elem_t pwm_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_start),           MP_ROM_PTR(&pwm_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),            MP_ROM_PTR(&pwm_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_period),          MP_ROM_PTR(&pwm_period_obj) },
    { MP_ROM_QSTR(MP_QSTR_duty),            MP_ROM_PTR(&pwm_duty_obj) },
    { MP_ROM_QSTR(MP_QSTR_deadzone),        MP_ROM_PTR(&pwm_deadzone_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_CH_A),            MP_ROM_INT(PWM_CH_A) },
    { MP_ROM_QSTR(MP_QSTR_CH_B),            MP_ROM_INT(PWM_CH_B) },
    { MP_ROM_QSTR(MP_QSTR_EDGE_ALIGNED),    MP_ROM_INT(PWM_EDGE_ALIGNED) },
    { MP_ROM_QSTR(MP_QSTR_CENTER_ALIGNED),  MP_ROM_INT(PWM_CENTER_ALIGNED) },
};
static MP_DEFINE_CONST_DICT(pwm_locals_dict, pwm_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    pyb_pwm_type,
    MP_QSTR_PWM,
    MP_TYPE_FLAG_NONE,
    print, pwm_print,
    make_new, pwm_make_new,
    locals_dict, &pwm_locals_dict
);
