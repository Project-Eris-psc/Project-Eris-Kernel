########################################################################### ###
#@File
#@Title         Select a window system
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

# Set the default window system.
# If you want to override the default, create a default_window_system.mk file
# that sets WINDOW_SYSTEM appropriately.  (There is a suitable example in 
# ../config/default_window_system_xorg.mk)
-include ../config/default_window_system.mk
WINDOW_SYSTEM ?= ews

# Check for WINDOW_SYSTEMs that aren't understood by the 
# common/window_systems/*.mk files. This should be kept in 
# sync with all the tests on WINDOW_SYSTEMs in those files
_window_systems := nullws nulldrmws ews ews_drm wayland xorg screen surfaceless gigacluster_ws integrity lws-generic

_unrecognised_window_system := $(strip $(filter-out $(_window_systems),$(WINDOW_SYSTEM)))
ifneq ($(_unrecognised_window_system),)
$(warning *** Unrecognised WINDOW_SYSTEM: $(_unrecognised_window_system))
$(warning *** WINDOW_SYSTEM was set via: $(origin WINDOW_SYSTEM))
$(error Supported Window Systems are: $(_window_systems))
endif

# If we get rid of OPK_DEFAULT and OPK_FALLBACK
# and just have a symlink to the correct one
# we can get rid of most of this.
ifeq ($(WINDOW_SYSTEM),xorg)
 MESA_EGL := 1
 SUPPORT_OPENGLES1_V1_ONLY := 1
 SUPPORT_VK_PLATFORMS := x11
 SUPPORT_DISPLAY_CLASS := 0
 SUPPORT_KMS := 1
 ifeq ($(PVR_XORG_USE_MODESETTING_DRIVER),1)
  SUPPORT_ACTIVE_FLUSH := 1
 endif
else ifeq ($(WINDOW_SYSTEM),wayland)
 MESA_EGL := 1
 SUPPORT_OPENGLES1_V1_ONLY := 1
 SUPPORT_VK_PLATFORMS := wayland
 SUPPORT_DISPLAY_CLASS := 0
 SUPPORT_KMS := 1
else ifeq ($(WINDOW_SYSTEM),surfaceless)
 MESA_EGL := 1
 SUPPORT_ACTIVE_FLUSH := 1
 SUPPORT_OPENGLES1_V1_ONLY := 1
 SUPPORT_DISPLAY_CLASS := 0
 SUPPORT_KMS := 1
else ifeq ($(WINDOW_SYSTEM),lws-generic)
 MESA_EGL := 1
 SUPPORT_OPENGLES1_V1_ONLY := 1
 SUPPORT_DISPLAY_CLASS := 0
 SUPPORT_KMS := 1
else ifeq ($(WINDOW_SYSTEM),ews)
 OPK_DEFAULT  := libpvrEWS_WSEGL.so
 OPK_FALLBACK := libpvrNULL_WSEGL.so
 SUPPORT_VK_PLATFORMS := null
 SUPPORT_SECURE_EXPORT ?= 1
 SUPPORT_DISPLAY_CLASS ?= 1
 ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
  EGL_EXTENSION_ANDROID_NATIVE_FENCE_SYNC := 1
 endif
else ifeq ($(WINDOW_SYSTEM),ews_drm)
 OPK_DEFAULT  := libpvrEWS_WSEGL.so
 OPK_FALLBACK := libpvrNULLDRM_WSEGL.so
 SUPPORT_VK_PLATFORMS := null
 SUPPORT_SECURE_EXPORT ?= 1
 SUPPORT_DISPLAY_CLASS := 0
 SUPPORT_KMS := 1
 ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
  EGL_EXTENSION_ANDROID_NATIVE_FENCE_SYNC := 1
 endif
else ifeq ($(WINDOW_SYSTEM),nullws)
 OPK_DEFAULT  := libpvrNULL_WSEGL.so
 OPK_FALLBACK := libpvrNULL_WSEGL.so
 SUPPORT_VK_PLATFORMS := null
 SUPPORT_DISPLAY_CLASS ?= 1
else ifeq ($(WINDOW_SYSTEM),nulldrmws)
 OPK_DEFAULT  := libpvrNULLDRM_WSEGL.so
 OPK_FALLBACK := libpvrNULLDRM_WSEGL.so
 SUPPORT_VK_PLATFORMS := null
 SUPPORT_DISPLAY_CLASS := 0
 SUPPORT_KMS := 1
else ifeq ($(WINDOW_SYSTEM),screen)
 OPK_DEFAULT  := libpvrSCREEN_WSEGL.so
 OPK_FALLBACK := libpvrSCREEN_WSEGL.so
 PVR_HANDLE_BACKEND := generic
 SUPPORT_VK_PLATFORMS := null
 SUPPORT_DISPLAY_CLASS ?= 1
else ifeq ($(WINDOW_SYSTEM),gigacluster_ws)
 OPK_DEFAULT  := libGCWSEGL.so
 OPK_FALLBACK := libGCWSEGL.so
 SUPPORT_VK_PLATFORMS := null
 SUPPORT_DISPLAY_CLASS := 1
 SUPPORT_GIGACLUSTER := 1
endif

ifeq ($(MESA_EGL),1)
 EGL_BASENAME_SUFFIX := _PVR_MESA
endif
