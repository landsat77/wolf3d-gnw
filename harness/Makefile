# Wolfenstein 3D (Wolf4SDL) — headless Linux harness and the music renderer
#
#   make            → build/wf_host, build/wf_render
#   make SAN=1      → with AddressSanitizer + UBSan
#
#   ./build/wf_host Wolf3D.wf -t 20000 -e 1000 -o /tmp/wf

BUILD_DIR = build
TARGET = $(BUILD_DIR)/wf_host
RENDER = $(BUILD_DIR)/wf_render

WOLF = ../src/wolf
PORT = ../src/port
RDIR = ../tools/render

include $(WOLF)/sources.mk

WOLF_SOURCES = $(addprefix $(WOLF)/,$(WOLF_FILES))
# the port code built with the game (wl_def.h, the game heap)
GLUED = wf_sdl wf_sound wf_core wf_state
PLAIN = wf_alloc wf_pack wf_file wf_music

SAN ?= 0
OPT ?= -O2 -g
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer -DWF_HOST_SYSTEM_MALLOC
LDFLAGS += -fsanitize=address,undefined
endif

INCLUDES = -I$(PORT) -I$(PORT)/sdl -I$(WOLF)
WARN = -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-missing-braces \
	-Wno-sign-compare -Wno-format-truncation -Wno-misleading-indentation -Wno-parentheses
GLUE = -include $(PORT)/gnw_wf_glue.h
CFLAGS = $(OPT) -MMD -std=gnu11 -DGNW $(WARN) $(INCLUDES)
GAME_CFLAGS = $(CFLAGS) $(GLUE)

WOLF_OBJECTS = $(addprefix $(BUILD_DIR)/w_,$(WOLF_FILES:.c=.o))
GLUED_OBJECTS = $(addprefix $(BUILD_DIR)/p_,$(addsuffix .o,$(GLUED)))
PLAIN_OBJECTS = $(addprefix $(BUILD_DIR)/p_,$(addsuffix .o,$(PLAIN)))

all: $(TARGET) $(RENDER)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/w_%.o: $(WOLF)/%.c | $(BUILD_DIR)
	$(CC) -c $(GAME_CFLAGS) $< -o $@

$(GLUED_OBJECTS): $(BUILD_DIR)/p_%.o: $(PORT)/%.c | $(BUILD_DIR)
	$(CC) -c $(GAME_CFLAGS) $< -o $@

$(PLAIN_OBJECTS): $(BUILD_DIR)/p_%.o: $(PORT)/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/h_wf_host.o: wf_host.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(WOLF_OBJECTS) $(GLUED_OBJECTS) $(PLAIN_OBJECTS) $(BUILD_DIR)/h_wf_host.o
	$(CC) $^ $(LDFLAGS) -lm -o $@

$(RENDER): $(RDIR)/wf_render.c $(RDIR)/opl.c $(RDIR)/opl.h | $(BUILD_DIR)
	$(CC) -O2 -std=gnu11 -DGNW -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
		$(RDIR)/wf_render.c $(RDIR)/opl.c -lm -o $@

-include $(wildcard $(BUILD_DIR)/*.d)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
