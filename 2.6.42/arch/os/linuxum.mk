nxp-cflags += -DLINUX -DLINUXUM -g -DDEBUG_USE_MEM_TAG -DDEBUG_MEMORY_LEAK
#nxp-cflags += -pg # temporary option for profiling
#XPLAT_EXTRA_LDFLAGS += -L$(nxp-root) -lc_p  # temporary option for profiling
#XPLAT_EXTRA_LDFLAGS += -L/usr/local/lib -lprofiler # use google profiler

#XPLAT_EXTRA_LDFLAGS += -static 
ifeq ($(nxp-cpu), tx49be)
XPLAT_EXTRA_LDFLAGS += -static 
endif

