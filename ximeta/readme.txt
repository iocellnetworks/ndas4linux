Version 0.0.2
---------
Source/binary distribution policy
 * For widely used operating systems such as linux-x86.
   - Binary: Lpx/Netdisk module. User API library
   - Source: User API headers. Command line interface.
 * For custom kernel and RTOSes
   - Binary: Lpx/netdisk object compiled for given platform. User API library
   - Source: User API header. Command line interface. SAL header, implementation, SAL-test code.

--------------
Directory Descriptions

/ source root directory
 - readme.txt
 - howtobuild.txt
 - make configuration (Make.config): current release version, directory tree to be built
/arch                         : Build configurations
/arch/cpu                     : CPU-dependent
/arch/os                      : OS-dependent
/arch/targets                 : Target board dependent
/arch/vendor                  : Vendor-dependent
/doc                          : Documents, documentation templates
/inc/                         : headers
/inc/jukebox                  : Jukebox headers
/inc/lpx                      : lpx headers
/inc/ndasuser                 : NDAS APIs
/inc/netdisk                  : Netdisk headers
/inc/raid                     : RAID headers
/inc/sal                      : System abtract layer (SAL) API
/inc/sal/generic              : Generic SAL headers
/inc/sal/linux                : Linux SAL headers
/inc/sal/ucosii               : uCosII SAL headers
/jukebox                      : Jukebox source code
/lpx                          : LPX protocol core implementation
/netdisk                      : Netdisk core implementation
/platform                     : Platform-dependent source code
                                including SAL and management application implementaion
/platform/broadq              : Play Station 2 media player
/platform/linux               : Linux kernel mode
/platform/linuxum             : Linux user mode
/platform/ps2dev              : Open source Play Station 2 development
/platform/ucosii              : uCosII
/platform/vxworks             : VxWorks (Dummy)
/raid                         : RAID 0/1/JBOD
/scripts                      : Utility scripts
/scripts/basic                : Build utility
/scripts/build                : Build scripts for binary distribution
/scripts/target               : Test utility on target boards
/scripts/valgrind             : Valgrind ( debugging / profiling tool ) scripts
/test                         : Test program source code
/xlib                         : xbuf, hash, dpc, event on SAL
