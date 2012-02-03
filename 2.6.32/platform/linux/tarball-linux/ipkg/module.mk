#-------------------------------------------------------------------------
# Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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
#-------------------------------------------------------------------------

#
# Variables
#
ndas-ipkg-path:=$(ndas_root)/ipkg
ndas-kernel-ipkg-path:=$(ndas-ipkg-path)/ndas-kernel
ndas-admin-ipkg-path:=$(ndas-ipkg-path)/ndas-admin
ndas-kernel-ipkg-modules-path:=$(ndas-kernel-ipkg-path)/lib/modules/$(NDAS_KERNEL_VERSION)/

#
# Targets
#
ipkg: ipkg-drivers ipkg-admin
ipkg-drivers: $(ndas-kernel-modules) 
	install -d -m 755 $(ndas-kernel-ipkg-path)
	install -d -m 755 $(ndas-kernel-ipkg-path)/CONTROL
	install -m 644 $(ndas-ipkg-path)/conffiles-kernel $(ndas-kernel-ipkg-path)/CONTROL/conffiles
	sed -e "s/NDAS_VERSION/$(ndas_version)/g" -e "s|NDAS_BUILD|$(ndas_build)|g" $(ndas-ipkg-path)/control-kernel > $(ndas-kernel-ipkg-path)/CONTROL/control
	chmod 644 $(ndas-kernel-ipkg-path)/CONTROL/control
	install -d -m 755  $(ndas-kernel-ipkg-modules-path); 
	for s in $^; \
	do \
		install -m 644 $$s $(ndas-kernel-ipkg-modules-path); \
		$(NDAS_CROSS_COMPILE_UM)strip $(ndas-kernel-ipkg-modules-path)/`echo $$s | sed -e 's|$(ndas_root)||g'`; \
	done
	install -m 644 $(ndas_root)/CREDITS.txt $(ndas-kernel-ipkg-modules-path)
	ipkg-build -c -o root -g root $(ndas-kernel-ipkg-path) $(ndas_root)

ipkg-admin: $(ndas-ndasadmin-out)
	install -d -m 755 $(ndas-admin-ipkg-path)
	install -d -m 755 $(ndas-admin-ipkg-path)/CONTROL
	install -m 644 $(ndas-ipkg-path)/conffiles-admin $(ndas-admin-ipkg-path)/CONTROL/conffiles
	install -m 644 $(ndas-ipkg-path)/control-admin $(ndas-admin-ipkg-path)/CONTROL/control
	sed -e 's|NDAS_VERSION|$(ndas_version)|g' -e 's|NDAS_BUILD|$(ndas_build)|g' $(ndas-ipkg-path)/control-admin > $(ndas-admin-ipkg-path)/CONTROL/control
	chmod 644 $(ndas-admin-ipkg-path)/CONTROL/control
	install -d -m 755 $(ndas-admin-ipkg-path)/usr/sbin
	install -m 755 $< $(ndas-admin-ipkg-path)/usr/sbin
	$(NDAS_CROSS_COMPILE_UM)strip $(ndas-admin-ipkg-path)/usr/sbin/$(<F)
	install -d -m 755 $(ndas-admin-ipkg-path)/etc/init.d/
	install -d -m 755 $(ndas-admin-ipkg-path)/usr/share/ndas
	install -m 755 $(ndas_root)/ndas.openwrt $(ndas-admin-ipkg-path)/etc/init.d/S60ndas
	install -m 755 $(ndas_root)/README $(ndas-admin-ipkg-path)/usr/share/ndas
	ipkg-build -c -o root -g root $(ndas-admin-ipkg-path) $(ndas_root)
clean: ipkg-clean
ipkg-clean:
	-rm -f $(ndas-kernel-ipkg-path)/CONTROL/conffiles
	-rm -f $(ndas-kernel-ipkg-path)/CONTROL/control
	-rm -fd $(ndas-kernel-ipkg-path)/CONTROL
	-rm -fd $(ndas-kernel-ipkg-path)
	-rm -f $(ndas-admin-ipkg-path)/CONTROL/conffiles
	-rm -f $(ndas-admin-ipkg-path)/CONTROL/control
	-rm -fd $(ndas-admin-ipkg-path)/CONTROL
	-rm -fd $(ndas-admin-ipkg-path)

