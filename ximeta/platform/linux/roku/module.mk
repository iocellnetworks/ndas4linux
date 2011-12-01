################################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
#
# XIMETA Internal code
# This is the trade secret of XIMETA, Inc.
# If you obtained this code without any permission of XIMETA, Inc.
# Please email to support@ximeta.com
# Thank You.
################################################################################

#roku-gui-app:=$(roku-path)/ndas-mount.roku
#roku-apps:=$(roku-path)/InstallNetD.roku \
#	$(roku-path)/netdisk.startup.script  \
#	$(roku-path)/UninstallNetD.roku \
#	$(roku-path)/swap

nxp-linux-addon-files:=$(wildcard $(nxp-linux-addon-path)/addon/*.cc \
						$(nxp-linux-addon-path)/addon/*.h ) \
					$(nxp-linux-addon-path)/addon/ndas.jpg \
					$(nxp-linux-addon-path)/addon/module.mk \
					$(nxp-linux-addon-path)/InstallNetD.roku \
					$(nxp-linux-addon-path)/UninstallNetD.roku \
					$(nxp-linux-addon-path)/ndas.startup.script 
 
					
					
								 
