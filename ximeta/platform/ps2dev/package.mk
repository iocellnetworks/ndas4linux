######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################
APPLICATION+= $(PACKAGE_FILE)
	
PACKAGE_NAME=broadq-2
PACKAGE_FILE=$(nxp-dist)/$(PACKAGE_NAME).tar.gz	
PACKAGE_PATH=$(nxp-dist)/$(PACKAGE_NAME)

PACK_SAL_SRC=$(patsubst $(SAL_PATH)/%,$(PACKAGE_PATH)/src/sal/%,$(SAL_SRC))
PACK_SAL_SRC+= $(PACKAGE_PATH)/src/sal/module.mk

PACK_SMAPX_SRC=$(patsubst $(SMAPX_PATH)/%,$(PACKAGE_PATH)/src/smapx/%,$(SMAPX_SRC)) 
PACK_SMAPX_SRC+= $(PACKAGE_PATH)/src/smapx/module.mk
PACK_SMAPX_SRC+= $(PACKAGE_PATH)/src/smapx/exports.tab
PACK_SMAPX_SRC+= $(PACKAGE_PATH)/src/smapx/imports.lst
PACK_SMAPX_SRC+= $(PACKAGE_PATH)/src/smapx/smapx.h
PACK_SMAPX_SRC+= $(PACKAGE_PATH)/src/smapx/smap.h
PACK_SMAPX_SRC+= $(PACKAGE_PATH)/src/smapx/irx_imports.h

PACK_IRX_SRC=$(patsubst $(IRX_PATH)/%,$(PACKAGE_PATH)/sample/irx/%,$(IRX_SRC))
PACK_IRX_SRC+= $(PACKAGE_PATH)/sample/irx/module.mk
PACK_IRX_SRC+= $(PACKAGE_PATH)/sample/irx/exports.tab
PACK_IRX_SRC+= $(PACKAGE_PATH)/sample/irx/imports.lst
PACK_IRX_SRC+= $(PACKAGE_PATH)/sample/irx/ndrpccmd.h
PACK_IRX_SRC+= $(PACKAGE_PATH)/sample/irx/ndasirx_imports.h

PACK_TEST_SRC=$(PACKAGE_PATH)/sample/test/ndbench.c $(PACKAGE_PATH)/sample/test/probe.c
PACK_TEST_SRC+= $(PACKAGE_PATH)/sample/test/module.mk

PACK_EESAMPLE_SRC=$(patsubst $(EESAMPLE_PATH)/%,$(PACKAGE_PATH)/sample/eesample/%,$(EESAMPLE_SRC))
PACK_EESAMPLE_SRC+=  $(PACKAGE_PATH)/sample/eesample/module.mk
# Other source and objects to copy
SAL_INCS=$(wildcard inc/sal/*.h)
PACK_SAL_INCS=$(patsubst inc/sal/%,$(PACKAGE_PATH)/inc/sal/%,$(SAL_INCS))
NDASUSER_INCS=ndasuser.h ndas_blk.h ndaserr.h
PACK_NDASUSER_INCS=$(patsubst %,$(PACKAGE_PATH)/inc/ndasuser/%,$(NDASUSER_INCS))
OTHER_OBJS=libndas.o libfat32lib.o
PACK_OTHER_OBJS=$(patsubst %,$(PACKAGE_PATH)/lib/%,$(OTHER_OBJS))
DOCS=$(wildcard platform/ps2dev/doc/*)
PACK_DOCS=$(patsubst platform/ps2dev/%,$(PACKAGE_PATH)/%,$(DOCS))
PS2SDK_IRXS=iomanX.irx ps2dev9.irx ps2ip.irx ps2ips.irx
PACK_PS2SDK_IRXS=$(patsubst %,$(PACKAGE_PATH)/lib/%,$(PS2SDK_IRXS))

$(PACKAGE_PATH):
	@mkdir -p $@
	
$(PACKAGE_PATH)/inc/%: inc/%
	@mkdir -p $(@D)
	cp $< $@
	
$(PACKAGE_PATH)/doc/%: platform/ps2dev/doc/%
	@mkdir -p $(@D)
	cp $< $@

$(PACKAGE_PATH)/lib/%.irx: platform/ps2dev/bins/%.irx
	@mkdir -p $(@D)
	cp $< $@

$(PACKAGE_PATH)/lib/%.o: $(nxp-dist)/%.o
	@mkdir -p $(@D)
	cp $< $@


$(PACKAGE_PATH)/src/%: platform/ps2dev/%
	@mkdir -p $(@D)
	cp $< $@

$(PACKAGE_PATH)/sample/irx/%: $(IRX_PATH)/%
	@mkdir -p $(@D)
	cp $< $@

$(PACKAGE_PATH)/sample/test/%: $(TEST_PATH)/%
	@mkdir -p $(@D)
	cp $< $@

$(PACKAGE_PATH)/sample/eesample/%: $(EESAMPLE_PATH)/%
	@mkdir -p $(@D)
	cp $< $@

$(PACKAGE_PATH)/Makefile: platform/ps2dev/package-main.mk
	cp $< $@
	
$(PACKAGE_PATH)/depend.sh: platform/ps2dev/depend.sh
	cp $< $@

$(PACKAGE_PATH)/ee.mk: platform/ps2dev/ee.mk
	cp $< $@

	
$(PACKAGE_FILE): $(nxp-dist)  $(PACKAGE_PATH) $(PACK_SAL_SRC) $(PACK_SMAPX_SRC) \
		$(PACK_IRX_SRC) $(PACK_TEST_SRC) $(PACK_EESAMPLE_SRC) \
		$(PACK_SAL_INCS) $(PACK_NDASUSER_INCS) \
		$(PACK_OTHER_OBJS) $(PACK_DOCS) $(PACK_PS2SDK_IRXS) \
		$(PACKAGE_PATH)/Makefile $(PACKAGE_PATH)/ee.mk $(PACKAGE_PATH)/depend.sh
	cd $(nxp-dist) ; \
		tar zcvf $@ \
		$(PACKAGE_NAME)/*
	make package_test
	
package_test: $(PACKAGE_FILE)	
	@-rm -rf $(PACKAGE_PATH)
	cd $(nxp-dist) ; \
		tar zxvf $(PACKAGE_FILE)
	cd $(PACKAGE_PATH) ; make 