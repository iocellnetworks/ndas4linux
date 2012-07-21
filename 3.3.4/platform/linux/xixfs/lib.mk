#-------------------------------------------------------------------------
# Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
# All rights reserved.
#
# LICENSE TERMS
#
# The free distribution and use of this software in both source and binary 
# form is allowed (with or without changes) provided that:
#
#   1. distributions of this source code include the above copyright 
#      notice, this list of conditions and the following disclaimer;
#
#   2. distributions in binary form include the above copyright
#      notice, this list of conditions and the following disclaimer
#      in the documentation and/or other associated materials;
#
#   3. the copyright holder's name is not used to endorse products 
#      built using this software without specific written permission. 
#      
# ALTERNATIVELY, provided that this notice is retained in full, this product
# may be distributed under the terms of the GNU General Public License (GPL v2),
# in which case the provisions of the GPL apply INSTEAD OF those given above.
# 
# DISCLAIMER
#
# This software is provided 'as is' with no explcit or implied warranties
# in respect of any properties, including, but not limited to, correctness 
# and fitness for purpose.
#-------------------------------------------------------------------------

ifeq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.4)
# the custom kernel Makefile 
ndas_kernel_mk:=$(xixfs_root)/.kernel.mk

# if 2.4 below	
#
# Copy the $(1)/Makefile as $(ndas_kernel_mk). 
# Append some more targets to get the variables.
#
# $(1); kernel source path
#
ndas_copy_kernel_mk= $(shell \
	if [ ! -f $(ndas_kernel_mk) ] ; then \
		mkdir -p `dirname $(ndas_kernel_mk)`; \
		cp $(1)/Makefile $(ndas_kernel_mk); \
		chmod u+w $(ndas_kernel_mk); \
		printf "\nndas.CC:\n" >> $(ndas_kernel_mk); \
		printf "\techo '\$$(CC)'\n" >> $(ndas_kernel_mk) ; \
		printf "\nndas.CFLAGS:\n" >> $(ndas_kernel_mk) ; \
		printf "\techo '\$$(CFLAGS)'\n" >> $(ndas_kernel_mk) ; \
		printf "\nndas.LDFLAGS:\n" >> $(ndas_kernel_mk) ; \
		printf "\techo '\$$(LDFLAGS)'\n" >> $(ndas_kernel_mk)  ; \
		printf "\nndas.CROSS_COMPILE:\n" >> $(ndas_kernel_mk) ; \
		printf "\techo '\$$(CROSS_COMPILE)'\n" >> $(ndas_kernel_mk)  ; \
		printf "\nndas.MODFLAGS:\n" >> $(ndas_kernel_mk) ; \
		printf "\techo '\$$(MODFLAGS)'\n" >> $(ndas_kernel_mk)  ; \
		printf "\nndas.MAKEFILES:\n" >> $(ndas_kernel_mk) ;\
		printf "\techo '\$$(MAKEFILES)'\n" >> $(ndas_kernel_mk)  ;\
	fi)
#
# Get the kernel variables
# 
# $(1): kernel variable to get. possible values are 
#   CC, CFLAGS, LDFLAGS, CROSS_COMPILE, MODFLAGS, MAKEFILES, KERNELRELEASE
#
ndas_kernel_variable= $(call ndas_copy_kernel_mk,$(NDAS_KERNEL_PATH)) \
	    $(shell cd $(NDAS_KERNEL_PATH) ; \
	    /usr/bin/make -s -f $(ndas_kernel_mk) ndas.$(1) | grep -v ^make)

# endif // 2.4. below	
endif
