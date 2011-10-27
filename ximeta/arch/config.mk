ifneq ($(nxpo-ndaslinux),y)

# Set default target

nxp-cpu?=x86
nxp-os?=linuxum
ifeq ($(nxp-vendor),)
nxp-target=$(nxp-cpu)_$(nxp-os)
else
nxp-target=$(nxp-vendor)
endif

include $(nxp-root)/arch/cpu/$(nxp-cpu).mk

include arch/os.mk

ifneq ($(nxp-vendor),)
include $(nxp-root)/arch/vendor/$(nxp-vendor).mk
endif

ifdef nxp-os_VERSION
nxp-target:=$(nxp-target)/$(nxp-os_VERSION)
endif

endif #ifneq ($(nxpo-ndaslinux),y)


