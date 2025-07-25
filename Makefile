# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Adrian "asie" Siekierka, 2023

WONDERFUL_TOOLCHAIN ?= /opt/wonderful
TARGET ?= wswan/bootfriend
include $(WONDERFUL_TOOLCHAIN)/target/$(TARGET)/makedefs.mk

# Metadata
# --------

NAME		:= ws-backup-tool

# Source code paths
# -----------------

INCLUDEDIRS	:= src
SOURCEDIRS	:= src
CBINDIRS	:= res

# Defines passed to all files
# ---------------------------

DEFINES		:= -DLIBWS_API_COMPAT=202504L

# Libraries
# ---------

LIBS		:= -lwsx -lws
LIBDIRS		:= $(WF_TARGET_DIR)

# Build artifacts
# ---------------

BUILDDIR	:= build
ELF		:= build/$(NAME).elf
ELF_STAGE1	:= build/$(NAME)_stage1.elf
EXECUTABLE	:= $(NAME).bfb

# Verbose flag
# ------------

ifeq ($(V),1)
_V		:=
else
_V		:= @
endif

# Source files
# ------------

ifneq ($(CBINDIRS),)
    SOURCES_CBIN	:= $(shell find -L $(CBINDIRS) -name "*.bin")
    INCLUDEDIRS		+= $(addprefix $(BUILDDIR)/,$(CBINDIRS))
endif
SOURCES_S	:= $(shell find -L $(SOURCEDIRS) -name "*.s")
SOURCES_C	:= $(shell find -L $(SOURCEDIRS) -name "*.c")

# Compiler and linker flags
# -------------------------

WARNFLAGS	:= -Wall

INCLUDEFLAGS	:= $(foreach path,$(INCLUDEDIRS),-I$(path)) \
		   $(foreach path,$(LIBDIRS),-I$(path)/include)

LIBDIRSFLAGS	:= $(foreach path,$(LIBDIRS),-L$(path)/lib)

ASFLAGS		+= -x assembler-with-cpp $(DEFINES) $(WF_ARCH_CFLAGS) \
		   $(INCLUDEFLAGS) -ffunction-sections -fdata-sections -fno-common

CFLAGS		+= -std=gnu11 $(WARNFLAGS) $(DEFINES) $(WF_ARCH_CFLAGS) \
		   $(INCLUDEFLAGS) -ffunction-sections -fdata-sections -fno-common -Os -g

LDFLAGS		:= -T$(WF_LDSCRIPT) $(LIBDIRSFLAGS) -Wl,--gc-sections -fno-common \
		   $(WF_ARCH_LDFLAGS) $(LIBS)

# Intermediate build files
# ------------------------

OBJS_ASSETS	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CBIN)))

HEADERS_ASSETS	:= $(patsubst %.bin,%_bin.h,$(addprefix $(BUILDDIR)/,$(SOURCES_CBIN)))

OBJS_SOURCES	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_S))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C)))

OBJS		:= $(OBJS_ASSETS) $(OBJS_SOURCES)

DEPS		:= $(OBJS:.o=.d)

# Targets
# -------

.PHONY: all clean

all: $(EXECUTABLE)

$(EXECUTABLE): $(ELF_STAGE1)
	@echo "  EXE     $@"
	$(_V)$(BUILDBFB) -o $(EXECUTABLE) --output-elf $(ELF) $(BUILDGATEFLAGS) $<

$(ELF_STAGE1): $(OBJS)
	@echo "  LD      $@"
	$(_V)$(CC) -r -o $@ $(OBJS) $(WF_CRT0) $(LDFLAGS)

clean:
	@echo "  CLEAN"
	$(_V)$(RM) $(EXECUTABLE) $(BUILDDIR)

# Rules
# -----

$(BUILDDIR)/%.s.o : %.s
	@echo "  AS      $<"
	@$(MKDIR) -p $(@D)
	$(_V)$(CC) $(ASFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(_V)$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.bin.o $(BUILDDIR)/%_bin.h : %.bin
	@echo "  BIN2C   $<"
	@$(MKDIR) -p $(@D)
	$(_V)$(WF)/bin/wf-bin2c -a 2 $(@D) $<
	$(_V)$(CC) $(CFLAGS) -MMD -MP -c -o $(BUILDDIR)/$*.bin.o $(BUILDDIR)/$*_bin.c

# Include dependency files if they exist
# --------------------------------------

-include $(DEPS)
