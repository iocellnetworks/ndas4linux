linux-um-test-path=platform/linuxum/test
LINUXUM_TESTS=ndemu.c #ndbench.c #gentest.c #ndemu.c multidisk.c probe.c  lanscsicli.c
LINUXUM_TEST_SRC= $(addprefix $(linux-um-test-path)/,\
	$(LINUXUM_TESTS))
LINUXUM_TEST_OBJ=$(LINUXUM_TEST_SRC:%.c=$(nxp-build)/%.o)
LINUXUM_NDASUM_LIB=$(nxp-dist)/libndasum.a
LINUXUM_TEST_APPLICATION=$(patsubst platform/linuxum/test/%.c,$(nxp-dist)/%.out,$(LINUXUM_TEST_SRC))
linux-um-internal-test-apps=\
	$(nxp-dist)/ndemu.out
#	$(nxp-dist)/ndbench.out
#	$(nxp-dist)/gentest.out
#	$(nxp-dist)/ndbench.out 
#	$(nxp-dist)/lanscsicli.out $(nxp-dist)/gentest.out $(nxp-dist)/ndasuserlib.out #  $(nxp-dist)/fattest.out 

linuxum-test-cmd=$(nxp-sal-src:$(nxp-root)/%.c=$(nxp-build)/%.cmd)
linuxum-test-dep=$(nxp-sal-src:$(nxp-root)/%.c=$(nxp-build)/%.dep)

include $(nxp-root)/test/module.mk #todo

nxp-extra-obj:=$(LINUXUM_TEST_OBJ) $(nxp-test-obj)
ifeq ($(nxp-cpu), tx49be)
nxp-cflags += -static
endif

#$(filter-out $(linux-um-internal-test-apps),$(LINUXUM_TEST_APPLICATION)):\
	$(nxp-dist)/%.out: $(nxp-build)/platform/linuxum/test/%.o \
		$(nxp-fat32lib-obj) \
		$(nxp-sal-obj) \
		$(nxp-lib-obj) \
		$(nxp-build)/test/%.o
#	$(CC) $(nxp-cflags) -o $@ $^ -lpthread -lreadline -lcurses

$(LINUXUM_NDASUM_LIB): $(nxp-sal-obj) $(nxp-lib-internal)
	$(AR) cru $@ $^

$(linux-um-internal-test-apps): \
	$(nxp-dist)/%.out:  $(nxp-build)/platform/linuxum/test/%.o \
		$(LINUXUM_NDASUM_LIB)
	$(CC) $(nxp-cflags) -lpthread -o $@ $^

linuxumtest-clean:
	rm -rf $(LINUXUM_TEST_OBJ) $(LINUXUM_TEST_APPLICATION) $(linuxum-test-cmd)\
		 $(linuxum-test-dep) $(LINUXUM_NDASUM_LIB)
	
