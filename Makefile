.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET      := NS-ctest-mock
APP_TITLEID := 4E532D6374657374
BUILD       := build
SOURCES     := src
OUT_DIR     := dist/$(APP_TITLEID)
BUILD_NSP   := $(BUILD)/$(TARGET).nsp

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS := -g -Wall -O2 -ffunction-sections -std=gnu23 \
	$(ARCH) $(DEFINES)
CFLAGS += $(INCLUDE) -D__SWITCH__

ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(TARGET).map

LIBS    := -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(BUILD)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(CURDIR)/$(SOURCES)
export DEPSDIR := $(CURDIR)/$(BUILD)
export LD := $(CC)

CFILES := $(notdir $(wildcard $(SOURCES)/*.c))
SFILES := $(notdir $(wildcard $(SOURCES)/*.s))

export OFILES := $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_JSON := $(TOPDIR)/$(TARGET).json
export BUILD_EXEFS_SRC :=

.PHONY: all clean build

all: $(OUT_DIR)/flags/boot2.flag $(OUT_DIR)/exefs.nsp $(OUT_DIR)/toolbox.json

$(BUILD_NSP): build
	@test -f $@

build:
	@mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(OUT_DIR)/flags/boot2.flag:
	@mkdir -p $(@D)
	@touch $@

$(OUT_DIR)/exefs.nsp: $(BUILD_NSP)
	@mkdir -p $(@D)
	@cp $< $@

$(OUT_DIR)/toolbox.json: toolbox.json
	@mkdir -p $(@D)
	@cp $< $@

clean:
	@rm -rf $(BUILD) out

else

DEPENDS := $(OFILES:.o=.d)

.PHONY: all

all: $(OUTPUT).nsp

$(OUTPUT).nsp: $(OUTPUT).nso $(OUTPUT).npdm
$(OUTPUT).nso: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
