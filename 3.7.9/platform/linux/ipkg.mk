
nxpl-ipkg-admin:=$(nxpl-tar-path)/ndas-admin_$(NDAS_VER_MAJOR).$(NDAS_VER_MINOR)-$(NDAS_VER_BUILD)_$(nxp-cpu).ipk
nxpl-ipkg-kernel:=$(nxpl-tar-path)/ndas-kernel_$(NDAS_VER_MAJOR).$(NDAS_VER_MINOR)-$(NDAS_VER_BUILD)_$(nxp-cpu).ipk
nxpl-ipkg-kernel-bin:=$(nxpl-ipkg-kernel).bin

$(nxpl-ipkg-kernel) $(nxpl-ipkg-admin): $(nxpl-tarball)
	cd $(nxpl-tar-path); \
	make ipkg

$(nxpl-ipkg-kernel-bin): $(nxpl-ipkg-kernel)
	/bin/sh $(nxp-root)/platform/linux/gen-sfx.sh $^ > $@

ipkg: $(nxpl-ipkg-kernel-bin) $(nxpl-ipkg-admin)

