################################################################################
# Copyright ( C ) 2012 IOCELL Networks
# All rights reserved.
#
# NDAS Internal code
# This is the trade secret of IOCELL Networks
# If you obtained this code without any permission of IOCELL Networks
# Please email to linux@iocellnetworks.com
# Thank You.
################################################################################
#
# This file is included by platform/linux/module.mk
#

#
# add a suffix to the tarball for easy downloading and identification
# 
ifeq ($(nxp-cpu), x86_64)
NDAS_RPM_ARCH=x86_64
NDAS_TARBALL_ARCH_MOD=x86_64
else
ifeq ($(nxp-cpu), arm)
NDAS_RPM_ARCH=arm
NDAS_TARBALL_ARCH_MOD=arm
else
NDAS_RPM_ARCH=i386 i486 i586 i686 athlon
NDAS_TARBALL_ARCH_MOD=x86
endif
endif

#
# debugger version
#
ifeq ($(nxpo-debug), y)
NDAS_TARBALL_ARCH_MOD:=${NDAS_TARBALL_ARCH_MOD}.dbg

endif

nxpl-tarball-name=ndas
nxp-ver-string=$(NDAS_VER_MAJOR).$(NDAS_VER_MINOR).$(NDAS_VER_BUILD).$(NDAS_TARBALL_ARCH_MOD)

#
# the source tarball to build the RPMs
#
nxpl-tarball:=$(nxp-dist)/$(nxpl-tarball-name)-$(nxp-ver-string).tar.gz
nxpl-tarball-agreed:=$(nxp-build)/$(nxpl-tarball-name)-$(nxp-ver-string).tar.gz

#
# the root path of the source code to archive the tarball
#
nxpl-tar-path:=$(nxp-build)/$(nxpl-tarball-name)-$(nxp-ver-string)
#
# Copy NDAS header files, SAL header files 
# into linux build tree
# TODO: move into common.mk
#

ifneq ($(nxpo-ndaslinux),y)

nxpl-public-headers:=inc/ndasuser/ndasuser.h inc/ndasuser/ndaserr.h \
	inc/ndasuser/def.h inc/ndasuser/info.h \
	inc/ndasuser/io.h inc/ndasuser/persist.h \
	inc/ndasuser/write.h \
	inc/ndasuser/bind.h \
	inc/ndasuser/mediaop.h\
	inc/sal/sal.h inc/sal/libc.h inc/sal/sync.h \
	inc/sal/time.h inc/sal/thread.h \
	inc/sal/net.h  inc/sal/mem.h inc/sal/debug.h \
	inc/sal/types.h \
	inc/sal/io.h \
	inc/sal/linux/debug.h \
	inc/sal/linux/sal.h \
	inc/sal/linux/libc.h\
	inc/xixfsevent/xixfs_event.h

else

nxpl-public-headers+= inc/lspx/lsp_util.h inc/lspx/lsp_type.h inc/lspx/lsp.h
nxpl-public-headers+= inc/lspx/lspspecstring.h inc/lspx/lsp_spec.h inc/lspx/lsp_ide_def.h inc/lspx/lsp_host.h
nxpl-public-headers+= inc/lspx/pshpack4.h

endif

#
# Toshiba, Roku need sal/linux/emb.h
#
ifeq ($(nxpo-embedded),y)
nxpl-public-headers+= inc/sal/linux/emb.h
endif

#
# Target file from public files	
#
nxpl-tar-headers:=$(nxpl-public-headers:%=$(nxpl-tar-path)/%)

#
# Source fles to be copied from the 'platform/linux/tarball'
#
nxpl-tar-sources:=$(shell (cd $(nxpl-tarball-path) ; \
	find . -type f ! -path \*.svn\*) )
	
#	
# Target files to be tar-ed into the tarball 
#
nxpl-tar-files:=$(patsubst ./%, \
	$(nxpl-tar-path)/%, $(nxpl-tar-sources))

## TODO : move into the proper make file
ifdef nxp-vendor
nxp-linux-addon-path:=platform/linux/$(nxp-vendor)
# $(nxp-linux-addon-files) should be set by $(nxp-linux-addon-path)/module.mk
endif
## end

#
# copy into tarball path 
#
nxp-linux-tarball-addon-files:= $(if $(nxp-linux-addon-path), \
	$(patsubst $(nxp-linux-addon-path)/%, \
		$(nxpl-tar-path)/%, \
		$(nxp-linux-addon-files)))

ifeq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.4)
K:= k
endif

####################################################################
# NDAS lib
#
nxp-lib-original=$(nxpl-tar-path)/libndas.a
nxp-lib-sfx=$(nxpl-tar-path)/libndas.a.sfx	
ifeq ($(nxpo-release),y)
nxp-lib-file:=$(nxp-lib-sfx)
else
nxp-lib-file:=$(nxp-lib-original)
endif

all: $(nxpl-tarball)
all-automated: $(nxpl-tarball) $(nxpl-tarball-agreed)

# Construct files for metabrain distribution.
mb-dist: $(nxpl-tar-files) \
	$(nxp-linux-tarball-addon-files) \
	$(nxpl-tar-headers) \
	$(nxp-lib-original) \
	$(nxpl-tar-path)/CREDITS.txt \
	$(nxpl-tar-path)/EULA.txt	\
	$(nxpl-tar-path)/version.mk
	cp -rf $(nxp-linux-addon-path)/Makefile $(nxpl-tar-path) # overwrite makefile
	cp -rf $(nxp-linux-addon-path)/README $(nxpl-tar-path) # overwrite makefile
	cp -rf $(nxp-linux-addon-path)/mknod.sh $(nxpl-tar-path) # overwrite makefile

#
# libndas.a to be tar-ed into tarball
#	
$(nxp-lib-sfx): $(nxp-lib) $(nxpl-path)/gen-sfx.sh
	/bin/sh $(nxpl-path)/gen-sfx.sh $(nxp-lib) > $@
#
# libndas.a the original lib file
#
$(nxp-lib-original):  $(nxp-lib)
	cp $< $@
#
# Automated batch script build
#
#
# the tarball to create Source RPM.
#	
$(nxpl-tarball): $(nxpl-tar-files) \
	$(nxp-linux-tarball-addon-files) \
	$(nxpl-tar-headers) \
	$(nxp-lib-file) \
	$(nxpl-tar-path)/CREDITS.txt \
	$(nxpl-tar-path)/EULA.txt \
	$(nxpl-tar-path)/version.mk
	chmod u+x $(nxpl-tar-path)/debian/rules
	chmod u+x $(nxpl-tar-path)/ndas $(nxpl-tar-path)/mknod.sh	 $(nxpl-tar-path)/ndas.suse
	tar zcvf $@ --directory=$(nxp-build) $(^:$(nxp-build:./%=%)/%=%)

ifeq ($(nxpo-automated),y)
#
# the tarball to create Binary RPM for automated cron build
#
$(nxpl-tarball-agreed): $(nxpl-tar-files) \
	$(nxp-linux-tarball-addon-files) \
	$(nxpl-tar-headers) \
	$(nxp-lib-original) \
	$(nxpl-tar-path)/CREDITS.txt \
	$(nxpl-tar-path)/EULA.txt	\
	$(nxpl-tar-path)/version.mk
	chmod u+x $(nxpl-tar-path)/debian/rules
	chmod u+x $(nxpl-tar-path)/ndas $(nxpl-tar-path)/mknod.sh	 $(nxpl-tar-path)/ndas.suse	
	tar zcvf $@ --directory=$(nxp-build) $(^:$(nxp-build:./%=%)/%=%) 
endif

#exe: $(nxp-lib-original)
#	make -C $(nxpl-tarball-path) 

complete: $(nxpl-tar-files) \
	$(nxp-linux-tarball-addon-files) \
	$(nxpl-tar-headers) \
	$(nxp-lib-original) \
	$(nxpl-tar-path)/CREDITS.txt \
	$(nxpl-tar-path)/EULA.txt \
	$(nxpl-tar-path)/version.mk
	( \
	   cd $(nxpl-tar-path) ; \
	   echo version:$(NDAS_KERNEL_VERSION); \
	   echo path:$(NDAS_KERNEL_PATH); \
	   echo arch:$(NDAS_KERNEL_ARCH); \
	   if [ ! -z $(NDAS_CROSS_COMPILE) ]; then \
		export NDAS_CROSS_COMPILE=$(NDAS_CROSS_COMPILE); \
	   fi; \
	   export NDAS_EXTRA_CFLAGS="$(NDAS_EXTRA_CFLAGS) $(nxp-public-cflags)";\
	   export NDAS_EXTRA_LDFLAGS="$(NDAS_EXTRA_LDFLAGS)";\
	   if [ ! -z $(NDAS_CROSS_COMPILE_UM) ] ; then \
	        export NDAS_CROSS_COMPILE_UM=$(NDAS_CROSS_COMPILE_UM); \
	   fi ; \
	   make all ndas-admin | tee ~/.ndas_build_log \
	)
#	cp $(nxpl-tar-path)/ndas_core.ko $(nxp-dist)	
#	cp $(nxpl-tar-path)/ndas_block.ko $(nxp-dist)	
#	cp $(nxpl-tar-path)/ndas_sal.ko $(nxp-dist)	
#	cp $(nxpl-tar-path)/ndasadmin $(nxp-dist)	
#	cp $(nxpl-tar-path)/ndas $(nxp-dist)	

emu: $(nxpl-tar-files) \
	$(nxp-linux-tarball-addon-files) \
	$(nxpl-tar-headers) \
	$(nxp-lib-original) \
	$(nxpl-tar-path)/CREDITS.txt \
	$(nxpl-tar-path)/EULA.txt \
	$(nxpl-tar-path)/version.mk
	( \
	   cd $(nxpl-tar-path) ; \
	   echo version:$(NDAS_KERNEL_VERSION); \
	   echo path:$(NDAS_KERNEL_PATH); \
	   echo arch:$(NDAS_KERNEL_ARCH); \
	   if [ ! -z $(NDAS_CROSS_COMPILE) ]; then \
		export NDAS_CROSS_COMPILE=$(NDAS_CROSS_COMPILE); \
	   fi; \
	   export NDAS_EXTRA_CFLAGS="$(NDAS_EXTRA_CFLAGS) $(nxp-public-cflags)";\
	   export NDAS_EXTRA_LDFLAGS="$(NDAS_EXTRA_LDFLAGS)";\
	   if [ ! -z $(NDAS_CROSS_COMPILE_UM) ] ; then \
	        export NDAS_CROSS_COMPILE_UM=$(NDAS_CROSS_COMPILE_UM); \
	   fi ; \
	   make emu-single | tee ~/.ndas_build_log \
	)
#	-cp $(nxpl-tar-path)/ndas_emu_s.ko $(nxp-dist)
#	-cp $(nxpl-tar-path)/ndas_emu_s.o $(nxp-dist)

#
# Create ipkg
#
ipkg: $(nxpl-tar-files) \
	$(nxp-linux-tarball-addon-files) \
	$(nxpl-tar-headers) \
	$(nxp-lib-original) \
	$(nxpl-tar-path)/CREDITS.txt \
	$(nxpl-tar-path)/EULA.txt \
	$(nxpl-tar-path)/version.mk
	( \
	   cd $(nxpl-tar-path) ; \
	   echo $(NDAS_KERNEL_VERSION); \
	   if [ ! -z $(NDAS_CROSS_COMPILE) ]; then \
		export NDAS_CROSS_COMPILE=$(NDAS_CROSS_COMPILE); \
	   fi; \
	   export NDAS_EXTRA_CFLAGS="$(NDAS_EXTRA_CFLAGS) $(nxp-public-cflags)";\
	   export NDAS_EXTRA_LDFLAGS="$(NDAS_EXTRA_LDFLAGS)";\
	   if [ ! -z $(NDAS_CROSS_COMPILE_UM) ] ; then \
	        export NDAS_CROSS_COMPILE_UM=$(NDAS_CROSS_COMPILE_UM); \
	   fi ; \
	   make all ndas-admin ipkg | tee ~/.log \
	)

#
# Create ipkg
#

tarball: $(nxpl-tar-files) 				\
	$(nxp-linux-tarball-addon-files) 	\
	$(nxpl-tar-headers) 				\
	$(nxp-lib-original) 				\
	$(nxpl-tar-path)/CREDITS.txt	 	\
	$(nxpl-tar-path)/EULA.txt 			\
	$(nxpl-tar-path)/version.mk
	( 									\
	   cd $(nxpl-tar-path) ; 			\
	   echo $(NDAS_KERNEL_PATH); 		\
	   echo $(NDAS_KERNEL_VERSION); 	\
	   if [ ! -z $(NDAS_CROSS_COMPILE) ]; then \
		export NDAS_CROSS_COMPILE=$(NDAS_CROSS_COMPILE); \
	   fi; \
	   export NDAS_EXTRA_CFLAGS="$(NDAS_EXTRA_CFLAGS) $(nxp-public-cflags)";\
	   export NDAS_EXTRA_LDFLAGS="$(NDAS_EXTRA_LDFLAGS)";\
	   if [ ! -z $(NDAS_CROSS_COMPILE_UM) ] ; then \
	        export NDAS_CROSS_COMPILE_UM=$(NDAS_CROSS_COMPILE_UM); \
	   fi ; \
	   make all \
	)

#
# The public header files
#
$(nxpl-tar-headers): $(nxpl-tar-path)/%.h : %.h
	@mkdir -p $(@D)
	cp $< $@

$(nxpl-tar-path)/CREDITS.txt: CREDITS.txt
	@mkdir -p $(@D)
	cp $< $@

$(nxpl-tar-path)/EULA.txt: EULA.txt
	@mkdir -p $(@D)
	cp $< $@

$(nxpl-tar-path)/version.mk: version.mk
	@mkdir -p $(@D)
	cp $< $@

#
# platform/linux/tarball/* files to be tar-ed into tarball
#
# Replace some variables in the spec file for rpm
$(nxpl-tar-path)/ndas.spec: $(nxpl-tarball-path)/ndas.spec changelog.txt FORCE
	@mkdir -p $(@D)
	cat $(filter-out FORCE,$^) | \
		sed -e "s|NDAS_RPM_VERSION|$(NDAS_VER_MAJOR).$(NDAS_VER_MINOR).$(NDAS_VER_BUILD).$(NDAS_TARBALL_ARCH_MOD)|g" |\
		sed -e "s|NDAS_MIN_VERSION|$(NDAS_VER_MAJOR).$(NDAS_VER_MINOR).$(NDAS_VER_BUILD)|g" |\
		sed -e "s|NDAS_RPM_RELEASE|$(DISTRIBUTION_VERSION)|g" |\
		sed -e "s|NDAS_RPM_BUILD|$(NDAS_VER_BUILD)|g" |\
		sed -e "s|NDAS_RPM_ARCH|$(NDAS_RPM_ARCH)|g" |\
		sed -e "s|NDAS_TARBALL_ARCH_MOD|$(NDAS_TARBALL_ARCH_MOD)|g" |\
		sed -e "s|NDAS_VENDOR_NAME|$(NDAS_VENDOR)|g" |\
		sed -e "s|NDAS_RPM_DOWNLOAD_URL|$(NDAS_LINUX_DOWNLOAD_URL)|g" |\
		sed -e "s|NDAS_RPM_HOME_URL|$(NDAS_HOME_URL)|g" \
		> $@


$(filter-out $(nxpl-tar-path)/ndas.spec, \
	$(nxpl-tar-files)): \
	$(nxpl-tar-path)/% : \
	$(nxpl-tarball-path)/%
	@mkdir -p $(@D)
	cp $^ $@

#
#
# Replace the Makefile debug invocation if needed
ifeq ($(nxpo-debug), y)
$(nxpl-tar-path)/Makefile: $(nxpl-tarball-path)/Makefile
	cat $(filter-out FORCE,$^) | \
		sed -e "s|#NDAS_DEBUG=y|NDAS_DEBUG=y|g" \
	> $@
endif

#
# platform/linux/$(nxp-vendor)/* files to be tar-ed into tarball
#
$(nxp-linux-tarball-addon-files): \
    $(nxpl-tar-path)/% : \
    $(nxp-linux-addon-path)/%
	@mkdir -p $(@D)
	cp $^ $@	

tarball-clean:
	rm -rf $(nxpl-tar-path)/
	rm -rf $(nxpl-tarball)


