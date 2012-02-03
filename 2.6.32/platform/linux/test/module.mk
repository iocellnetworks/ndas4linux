#-------------------------------------------------------------------------
# Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
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
#
# platform/linux/test/*.c
# equivalent test/*.c file should exist.
#
#LINUX_TEST_SRC:=lpxtxtest.c lpxrxtest.c random-access.c
LINUX_TEST_SRC:=random-access.c
LINUX_TEST_SRC:=$(LINUX_TEST_SRC:%.c=$(LINUX_TEST_PATH)/%.c)
LINUX_TEST_OBJ:=$(LINUX_TEST_SRC:%.c=$(nxp-build)/%.o)
nxpl-test-apps:=$(LINUX_TEST_SRC:$(LINUX_TEST_PATH)/%.c=$(nxp-dist)/%.out)

TEST_PATH=test
#
# include the corresponding test/*.c file of LINUX_TEST_SRC
#
TEST_SRC:=$(LINUX_TEST_SRC:$(LINUX_TEST_PATH)/%.c=$(TEST_PATH)/%.c)
TEST_OBJ:=$(TEST_SRC:%.c=$(nxp-build)/%.o)

$(nxp-build)/$(TEST_PATH):
	@mkdir -p $@

$(nxp-build)/$(LINUX_TEST_PATH):
	@mkdir -p $@

$(nxp-build)/$(LINUX_TEST_PATH)/%-fix.sh: $(LINUX_TEST_PATH)/function-map.txt
	@mkdir -p $(@D)
	cat $< | \
		awk 'BEGIN{ printf "sed"}{ printf " -e \"s|%s|%s|g\"",$$1,$$2}' >\
		$@
#
# Convert lpx/xplat version of test source codes into the berkely socket/standard c version.	
#
$(LINUX_TEST_OBJ:%.o=%.c): $(nxp-build)/$(LINUX_TEST_PATH)/%.c: \
		$(TEST_PATH)/%.c \
		$(LINUX_TEST_PATH)/%.c \
		$(nxp-build)/$(LINUX_TEST_PATH)/%-fix.sh
	@mkdir -p $(@D)
	cat $(TEST_PATH)/$(notdir $<) $(LINUX_TEST_PATH)/$(notdir $<)| \
		/bin/sh $(@:%.c=%-fix.sh) > $@

$(LINUX_TEST_OBJ): %.o : %.c
	$(call nxp_if_changed,apps_o_c)

$(nxpl-test-apps): $(nxp-dist)/%.out : $(nxp-build)/$(LINUX_TEST_PATH)/%.o 
	$(APP_CC) $(APP_LDFLAGS) -o $@ $^

$(nxp-build)/$(LINUX_TEST_PATH)/.done: $(nxp-build)/$(LINUX_TEST_PATH) $(nxpl-test-apps)
	@touch $@

