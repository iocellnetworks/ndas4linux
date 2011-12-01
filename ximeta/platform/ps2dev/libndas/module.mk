######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

NDAS_LIB_EXPORT_LST=platform/ps2dev/libndas/ndas_exports.lst

ND_LIBS:=$(addprefix $(nxp-dist)/, libndasuser.o libtest.o libnetdisk.o liblpx.o libxlib.o) 

MODULE_OBJ += $(nxp-dist)/libndas.o

$(nxp-dist)/libndas.o: $(NDAS_LIB_EXPORT_LST) $(ND_LIBS)
	$(LD) -r -o $@ $(LDFLAGS) -x -retain-symbols-file $(NDAS_LIB_EXPORT_LST) $(ND_LIBS)
