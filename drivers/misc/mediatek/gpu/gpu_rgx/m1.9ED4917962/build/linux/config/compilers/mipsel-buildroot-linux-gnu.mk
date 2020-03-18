# 32-bit MIPS compiler
TARGET_PRIMARY_ARCH := target_mips-buildroot
SYSROOT ?= $(shell $(CROSS_COMPILE)gcc -print-sysroot)
