################################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
#
# XIMETA Internal code
# This is the trade secret of XIMETA, Inc.
# If you obtained this code without any permission of XIMETA, Inc.
# Please email to support@ximeta.com
# Thank You.
################################################################################
#
# Change it here or specify it on the "make" command line
#
nxpl-path=platform/linux
ifeq ($(nxpo-ndaslinux),y)
nxpl-tarball-path=$(nxpl-path)/tarball-linux
else
nxpl-tarball-path=$(nxpl-path)/tarball-tag
endif
ifneq ($(nxp-vendor),)
nxpl-vendor-path=$(nxpl-path)/$(nxp-vendor)
endif

all: 

#
# Tarball
#
include $(nxpl-path)/tarball.mk

os-clean: tarball-clean clean-xixfs
	
#
# RPM
#
include $(nxpl-path)/rpm.mk

#
# IPKG for openwrt
# 
ifeq ($(nxp-vendor),openwrt)
include $(nxpl-path)/ipkg.mk
endif
#	
#
# Test make file
#	
include $(nxpl-path)/test.mk

ifneq ($(nxp-vendor),)
#
# Vendor specific files
# 
-include $(nxpl-vendor-path)/module.mk
endif

include $(nxpl-path)/xixfs/module.mk
