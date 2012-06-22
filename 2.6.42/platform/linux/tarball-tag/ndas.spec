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

# Epoch is increased to 1 due to version scheme changes
%define ndas_rpm_epoch 1
%define ndas_rpm_version 2.6
%define ndas_rpm_build 42
%define ndas_kernel_version %(if [ -z "$NDAS_KERNEL_VERSION" ]; then uname -r ; else echo $NDAS_KERNEL_VERSION; fi)
%define ndas_kernel_version_simple %( echo %{ndas_kernel_version} | sed -e 's|-smp4G||g' -e 's|-bigsmp||g' -e 's|-psmp||g' -e 's|-smp||g' -e 's|smp||g' -e 's|xen0||' -e 's|xenU||' -e 's|-xenpae||' -e 's|-xen||' -e 's|xen||' -e 's|-default||' -e 's|-hugemem||' -e 's|hugemem||' -e 's|-kdump||' -e 's|kdump||' -e 's|PAE||' -e 's|-debug||' -e 's|-bigmem||' -e 's|bigmem||' -e 's|-SLRS||' -e 's|SLRS||' -e 's|-enterprise||' -e 's|enterprise||')
%define ndas_kernel_version_caninical_name %( echo %{ndas_kernel_version_simple} | sed -e 's|-|_|g')
%define ndas_rpm_release %{ndas_kernel_version_caninical_name}.%{ndas_rpm_build}
#
# Distros
#
%define ndas_redhat %(test ! -n "$NDAS_REDHAT"; echo $?)
%define ndas_suse %(test ! -n "$NDAS_SUSE"; echo $?)

%if !%{ndas_redhat} && !%{ndas_suse}
%define ndas_redhat %(test ! -f /etc/fedora-release -a ! -f /etc/redhat-release -a ! -f /etc/mandrake-release -a ! -f /etc/mandriva-release -a ! -f /etc/mandrakelinux-release; echo $?)
%define ndas_suse %(test ! -f /etc/SuSE-release; echo $?)
%endif
#
# Identify which kernel version we will build the NDAS driver for.
#
%define ndas_for_2_4 %(echo %{ndas_kernel_version} | grep -c "^2\.4\.")
%define ndas_for_2_6 %(echo %{ndas_kernel_version} | grep -c "^2\.6\.")
%define ndas_for_2_6_25 %(echo %{ndas_kernel_version} | grep -c "^2\.6\.25")
%define ndas_for_2_6_26 %(echo %{ndas_kernel_version} | grep -c "^2\.6\.26")
#
# If smp or UP(default for SuSE), or xen0, xenU, xen(SuSE), xenpae(SuSE 10.1 or above), bigsmp(SuSE 9.1 or aboe), smp4G(9.0 or below) hugemem(Redhat EL, CentOS), kdump(FC5)
#
%define ndas_for_smp4G %(echo %{ndas_kernel_version} | grep -c smp4G)
%define ndas_for_psmp %(echo %{ndas_kernel_version} | grep -c psmp)
%define ndas_for_smp %(echo %{ndas_kernel_version} | grep -v smp4G | grep -v bigsmp| grep -v psmp| grep -c smp)
%define ndas_for_xen0 %(echo %{ndas_kernel_version} | grep -c xen0)
%define ndas_for_xenU %(echo %{ndas_kernel_version} | grep -c xenU)
%define ndas_for_xenpae %(echo %{ndas_kernel_version} | grep -c xenpae)
%define ndas_for_default %(echo %{ndas_kernel_version} | grep -c default)
%define ndas_for_deflt %(echo %{ndas_kernel_version} | grep -c deflt)
%define ndas_for_xen %(echo %{ndas_kernel_version} | grep -v xen0 | grep -v xenU | grep -v xenpae | grep -c xen)
%define ndas_for_bigsmp %(echo %{ndas_kernel_version} | grep -c bigsmp)
%define ndas_for_hugemem %(echo %{ndas_kernel_version} | grep -c hugemem)
%define ndas_for_kdump %(echo %{ndas_kernel_version} | grep -c kdump)
%define ndas_for_PAE %(echo %{ndas_kernel_version} | grep -c PAE)
%define ndas_for_debug %(echo %{ndas_kernel_version} | grep -c debug)
%define ndas_for_bigmem %(echo %{ndas_kernel_version} | grep -c bigmem)
%define ndas_for_SLRS %(echo %{ndas_kernel_version} | grep -c SLRS)
%define ndas_for_enterprise %(echo %{ndas_kernel_version} | grep -c enterprise)
#
# postfix name of package name ie: -smp, -hugmem
#
%define ndas_name_postfix %(echo "") 
%if %{ndas_for_debug}
%define ndas_name_postfix debug
%endif
%if %{ndas_for_PAE}
%define ndas_name_postfix PAE
%endif
%if %{ndas_for_kdump}
%define ndas_name_postfix kdump
%endif
%if %{ndas_for_bigmem}
%define ndas_name_postfix bigmem
%endif
%if %{ndas_for_hugemem}
%define ndas_name_postfix hugemem
%endif
%if %{ndas_for_default}
%define ndas_name_postfix default
%endif
%if %{ndas_for_deflt}
%define ndas_name_postfix deflt
%endif
%if %{ndas_for_smp}
%define ndas_name_postfix smp
%endif
%if %{ndas_for_psmp}
%define ndas_name_postfix psmp
%endif
%if %{ndas_for_smp4G}
%define ndas_name_postfix smp4G
%endif
%if %{ndas_for_xen0}
%define ndas_name_postfix xen0
%endif
%if %{ndas_for_xenU}
%define ndas_name_postfix xenU
%endif
%if %{ndas_for_xenpae}
%define ndas_name_postfix xenpae
%endif
%if %{ndas_for_xen}
%define ndas_name_postfix xen
%endif
%if %{ndas_for_bigsmp}
%define ndas_name_postfix bigsmp
%endif
%if %{ndas_for_SLRS}
%define ndas_name_postfix SLRS
%endif
%if %{ndas_for_enterprise}
%define ndas_name_postfix enterprise
%endif

#
# Define
#  1. the rpm name of this package
#  2. the rpm name of the kernel on which this rpm depends 
#
%if %{ndas_suse}
#
# SuSE kernel module rpm name
#
#%define ndas_suse_rpm_name km_ndas
# SuSE kernel module has names like "km_{module_name}", but for now, we will use ndas-kernel.
#
%define ndas_suse_rpm_name ndas-kernel
#
# Add post fix such as smp, psmp, smp4G and so on.
#
%define ndas_rpm_name %{ndas_suse_rpm_name}-%{ndas_name_postfix}

%define ndas_rpm_kernel %(if [ "%{ndas_for_2_4}" == "1" ]; then echo "k_" ; else echo "kernel-"; fi)%{ndas_name_postfix}
%else

%if "%{ndas_name_postfix}" == ""
%define ndas_rpm_name ndas-kernel
%define ndas_rpm_kernel kernel
%else
%define ndas_rpm_name ndas-kernel-%{ndas_name_postfix}
%define ndas_rpm_kernel kernel-%{ndas_name_postfix}
%endif
%endif

# 
# Module's extension
#
%if %{ndas_for_2_4}
%define module_ext o
%else
%define module_ext ko
%endif
#
# The maximum hard disk supported by NDAS Host in the same time
#
# For kernel 2.4.x , 32
#
%define ndas_max_slot_24 32
# For kernel 2.6.x  (TODO: should be 1048576)

%define ndas_max_slot_26 16

%if %{ndas_for_2_4}
%define ndas_max_slot %(if [ -z "$NDAS_MAX_SLOT" ]; then echo %{ndas_max_slot_24}; else echo $NDAS_MAX_SLOT; fi)
%else
%define ndas_max_slot %(if [ -z "$NDAS_MAX_SLOT" ]; then echo %{ndas_max_slot_26}; else echo $NDAS_MAX_SLOT; fi)
%endif
# Turn off debuginfo package
%define debug_package %{nil}

Summary: NDAS Software for Linux operating system
Name: %{ndas_rpm_name}
Epoch: %{ndas_rpm_epoch}
Version: %{ndas_rpm_version}
Release: %{ndas_rpm_release}
Vendor: IOCELL Networks
Packager: Gene Park (jhpark)
License: Proprietary Copyright (c) IOCELL Networks
Group: System Environment/Kernel
Source0: ndas-%{ndas_rpm_version}-%{ndas_rpm_build}.tar.gz
URL: http://linux.iocellnetworks.com
BuildRoot: %{_tmppath}/ndas-buildroot
%if %{ndas_suse}
BuildPrereq: /bin/uname /usr/bin/sed 
%else
BuildPrereq: /bin/uname /bin/sed 
%endif
 
%if %{ndas_for_2_6_25} || %{ndas_for_2_6_26}
%else
Prereq: %{ndas_rpm_kernel} = %{ndas_kernel_version_simple}
%endif
Obsoletes: ndas
Provides: ndas
ExclusiveArch: i386 i486 i586 i686 athlon
%description 
%{ndas_rpm_name} - This software allows the user to access the NDAS device via the Network. This package only works with kernel version %{ndas_kernel_version_simple}.

%package -n ndas-admin
Summary: Administration tool for NDAS device.
Group: System Environment/Base
Release: %{ndas_rpm_build}
Prereq: ndas = %{ndas_rpm_version}-%{ndas_rpm_build}
%description -n ndas-admin
ndas-admin - This program allows users to register/enable/disable/unregister NDAS device.

%prep
%setup -q -n ndas-%{ndas_rpm_version}-%{ndas_rpm_build}
%build
export NDAS_EXTRA_CFLAGS="$NDAS_EXTRA_CFLAGS -DNDAS_MAX_SLOT=%{ndas_max_slot}"
make %{?ndas_make_opts} ndas-kernel ndas-admin
echo %{_rpmdir}/%{_target_cpu}/%{name}-%{ndas_rpm_version}-%{ndas_rpm_release}.%{_target_cpu}.rpm > /tmp/.file || true 
%install
rm -rf $RPM_BUILD_ROOT
install -d -m 755 $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas
install -m 644 ndas_sal.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_sal.%{module_ext}
install -m 644 ndas_core.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_core.%{module_ext}
install -m 644 ndas_block.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_block.%{module_ext}
%if %{ndas_for_2_6}
install -m 644 ndas_block.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_scsi.%{module_ext}
%endif
install -m 444 CREDITS.txt $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/CREDITS.txt

install -d -m 755 $RPM_BUILD_ROOT/usr/share/ndas/
install -d -m 755 $RPM_BUILD_ROOT%{_sbindir}/
install -d -m 755 $RPM_BUILD_ROOT%{_sysconfdir}/init.d
install -m 700 mknod.sh $RPM_BUILD_ROOT/usr/share/ndas/mknod.sh
install -s -m 755 ndasadmin $RPM_BUILD_ROOT%{_sbindir}/ndasadmin
install -m 755 ndas $RPM_BUILD_ROOT/usr/share/ndas
install -m 755 ndas.suse $RPM_BUILD_ROOT/usr/share/ndas
install -m 644 README $RPM_BUILD_ROOT/usr/share/ndas/README
install -m 644 INSTALL $RPM_BUILD_ROOT/usr/share/ndas/INSTALL
install -m 644 EULA.txt $RPM_BUILD_ROOT/usr/share/ndas/EULA.txt
%post
depmod > /dev/null 2>&1 || true
%post -n ndas-admin
if [ -f /dev/ndas ] ; then
    rm -f /dev/ndas; # for ticket #44 of ndas4linux.iocellnetworks.com
fi     

if [ ! -f /etc/SuSE-release ] ; then
    ln -sf /usr/share/ndas/ndas %{_sysconfdir}/init.d/ndas;
    /sbin/chkconfig --add ndas;
    /sbin/chkconfig --level 2345 ndas on ;
else
    ln -sf /usr/share/ndas/ndas.suse %{_sysconfdir}/init.d/ndas;
    /sbin/chkconfig --add ndas;
fi

/bin/sh /etc/init.d/ndas start
%preun -n ndas-admin
/bin/sh /etc/init.d/ndas stop
%postun -n ndas-admin
rm -f %{_sysconfdir}/rc?.d/[SK]??ndas > /dev/null 2>&1 || /bin/true
rm -f %{_sysconfdir}/init.d/ndas
/sbin/chkconfig --del ndas > /dev/null 2>&1 || /bin/true

rm -rf /dev/nd* || /bin/true
%clean
rm -rf $RPM_BUILD_ROOT
%files
/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_sal.%{module_ext}
/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_core.%{module_ext}
/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_block.%{module_ext}
%if %{ndas_for_2_6}
/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_scsi.%{module_ext}
%endif
/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/CREDITS.txt
%files -n ndas-admin
%{_sbindir}/ndasadmin
/usr/share/ndas/mknod.sh
/usr/share/ndas/README
/usr/share/ndas/EULA.txt
/usr/share/ndas/INSTALL
/usr/share/ndas/ndas
/usr/share/ndas/ndas.suse

%doc 
%changelog

Tue Nov 29 2011 Version 2.6
	* Begin licensing entire source with GPL
	* Added gpl-2.0.txt
	* Change copyright to IOCELL Networks
	* Versions < 2.6.22 will be deprecated so users needing earlier
	  versions must download legacy version from Ximeta. Only
	  ndas-1.1-24 x86, x64, arm will be made available.

Wed Jun 11 2008 Version 1.1-23
	* Support 2.6.25 kernel
	* Support scsi interface
	* Support DVD/CD device
	* Support DISK device
Thu Apr 24 2008 Version 1.1-22
	* Fixed 64 bit related warnings.
Fri Apr 18 2008 Version 1.1-21
	* Now works with 2.6.24 kernel
Mon Apr 14 2008 Version 1.1-20
	* Patched 1.1-16 ~ 1.1-20
	* Increased performance & stability
Fri Nov 30 2007 Version 1.1-15
	* Fixed a problem that writes into an NDAS block device failed.
Mon Oct 22 2007 Version 1.1-13
	* Added support for 2.6.23 kernel.
	* Fixed a problem that single block module build didn't work with 2.6
	  kernel build.
	* "slot not found" is fixed in some cases.
 Thu Oct 4 2007 Version 1.1-12
	* Fixed a problem that compilation is failed at kernel version 2.6.18
	* Retried IO very long time before returning failure when IO
	* Fixed a problem that incorrect error message is shown when disabling
	  NDAS devices in 2.4 kernel.
Mon Oct 1 2007 Version 1.1-11
	* Fixed a problem that ndasadmin crashed with invalid key value.
	* Slight performance improvement on low performance machine.
	* Added option to register without blocking. Device status is checked
	  when registering.
	* Added a build rule to build the modules in single binary. Execute
	"make blk-single"
Fri Aug 15 2007 Version 1.1-10
	* Fixed a bug that devices with 1.0 NDAS chip were inaccessible.
Fri Aug 10 2007 Version 1.1-9
	* Recognize ATAPI device(Not handling of actual device is not
	  implemented yet).
	* Fixed a bug that device folder in /proc/ndas/devices is not created
	  for the devices that is created by configuration file.
	* Fixed a bug that a disable device is automatically enabled when it
	  becomes online
	* Show more device information
	* Device's status is not checked at registration step, which enables
	  batch registration without actual devices.
	* Fixed the high load problem.(Fixed at 1.1-8 but released binary
	  didn't include the fix)
	* Start-up device's proc/ndas/devices entry was not created.
	* Reading proc files does not block.
	* Changed shared-write policy for better performance.
	* Added support for multi-bay NDAS devices
Mon Jul 9 2007 Version 1.1-8
	* Fixed the high load problem.
Thu Jul 3 2007 Version 1.1-7
	* Fixed a compilation problem on Linux kernel 2.6.22.
	* Fixed a x86_64 rpmbuild from tarball problem.
 Thu Jun 15 2007 Version 1.1-6
	* Fixed a rpmbuild problem.
Thu Jun 15 2007 Version 1.1-5
	* Changed init script show more information.
Thu Jun 15 2007 Version 1.1-4
	* Fixed debian package bugs that prevent running init script.
Thu Jun 14 2007 Version 1.1-3
	* Suppport Debian package build
Thu Jun 7 2007 Version 1.1-2
	* Suppport RPM build for x86_64
Tue Jun 5 2007 Version 1.1-1
	* Changed version numbering.
	* Support serial-number registration and id registrion with single
	  binary.
	* Fixed memory leak in some cases.
	* Use shorter device name for devices with long(16 digit) serial to
	  avoid
	* confliction in sysfs.
	* Fixed a problem that Linux driver cannot access NDAS emulator.
Wed May 16 2007 Version 1.0.5
	* x86_64 support.
	* Fixed a bug that prevented connecting to NDAS when bond interface
	  exists.
	* Added a flow control, which may increase performance of system with
	  RAID or switch/NIC with small buffer.
	* Connection timeout time has increased.
	* Fixed "Slot not found" error.
	* Fixed hanging problem while IO in SMP system.
	* Fixed a problem that caused crashing while unloading NDAS modules.
	* Changed /dev/ndas to character device. The kernels without udev need
	  to rerun mknod.sh
	* Changed device file name created by udev.
	* Fixed a problem that NDAS driver prevented removal of NIC kernel
	  module.
	* Fixed a problem that NDAS driver didn't handle up/down of network
	  devices.
Wed Feb 28 2007 Version 1.0.4
	* Fixed a bug that a NDAS device is stuck in the Connecting status.
	* Recognize and configure correctly new NDAS Giga chip.
	* Added support for NDAS for Seagate hardware.Version 1.0.3
	* Fixed the crash on registering after
	register/enable/disable/unregisterVersion 1.0.2
	* Fixed the bug when registering 5 or more disks
	* Added the more routine to identify the cause on starting the
	serviceVersion 1.0.1
	* Added the routines to check the kernel version on ndas_sal module
	instertion
	* Avoid cfq io elevation scheduler due to incompatibility issue
	* Compatible with SuSE 9.0 2.4.21-99 kernels, Redhat Enterprise Linux
	  2.x and CentOS 2.x.
Version 1.0.0
	* /proc/ndas/devices/[registered name] directory
	is provided
	* Compatible with Debian, Ubuntu, Redhat Enterprise Linux, Gentoo,
	  SuSE,	Fedora Core, Mandriva, CentOS.
	* Package is seperated into ndas-kernel-* and ndas-admin. so that the
	  multiple kernel packages can be installed into one system for the
	  multiple kernels.
Version 0.9.9
	* Package name is changed as km_ndas-default, km_ndas-smp for SuSE.
Version 0.9.8
	* Block device path is now /dev/ndas-[NDAS serial]:[unit #] for 2.6.x
	  kernel
	* The previous enabled devices are enabled at the booting time and will
	  be mounted by autofs if the disk is online.
Version 0.9.6
	* GFS Support
Version 0.9.5
	* First public releaseVersion 0.9.4
	* Fixed possible deadlock. Version 0.9.3
	* Fixed problems that make kernel panic when communicating with PC side
	  software.
Version 0.9.2
	* Fixed problems that driver hangs while reading.
	* Minor bug fixes.Version 0.9.0
	* Write-performance optimization. Fixed a problem that hang.
	* Fixed a problem using VFAT in Linux 2.6 kernel.
	* Reconnection funtion is enabled.
