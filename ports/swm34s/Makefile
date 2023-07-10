# Select the board to build for: if not given on the command line, then default to SWM34SLite
BOARD ?= SWM34SLite
ifeq ($(wildcard boards/$(BOARD)/.),)
$(error Invalid BOARD specified)
endif

# If the build directory is not given, make it reflect the board name.
BUILD ?= build-$(BOARD)


CROSS_COMPILE ?= arm-none-eabi-
include ../../py/mkenv.mk

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h $(BUILD)/pins_qstr.h
include $(TOP)/py/py.mk

MICROPY_VFS_FAT = 1
include $(TOP)/extmod/extmod.mk


H_INC = \
	-I. \
	-I$(TOP) \
	-I$(TOP)/lib/cmsis/inc \
	-Iboards/$(BOARD) \
	-I$(BUILD) \
	-I$(BUILD)/genhdr \
	-Ichip \
	-Imisc \
	-Imods \


CFLAGS += -mthumb -mcpu=cortex-m33 -mfloat-abi=soft -fsingle-precision-constant
CFLAGS += -ffunction-sections -fdata-sections -funsigned-char -O1 -g  $(H_INC)

LDFLAGS += -nostdlib -T swm34s.ld -Map=$@.map --gc-sections --print-memory-usage


C_SRC = \
	main.c \
	mphalport.c \
	$(BUILD)/pins.c \
	misc/bufhelper.c \
	misc/gccollect.c \
	misc/random.c \

C_MODS = $(addprefix mods/, \
	modmachine.c \
	pybpin.c \
	pybuart.c \
	pybflash.c \
    pybtimer.c \
	pybspi.c \
	pybi2c.c \
	pybcan.c \
	pybadc.c \
	pybpwm.c \
    pybrtc.c \
    pybwdt.c \
)

C_SHARED = $(addprefix shared/, \
	libc/string0.c \
	runtime/pyexec.c \
	runtime/interrupt_char.c \
	runtime/sys_stdio_mphal.c \
	runtime/gchelper_generic.c \
	readline/readline.c \
	timeutils/timeutils.c \
)

C_CHIP = $(addprefix chip/, \
	SWM341_adc.c \
	SWM341_can.c \
	SWM341_cordic.c \
	SWM341_crc.c \
	SWM341_dac.c \
	SWM341_div.c \
	SWM341_dma.c \
	SWM341_dma2d.c \
	SWM341_exti.c \
	SWM341_flash.c \
	SWM341_gpio.c \
	SWM341_i2c.c \
	SWM341_iofilt.c \
	SWM341_jpeg.c \
	SWM341_lcd.c \
	SWM341_port.c \
	SWM341_pwm.c \
	SWM341_qei.c \
	SWM341_rtc.c \
	SWM341_sdio.c \
	SWM341_sdram.c \
	SWM341_sfc.c \
	SWM341_sleep.c \
	SWM341_spi.c \
	SWM341_timr.c \
	SWM341_uart.c \
	SWM341_usbd.c \
	SWM341_usbh.c \
	SWM341_wdt.c \
	system_SWM341.c \
)

S_CHIP = $(addprefix chip/, \
	startup/gcc/startup_SWM341.s \
)

C_LIBM = $(addprefix lib/libm/, \
	acoshf.c \
	asinfacosf.c \
	asinhf.c \
	atan2f.c \
	atanf.c \
	atanhf.c \
	ef_rem_pio2.c \
	ef_sqrt.c \
	erf_lgamma.c \
	fmodf.c \
	kf_cos.c \
	kf_rem_pio2.c \
	kf_sin.c \
	kf_tan.c \
	log1pf.c \
	math.c \
	nearbyintf.c \
	roundf.c \
	sf_cos.c \
	sf_erf.c \
	sf_frexp.c \
	sf_ldexp.c \
	sf_modf.c \
	sf_sin.c \
	sf_tan.c \
	wf_lgamma.c \
	wf_tgamma.c \
)

OBJ  = $(PY_O) 
OBJ += $(addprefix $(BUILD)/, $(C_SRC:.c=.o) $(C_MODS:.c=.o) $(C_SHARED:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(C_CHIP:.c=.o) $(S_CHIP:.s=.o) $(C_LIBM:.c=.o))


# List of sources for qstr extraction
SRC_QSTR += $(C_MODS) $(C_SHARED)


all: $(BUILD)/micropython.elf

$(BUILD)/micropython.elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^ $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
	$(Q)$(CROSS_COMPILE)objdump -d $@ > $@.dis
	$(Q)$(OBJCOPY) -O binary $@ $@.bin


# Making OBJ use an order-only dependency on the generated pins.h file has the side 
# effect of making the pins.h file before we actually compile any of the objects. 
# The normal dependency generation will deal with the case when pins.h is modified. 
$(OBJ): | $(HEADER_BUILD)/pins.h

# Call make-pins.py to generate both pins.c and pins.h
$(BUILD)/pins.c $(HEADER_BUILD)/pins.h $(BUILD)/pins_qstr.h: chip/SWM341_port.h boards/SWM341_prefix.c | $(HEADER_BUILD)
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) boards/make-pins.py --af chip/SWM341_port.h --prefix boards/SWM341_prefix.c --hdr $(BUILD)/pins.h --qstr $(BUILD)/pins_qstr.h --af-const $(HEADER_BUILD)/pins_af_const.h > $(BUILD)/pins.c


# always include MicroPython make rules
include $(TOP)/py/mkrules.mk
