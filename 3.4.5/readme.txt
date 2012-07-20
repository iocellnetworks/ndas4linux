# -----------------------------------------------------------------------------
# Copyright (c) 2011 IOCELL Networks Corp., Plainsboro NJ 08536, USA
# All rights reserved.
# -----------------------------------------------------------------------------

Version 2.6.0

Source/binary distribution policy
 * Version Pakages for widely used operating systems such as linux-x86.
   - Binary: Lpx/Netdisk module. User API library
   - Source: User API headers. Command line interface.
   - License: IOCELL Networks - GPL
 * Custom Kernel and any other applications or distributions
   - Source: Kernel modules and command line interface available by request
   - License: GPL

--------------
Directory Descriptions

/ source root directory
  - COPYRIGHT           : Use for IOCELL Networks' scripts and files
  - CREDITS.txt         : Credit for open source component developers
  - ELUA.txt            : Applied to IOCELL Networks distribution
  - IOCELL Networks GPL : When IOCELL Networks copyright applies with GPL
  - Makefile            : Compile NDAS For Linux OS in various architectures
  - changelog.txt       : Track development of NDAS For Linux
  - common.mk           : adds compiler flags for various nxpo-* options
  - gpl-2.0.txt         : GNU GENERAL PUBLIC LICENSE Version 2, June 1991
  - ndaslinux.mk        : make nxp-* options. also some defined export paths
  - readme.txt          : Describing the contents of NDAS For Linux Project
  - version.mk          : Sets the NDAS version

    /arch               : make nxp* options search here for configurations
    /arch/cpu           : CPU-dependent
    /arch/os            : OS-dependent
    /arch/targets       : Target board dependent
    /arch/vendor        : Vendor-dependent
    /dist               : version packages. users must compile and install
    /doc                : documentation, templates, build instructions
    /inc/               : headers
      - dbgcfg.h        : change the debugging output for various components
      - dbgcfg.default  : restore dbgcfg.h to default settings with cp
    /inc/lpx            : lpx protocol headers (NDAS Transport Protocol)
    /inc/lspx           : lspx headers (ATA disk communications)
    /inc/ndasuser       : NDAS APIs
    /inc/netdisk        : Netdisk headers
    /inc/raid           : NDAS RAID headers
    /inc/sal            : System abtract layer (SAL) API
    /inc/sal/generic    : Generic SAL headers
    /inc/sal/linux      : Linux SAL headers
    /inc/sal/ucosii     : uCosII SAL headers
    /inc/xixfsevent		: possibly (Xi)meta's xfs file system events?
    /inc/xlib           : Library headers - dpc, gtypes, buffer and hash
    /lpx                : LPX protocol core implementation
	 /lspx					: LSPX core implementation
	 /netdisk            : Netdisk core implementation
    /platform           : Platform-dependent source code including SAL and
                          management application implementation
    /platform/linux     : Linux kernel mode
    /platform/linuxum   : Linux user mode
    /raid               : RAID 0/1/JBOD
    /scripts            : Utility scripts
    /scripts/basic      : Build utility
    /scripts/build      : Build scripts for binary distribution
    /scripts/target     : Test utility on target boards
    /scripts/valgrind   : Valgrind ( debugging / profiling tool ) scripts
    /test               : Test program source code
    /xlib               : xbuf, hash, dpc, event on SAL
