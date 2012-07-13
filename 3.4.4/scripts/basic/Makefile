###
# Makefile.basic list the most basic programs used during the build process.
# The programs listed herein is what is needed to do the basic stuff,
# such as splitting .config and fix dependency file.
# This initial step is needed to avoid files to be recompiled
# when kernel configuration changes (which is what happens when
# .config is included by main Makefile.
# ---------------------------------------------------------------------------
# fixdep: 	 Used to generate dependency information during build process

host-progs	:= fixdep
always		:= $(host-progs)
fixdep: fixdep.c
	$(CC) -o $@ $^
