######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

nxp-sal-src:=$(wildcard $(nxp-sal-path)/*.c)
nxp-sal-obj:=$(nxp-sal-src:%.c=$(nxp-build)/%.o)
