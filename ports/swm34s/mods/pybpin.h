#ifndef __MODS_PYBPIN_H__
#define __MODS_PYBPIN_H__

typedef struct {
	const qstr        name;
	uint32_t          value;
} pin_af_t;

typedef struct {
    const mp_obj_base_t base;
    const qstr          name;
    PORT_TypeDef       *port;
    GPIO_TypeDef       *gpio;
    uint8_t             pbit;           // pin bit
    qstr                af;             // pin_af_t name
    const pin_af_t     *afs;
    uint8_t             afn;            // alternative function count
    uint8_t             dir;
    uint8_t             pull;

    uint8_t             IRQn;
    uint8_t             irq_trigger;
    uint8_t             irq_priority;   // 中断优先级
    mp_obj_t            irq_callback;   // 中断处理函数
} pin_obj_t;

#include "pins.h"

extern const mp_obj_type_t pin_type;

extern const mp_obj_dict_t pins_locals_dict;

pin_obj_t *pin_find_by_name(mp_obj_t name);

void pin_config(pin_obj_t *self, uint alt, uint dir, uint pull);
void pin_config_by_name(const char *pin_name, const char *af_name);
void pin_config_by_func(mp_obj_t obj, const char *format, uint id);

#endif
