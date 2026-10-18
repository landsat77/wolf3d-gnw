# Wolfenstein 3D (Wolf4SDL) for Game & Watch Retro-Go SD — GWHB homebrew.
#
#   make                  — build + pack → Wolf3D.bin
#   make docker           — same build inside the Retro-Go SD builder image
#   make harness          — headless Linux test harness (harness/wf_host.c)
#
# SD card (Retro-Go SD v2.0.0 or later):
#   /homebrews/Wolf3D.bin (this homebrew)
#   /homebrews/Wolf3D.wf  (game data: tools/wf_pack.py, same name)
#
# Verbose compiler lines: make V=

#######################################
# Project identity
#######################################
PROJECT_KIND := homebrew

CORE_NAME  := wf
CORE_ENTRY := app_main

HB_NAME    := Wolfenstein 3D
PACKED_BIN := Wolf3D.bin

WOLF_SRC := src/wolf
WF_PORT  := src/port

include $(WOLF_SRC)/sources.mk

# Wolf4SDL on the mini SDL of the port (the game heap, stdio of the pack:
# gnw_wf_glue.h)
WOLF_C_SOURCES := $(addprefix $(WOLF_SRC)/,$(WOLF_FILES)) \
	$(WF_PORT)/wf_sdl.c $(WF_PORT)/wf_sound.c $(WF_PORT)/wf_core.c $(WF_PORT)/wf_state.c

# Game & Watch side: heap, asset pack, files, music, retro-go glue
CORE_C_SOURCES := $(WOLF_C_SOURCES) \
$(WF_PORT)/wf_alloc.c \
$(WF_PORT)/wf_pack.c \
$(WF_PORT)/wf_file.c \
$(WF_PORT)/wf_libc.c \
$(WF_PORT)/wf_music.c \
$(WF_PORT)/wf_main.c

CORE_C_INCLUDES := -I$(WF_PORT) -I$(WF_PORT)/sdl -I$(WOLF_SRC)

CORE_C_DEFS := -DPROJECT_KIND_HOMEBREW=1 -DWF_DEVICE=1

OPT := -O2

CORE_LDLIBS := -lgcc

GNW_CORE_SDK ?= sdk
BUILD_DIR ?= build/$(PROJECT_KIND)

include $(GNW_CORE_SDK)/Makefile

#######################################
# Wolf4SDL specific flags
#######################################
WOLF_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(WOLF_C_SOURCES:.c=.o)))
WOLF_FLAGS := -DGNW -include $(WF_PORT)/gnw_wf_glue.h -std=gnu11 \
	-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
	-Wno-unused-parameter -Wno-missing-braces -Wno-format-truncation -Wno-sign-compare \
	-Wno-misleading-indentation -Wno-parentheses -Wno-maybe-uninitialized
$(WOLF_OBJECTS): CFLAGS += $(WOLF_FLAGS)
$(C_OBJECTS): Makefile

#######################################
# Packed header version
#######################################
CORE_VERSION ?= $(shell git describe --tags --dirty 2>/dev/null || echo NOTAG)

#######################################
# Pack (GWHB)
#######################################
PACK_HOMEBREW := $(GNW_CORE_SDK)/tools/pack_homebrew.py
GEN_COVER     := scripts/gen_homebrew_cover.py
COVER_JPG     := $(BUILD_DIR)/cover.jpg
COVER_WIDTH   ?= 128
COVER_HEIGHT  ?= 96

.PHONY: pack cover
cover: $(COVER_JPG)

# Must stay <= 186x100 and <= 10 KiB (firmware cover cache).
$(COVER_JPG): $(GEN_COVER) | $(BUILD_DIR)
	$(V)$(ECHO) "[ COVER ] $(COVER_JPG) $(COVER_WIDTH)x$(COVER_HEIGHT)"
	$(V)python3 $(GEN_COVER) \
		--out $(COVER_JPG) \
		--title "$(HB_NAME)" \
		--width $(COVER_WIDTH) \
		--height $(COVER_HEIGHT)

pack: $(TARGET_BIN) $(COVER_JPG)
	$(V)$(ECHO) [ PACK GWHB ] $(PACKED_BIN) version=$(CORE_VERSION)
	$(V)python3 $(PACK_HOMEBREW) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--name "$(HB_NAME)" --version "$(CORE_VERSION)" \
		--cover $(COVER_JPG) \
		--out $(PACKED_BIN)

all: pack

clean::
	$(V)rm -f $(PACKED_BIN) $(COVER_JPG)

# Read-only helpers for CI / scripts (make print-PACKED_BIN, etc.).
.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-DOCKER_IMAGE \
	print-TARGET_ELF print-TARGET_MAP print-CORE_VERSION
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-DOCKER_IMAGE:
	@echo $(DOCKER_IMAGE)
print-TARGET_ELF:
	@echo $(TARGET_ELF)
print-TARGET_MAP:
	@echo $(BUILD_DIR)/$(CORE_NAME)_core.map
print-CORE_VERSION:
	@echo $(CORE_VERSION)

#######################################
# Headless Linux harness (same game + port code, virtual clock)
#######################################
.PHONY: harness
harness:
	$(MAKE) -C harness

#######################################
# Docker (same image as the firmware repo)
#######################################
.PHONY: docker docker_pull docker_shell

RELEASE_VERSION ?= v1.5
DOCKER_REPOSITORY ?= sylverb/retro-go-sd-builder
DOCKER_IMAGE ?= $(DOCKER_REPOSITORY):$(RELEASE_VERSION)

DOCKER_TTY_FLAG := $(shell if [ -t 0 ]; then echo -it; else echo; fi)
DOCKER_USER := $(shell id -u):$(shell id -g)
DOCKER_RUN := docker run --rm $(DOCKER_TTY_FLAG) \
	--user $(DOCKER_USER) \
	-v "$(CURDIR):/opt/workdir" \
	-w /opt/workdir \
	$(DOCKER_IMAGE)

docker:
	$(V)$(ECHO) "[ DOCKER ]" $(DOCKER_IMAGE)
	$(V)$(DOCKER_RUN) make --no-print-directory -j$$(nproc)

docker_pull:
	$(V)docker pull $(DOCKER_IMAGE)

docker_shell:
	$(DOCKER_RUN) bash
