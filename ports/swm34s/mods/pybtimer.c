#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#include "pybtimer.h"
#include "pybpin.h"


#define TIMR_IRQ_TIMEOUT   1
#define TIMR_IRQ_CAPTURE   2


/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    PYB_TIMER_0   =  0,
    PYB_TIMER_1   =  1,
    PYB_TIMER_2   =  2,
    PYB_TIMER_3   =  3,
    PYB_TIMER_4   =  4,
    PYB_BTIMER_0  =  10,
    PYB_BTIMER_1  =  11,
    PYB_BTIMER_2  =  12,
    PYB_BTIMER_3  =  13,
    PYB_BTIMER_4  =  14,
    PYB_BTIMER_5  =  15,
    PYB_BTIMER_6  =  16,
    PYB_BTIMER_7  =  17,
    PYB_BTIMER_8  =  18,
    PYB_BTIMER_9  =  19,
    PYB_BTIMER_10 =  20,
    PYB_BTIMER_11 =  21,
    PYB_NUM_TIMERS
} pyb_timer_id_t;

typedef struct {
    mp_obj_base_t base;
    pyb_timer_id_t timer_id;
    TIMR_TypeDef *TIMRx;
    uint32_t period;
    uint8_t prediv;
    uint8_t  mode;

    uint8_t  IRQn;
    uint8_t  irq_flags;         // 中断标志
    uint8_t  irq_trigger;
    uint8_t  irq_priority;      // 中断优先级
    mp_obj_t irq_callback;      // 中断处理函数
} pyb_timer_obj_t;


/******************************************************************************
 DEFINE PRIVATE DATA
 ******************************************************************************/
static pyb_timer_obj_t pyb_timer_obj[PYB_NUM_TIMERS] = {
    { {&pyb_timer_type}, .timer_id = PYB_TIMER_0,   .TIMRx = TIMR0,   .period = 0, .IRQn = TIMR0_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_TIMER_1,   .TIMRx = TIMR1,   .period = 0, .IRQn = TIMR1_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_TIMER_2,   .TIMRx = TIMR2,   .period = 0, .IRQn = TIMR2_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_TIMER_3,   .TIMRx = TIMR3,   .period = 0, .IRQn = TIMR3_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_TIMER_4,   .TIMRx = TIMR4,   .period = 0, .IRQn = TIMR4_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_0,  .TIMRx = BTIMR0,  .period = 0, .IRQn = BTIMR0_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_1,  .TIMRx = BTIMR1,  .period = 0, .IRQn = BTIMR1_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_2,  .TIMRx = BTIMR2,  .period = 0, .IRQn = BTIMR2_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_3,  .TIMRx = BTIMR3,  .period = 0, .IRQn = BTIMR3_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_4,  .TIMRx = BTIMR4,  .period = 0, .IRQn = BTIMR4_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_5,  .TIMRx = BTIMR5,  .period = 0, .IRQn = BTIMR5_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_6,  .TIMRx = BTIMR6,  .period = 0, .IRQn = BTIMR6_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_7,  .TIMRx = BTIMR7,  .period = 0, .IRQn = BTIMR7_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_8,  .TIMRx = BTIMR8,  .period = 0, .IRQn = BTIMR8_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_9,  .TIMRx = BTIMR9,  .period = 0, .IRQn = BTIMR9_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_10, .TIMRx = BTIMR10, .period = 0, .IRQn = BTIMR10_IRQn },
    { {&pyb_timer_type}, .timer_id = PYB_BTIMER_10, .TIMRx = BTIMR11, .period = 0, .IRQn = BTIMR11_IRQn },
};


/******************************************************************************/
/* MicroPython bindings                                                      */

static void timer_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    pyb_timer_obj_t *self = self_in;

    switch(self->mode)
    {
    case TIMR_MODE_TIMER:
        mp_printf(print, "Timer(%u, period=%uuS)", self->timer_id, self->period);
        break;

    case TIMR_MODE_COUNTER:
        mp_printf(print, "Counter(%u, period=%u)", self->timer_id, self->period);
        break;
    }
}


static mp_obj_t timer_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_id, ARG_period, ARG_mode, ARG_irq, ARG_callback,  ARG_priority, ARG_pin };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,        MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_period,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 100000} },
        { MP_QSTR_mode,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = TIMR_MODE_TIMER} },
        { MP_QSTR_irq,       MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = TIMR_IRQ_TIMEOUT} },
        { MP_QSTR_callback,  MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_priority,  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 3} },
        { MP_QSTR_pin,       MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    uint timer_id = args[ARG_id].u_int;
    if(timer_id >= PYB_NUM_TIMERS)
    {
        mp_raise_OSError(MP_ENODEV);
    }
    pyb_timer_obj_t *self = &pyb_timer_obj[timer_id];

    self->period = args[ARG_period].u_int;

    self->mode = args[ARG_mode].u_int;
    if(self->mode == TIMR_MODE_TIMER)
    {
        self->prediv = CyclesPerUs;
    }
    else if(self->mode == TIMR_MODE_COUNTER)
    {
        self->prediv = 1;

        if(self->timer_id >= PYB_BTIMER_0)
            mp_raise_ValueError("BTIMR cannot work as Counter");

        if(args[ARG_pin].u_obj != mp_const_none)
        {
            pin_config_by_func(args[ARG_pin].u_obj, "%s_TIMR%d_IN", self->timer_id);
        }
    }
    else
    {
        mp_raise_ValueError("invalid mode value");
    }

    self->irq_trigger  = args[ARG_irq].u_int;
    self->irq_callback = args[ARG_callback].u_obj;
    self->irq_priority = args[ARG_priority].u_int;
    if(self->irq_priority > 7)
    {
        mp_raise_ValueError("invalid priority value");
    }
    else
    {
        NVIC_SetPriority(self->IRQn, self->irq_priority);
        NVIC_EnableIRQ(self->IRQn);
    }

    TIMR_Init(self->TIMRx, self->mode, self->prediv, self->period, self->irq_trigger & TIMR_IRQ_TIMEOUT);

    return self;
}


static mp_obj_t timer_period(size_t n_args, const mp_obj_t *args)
{
    pyb_timer_obj_t *self = args[0];

    if(n_args == 1)     // get
    {
        return mp_obj_new_int(self->period);
    }
    else                // set
    {
        self->period = mp_obj_get_int(args[1]);

        self->TIMRx->LOAD = self->period - 1;

        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(timer_period_obj, 1, 2, timer_period);


static mp_obj_t timer_value(mp_obj_t self_in)
{
    pyb_timer_obj_t *self = self_in;

    uint value = self->period - TIMR_GetCurValue(self->TIMRx);  // 向下计数

    return mp_obj_new_int(value);
}
static MP_DEFINE_CONST_FUN_OBJ_1(timer_value_obj, timer_value);


static mp_obj_t timer_start(mp_obj_t self_in)
{
    pyb_timer_obj_t *self = self_in;

    TIMR_Start(self->TIMRx);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(timer_start_obj, timer_start);


static mp_obj_t timer_stop(mp_obj_t self_in)
{
    pyb_timer_obj_t *self = self_in;

    TIMR_Stop(self->TIMRx);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(timer_stop_obj, timer_stop);


static mp_obj_t timer_irq_flags(mp_obj_t self_in)
{
    pyb_timer_obj_t *self = self_in;

    return mp_obj_new_int(self->irq_flags);
}
static MP_DEFINE_CONST_FUN_OBJ_1(timer_irq_flags_obj, timer_irq_flags);


static mp_obj_t timer_irq_enable(mp_obj_t self_in, mp_obj_t irq_trigger)
{
    pyb_timer_obj_t *self = self_in;

    uint trigger = mp_obj_get_int(irq_trigger);

    if(trigger & TIMR_IRQ_TIMEOUT)
    {
        self->irq_trigger |= TIMR_IRQ_TIMEOUT;

        TIMR_INTEn(self->TIMRx);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(timer_irq_enable_obj, timer_irq_enable);


static mp_obj_t timer_irq_disable(mp_obj_t self_in, mp_obj_t irq_trigger)
{
    pyb_timer_obj_t *self = self_in;

    uint trigger = mp_obj_get_int(irq_trigger);

    if(trigger & TIMR_IRQ_TIMEOUT)
    {
        self->irq_trigger &= ~TIMR_IRQ_TIMEOUT;

        TIMR_INTDis(self->TIMRx);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(timer_irq_disable_obj, timer_irq_disable);


void TIMR_Handler(pyb_timer_obj_t *self)
{
    if(TIMR_INTStat(self->TIMRx))
    {
        TIMR_INTClr(self->TIMRx);

        if(self->irq_trigger & TIMR_IRQ_TIMEOUT)
            self->irq_flags |= TIMR_IRQ_TIMEOUT;
    }

    if(self->irq_callback != mp_const_none)
    {
        gc_lock();
        nlr_buf_t nlr;
        if(nlr_push(&nlr) == 0)
        {
            mp_call_function_1(self->irq_callback, self);
            nlr_pop();
        }
        else
        {
            self->irq_callback = mp_const_none;

            printf("uncaught exception in Timer(%u) interrupt handler\n", self->timer_id);
            mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
        }
        gc_unlock();
    }

    self->irq_flags = 0;
}

void TIMR0_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_TIMER_0]);
}

void TIMR1_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_TIMER_1]);
}

void TIMR2_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_TIMER_2]);
}

void TIMR3_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_TIMER_3]);
}

void TIMR4_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_TIMER_4]);
}

void BTIMR0_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_0]);
}

void BTIMR1_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_1]);
}

void BTIMR2_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_2]);
}

void BTIMR3_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_3]);
}

void BTIMR4_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_4]);
}

void BTIMR5_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_5]);
}

void BTIMR6_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_6]);
}

void BTIMR7_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_7]);
}

void BTIMR8_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_8]);
}

void BTIMR9_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_9]);
}

void BTIMR10_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_10]);
}

void BTIMR11_Handler(void)
{
    TIMR_Handler(&pyb_timer_obj[PYB_BTIMER_11]);
}


static const mp_rom_map_elem_t timer_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_period),              MP_ROM_PTR(&timer_period_obj) },
    { MP_ROM_QSTR(MP_QSTR_value),               MP_ROM_PTR(&timer_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_start),               MP_ROM_PTR(&timer_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),                MP_ROM_PTR(&timer_stop_obj) },

    { MP_ROM_QSTR(MP_QSTR_irq_flags),           MP_ROM_PTR(&timer_irq_flags_obj) },
    { MP_ROM_QSTR(MP_QSTR_irq_enable),          MP_ROM_PTR(&timer_irq_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_irq_disable),         MP_ROM_PTR(&timer_irq_disable_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_TIMER),               MP_ROM_INT(TIMR_MODE_TIMER) },
    { MP_ROM_QSTR(MP_QSTR_COUNTER),             MP_ROM_INT(TIMR_MODE_COUNTER) },

    { MP_ROM_QSTR(MP_QSTR_IRQ_TIMEOUT),         MP_ROM_INT(TIMR_IRQ_TIMEOUT) },
//  { MP_ROM_QSTR(MP_QSTR_IRQ_CAPTURE),         MP_ROM_INT(TIMR_IRQ_CAPTURE) },
};
static MP_DEFINE_CONST_DICT(timer_locals_dict, timer_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    pyb_timer_type,
    MP_QSTR_Timer,
    MP_TYPE_FLAG_NONE,
    print, timer_print,
    make_new, timer_make_new,
    locals_dict, &timer_locals_dict
);
