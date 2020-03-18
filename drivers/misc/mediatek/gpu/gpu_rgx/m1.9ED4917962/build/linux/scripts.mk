########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

ifeq ($(SUPPORT_ANDROID_PLATFORM),)

define if-component
 ifneq ($$(filter $(1),$$(COMPONENTS)),)
  M4DEFS += $(2)
 endif
endef

define if-kernel-component
 ifneq ($$(filter $(1),$$(KERNEL_COMPONENTS)),)
  M4DEFS_K += $(2)
 endif
endef

# common.m4 lives here
#
M4FLAGS := -I$(MAKE_TOP)/scripts

# These defs are required for both KM and UM install script.
M4DEFS_K := \
 -DPVRVERSION="$(PVRVERSION)" \
 -DPVR_BUILD_DIR=$(PVR_BUILD_DIR) \
 -DPVRSRV_MODNAME=$(PVRSRV_MODNAME)

ifeq ($(PDUMP),1)
 M4DEFS_K += -DPDUMP=1
endif

ifneq ($(DISPLAY_CONTROLLER),)
 $(eval $(call if-kernel-component,$(DISPLAY_CONTROLLER),\
  -DDISPLAY_CONTROLLER=$(DISPLAY_CONTROLLER)))
endif

ifneq ($(HDMI_CONTROLLER),)
 $(eval $(call if-kernel-component,$(HDMI_CONTROLLER),\
  -DHDMI_CONTROLLER=$(HDMI_CONTROLLER)))
endif

# These defs are not derived from user variables
#
M4DEFS := \
 -DSOLIB_VERSION=$(PVRVERSION_MAJ).$(PVRVERSION_MIN).$(PVRVERSION_BUILD)

M4DEFS += \
  -DRGX_FW_FILENAME=$(RGX_FW_FILENAME)

ifeq ($(MESA_EGL),1)
 M4DEFS += -DSUPPORT_MESA=1
endif

ifeq ($(SUPPORT_BUILD_LWS),1)
 M4DEFS += -DLWS_INSTALL_TREE=1
endif

ifeq ($(SUPPORT_RAY_TRACING),1)
 M4DEFS += -DSUPPORT_RAY_TRACING=1
endif

# XOrg support options are convoluted, so don't bother with if-component.
ifneq ($(filter pvr_video,$(COMPONENTS)),) # This is an X build
 M4DEFS += -DSUPPORT_XORG=1
 M4DEFS += -DXORG_EXPLICIT_PVR_SERVICES_LOAD=$(XORG_EXPLICIT_PVR_SERVICES_LOAD)

 ifneq ($(MESA_EGL),1)
  $(eval $(call if-component,opengl_mesa,-DSUPPORT_MESA=1))
 endif

 M4DEFS += -DPVR_XORG_DESTDIR=$(LWS_PREFIX)/bin
 M4DEFS += -DPVR_CONF_DESTDIR=$(XORG_CONFDIR)

 ifneq ($(SUPPORT_BUILD_LWS),)
  $(eval $(call if-component,pvr_input,-DSUPPORT_DDX_INPUT=1))
  $(eval $(call if-component,opengl_mesa,-DSUPPORT_LIBGL=1))
 endif
else ifeq ($(WINDOW_SYSTEM),wayland)
 M4DEFS += -DSUPPORT_WAYLAND=1
else ifeq ($(WINDOW_SYSTEM),surfaceless)
 M4DEFS += -DSUPPORT_SURFACELESS=1
else
 $(eval $(call if-component,opengl,-DSUPPORT_LIBGL=1))
endif

# Map other COMPONENTS on to SUPPORT_ defs
#
$(eval $(call if-component,opengles1,\
 -DSUPPORT_OPENGLES1=1 -DOGLES1_MODULE=$(opengles1_target) \
 -DSUPPORT_OPENGLES1_V1_ONLY=0))
$(eval $(call if-component,opengles3,\
 -DSUPPORT_OPENGLES3=1 -DOGLES3_MODULE=$(opengles3_target)))
$(eval $(call if-component,egl,\
 -DSUPPORT_LIBEGL=1 -DEGL_MODULE=$(egl_target)))
$(eval $(call if-component,libglslcompiler,\
 -DSUPPORT_SOURCE_SHADER=1))
$(eval $(call if-component,opencl,\
 -DSUPPORT_OPENCL=1 -DOPENCL_MODULE=$(opencl_target)))
$(eval $(call if-component,pvrcldnn,\
 -DSUPPORT_CLDNN=1))
$(eval $(call if-component,openrl,\
 -DSUPPORT_OPENRL=1))
$(eval $(call if-component,renderscript,\
 -DSUPPORT_RSC=1))
$(eval $(call if-component,renderscript_sha1,\
 -DSUPPORT_RSC_SHA1=1))
$(eval $(call if-component,vulkan,\
 -DSUPPORT_UF_WRITER=1))
$(eval $(call if-component,libufwriter,\
 -DSUPPORT_UF_WRITER=1))
$(eval $(call if-component,rscompiler,\
 -DSUPPORT_RS_COMPILER=1))
$(eval $(call if-component,rscreplay,\
 -DSUPPORT_RSC_REPLAY=1))
$(eval $(call if-component,opengl opengl_mesa,\
 -DSUPPORT_OPENGL=1))
$(eval $(call if-component,vulkan,\
	-DSUPPORT_VULKAN=1))
$(eval $(call if-component,pvr_vk_loader,\
 -DSUPPORT_VK_LOADER=internal))
$(eval $(call if-component,vklayer_img_systrace,\
 -DSUPPORT_VKLAYER_IMG_SYSTRACE=1))
$(eval $(call if-component,trace_layer,\
 -DSUPPORT_VK_TRACING_EXT=1))
$(eval $(call if-component,null_ws,\
 -DSUPPORT_NULL_WS=1))
$(eval $(call if-component,gigacluster_gles,\
 -DSUPPORT_GIGACLUSTER=1 -DSUPPORT_GIGACLUSTER_GLES=1, -DGIGACLUSTER_GLES_MODULE=$(gigacluster_gles_target)))
$(eval $(call if-component,gigacluster_egl,\
 -DSUPPORT_GIGACLUSTER=1 -DSUPPORT_GIGACLUSTER_EGL=1 -DGIGACLUSTER_EGL_MODULE=$(gigacluster_egl_target)))
$(eval $(call if-component,gigacluster_ws,\
 -DSUPPORT_GIGACLUSTER=1 -DSUPPORT_GIGACLUSTER_WS=1))
$(eval $(call if-component,null_drm_ws,\
 -DSUPPORT_NULL_DRM_WS=1))
$(eval $(call if-component,null_remote,\
 -DSUPPORT_NULL_REMOTE=1))
$(eval $(call if-component,ews_ws,\
 -DSUPPORT_EWS=1))
$(eval $(call if-component,ews_wm,\
 -DSUPPORT_LUA=1))
$(eval $(call if-component,graphicshal,\
 -DSUPPORT_GRAPHICS_HAL=1))
$(eval $(call if-component,xeglinfo,\
 -DSUPPORT_XUNITTESTS=1))
$(eval $(call if-component,rgx_compute_test,\
 -DSUPPORT_COMPUTE=1))
$(eval $(call if-component,libpvrdebugger,\
 -DSUPPORT_DEBUGGER=1))

ifeq ($(PVR_REMOTE),1)
 M4DEFS += -DPVR_REMOTE=1
endif

ifeq ($(PVR_REMVIEW),1)
 M4DEFS += -DPVR_REMVIEW=1
endif

ifeq ($(SUPPORT_BUILD_LWS),1)
# Build UM script using old scheme using M4
define create-install-um-script-m4
$(RELATIVE_OUT)/$(1)/install_um.sh: $(PVRVERSION_H) $(CONFIG_MK) \
 $(MAKE_TOP)/scripts/common.m4 \
 $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_um.sh.m4 \
 | $(RELATIVE_OUT)/$(1)
	$$(if $(V),,@echo "  GEN     " $$(call relative-to-top,$$@))
	$(M4) $(M4FLAGS) $(M4DEFS) $(M4DEFS_K) \
	  $(MAKE_TOP)/scripts/common.m4 \
	  $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_um.sh.m4 > $$@
install_script: $(RELATIVE_OUT)/$(1)/install_um.sh
endef

$(foreach _t,$(TARGET_ALL_ARCH),$(eval $(call create-install-um-script-m4,$(_t))))
endif

init_script_install_path := $${RC_DESTDIR}

$(TARGET_NEUTRAL_OUT)/rc.pvr: $(PVRVERSION_H) $(CONFIG_MK) $(CONFIG_KERNEL_MK) \
 $(MAKE_TOP)/scripts/rc.pvr.m4 $(MAKE_TOP)/scripts/common.m4 \
 $(MAKE_TOP)/$(PVR_BUILD_DIR)/rc.pvr.m4 \
 | $(TARGET_NEUTRAL_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(M4) $(M4FLAGS) $(M4DEFS) $(M4DEFS_K) $(MAKE_TOP)/scripts/rc.pvr.m4 \
		$(MAKE_TOP)/$(PVR_BUILD_DIR)/rc.pvr.m4 > $@
	$(CHMOD) +x $@
init_script: $(TARGET_NEUTRAL_OUT)/rc.pvr

$(GENERATED_CODE_OUT)/init_script:
	$(make-directory)

$(GENERATED_CODE_OUT)/init_script/.install: init_script_install_path := $(init_script_install_path)
$(GENERATED_CODE_OUT)/init_script/.install: | $(GENERATED_CODE_OUT)/init_script
	@echo 'install_file rc.pvr $(init_script_install_path)/rc.pvr "boot script" 0755 0:0' >$@
endif # ifeq ($(SUPPORT_ANDROID_PLATFORM),)

# This code mimics the way Make processes our implicit/explicit goal list.
# It tries to build up a list of components that were actually built, from
# whence an install script is generated.
#
ifneq ($(MAKECMDGOALS),)
BUILT_UM := $(MAKECMDGOALS)
ifneq ($(filter build services_all components,$(MAKECMDGOALS)),)
BUILT_UM += $(COMPONENTS)
endif
BUILT_UM := $(sort $(filter $(ALL_MODULES) init_script xorg wl,$(BUILT_UM)))
else
BUILT_UM := $(sort $(COMPONENTS))
endif

ifneq ($(MAKECMDGOALS),)
BUILT_KM := $(MAKECMDGOALS)
ifneq ($(filter build services_all kbuild,$(MAKECMDGOALS)),)
BUILT_KM += $(KERNEL_COMPONENTS)
endif
BUILT_KM := $(sort $(filter $(ALL_MODULES),$(BUILT_KM)))
else
BUILT_KM := $(sort $(KERNEL_COMPONENTS))
endif

INSTALL_UM_MODULES := \
 $(strip $(foreach _m,$(BUILT_UM),\
  $(if $(filter doc module_group,$($(_m)_type)),,\
   $(if $(filter host_%,$($(_m)_arch)),,\
    $(if $($(_m)_install_path),$(_m),\
     $(warning WARNING: UM $(_m)_install_path not defined))))))

# Build up a list of installable shared libraries. The shared_library module
# type is specially guaranteed to define $(_m)_target, even if the Linux.mk
# itself didn't. The list is formatted with <module>:<target> pairs e.g.
# "moduleA:libmoduleA.so moduleB:libcustom.so" for later processing.
ALL_SHARED_INSTALLABLE := \
 $(sort $(foreach _a,$(ALL_MODULES),\
  $(if $(filter shared_library,$($(_a)_type)),$(_a):$($(_a)_target),)))

# Handle implicit install dependencies. Executables and shared libraries may
# be linked against other shared libraries. Avoid requiring the user to
# specify the program's binary dependencies explicitly with $(m)_install_extra
INSTALL_UM_MODULES := \
 $(sort $(INSTALL_UM_MODULES) \
  $(foreach _a,$(ALL_SHARED_INSTALLABLE),\
   $(foreach _m,$(INSTALL_UM_MODULES),\
    $(foreach _l,$($(_m)_libs),\
     $(if $(filter lib$(_l).so,$(word 2,$(subst :, ,$(_a)))),\
                               $(word 1,$(subst :, ,$(_a))))))))

# Add explicit dependencies that must be installed 
INSTALL_UM_MODULES := \
 $(sort $(INSTALL_UM_MODULES) \
  $(foreach _m,$(INSTALL_UM_MODULES),\
   $($(_m)_install_dependencies)))

define calculate-um-fragments
# Work out which modules are required for this arch.
INSTALL_UM_MODULES_$(1) := \
 $$(strip $$(foreach _m,$(INSTALL_UM_MODULES),\
  $$(if $$(filter $(1),$$(INTERNAL_ARCH_LIST_FOR_$$(_m))),$$(_m))))

INSTALL_UM_FRAGMENTS_$(1) := $$(foreach _m,$$(INSTALL_UM_MODULES_$(1)),$(RELATIVE_OUT)/$(1)/intermediates/$$(_m)/.install)

.PHONY: install_um_$(1)_debug
install_um_$(1)_debug: $$(INSTALL_UM_FRAGMENTS_$(1))
	$(CAT) $$^
endef

$(foreach _t,$(TARGET_ALL_ARCH) target_neutral,$(eval $(call calculate-um-fragments,$(_t))))

ifneq ($(filter init_script, $(INSTALL_UM_MODULES)),)
 INSTALL_UM_FRAGMENTS_target_neutral += $(GENERATED_CODE_OUT)/init_script/.install
endif

INSTALL_KM_FRAGMENTS := \
 $(strip $(foreach _m,$(BUILT_KM),\
  $(if $(filter-out kernel_module,$($(_m)_type)),,\
   $(if $($(_m)_install_path),\
    $(TARGET_PRIMARY_OUT)/intermediates/$(_m)/.install,\
     $(warning WARNING: KM $(_m)_install_path not defined)))))

.PHONY: install_km_debug
install_km_debug: $(INSTALL_KM_FRAGMENTS)
	$(CAT) $^

ifneq ($(INSTALL_KM_FRAGMENTS),)
$(TARGET_PRIMARY_OUT)/install_km.sh: $(INSTALL_KM_FRAGMENTS) $(CONFIG_KERNEL_MK) | $(TARGET_PRIMARY_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(ECHO) KERNELVERSION=$(KERNEL_ID)                            >  $@
	$(ECHO) MOD_DESTDIR=$(patsubst %/,%,$(PVRSRV_MODULE_BASEDIR)) >> $@
ifeq ($(SUPPORT_ANDROID_PLATFORM),)
	$(ECHO) check_module_directory /lib/modules/$(KERNEL_ID)      >> $@
endif
	$(CAT) $(INSTALL_KM_FRAGMENTS)                                >> $@
install_script_km: $(TARGET_PRIMARY_OUT)/install_km.sh
endif

# Build UM script using new scheme which does not use M4 for anything
# (Works for everything other than SUPPORT_BUILD_LWS)
define create-install-um-script
ifneq ($$(INSTALL_UM_FRAGMENTS_$(1)),)
$(RELATIVE_OUT)/$(1)/install_um.sh: $$(INSTALL_UM_FRAGMENTS_$(1)) | $(RELATIVE_OUT)/$(1)
	$(if $(V),,@echo "  GEN     " $$(call relative-to-top,$$@))
	$(CAT) $$(INSTALL_UM_FRAGMENTS_$(1)) > $$@
install_script: $(RELATIVE_OUT)/$(1)/install_um.sh
endif
endef
$(eval $(call create-install-um-script,target_neutral))

ifneq ($(SUPPORT_BUILD_LWS),1)
$(foreach _t,$(TARGET_ALL_ARCH),$(eval $(call create-install-um-script,$(_t))))
endif

# Build the top-level install script that drives the install.
ifneq ($(SUPPORT_ANDROID_PLATFORM),)
ifneq ($(SUPPORT_ARC_PLATFORM),)
install_sh_template := $(MAKE_TOP)/scripts/install.sh.tpl
else
install_sh_template := $(MAKE_TOP)/common/android/install.sh.tpl
endif
else
install_sh_template := $(MAKE_TOP)/scripts/install.sh.tpl
endif

$(RELATIVE_OUT)/install.sh: $(PVRVERSION_H) | $(RELATIVE_OUT)
# In customer packages only one of config.mk or config_kernel.mk will exist.
# We can depend on either one, as long as we rebuild the install script when
# the config options it uses change.
$(RELATIVE_OUT)/install.sh: $(call if-exists,$(CONFIG_MK),$(CONFIG_KERNEL_MK))
$(RELATIVE_OUT)/install.sh: $(install_sh_template)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(ECHO) 's/\[PVRVERSION\]/$(subst /,\/,$(PVRVERSION))/g;'            > $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[PVRBUILD\]/$(BUILD)/g;'                                >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[PRIMARY_ARCH\]/$(TARGET_PRIMARY_ARCH)/g;'              >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[ARCHITECTURES\]/$(TARGET_ALL_ARCH) target_neutral/g;'  >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[LWS_PREFIX\]/$(subst /,\/,$(LWS_PREFIX))/g;'           >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[APP_DESTDIR\]/$(subst /,\/,$(APP_DESTDIR))/g;'         >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[BIN_DESTDIR\]/$(subst /,\/,$(BIN_DESTDIR))/g;'         >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[SHADER_DESTDIR\]/$(subst /,\/,$(SHADER_DESTDIR))/g;'   >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[FW_DESTDIR\]/$(subst /,\/,$(FW_DESTDIR))/g;'           >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[SHLIB_DESTDIR\]/$(subst /,\/,$(SHLIB_DESTDIR))/g;'     >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[INCLUDE_DESTDIR\]/$(subst /,\/,$(INCLUDE_DESTDIR))/g;' >> $(RELATIVE_OUT)/install.sh.sed
	$(ECHO) 's/\[TEST_DESTDIR\]/$(subst /,\/,$(TEST_DESTDIR))/g;'       >> $(RELATIVE_OUT)/install.sh.sed
	@sed -f $(RELATIVE_OUT)/install.sh.sed $< > $@
	$(CHMOD) +x $@
	$(RM) $(RELATIVE_OUT)/install.sh.sed
install_script: $(RELATIVE_OUT)/install.sh
install_script_km: $(RELATIVE_OUT)/install.sh
