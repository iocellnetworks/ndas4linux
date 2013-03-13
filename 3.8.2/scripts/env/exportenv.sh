#!/bin/sh

# set your cross compiler location
export NDAS_CROSS_COMPILE

# add options to compilation
export NDAS_EXTRA_CFLAGS

# set the kernel arm, x86, x86_64 mips ... 
export NDAS_KERNEL_ARCH

# change this for kernels other than current running
export NDAS_KERNEL_PATH

# example 2.6
export NDAS_KERNEL_VERSION

# This can be 1 - 10 for global debug error levels. 
# Best to set at 1 and use dbgfg.h for fine tuning.
export NDAS_DEBUG

# maybe this is not used
# export NDAS_OPENWRT_PATH


