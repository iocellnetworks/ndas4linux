readme.txt

                       == Xixcore ==

* What is this?
  a multiplaform core logic/data structure library for Xixfs.
  Xixcore includes platform independent part called 'core' and
  platform abstraction part called 'Xixcore system'.

* directories
  - core: platform independent code
  - sysdep: platform dependent code in each sub-directories.
  - sysdep/linux: linux kernel abstraction code and build utility.
  - sysdep/ndasxplt: NDAS cross platform abstraction code and build utility.
  - sysdep/osx: OS/X kernel platform abstraction code and build utility.
  - sysdep/winnt: Windows NT kernel platform abstraction code and build utility.
  - include: Xixcore public include files.
  - include/xcsystem: Xixcore system include files.
  - include/xcsystem/linux: Linux dependent include files.
  - include/xcsystem/ndasxplt: NDAS cross platform dependent include files.
  - include/xcsystem/osx: Apple OS/X dependent include files.
  - include/xcsystem/winnt: Windows NT dependent include files.
  - include/xixcore: Xixcore files.
 
 * How to add another platform?
  - Create a platform include directory under 'include/xcsystem' directory.
  - Create a platform include files such as 'xcsysdep.h'.
  - Create a platform code/build directory under 'sysdep' directory.
  - Create a platform code/build files such as 'xcsystem.c' under 'sysdep' directory.
  - 'xcsystem.c' and platform files must implement APIs in 'include/xcsystem/system.h'.
  - 'xcsysdep.h' and platform includes files must define structures in 'include/systypes.h'.
 
Copyright ( C ) 2012 IOCELL Networks
All rights reserved.
