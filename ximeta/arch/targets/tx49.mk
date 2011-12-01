linuxum-mips-be-debug:
	make nxp-cpu=tx49be nxpo-lfs=y nxpo-debug=y nxp-os=linuxum nxpo-pnp=y nxpo-sio=y nxpo-asy=y all
	cp /home/sjcho/xplat/release/tx49be_linuxum/lfstest.out /home/sjcho/ndas_be

linuxum-mips-be-clean:
	make nxp-cpu=tx49be nxp-os=linuxum clean

linuxum-mips-le-debug:
	make nxp-cpu=tx49le nxpo-debug=y nxp-os=linuxum nxpo-pnp=y nxpo-sio=y nxpo-asy=y all
	cp /home/sjcho/xplat/release/tx49le_linuxum/ndbench.out /home/sjcho/ndas

linuxum-mips-le-clean:
	make nxp-cpu=tx49le nxp-os=linuxum clean


RBTX4938_24_DIST_BIN=ndas_sal.o ndas_core.o ndas_block.o ndasadmin
RBTX4938_26_DIST_BIN=ndas_sal.ko ndas_core.ko ndas_block.ko ndasadmin
RBTX4938_LE_DIST_DES=/home/sjcho/ndas
RBTX4938_MVLLE_DIST_DES=/home/sjcho/ndas
RBTX4938_MVLBE_DIST_DES=/home/sjcho/ndas_mvl
RBTX4938_BE_DIST_DES=/home/sjcho/ndas_be
rbtx4938_le-install:
	cp -f $(addprefix ../xplat-objs/tx49le_linux/ndas-$(nxp-build-version)/,$(RBTX4938_24_DIST_BIN)) $(RBTX4938_LE_DIST_DES)
rbtx4938_be-install:
	cp -f $(addprefix ../xplat-objs/rbtx4938_be/ndas-$(nxp-build-version)/,$(RBTX4938_26_DIST_BIN)) $(RBTX4938_BE_DIST_DES)
rbtx4938_mvlle-install:
	cp -f $(addprefix ../xplat-objs/sharp/ndas-$(nxp-build-version)/,$(RBTX4938_24_DIST_BIN)) $(RBTX4938_MVLLE_DIST_DES)

rbtx4938_mvlle-package:
	make -C ../xplat-objs/sharp/ndas-$(nxp-build-version) clean
	tar zcf ndas_mips_mvlle.$(nxp-build-version).tgz -C ../xplat-objs/tx49mvlle_linux ndas-$(nxp-build-version)

rbtx4938_mvlle-rel: #release
	export NDAS_KERNEL_VERSION=2.4.20_mvl31 ;\
	export NDAS_KERNEL_PATH=/opt/montavista/previewkit/lsp/toshiba-rbhma4500-mips_fp_le-previewkit/linux-2.4.20_mvl31;\
	make nxp-os=linux nxp-cpu=tx49mvlle nxp-vendor=sharp nxpo-release=y nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=;\
	make -C ../xplat-objs/sharp/ndas-$(nxp-build-version)
	make rbtx4938_mvlle-install

rbtx4938_mvlle-dev:
	export NDAS_KERNEL_VERSION=2.4.20_mvl31;\
	export NDAS_KERNEL_PATH=/opt/montavista/previewkit/lsp/toshiba-rbhma4500-mips_fp_le-previewkit/linux-2.4.20_mvl31;\
	make nxp-os=linux nxp-cpu=tx49mvlle nxp-vendor=sharp nxpo-debug=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=y nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=y ;\
	make -C ../xplat-objs/sharp/ndas-$(nxp-build-version)
	make rbtx4938_le-install

rbtx4938_mvlle-clean:
	make -C ../xplat-objs/tx49le_linux/ndas-$(nxp-build-version) clean
	make nxp-os=linux nxp-cpu=tx49mvlle nxp-vendor=sharp nxpo-debug=y nxpo-pnp=y clean

rbtx4938_mvlbe-install:
	cp -f $(addprefix ../xplat-objs/tx49mvlbe_linux/ndas-$(nxp-build-version)/,$(RBTX4938_24_DIST_BIN)) $(RBTX4938_MVLBE_DIST_DES)

rbtx4938_mvlbe-package:
	make -C ../xplat-objs/tx49mvlbe_linux/ndas-$(nxp-build-version) clean
	tar zcf ndas_mips_mvlbe.$(nxp-build-version).tgz -C ../xplat-objs/tx49mvlbe_linux ndas-$(nxp-build-version)

sharp: rbtx4938_mvlbe-rel
sharpd: rbtx4938_mvlbe-dev
sharp-clean: rbtx4938_mvlbe-clean

rbtx4938_mvlbe-rel: #release
	export NDAS_KERNEL_VERSION=2.4.20_mvl31 ;\
	export NDAS_KERNEL_PATH=/root/src/mvl31_tx49be; \
	make nxp-os=linux nxp-cpu=tx49mvlbe nxp-vendor=sharp nxpo-release=y nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=;\
	make -C ../xplat-objs/tx49mvlbe_linux/ndas-$(nxp-build-version)
	make rbtx4938_mvlbe-install

rbtx4938_mvlbe-dev:
	export NDAS_KERNEL_VERSION=2.4.20_mvl31;\
	export NDAS_KERNEL_PATH=/root/src/mvl31_tx49be; \
	make nxp-os=linux nxp-cpu=tx49mvlbe nxp-vendor=sharp nxpo-debug=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=y ;\
	make -C ../xplat-objs/tx49mvlbe_linux/ndas-$(nxp-build-version)
	make rbtx4938_mvlbe-install

rbtx4938_mvlbe-clean:
	make -C ../xplat-objs/tx49be_linux/ndas-$(nxp-build-version) clean
	make nxp-os=linux nxp-cpu=tx49mvlbe nxp-vendor=sharp nxpo-debug=y nxpo-pnp=y clean

rbtx4938_le-package:
#	make rbtx4938_le-rel
	make -C ../xplat-objs/tx49le_linux/ndas-$(nxp-build-version) clean
	tar zcf ndas_mips_le.$(nxp-build-version).tgz -C ../xplat-objs/tx49le_linux ndas-$(nxp-build-version)

rbtx4938_le-rel: #release
	export NDAS_KERNEL_VERSION=2.4.20_mvl31 ;\
	export NDAS_KERNEL_PATH=/opt/montavista/previewkit/lsp/toshiba-rbhma4500-mips_fp_le-previewkit/linux-2.4.20_mvl31;\
	make nxp-os=linux nxp-cpu=tx49le nxp-vendor=rbtx4938_le nxpo-release=y nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=;\
	make -C ../xplat-objs/tx49le_linux/ndas-$(nxp-build-version)
	make rbtx4938_le-install

rbtx4938_le-dev:
	export NDAS_KERNEL_VERSION=2.4.20_mvl31;\
	export NDAS_KERNEL_PATH=/opt/montavista/previewkit/lsp/toshiba-rbhma4500-mips_fp_le-previewkit/linux-2.4.20_mvl31; \
	make nxp-os=linux nxp-cpu=tx49le nxp-vendor=rbtx4938_le nxpo-debug=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=y nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=y ;\
	make -C ../xplat-objs/tx49le_linux/ndas-$(nxp-build-version)
	make rbtx4938_le-install
rbtx4938_le-clean:
	make -C ../xplat-objs/tx49le_linux/ndas-$(nxp-build-version) clean
	make nxp-os=linux nxp-cpu=tx49le nxp-vendor=rbtx4938_le nxpo-debug=y nxpo-pnp=y clean

rbtx4938_be-rel: #release
	export NDAS_KERNEL_VERSION=2.6.10;\
	export NDAS_KERNEL_PATH=/root/src/linux-2.6.10-r4k/build_be
	make nxp-os=linux nxp-cpu=tx49be nxp-vendor=rbtx4938_be nxpo-release=y nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=y nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=;\
	make -C ../xplat-objs/rbtx4938_be/ndas-$(nxp-build-version)
	make rbtx4938_be-install
rbtx4938_be-package:
	make rbtx4938_be-rel
	make -C ../xplat-objs/rbtx4938_be/ndas-$(nxp-build-version) clean
	tar zcf ndas_mips_be.$(nxp-build-version).tgz -C ../xplat-objs/rbtx4938_be ndas-$(nxp-build-version)

rbtx4938_be-dev:
	export NDAS_KERNEL_VERSION=2.6.10;\
	export NDAS_KERNEL_PATH=/root/src/linux-2.6.10-r4k/build_be
	make EXPORT_LOCAL=y nxp-os=linux nxp-cpu=tx49be nxp-vendor=rbtx4938_be nxpo-debug=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=y nxpo-embedded=y mb-dist 
	export NDAS_DEBUG=y ;\
	make -C ../xplat-objs/rbtx4938_be/ndas-$(nxp-build-version)
	make rbtx4938_be-install

rbtx4938_be-clean:
	make nxp-os=linux nxp-cpu=tx49be nxp-vendor=rbtx4938_be nxpo-debug=y nxpo-sio=y nxpo-pnp=y clean

