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
# This file is included by platform/linux/module.mk
#
nxpl-rpm-path:=$(nxp-build)/rpm
nxpl-rpm-subpath:=$(nxpl-rpm-path)/BUILD \
    $(nxpl-rpm-path)/RPMS \
    $(nxpl-rpm-path)/RPMS/athlon \
    $(nxpl-rpm-path)/RPMS/i386 \
    $(nxpl-rpm-path)/RPMS/i486 \
    $(nxpl-rpm-path)/RPMS/i686 \
    $(nxpl-rpm-path)/RPMS/noarch \
    $(nxpl-rpm-path)/SOURCES \
    $(nxpl-rpm-path)/SPECS \
    $(nxpl-rpm-path)/SRPMS \
    $(nxpl-rpm-path)/tmp     
NDAS_KERNEL_VERSION?=$(shell uname -r)
NDAS_KERNEL_ARCH?=$(shell uname -m)
_fixed_kversion:=$(subst -smp,,$(_fixed_kversion))
_fixed_kversion:=$(subst -default,,$(_fixed_kversion))
_fixed_kversion:=$(subst -bigsmp,,$(_fixed_kversion))
_fixed_kversion:=$(subst -xen,,$(_fixed_kversion))
_fixed_kversion=$(subst -,_,$(NDAS_KERNEL_VERSION))
_fixed_kversion:=$(subst smp,,$(_fixed_kversion))
_fixed_kversion:=$(subst xen0,,$(_fixed_kversion))
_fixed_kversion:=$(subst xenU,,$(_fixed_kversion))
nxpl-rpm-subname:=$(findstring $(NDAS_KERNEL_VERSION),smp)
_smp:=$(findstring smp,$(NDAS_KERNEL_VERSION))
_xen0:=$(findstring xen0,$(NDAS_KERNEL_VERSION))
_xenU:=$(findstring xenU,$(NDAS_KERNEL_VERSION))
_xen:=$(findstring -xen,$(NDAS_KERNEL_VERSION))
_default:=$(findstring -default,$(NDAS_KERNEL_VERSION))
_bigsmp:=$(findstring -bigsmp,$(NDAS_KERNEL_VERSION))
nxpl-rpm-name:=ndas
ifeq ($(_smp),smp)
nxpl-rpm-name:=ndas-smp
endif
ifeq ($(_xen0),xen0)
nxpl-rpm-name:=ndas-xen0
endif
ifeq ($(_xenU),xenU)
nxpl-rpm-name:=ndas-xenU
endif
ifeq ($(_default),default)
nxpl-rpm-name:=ndas-default
endif
ifeq ($(_default),bigsmp)
nxpl-rpm-name:=ndas-bigsmp
endif

#
# the binary RPM to verify the tarball
# $(1) : arch as i386, i585, i686, ppc, arm, mipsel, x64, athlon
nxpl-rpm=$(nxpl-rpm-path)/RPMS/$(1)/$(nxpl-rpm-name)-$(nxp-build-version)-$(_fixed_kversion).$(nxp-build-number).$(1).rpm
#
# The Source RPM to verify the tarball  
# $(1) : arch as i386, i585, i686, ppc, arm, mipsel, x64, athlon
nxpl-srpm=$(nxpl-rpm-path)/SRPMS/$(nxpl-rpm-name)-$(nxp-build-version)-$(_fixed_kversion).$(nxp-build-number).$(1).src.rpm

#
# Source RPM and Binary RPM 
#
$(call nxpl-rpm,i686) $(call nxpl-srpm, i686): $(nxpl-tarball) $(nxpl-rpm-subpath)
	rpmbuild -ta $(nxpl-tarball) --target=i686 --nodeps \
		--define "_topdir `pwd`/$(nxpl-rpm-path)" \
		--define "_tmppath `pwd`/$(nxpl-rpm-path)"
#
# RPM build tree, sub directories
#
$(nxpl-rpm-subpath):
	@mkdir -p $@
	
# To test the source rpm
i686-rpm: $(call nxpl-rpm,i686) 
	echo $(call nxpl-rpm,i686) > $(nxp-build)/.file
.PHONY: i686-rpm
clean: clean-rpm
clean-rpm: 
	rm -rf $(call nxpl-rpm,i686) \
		$(nxpl-rpm-path) $(nxp-build)/.file
