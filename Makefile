export TOPDIR ?= $(CURDIR)
CFILES ?= $(notdir $(wildcard $(VPATH)/*.c))
DISTDIR ?= atmosphere/contents/4E532D6374657374

APP_AUTHOR = qwerty吃小庄
APP_JSON = $(VPATH)/app.json
APP_TITLE = NS-ctest-mock
APP_VERSION = 0.1.0
ASFLAGS = -g
CFLAGS = -g -O3 -ffunction-sections -std=gnu23 -Wall -Wextra -Wpedantic $(foreach dir,$(PORTLIBS) $(LIBNX),-isystem $(dir)/include) -I$(DEPSDIR) -D__SWITCH__
DEPSDIR = $(TOPDIR)/build
LD = $(CC)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g -Wl,-Map,$(APP_TITLE).map
LIBPATHS = $(foreach dir,$(PORTLIBS) $(LIBNX),-L$(dir)/lib)
LIBS = -lnx
OFILES = $(CFILES:.c=.o)
OUTPUT = $(DEPSDIR)/$(APP_TITLE)
VPATH = $(TOPDIR)/src

include $(DEVKITPRO)/libnx/switch_rules

ifneq ($(notdir $(CURDIR)), build)

.PHONY: all clean lint
all: $(DISTDIR)/flags/boot2.flag $(DISTDIR)/exefs.nsp $(DISTDIR)/toolbox.json
$(DISTDIR)/flags/boot2.flag:
	@mkdir -p $(@D)
	@touch $@
$(DISTDIR)/exefs.nsp: $(OUTPUT).nsp
	@mkdir -p $(@D)
	@ln -f $< $@
$(DISTDIR)/toolbox.json: $(VPATH)/toolbox.json
	@mkdir -p $(@D)
	@ln -f $< $@
$(OUTPUT).nsp: $(APP_JSON) $(CFILES)
	@mkdir -p $(DEPSDIR)
	@$(MAKE) -C $(DEPSDIR) -f $(TOPDIR)/Makefile
clean:
	@rm -rf atmosphere $(DEPSDIR)
lint:
	@set -e; for source in $(CFILES); do \
		echo lint ... $$(basename $$source); \
		$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -fsyntax-only $(VPATH)/$$source; \
	done

else

.PHONY: all
all: $(OUTPUT).nsp
$(OUTPUT).nsp: $(OUTPUT).nso $(OUTPUT).npdm
$(OUTPUT).nso: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(OFILES:.o=.d)

endif
