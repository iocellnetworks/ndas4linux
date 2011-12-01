
File descriptions
 * ndas/: library and some of sources and headers for NDAS driver.
 * ndas/inc/ndasuser/: Header files for using NDAS library.
 * ndas/inc/sal/ : Header files for SAL(System Abstraction Layer). 
 * ndas/sal : Source files for SAL. 
 * ndas/Makefile: Makefile for generating NDAS libary 
 * ndas/libndas-debug.a, libndas-release.a: NDAS library 
 * sdk/ : Collection of modified files from Syabas SDK.
 * sdk/main.c : Includes a sample usage of NDAS library
 * sdk/include/lpx.h : Header for LPX protocol used by NDAS library
 * sdk/sdesigns/enet.c : Added code for forwarding incoming packet to NDAS library. Need to add "-DINCLUDE_NDAS" option to enable NDAS library.

How to build NDAS library
 In "ndas" directory, there is Makefile which generates "libndas.a" from base library file and SAL sources. You can select a library among debug version and release version by setting "USE_DEBUG_VER" flag in Makefile. 
 Source files in "sal" directory are necessary if you need to modify system dependent functions such as process priorities used by NDAS library and network interface.

For APIs, see ndasuser.h file.

