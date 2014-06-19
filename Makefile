
#
# fxQuake Makefile for GNU Make
#

DEBUG?=N

MOUNT_DIR=.
BUILD_DIR=build
QBASE_DIR?=.

#
# SETUP ENVIRONMENT
#

ifneq (,$(findstring $(shell uname -s),FreeBSD NetBSD OpenBSD))
UNIX=bsd
else
ifneq (,$(findstring Linux,$(shell uname -s)))
UNIX=linux
endif
endif

CC=gcc
STRIP=strip

LDFLAGS=-L/usr/local/lib -lm -lX11 -lXpm -lXext -lXxf86dga -lXxf86vm -lGL
BASE_CFLAGS=-I/usr/local/include -DQBASEDIR="$(QBASE_DIR)" -Wall -Wno-trigraphs

ifeq ($(DEBUG),Y)
CFLAGS=$(BASE_CFLAGS) -DDEBUG -g
do_strip=
else
CFLAGS=$(BASE_CFLAGS) -DNDEBUG -O2 -ffast-math -frename-registers -fweb
cmd_strip=$(STRIP) $(1)
define do_strip
	$(call cmd_strip,$(1))
endef
endif

DO_OBJ_CC=$(CC) $(CFLAGS) -o $@ -c $<

#
# RULES FOR MAKE
#

OBJ_DIR=$(BUILD_DIR)/obj

BIN=fxquake

.PHONY: default clean
default: all
all: $(OBJ_DIR) $(BIN)

$(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(MOUNT_DIR)/%.c
	$(DO_OBJ_CC)

#
# OBJS
#

OBJS= \
	cd_$(UNIX).o \
	chase.o \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_tent.o \
	cmd.o \
	common.o \
	console.o \
	crc.o \
	cvar.o \
	gl_anim.o \
	gl_draw.o \
	gl_efrag.o \
	gl_light.o \
	gl_main.o \
	gl_mesh.o \
	gl_misc.o \
	gl_model.o \
	gl_part.o \
	gl_screen.o \
	gl_surf.o \
	host.o \
	host_cmd.o \
	in_x.o \
	keys.o \
	mathlib.o \
	menu.o \
	net_dgrm.o \
	net_loop.o \
	net_main.o \
	net_bsd.o \
	net_udp.o \
	pr_cmds.o \
	pr_edict.o \
	pr_exec.o \
	sbar.o \
	snd_dma.o \
	snd_mem.o \
	snd_mix.o \
	snd_unix.o \
	sv_main.o \
	sv_move.o \
	sv_phys.o \
	sv_user.o \
	sys_unix.o \
	vid_glx.o \
	view.o \
	wad.o \
	world.o \
	zone.o

#
# BUILD PROJECT
#

$(BIN): $(OBJ_DIR) $(addprefix $(OBJ_DIR)/,$(OBJS))
	$(CC) $(CFLAGS) -o $@ $(addprefix $(OBJ_DIR)/,$(OBJS)) $(LDFLAGS)
	$(call do_strip,$@)
	mv $@ $(BUILD_DIR)/

#
# CLEAN PROJECT
#

clean:
	rm -rf $(BUILD_DIR)
