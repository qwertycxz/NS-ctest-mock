ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro)
endif

export TOPDIR ?= $(CURDIR)
APP_ID ?= 4E532D6374657374
CFILES ?= $(sort $(shell find $(VPATH) -type f -name *.c -printf '%P\n'))
LINTER ?= $(CC) -MMD -MP -fanalyzer -fsyntax-only $(CFLAGS)
FORMATFILES ?= $(shell git ls-files *.c *.h *.json)
ODIRS ?= $(dir $(OFILES))
V_VERSION ?= $(shell git describe --match v* --tags)

.EXTRA_PREREQS = $(TOPDIR)/Makefile
APP_AUTHOR = qwerty吃小庄
APP_JSON = $(DEPSDIR)/app.json
APP_TITLE = NS-ctest-mock
APP_VERSION = $(V_VERSION:v%=%)
CFLAGS = \
	-O3 \
	-Wall \
	-Walloc-zero \
	-Walloca \
	-Wbad-function-cast \
	-Wcast-align \
	-Wcast-qual \
	-Wduplicated-branches \
	-Wduplicated-cond \
	-Wextra \
	-Wflex-array-member-not-at-end \
	-Wformat-nonliteral \
	-Wformat-security \
	-Wformat-signedness \
	-Wformat-y2k \
	-Winit-self \
	-Winline \
	-Winvalid-pch \
	-Winvalid-utf8 \
	-Wjump-misses-init \
	-Wlogical-op \
	-Wmissing-include-dirs \
	-Wmultichar \
	-Wnested-externs \
	-Wno-old-style-declaration \
	-Wnull-dereference \
	-Wopenacc-parallelism \
	-Wpacked \
	-Wpedantic \
	-Wredundant-decls \
	-Wshadow \
	-Wstrict-prototypes \
	-Wunused-macros \
	-Wuseless-cast \
	-Wvector-operation-performance \
	-Wwrite-strings \
	-Wzero-as-null-pointer-constant \
	-std=c23 \
	$(foreach dir,$(PORTLIBS) $(LIBNX),-isystem $(dir)/include)
DEPSDIR = $(TOPDIR)/build
LD = $(CC)
LDFLAGS = -specs=$(LIBNX)/switch.specs
LIBPATHS = $(foreach dir,$(PORTLIBS) $(LIBNX),-L$(dir)/lib)
LIBS = -lnx
OFILES = $(CFILES:.c=.o)
OUTPUT = $(DEPSDIR)/$(APP_TITLE)
VPATH = $(TOPDIR)/src

include $(DEVKITPRO)/libnx/switch_rules

ifneq ($(notdir $(CURDIR)), build)

.PHONY: all clean format format.check lint lint.fix
all: atmosphere/contents/$(APP_ID)/flags/boot2.flag atmosphere/contents/$(APP_ID)/exefs.nsp atmosphere/contents/$(APP_ID)/toolbox.json
atmosphere/contents/$(APP_ID)/flags/boot2.flag:
	mkdir -p $(@D)
	touch $@
atmosphere/contents/$(APP_ID)/exefs.nsp: $(OUTPUT).nsp
	mkdir -p $(@D)
	ln -f $< $@
$(OUTPUT).nsp: $(APP_JSON)
	$(MAKE) -C $(DEPSDIR) -f $(TOPDIR)/Makefile $@
atmosphere/contents/$(APP_ID)/toolbox.json: $(DEPSDIR)/toolbox.json
	mkdir -p $(@D)
	ln -f $< $@
$(APP_JSON) $(DEPSDIR)/toolbox.json &: $(OUTPUT).elf $(VPATH)/app.py
	python $(VPATH)/app.py $(OUTPUT).elf $(APP_ID) $(APP_TITLE)
$(OUTPUT).elf: $(CFILES)
	mkdir -p $(DEPSDIR)
	$(MAKE) -C $(DEPSDIR) -f $(TOPDIR)/Makefile $@
clean:
	rm -rf atmosphere $(DEPSDIR)
format: $(FORMATFILES:%=$(DEPSDIR)/%.format)
$(DEPSDIR)/%.format: % .clang-format
	mkdir -p $(@D)
	clang-format -i --Wno-error=unknown $<
	touch $@
format.check: $(FORMATFILES:%=$(DEPSDIR)/%.format.check)
$(DEPSDIR)/%.format.check: % .clang-format
	mkdir -p $(@D)
	clang-format -n -Werror --Wno-error=unknown $<
	touch $@
lint: $(CFILES:%.c=$(DEPSDIR)/%.lint)
$(DEPSDIR)/%.lint: $(VPATH)/%.c
	mkdir -p $(@D)
	$(LINTER) -Werror -MF $(@:.lint=.lint.d) -MT $@ $<
	touch $@
lint.fix: $(CFILES:%.c=$(DEPSDIR)/%.lint.fix)
$(DEPSDIR)/%.lint.fix: $(VPATH)/%.c
	mkdir -p $(@D)
	$(LINTER) -dumpdir build/ -fdiagnostics-format=sarif-file -fdiagnostics-generate-patch -MF $(@:.lint.fix=.lint.fix.d) -MT $@ $< 2>&1 | git apply -p 0 --allow-empty --unsafe-paths
	touch $@
-include $(CFILES:%.c=$(DEPSDIR)/%.lint.d)

else

.PHONY: all
all: $(OUTPUT).nsp
$(OUTPUT).nsp: $(OUTPUT).npdm $(OUTPUT).nso
$(OUTPUT).nso: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES): | $(ODIRS:%=$(DEPSDIR)/%)
$(ODIRS:%=$(DEPSDIR)/%):
	mkdir -p $@
-include $(OFILES:.o=.d)

endif
