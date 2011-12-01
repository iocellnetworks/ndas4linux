######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################
#
# Makefile for Netdisk Port on PS2DEV
#
PACKAGE_NAME:=broadq-1
PACKAGE_DIR:=$(nxp-dist)/$(PACKAGE_NAME)
SAL_SRC:= $(shell echo sal/*.c sal/Makefile)
IRX_SRC:=irx/exports.tab irx/imports.lst irx/ndasirx_imports.h irx/Makefile irx/simple_irx.c irx/ndirx.c irx/ndrpccmd.h
PS2SDK_IRXS:=bins/iomanX.irx bins/ps2dev9.irx bins/ps2ip.irx bins/ps2ips.irx
SMAPX_SRC:=$(shell echo smapx/*.h smapx/*.c smapx/exports.tab smapx/imports.lst smapx/Makefile)
NDASUSER_INCS:=$(shell echo $(nxp-root)/inc/ndasuser/{ndasuser.h,ndas_blk.h,ndaserr.h})
SAL_INCS:=$(shell echo $(nxp-root)/inc/sal/*.h)
TEST_SRCS:=$(nxp-root)/test/ndbench.c $(nxp-root)/test/Makefile
EESAMPLE_SRCS:=$(shell echo eesample/{*.c,Makefile})
OTHER_OBJS:=$(nxp-dist)/libndas.o $(nxp-dist)/libfat32.o 
DOCS:=$(shell echo doc/*)
# smapx.h
CFLAGS += -I$(shell pwd)/smapx

all: compile ready_for_naplink package package-test

compile:
	make -C sal all
	make -C libndas all
	make -C irx all
	make -C eesample all
	make -C smapx all

ready_for_naplink:
	cp $(nxp-dist)/ps2ndas.irx bins/
	cp $(nxp-dist)/ps2smapx.irx bins/
	cp $(shell echo $(nxp-dist)/*.elf) bins/

package: $(SAL_SRC) $(IRX_SRC) $(PS2SDK_IRXS) $(SMAPX_SRC) $(NDASUSER_INCS) $(SAL_INCS) package.mk $(OTHER_OBJS) $(EESAMPLE_SRCS) $(DOCS)
	rm -rf $(PACKAGE_DIR)
	mkdir -p $(PACKAGE_DIR)/inc/ndasuser
	mkdir -p $(PACKAGE_DIR)/inc/sal
	mkdir -p $(PACKAGE_DIR)/lib
	mkdir -p $(PACKAGE_DIR)/doc
	mkdir -p $(PACKAGE_DIR)/src/sal
	mkdir -p $(PACKAGE_DIR)/src/smapx
	mkdir -p $(PACKAGE_DIR)/sample/irx
	mkdir -p $(PACKAGE_DIR)/sample/test
	mkdir -p $(PACKAGE_DIR)/sample/eesample
	cp package.mk $(PACKAGE_DIR)/Makefile
	cp $(SAL_INCS) $(PACKAGE_DIR)/inc/sal
	cp $(NDASUSER_INCS) $(PACKAGE_DIR)/inc/ndasuser
	cp $(OTHER_OBJS) $(PACKAGE_DIR)/lib
	cp $(PS2SDK_IRXS) ${PACKAGE_DIR}/lib
	cp $(SAL_SRC) $(PACKAGE_DIR)/src/sal
	cp $(SMAPX_SRC) $(PACKAGE_DIR)/src/smapx
	cp $(IRX_SRC) $(PACKAGE_DIR)/sample/irx
	cp $(TEST_SRCS) $(PACKAGE_DIR)/sample/test
	cp $(EESAMPLE_SRCS) $(PACKAGE_DIR)/sample/eesample
	cp $(DOCS) $(PACKAGE_DIR)/doc
	cd $(nxp-dist) ; \
		tar zcvf $(PACKAGE_NAME).tar.gz \
		$(PACKAGE_NAME)/*

package-test: 
	cd $(nxp-dist) ; rm -rf $(PACKAGE_DIR) ;\
		tar zxvf $(PACKAGE_NAME).tar.gz
	/bin/bash -l -c "cd $(PACKAGE_DIR) ; make"
clean:
	make -C sal clean
	make -C irx clean
	make -C libndas clean
	make -C eesample clean
