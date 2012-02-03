#-------------------------------------------------------------------------
# Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
# All rights reserved.
#
# LICENSE TERMS
#
# The free distribution and use of this software in both source and binary 
# form is allowed (with or without changes) provided that:
#
#   1. distributions of this source code include the above copyright 
#      notice, this list of conditions and the following disclaimer;
#
#   2. distributions in binary form include the above copyright
#      notice, this list of conditions and the following disclaimer
#      in the documentation and/or other associated materials;
#
#   3. the copyright holder's name is not used to endorse products 
#      built using this software without specific written permission. 
#      
# ALTERNATIVELY, provided that this notice is retained in full, this product
# may be distributed under the terms of the GNU General Public License (GPL v2),
# in which case the provisions of the GPL apply INSTEAD OF those given above.
# 
# DISCLAIMER
#
# This software is provided 'as is' with no explcit or implied warranties
# in respect of any properties, including, but not limited to, correctness 
# and fitness for purpose.
#-------------------------------------------------------------------------


xixfs_root=$(nxp-root)/platform/linux/xixfs
xplat_path=$(nxp-root)

xixfs: FORCE
	@echo Invoking make for xixfs
	$(MAKE) -C $(xixfs_root) \
		ndas_root=$(xixfs_root) \
		xplat_path=$(xplat_path)

clean-xixfs:
	@echo Invoking make for clean-xixfs
	$(MAKE) -C $(xixfs_root) \
		ndas_root=$(xixfs_root) \
		xplat_path=$(xplat_path) \
		clean-xixfs

FORCE:
