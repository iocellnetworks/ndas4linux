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
#
# Variables and rules for test on the VMWare platform
#
ifeq ($(nxpo-vmware),y)

	TEST_NETDISK_MAC?="00:0B:D0:00:FE:EB"
	TEST_NDAS_ID?="AF7R2-2MKK4-UHG9S-5RHTG"
	TEST_NDAS_KEY?="8QT6U"
	TEST_SERVER?=redhat9-server
	TEST_CLIENT?=redhat9-client
	
	VMWARE_HOST?=192.168.0.145
	VMWARE_HOST_USERNAME?=jhpark
	VMWARE_HOST_PASSWORD?=abcd123
	
	VMWARE_CONFIG?="C:\jhpark\apps\vms\linux\redhat9-client\Red Hat Linux 9.0.vmx"
	VMWARE_SERVER_CONFIG?="C:\jhpark\apps\vms\linux\redhat9-server\Red Hat Linux 9.0.vmx"
	vmware-cmd:="/usr/lib/vmware-api/SampleScripts/vmware-cmd"
	vmware-cmdline:=$(vmware-cmd) \
			-H $(VMWARE_HOST) \
			-U $(VMWARE_HOST_USERNAME) \
			-P $(VMWARE_HOST_PASSWORD) 
	
	VMWARE_PATH?=/mnt/gene/jhpark/apps/vms/linux/redhat9-client
	VMWARE_SERVER_PATH?=/mnt/gene/jhpark/apps/vms/linux/redhat9-server
	
	NOW:=$(shell date +%Y%m%d-%H%M%S)
	XTERM_GEOMETRY_CONSOLE_LOG=+10+10
	XTERM_GEOMETRY_CMD=+10+400
	XTERM_GEOMETRY_SERVER_CONSOLE_LOG=+420+10
	
	#
	# Functions 
	#
	
	## Copy the compiled objects into target test system. ##
	# $(1): vmware guest os ip address/domain name.
	#
	nxpl-install= scp $(2) root@$(1):/tmp; 
	
	#	ssh -f -x -l root $(1) /sbin/insmod /tmp/$(<F); \
	#	ssh -f -x -l root $(1) /tmp/mknod.sh
	
	## Revert the snapshot of the vmware test system ##
	# $(1): VMWARE configuration file
	nxpl-revert= $(vmware-cmdline) $(1) stop hard 
	## Set the log file 
	# $(1): VMWARE configuration file
	# $(2): local mounted path of vmware serial console log file
	# $(3): geomety for xterm to display console log file
	nxpl-set-logfile= $(vmware-cmdline) $(1) setconfig serial0.fileName serial-$(NOW).txt ;\
		$(vmware-cmdline) $(1) connectdevice serial0 ;\
		xterm -title client -sl 10000 -geometry $(3) \
			-e tail -f $(2)/serial-$(NOW).txt &
	
	test-all: all revert-server revert execute-server execute
	
	# install into 
	install-into-vmware: 
		@echo Installing  $(shell cat /tmp/.file) into the test client $(TEST_CLIENT)
		$(call nxpl-install,$(TEST_CLIENT),  $(shell cat /tmp/.file))
		$(call nxpl-install,$(TEST_CLIENT),  $(shell dirname `cat /tmp/.file`)/ndas-admin-${nxp-build-version}-${nxp-build-number}.i686.rpm)
		-ssh -l root $(TEST_CLIENT) rdate -s time-c.timefreq.bldrdoc.gov
		ssh -l root $(TEST_CLIENT) rpm -ivh /tmp/$(notdir $(shell cat /tmp/.file))
		ssh -l root $(TEST_CLIENT) rpm -ivh /tmp/ndas-admin-${nxp-build-version}-${nxp-build-number}.i686.rpm
	
	test-execute: install-into-vmware
		@echo Exceute client test program
		@ssh -x -l root $(TEST_CLIENT) /tmp/$(notdir $(LINUX_CLIENT_TEST_APP))
	
	$(vmware-cmd):
		@if [ ! -f $(vmware-cmd) ] ; then \
			echo Please install VMware-VmPerlAPI tools to revert guest os;\
			echo And execute the following commands; \
			echo ln -sf /lib/libcrypto.so.0.9.7a /usr/bin/libcrypto.so.0.9.7;\
			echo ln -sf /lib/libssl.so.0.9.7a /usr/bin/libssl.so.0.9.7; \
			echo Make sure the perl is installed; \
			exit 1 ;\
		fi
	vmware-revert: $(vmware-cmd)
		@echo Reverting to the snapshot of vmware $(TEST_CLIENT) 
		$(call nxpl-revert,$(VMWARE_CONFIG))
	
	vmware-setlog: $(vmware-cmd)
		$(call nxpl-set-logfile,$(VMWARE_CONFIG),$(VMWARE_PATH), $(XTERM_GEOMETRY_CONSOLE_LOG))
	
	test-xterm: 
		-ssh -X -f -l root $(TEST_CLIENT) xterm -title $(TEST_CLIENT) \
			-geometry $(XTERM_GEOMETRY_CMD)
	
	$(HOME)/.ssh/:
		mkdir -p $@
		chmod og-rwx $@
	
	$(HOME)/.ssh/id_dsa $(HOME)/.ssh/id_dsa.pub : $(HOME)/.ssh/
		ssh-keygen -t dsa -N "" -f $@
	
	install-ssh-keys: $(HOME)/.ssh/id_dsa.pub $(HOME)/.ssh/id_dsa
		cat $(HOME)/.ssh/id_dsa.pub | \
			ssh -x -l root $(TEST_CLIENT) mkdir -p .ssh ";" \
			touch .ssh/authorized_keys ";" \
			chmod 700 .ssh ";" \
			chmod 600 .ssh/authorized_keys ";" \
			cat ">>" .ssh/authorized_keys
	install-ssh-keys-server: $(HOME)/.ssh/id_dsa.pub $(HOME)/.ssh/id_dsa
		cat $(HOME)/.ssh/id_dsa.pub | \
			ssh -x -l root $(TEST_SERVER) mkdir -p .ssh ";" \
			touch .ssh/authorized_keys ";" chmod 700 .ssh ";" \
			chmod 600 .ssh/authorized_keys ";" \
			cat ">>" .ssh/authorized_keys
	
	help:help-linux
	help-linux:
		@echo -- Environment Variables for linux kernel driver test 
		@echo -- on Ximeta\'s VMWARE SERVER
		@echo -- This is included as help from platform/linux/test.mk
		@echo set VMWARE_HOST as the host where the vmware is running
		@echo set VMWAWARE_HOST_USERNAME as the username of the host where the vmware is running
		@echo set VMWARE_HOST_PASSWORD as the password of the username
		@echo set VMWARE_CONFIG as the configuration file of the vmware guest os 
		@echo set VMWARE_PATH as the local mounted path of the configuration of vmware guest os
	
endif
