nxpp-path:=platform/broadq

broadq-prepare-files:=libndas.a
broadq-prepare-files:=$(broadq-prepare-files:%=$(nxpp-path)/ximeta/lib/%) $(nxpp-path)/CREDITS.txt
broadq-ximeta-headers:=$(wildcard inc/sal/*.h inc/sal/generic/*.h inc/ndasuser/*.h)
broadq-ximeta-headers:=$(broadq-ximeta-headers:%=$(nxpp-path)/ximeta/%)

BROADQ_BUILD?=100
broadq-package:=$(nxp-build)/ndas-broadq-b$(BROADQ_BUILD).tar.gz
# list the file under the directory without the svn files
broadq-package-files=$(shell cd $(nxpp-path); find ximeta \
	-type f \
	! -path \*.svn\* \
	! -name module.mk ) netdisk/netdisk.c Makefile CREDITS.txt

all: $(broadq-package)
	svn update platform/broadq/ximeta/src/sal/net.iop.o
	touch platform/broadq/ximeta/src/sal/net.iop.o
	make -C $(nxpp-path)

$(broadq-package): $(broadq-prepare-files) $(broadq-ximeta-headers)
	@mkdir -p $(@D)
	FILE=`pwd`/$@ ; \
	cd $(nxpp-path) ; tar zcvf $$FILE $(broadq-package-files)
clean: clean-broadq
clean-broadq:
	make -C $(nxpp-path) clean
	rm -rf $(broadq-ximeta-headers) $(broadq-prepare-files)

$(nxpp-path)/scripts/lib.mk : scripts/lib.mk
	@mkdir -p $(@D)
	cp $^ $@

quiet_nxp_cmd_ps2_ld= PS2-LD $@
      nxp_cmd_ps2_ld= $(LD) -r -x -mmipsirx -o $@ $(filter-out FORCE,$^)

$(nxpp-path)/ximeta/lib/libndas.a: $(nxp-build)/ndas-bq.o FORCE
	@mkdir -p $(@D)
	$(AR) cru $@ $<

$(nxp-build)/ndas-bq.o: $(nxp-build)/ndas-bq-internal.o FORCE
ifdef XPLAT_OBFUSCATE
	$(call obfuscate,$<,$@)	
else
	$(OBJCOPY) --redefine-sym _start=1x $< $@
endif

$(nxp-build)/ndas-bq-internal.o: $(nxp-netdisk-obj) \
                $(nxp-xlib-obj) \
                $(nxp-lpx-obj) $(nxpp-path)/_start.o \
		FORCE
	@mkdir -p $(@D)
	$(call nxp_if_changed,ps2_ld)

$(nxpp-path)/_start.o: %.o : %.c
	$(call nxp_if_changed_dep,cc_o_c)

$(nxpp-path)/CREDITS.txt: CREDITS.txt
	cp $^ $@

$(broadq-ximeta-headers): $(nxpp-path)/ximeta/%.h : %.h
	@mkdir -p $(@D)
	cp $^ $@
	
broadq-clean:
