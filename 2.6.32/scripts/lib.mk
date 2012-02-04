######################################################################
# Copyright ( C ) 2012 IOCELL Networks
# All rights reserved.
######################################################################
nxp-comma := ,
nxp-depfile = $(subst $(nxp-comma),_,$(@D)/$(@F).d)
# ===========================================================================
# Generic stuff
# ===========================================================================

# function to only execute the passed command if necessary
# >'< substitution is for echo to work, >$< substitution to preserve $ when reloading .cmd file
# note: when using inline perl scripts [perl -e '...$$t=1;...'] in $(cmd_xxx) double $$ your perl vars

nxp_if_changed = $(if $(strip $? \
		          $(filter-out $(nxp_cmd_$(1)),$(nxp_cmd_$@))\
			  $(filter-out $(nxp_cmd_$@),$(nxp_cmd_$(1)))),\
	@set -e; \
	mkdir -p $(@D); \
	$(if $($(nxp_quiet)nxp_cmd_$(1)),echo '  $(subst ','\'',$($(nxp_quiet)nxp_cmd_$(1)))';) \
	$(nxp_cmd_$(1)); \
	echo 'nxp_cmd_$@ := $(subst $$,$$$$,$(subst ','\'',$(nxp_cmd_$(1))))' > $(@D)/$(@F).cmd)


# execute the command and also postprocess generated .d dependencies
# file

nxp_if_changed_dep = $(if $(strip $? $(filter-out FORCE $(wildcard $^),$^)\
		          $(filter-out $(nxp_cmd_$(1)),$(nxp_cmd_$@))\
			  $(filter-out $(nxp_cmd_$@),$(nxp_cmd_$(1)))),\
	@set -e; \
	mkdir -p $(@D); \
	$(if $($(nxp_quiet)nxp_cmd_$(1)),echo '  $(subst ','\'',$($(nxp_quiet)nxp_cmd_$(1)))';) \
	$(nxp_cmd_$(1)); \
	scripts/basic/fixdep $(nxp-depfile) $@ '$(subst $$,$$$$,$(subst ','\'',$(nxp_cmd_$(1))))' > $(@D)/.$(@F).tmp; \
	rm -f $(nxp-depfile); \
	mv -f $(@D)/.$(@F).tmp $(@D)/$(@F).cmd)

# Usage: $(call nxp_if_changed_rule,foo)
# will check if $(nxp_cmd_foo) changed, or any of the prequisites changed,
# and if so will execute $(nxp_rule_foo)

nxp_if_changed_rule = $(if $(strip $? \
		               $(filter-out $(nxp_cmd_$(1)),$(nxp_cmd_$@))\
			       $(filter-out $(nxp_cmd_$@),$(nxp_cmd_$(1)))),\
			@set -e; \
			$(nxp_rule_$(1)))

# If quiet is set, only print short version of command

nxp_cmd = @$(if $($(nxp_quiet)nxp_cmd_$(1)),echo '  $(subst ','\'',$($(nxp_quiet)nxp_cmd_$(1)))' &&) $(nxp_cmd_$(1))

#	$(call descend,<dir>,<target>)
#	Recursively call a sub-make in <dir> with target <target> 
# Usage is deprecated, because make do not see this as an invocation of make.
#xplat# xplat_descend =$(Q)$(MAKE) -f $(if $(KBUILD_SRC),$(srctree)/)scripts/Makefile.build obj=$(1) $(2)

# Shorthand for $(Q)$(MAKE) -f scripts/Makefile.build obj=
# Usage:
# $(Q)$(MAKE) $(build)=dir
#xplat#xplat_build := -f $(if $(KBUILD_SRC),$(srctree)/)scripts/Makefile.build obj
