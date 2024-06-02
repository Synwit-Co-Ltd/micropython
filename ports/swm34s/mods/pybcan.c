#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "py/objtuple.h"
#include "py/objarray.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/binary.h"
#include "py/stream.h"
#include "py/mphal.h"

#include "misc/bufhelper.h"

#include "mods/pybpin.h"
#include "mods/pybcan.h"


#define CAN_TX_SUCC       1   // Transmit Success
#define CAN_TX_FAIL       2   // Transmit Failure

#define CAN_STAT_EACT     0   // ERROR ACTIVE,  1 error_counter <   96
#define CAN_STAT_EWARN    1   // ERROR WARNING, 1 error_counter >=  96
#define CAN_STAT_EPASS    2   // ERROR PASSIVE, 1 error_counter >= 127
#define CAN_STAT_BUSOFF   3   // BUS OFF,       1 error_counter >= 255


/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef enum {
    PYB_CAN_0   = 0,
    PYB_CAN_1   = 1,
    PYB_NUM_CANS
} pyb_can_id_t;


typedef struct {
    mp_obj_base_t base;
    mp_uint_t can_id;
    CAN_TypeDef *CANx;
    uint baudrate;
    uint8_t  mode;

    struct {
        uint8_t  mode;              // CAN_FRAME_STD、CAN_FRAME_EXT
        union {
            uint32_t mask32b;		// check & mask == ID & mask 的Message通过过滤
            struct {				// 1 must match    0 don't care
                uint16_t mask16b1;
                uint16_t mask16b2;
            };
        };
        union {
            uint32_t check32b;
            struct {
                uint16_t check16b1;
                uint16_t check16b2;
            };
        };
    } filters[16];

    uint8_t  IRQn;
    uint8_t  irq_flags;
    uint8_t  irq_trigger;
    uint8_t  irq_priority;   // 中断优先级
    mp_obj_t irq_callback;   // 中断处理函数
} pyb_can_obj_t;


/******************************************************************************
 DEFINE PRIVATE DATA
 ******************************************************************************/
static pyb_can_obj_t pyb_can_obj[PYB_NUM_CANS] = {
    { {&pyb_can_type}, .can_id=PYB_CAN_0, .CANx=CAN0, .IRQn=CAN0_IRQn },
    { {&pyb_can_type}, .can_id=PYB_CAN_1, .CANx=CAN1, .IRQn=CAN1_IRQn },
};


/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static void set_filter(pyb_can_obj_t *self, uint32_t filter_id, mp_obj_t *filter)
{
    uint len;
    mp_obj_t *items;

    mp_obj_get_array(filter, &len, &items);

    self->filters[filter_id].mode = mp_obj_get_int(items[0]);

    if(self->filters[filter_id].mode == CAN_FRAME_EXT)  // (CAN_FRAME_EXT, (check32b, mask32b))
    {
        mp_obj_get_array(items[1], &len, &items);
        self->filters[filter_id].check32b = mp_obj_get_int(items[0]);
        self->filters[filter_id].mask32b  = mp_obj_get_int(items[1]);
    }
    else                                    // (CAN_FRAME_STD, (check16b, mask16b), (check16b, mask16b))
    {
        uint len2;
        mp_obj_t *items2;

        mp_obj_get_array(items[1], &len2, &items2);
        self->filters[filter_id].check16b1 = mp_obj_get_int(items2[0]);
        self->filters[filter_id].mask16b1  = mp_obj_get_int(items2[1]);

        mp_obj_get_array(items[2], &len2, &items2);
        self->filters[filter_id].check16b2 = mp_obj_get_int(items2[0]);
        self->filters[filter_id].mask16b2  = mp_obj_get_int(items2[1]);
    }

    if(self->filters[filter_id].mode == CAN_FRAME_EXT)
    {
        CAN_SetFilter32b(self->CANx, filter_id, self->filters[filter_id].check32b, self->filters[filter_id].mask32b);
    }
    else
    {
        CAN_SetFilter16b(self->CANx, filter_id, self->filters[filter_id].check16b1, self->filters[filter_id].mask16b1,
                                                self->filters[filter_id].check16b2, self->filters[filter_id].mask16b2);
    }
}


static mp_obj_t get_filter(pyb_can_obj_t *self, uint32_t filter_id)
{
    if(self->filters[filter_id].mode == CAN_FRAME_EXT)
    {
        mp_obj_t filter = mp_obj_new_tuple(2, NULL);
        mp_obj_t *field = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(filter))->items;
        
        field[0] = MP_OBJ_NEW_QSTR(MP_QSTR_FRAME_EXT);
        field[1] = mp_obj_new_tuple(2, NULL);
        
        mp_obj_t *items = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(field[1]))->items;
        items[0] = mp_obj_new_int(self->filters[filter_id].check32b);
        items[1] = mp_obj_new_int(self->filters[filter_id].mask32b);

        return filter;
    }
    else
    {
        mp_obj_t filter = mp_obj_new_tuple(3, NULL);
        mp_obj_t *field = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(filter))->items;
        
        field[0] = MP_OBJ_NEW_QSTR(MP_QSTR_FRAME_STD);
        field[1] = mp_obj_new_tuple(2, NULL);
        field[2] = mp_obj_new_tuple(2, NULL);
        
        mp_obj_t *items = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(field[1]))->items;
        items[0] = mp_obj_new_int(self->filters[filter_id].check16b1);
        items[1] = mp_obj_new_int(self->filters[filter_id].mask16b1);
        
        items = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(field[2]))->items;
        items[0] = mp_obj_new_int(self->filters[filter_id].check16b2);
        items[1] = mp_obj_new_int(self->filters[filter_id].mask16b2);

        return filter;
    }
}


static void get_bit_time(uint baudrate, uint *bs1, uint *bs2)
{
    switch(baudrate)
    {
    case 1000000:   // BRP = 60M/1M/(1+9+5) = 4
    case 500000:
    case 250000:
    case 125000:    // BRP = 60M/125k/(1+9+5) = 32
        *bs1 = CAN_BS1_9tq;
        *bs2 = CAN_BS2_5tq;
        break;

    case 800000:    // BRP = 60M/800k/(1+16+8) = 3
    case 400000:
    case 200000:
    case 100000:    // BRP = 60M/100k/(1+16+8) = 24
        *bs1 = CAN_BS1_16tq;
        *bs2 = CAN_BS2_8tq;
        break;

    default:
        mp_raise_ValueError("unsupported baudrate");
    }
}


/******************************************************************************/
// MicroPython bindings

static void can_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    pyb_can_obj_t *self = self_in;

    qstr mode;
    switch(self->mode)
    {
        case CAN_MODE_NORMAL:   mode = MP_QSTR_Normal;    break;
        case CAN_MODE_LISTEN:   mode = MP_QSTR_Listen;    break;
        case CAN_MODE_SELFTEST: mode = MP_QSTR_SelfTest;  break;
    }

    mp_printf(print, "CAN(%u, %u, mode=%q)", self->can_id, self->baudrate, mode);
}


static mp_obj_t can_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_id, ARG_baudrate, ARG_mode, ARG_irq, ARG_callback, ARG_priority, ARG_rxd, ARG_txd };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,       MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_baudrate,	MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 100000} },
        { MP_QSTR_mode,     MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = CAN_MODE_NORMAL} },
        { MP_QSTR_irq,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_callback, MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_priority, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 4} },
        { MP_QSTR_rxd,      MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_txd,      MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint can_id = args[ARG_id].u_int;
    if(can_id >= PYB_NUM_CANS)
    {
        mp_raise_OSError(MP_ENODEV);
    }
    pyb_can_obj_t *self = &pyb_can_obj[can_id];

    self->mode = args[ARG_mode].u_int;
    if((self->mode != CAN_MODE_NORMAL) && (self->mode != CAN_MODE_LISTEN) && (self->mode != CAN_MODE_SELFTEST))
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
    }

    if(args[ARG_rxd].u_obj != mp_const_none)
    {
        pin_config_by_func(args[ARG_rxd].u_obj, "%s_CAN%d_RX", self->can_id);
    }

    if(args[ARG_txd].u_obj != mp_const_none)
    {
        pin_config_by_func(args[ARG_txd].u_obj, "%s_CAN%d_TX", self->can_id);
    }

    uint bs1, bs2;
    self->baudrate = args[ARG_baudrate].u_int;
    get_bit_time(self->baudrate, &bs1, &bs2);

    CAN_InitStructure CAN_initStruct;
    CAN_initStruct.Mode = self->mode;
    CAN_initStruct.CAN_bs1 = bs1;
    CAN_initStruct.CAN_bs2 = bs2;
    CAN_initStruct.CAN_sjw = CAN_SJW_2tq;
    CAN_initStruct.Baudrate = self->baudrate;
    CAN_initStruct.RXNotEmptyIEn = 0;
    CAN_initStruct.ArbitrLostIEn = 0;
    CAN_initStruct.ErrPassiveIEn = 0;
    CAN_Init(self->CANx, &CAN_initStruct);
    CAN_Open(self->CANx);

	return self;
}


static mp_obj_t can_baudrate(size_t n_args, const mp_obj_t *args)
{
    pyb_can_obj_t *self = args[0];

    if(n_args == 1)     // get
    {
        return mp_obj_new_int(self->baudrate);
    }
    else                // set
    {
        uint bs1, bs2;
        self->baudrate = mp_obj_get_int(args[1]);
        get_bit_time(self->baudrate, &bs1, &bs2);

        CAN_SetBaudrate(self->CANx, self->baudrate, bs1, bs2, CAN_SJW_2tq);

        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(can_baudrate_obj, 1, 2, can_baudrate);


static mp_obj_t can_any(mp_obj_t self_in)
{
    pyb_can_obj_t *self = self_in;

    return mp_obj_new_bool(CAN_RXDataAvailable(self->CANx));
}
static MP_DEFINE_CONST_FUN_OBJ_1(can_any_obj, can_any);


static mp_obj_t can_full(mp_obj_t self_in)
{
    pyb_can_obj_t *self = self_in;

    return mp_obj_new_bool(!CAN_TXBufferReady(self->CANx));
}
static MP_DEFINE_CONST_FUN_OBJ_1(can_full_obj, can_full);


static mp_obj_t can_done(mp_obj_t self_in)
{
    pyb_can_obj_t *self = self_in;
    
    if(self->CANx->SR & CAN_SR_TXBR_Msk)
    {
        if(self->CANx->SR & CAN_SR_TXOK_Msk)
            return mp_obj_new_int(CAN_TX_SUCC);
        else
            return mp_obj_new_int(CAN_TX_FAIL);
    }
    else
    {
        return mp_const_false;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(can_done_obj, can_done);


// used when can_full() return False
static mp_obj_t can_send(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum { ARG_id, ARG_data, ARG_format, ARG_remoteReq, ARG_retry };
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,        MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_data,                        MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_format,    MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = CAN_FRAME_STD} },
        { MP_QSTR_remoteReq, MP_ARG_KW_ONLY  | MP_ARG_BOOL,{.u_bool= false} },
        { MP_QSTR_retry,     MP_ARG_KW_ONLY  | MP_ARG_BOOL,{.u_bool= true} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    pyb_can_obj_t *self = pos_args[0];

    // get the buffer to send from
    mp_buffer_info_t bufinfo;
    uint8_t tmp[1];
    pyb_buf_get_for_send(args[ARG_data].u_obj, &bufinfo, tmp);

    if(bufinfo.len > 8)
    {
        mp_raise_ValueError("CAN data more than 8 bytes");
    }

    if(args[ARG_remoteReq].u_bool)
    {
        CAN_TransmitRequest(self->CANx, args[ARG_format].u_int, args[ARG_id].u_int, !args[ARG_retry].u_bool);
    }
    else
    {
        CAN_Transmit(self->CANx, args[ARG_format].u_int, args[ARG_id].u_int, bufinfo.buf, bufinfo.len, !args[ARG_retry].u_bool);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(can_send_obj, 3, can_send);


// used when can_any() return True
static mp_obj_t can_read(mp_obj_t self_in)
{
    pyb_can_obj_t *self = self_in;

    CAN_RXMessage msg;

    CAN_Receive(self->CANx, &msg);

    mp_obj_t tuple = mp_obj_new_tuple(3, NULL);
    mp_obj_t *items = ((mp_obj_tuple_t*)MP_OBJ_TO_PTR(tuple))->items;

    items[0] = (msg.format == CAN_FRAME_STD) ? MP_OBJ_NEW_QSTR(MP_QSTR_FRAME_STD) : MP_OBJ_NEW_QSTR(MP_QSTR_FRAME_EXT);
    items[1] = mp_obj_new_int(msg.id);
    if(msg.remote)
        items[2] = mp_const_true;
    else
        items[2] = mp_obj_new_bytes(&msg.data[0], msg.size);

    return tuple;
}
static MP_DEFINE_CONST_FUN_OBJ_1(can_read_obj, can_read);


static mp_obj_t can_state(mp_obj_t self_in)
{
    pyb_can_obj_t *self = self_in;

    uint state = 0;
    uint can_sr = self->CANx->SR;
    uint can_if = self->CANx->IF;

    if(can_sr & CAN_SR_BUSOFF_Msk)
        state = CAN_STAT_BUSOFF;
    else if(can_if & CAN_IF_ERRPASS_Msk)
        state = CAN_STAT_EPASS;
    else if(can_sr & CAN_SR_ERRWARN_Msk)
        state = CAN_STAT_EWARN;
    else
        state = CAN_STAT_EACT;

    return mp_obj_new_int(state);
}
static MP_DEFINE_CONST_FUN_OBJ_1(can_state_obj, can_state);


static mp_obj_t can_filter(size_t n_args, const mp_obj_t *args)
{
    pyb_can_obj_t *self = args[0];

    uint filter_id = mp_obj_get_int(args[1]);

    if(n_args == 2)     // get
    {
        return get_filter(self, filter_id);
    }
    else                // set
    {
        CAN_Close(self->CANx);

        set_filter(self, filter_id, args[2]);

        CAN_Open(self->CANx);

        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(can_filter_obj, 2, 3, can_filter);


void CANx_Handler(pyb_can_obj_t *self)
{
    if(CAN_INTStat(self->CANx) & CAN_IT_RX_NOTEMPTY != 0)
    {
        //CAN_Receive(CAN, &CAN_RXMsg);
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
            // Uncaught exception; disable the callback so it doesn't run again.
            self->irq_callback = mp_const_none;

            mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
        }

        gc_unlock();
    }
    
    self->irq_flags = 0;
}

void CAN0_Handler(void)
{
    CANx_Handler(&pyb_can_obj[0]);
}

void CAN1_Handler(void)
{
    CANx_Handler(&pyb_can_obj[1]);
}


static const mp_rom_map_elem_t can_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_baudrate),     MP_ROM_PTR(&can_baudrate_obj) },
    { MP_ROM_QSTR(MP_QSTR_any),          MP_ROM_PTR(&can_any_obj) },
    { MP_ROM_QSTR(MP_QSTR_full),         MP_ROM_PTR(&can_full_obj) },
    { MP_ROM_QSTR(MP_QSTR_done),         MP_ROM_PTR(&can_done_obj) },
    { MP_ROM_QSTR(MP_QSTR_send),         MP_ROM_PTR(&can_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),         MP_ROM_PTR(&can_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_state),        MP_ROM_PTR(&can_state_obj) },
    { MP_ROM_QSTR(MP_QSTR_filter),       MP_ROM_PTR(&can_filter_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_NORMAL),       MP_ROM_INT(CAN_MODE_NORMAL) },
    { MP_ROM_QSTR(MP_QSTR_SILENT),       MP_ROM_INT(CAN_MODE_LISTEN) },      // not send ACK to the CAN bus, even when a message is received successfully
    { MP_ROM_QSTR(MP_QSTR_SELFTEST),     MP_ROM_INT(CAN_MODE_SELFTEST) },    // perform a successful transmission, even if no acknowledge is received

    { MP_ROM_QSTR(MP_QSTR_FRAME_STD),    MP_ROM_INT(CAN_FRAME_STD) },
    { MP_ROM_QSTR(MP_QSTR_FRAME_EXT),    MP_ROM_INT(CAN_FRAME_EXT) },

    { MP_ROM_QSTR(MP_QSTR_TX_SUCC),      MP_ROM_INT(CAN_TX_SUCC) },
    { MP_ROM_QSTR(MP_QSTR_TX_FAIL),      MP_ROM_INT(CAN_TX_FAIL) },
    
    { MP_ROM_QSTR(MP_QSTR_STAT_EACT),    MP_ROM_INT(CAN_STAT_EACT)   },
    { MP_ROM_QSTR(MP_QSTR_STAT_EWARN),   MP_ROM_INT(CAN_STAT_EWARN)  },
    { MP_ROM_QSTR(MP_QSTR_STAT_EPASS),   MP_ROM_INT(CAN_STAT_EPASS)  },
    { MP_ROM_QSTR(MP_QSTR_STAT_BUSOFF),  MP_ROM_INT(CAN_STAT_BUSOFF) },
};
static MP_DEFINE_CONST_DICT(can_locals_dict, can_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    pyb_can_type,
    MP_QSTR_CAN,
    MP_TYPE_FLAG_NONE,
    print, can_print,
    make_new, can_make_new,
    locals_dict, &can_locals_dict
);
