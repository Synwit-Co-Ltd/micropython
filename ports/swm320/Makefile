# Select the board to build for: if not given on the command line, then default to SWM320Lite
BOARD ?= SWM320Lite
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


CFLAGS += -mthumb -mcpu=cortex-m4 -mfloat-abi=soft -fsingle-precision-constant
CFLAGS += -ffunction-sections -fdata-sections -funsigned-char -O1 -g  $(H_INC)

LDFLAGS += -nostdlib -T swm320.ld -Map=$@.map --gc-sections --print-memory-usage


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
    pybpulse.c \
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
	SWM320_adc.c \
	SWM320_can.c \
	SWM320_crc.c \
	SWM320_dma.c \
	SWM320_exti.c \
	SWM320_flash.c \
	SWM320_gpio.c \
	SWM320_i2c.c \
	SWM320_lcd.c \
	SWM320_port.c \
	SWM320_pwm.c \
	SWM320_rtc.c \
	SWM320_sdio.c \
	SWM320_spi.c \
	SWM320_timr.c \
	SWM320_uart.c \
	SWM320_wdt.c \
	system_SWM320.c \
)

S_CHIP = $(addprefix chip/startup/gcc/, \
	startup_SWM320.s \
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
OBJ += $(addprefix $(BUILD)/, $(C_CHIP:.c=.o) $(S_CHIP:.s=.o) $(C_LIBM:.c=.o) )


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
$(BUILD)/pins.c $(HEADER_BUILD)/pins.h $(BUILD)/pins_qstr.h: chip/SWM320_port.h boards/SWM320_prefix.c | $(HEADER_BUILD)
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) boards/make-pins.py --af chip/SWM320_port.h --prefix boards/SWM320_prefix.c --hdr $(HEADER_BUILD)/pins.h --qstr $(BUILD)/pins_qstr.h > $(BUILD)/pins.c


# always include MicroPython make rules
include $(TOP)/py/mkrules.mk
