#!/usr/bin/make -f

include Makefile.config

# Faster alternative makefile, for development only.

C := \
	modules/image_special/pdp_chrot.c \
	modules/image_special/pdp_scanxy.c \
	modules/image_special/pdp_array.c \
	modules/image_special/pdp_grey2mask.c \
	modules/image_special/pdp_scan.c \
	modules/image_special/pdp_histo.c \
	modules/image_special/pdp_scale.c \
	modules/image_special/pdp_cog.c \
	modules/image_special/pdp_scope.c \
	modules/image_basic/pdp_cheby.c \
	modules/image_basic/pdp_stateless.c \
	modules/image_basic/pdp_bq.c \
	modules/image_basic/pdp_mul.c \
	modules/image_basic/pdp_gain.c \
	modules/image_basic/pdp_logic.c \
	modules/image_basic/pdp_mix.c \
	modules/image_basic/pdp_add.c \
	modules/image_basic/pdp_conv.c \
	modules/image_basic/pdp_plasma.c \
	modules/image_basic/pdp_constant.c \
	modules/image_basic/pdp_randmix.c \
	modules/image_basic/pdp_noise.c \
	modules/image_basic/pdp_zoom.c \
	modules/matrix_basic/pdp_mat_vec.c \
	modules/matrix_basic/pdp_mat_lu.c \
	modules/matrix_basic/pdp_mat_mul.c \
	modules/generic/pdp_route.c \
	modules/generic/pdp_convert.c \
	modules/generic/pdp_udp_send.c \
	modules/generic/pdp_del.c \
	modules/generic/pdp_description.c \
	modules/generic/pdp_inspect.c \
	modules/generic/pdp_loop.c \
	modules/generic/pdp_rawin.c \
	modules/generic/pdp_snap.c \
	modules/generic/pdp_rawout.c \
	modules/generic/pdp_udp_receive.c \
	modules/generic/pdp_metro.c \
	modules/generic/pdp_trigger.c \
	modules/generic/pdp_reg.c \
	modules/image_io/pdp_glx.c \
	modules/image_io/pdp_v4l.c \
	modules/image_io/pdp_xv.c \
	modules/image_io/pdp_qt.c \
	modules/test/pdp_dpd_test.c \
	puredata/pdp_comm.c \
	puredata/pdp_imagebase.c \
	puredata/pdp_control.c \
	puredata/pdp_queue.c \
	puredata/pdp_dpd_base.c \
	puredata/pdp_ut.c \
	puredata/pdp_base.c \
	puredata/pdp_compat.c \
	system/type/pdp_image.c \
	system/type/pdp_matrix.c \
	system/type/pdp_bitmap.c \
	system/image/pdp_llconv_portable.c \
	system/image/pdp_llconv.c \
	system/image/pdp_resample.c \
	system/image/pdp_imageproc_common.c \
	system/image/pdp_imageproc_gcc_mmx.c \
	system/pdp.c \
	system/png/pdp_png.c \
	system/kernel/pdp_mem.c \
	system/kernel/pdp_packet.c \
	system/kernel/pdp_debug.c \
	system/kernel/pdp_dpd_command.c \
	system/kernel/pdp_post.c \
	system/kernel/pdp_symbol.c \
	system/kernel/pdp_type.c \
	system/kernel/pdp_list.c \
	system/net/pdp_net.c \
	system/zl/v4l.c \
	system/zl/glx.c \
	system/zl/xv.c \
	system/zl/xwindow.c \
	modules/image_io/pdp_sdl.c \


C_SCAF := \
	scaf/pdp/pdp_ca_system.c \
	scaf/pdp/pdp_ca.c \

#	debug/teststuff.c \
#	modules/matrix_basic/clusterstuff.c \
#	system/kernel/pdp_packet2.c \

C_OPENGL := \
	opengl/modules/pdp_3d_state.c \
	opengl/modules/pdp_3d_view.c \
	opengl/modules/pdp_3d_subcontext.c \
	opengl/modules/pdp_3d_context.c \
	opengl/modules/pdp_3d_push.c \
	opengl/modules/pdp_3d_draw.c \
	opengl/modules/pdp_3d_windowcontext.c \
	opengl/modules/pdp_3d_dlist.c \
	opengl/modules/pdp_3d_light.c \
	opengl/modules/pdp_3d_for.c \
	opengl/modules/pdp_3d_drawmesh.c \
	opengl/modules/pdp_3d_color.c \
	opengl/modules/pdp_3d_snap.c \
	opengl/system/setup.c \
	opengl/system/pdp_texture.c \
	opengl/system/pdp_3Dcontext_common.c \
	opengl/system/pdp_3dp_base.c \
	opengl/system/pdp_opengl.c \
	opengl/system/pdp_mesh.c \
	opengl/system/pdp_3Dcontext_glx.c \


C_MMX := \
	system/image/pdp_llconv_mmx.c \
	system/image/pdp_imageproc_mmx.c \
	system/mmx/pdp_mmx_test.c \

CC := gcc
# CFLAGS := -Iinclude -Iopengl/include -Iscaf/include
CFLAGS_MMX := -mmmx # Necessary on i32
CFLAGS := -I../ -I../system -Isystem -Iinclude -Iopengl/include -Iscaf/include -fPIC -O3 $(PDP_CFLAGS) $(CFLAGS_MMX) -g #-DHAVE_LIBV4L1_VIDEODEV_H 
BUILD := build
ARCH := pd_linux
LDFLAGS := -rdynamic -shared
LIBS :=  $(PDP_LIBS)
PDP := $(BUILD)/pdp.$(ARCH)


# .SECONDARY:  ## This is tricky stuff...
.DELETE_ON_ERROR:

.PHONY: all
all: $(PDP)

.PHONY: clean
clean:
	rm -rf build

O := $(patsubst %.c,$(BUILD)/%.o,$(C))
D := $(O:.o=.d)
$(BUILD)/%.d: %.c
	@echo [d] $(notdir $@)
	@mkdir -p $(dir $@)
	@$(CC) -MT $(basename $@).o -MM $(CFLAGS) $< >$@
-include $(D)

$(BUILD)/%.o: %.c $(BUILD)/%.d
	@echo [o] $(notdir $@)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pdp.$(ARCH): $(O)
	@echo [pd_linux] $(notdir $@)
	@$(CC) $(LDFLAGS) -o $@ $(O) $(LIBS)

%.dasm: %.o
	objdump -d $< >$@

.PHONY: test
PD := pd.local
test: $(PDP) $(BUILD)/system/image/pdp_imageproc_gcc_mmx.dasm
	cd test ; PDP_TEST=v4l $(PD) -nogui test-v4l.pd

# Symlink
pdp.$(ARCH): $(BUILD)/pdp.$(ARCH)
	rm -f $@
	ln -s $< $@


cscope.files: 
	find \
		-regex '.*\.[ch][px]?[px]?' \
		-print >$@

cscope: cscope.files
#	cscope -kbq
	cscope -kb
