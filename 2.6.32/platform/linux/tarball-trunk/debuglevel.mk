#-------------------------------------------------------------------------
# Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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


ifdef NDAS_DEBUGLEVEL
ndas-app-cflags+= -DDEBUG_LEVEL=$(NDAS_DEBUGLEVEL)
ndas-kernel-cflags+= -DDEBUG_LEVEL=$(NDAS_DEBUGLEVEL)
EXTRA_CFLAGS+= -DDEBUG_LEVEL=$(NDAS_DEBUGLEVEL)
endif
ifdef NDAS_DEBUGLEVEL_BLOCK
ndas-app-cflags+= -DDEBUG_LEVEL_BLOCK=$(NDAS_DEBUGLEVEL_BLOCK)
ndas-kernel-cflags+= -DDEBUG_LEVEL_BLOCK=$(NDAS_DEBUGLEVEL_BLOCK)
EXTRA_CFLAGS+= -DDEBUG_LEVEL_BLOCK=$(NDAS_DEBUGLEVEL_BLOCK)
endif
ifdef NDAS_DEBUG_DROP_BROADCAST
ndas-app-cflags+= -DNDAS_DEBUG_DROP_BROADCAST
ndas-kernel-cflags+= -DDNDAS_DEBUG_DROP_BROADCAST
EXTRA_CFLAGS+= -DDNDAS_DEBUG_DROP_BROADCAST
endif
