include platform/linuxum/sal/module.mk
include platform/linuxum/test/module.mk

#NDAS_EXTRA_CFLAGS = -DDEBUG_USE_MEM_TAG
all: $(LINUXUM_TEST_APPLICATION)

os-clean: linuxumtest-clean linuxum-sal-clean

