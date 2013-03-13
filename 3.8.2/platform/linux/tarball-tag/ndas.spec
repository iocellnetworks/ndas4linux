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
%define ndas_rpm_version NDAS_RPM_VERSION

#
# Use the fx identifier as the rpm release for fedora version
#
%define ndas_rpm_release %(if [ -f /etc/fedora-release ]; then uname -r | cut -d "." -f 4 ; else 1; fi)

%define ndas_min_version NDAS_MIN_VERSION
%define ndas_kernel_version %(if [ -z "$NDAS_KERNEL_VERSION" ]; then uname -r ; else echo $NDAS_KERNEL_VERSION; fi)
%define ndas_kernel_version_simple %( echo %{ndas_kernel_version} | sed -e 's|-smp4G||g' -e 's|-bigsmp||g' -e 's|-psmp||g' -e 's|-smp||g' -e 's|smp||g' -e 's|xen0||' -e 's|xenU||' -e 's|-xenpae||' -e 's|-xen||' -e 's|xen||' -e 's|-default||' -e 's|-hugemem||' -e 's|hugemem||' -e 's|-kdump||' -e 's|kdump||' -e 's|PAE||' -e 's|-debug||' -e 's|-bigmem||' -e 's|bigmem||' -e 's|-SLRS||' -e 's|SLRS||' -e 's|-enterprise||' -e 's|enterprise||')
%define ndas_kernel_version_caninical_name %( echo %{ndas_kernel_version_simple} | sed -e 's|-|_|g')
%define ndas_rpm_packager %(echo "ndasusers at iocellnetworks.com")
%define ndas_homepage NDAS_RPM_HOME_URL
%define ndas_version_download_url NDAS_RPM_DOWNLOAD_URL

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

#
# SuSE kernel module rpm name
#
%if %{ndas_suse}

#define ndas_suse_rpm_name km_ndas
# SuSE kernel module has names like "km_{module_name}", but for now, we will use ndas-kernel.
#
%define ndas_suse_rpm_name ndas-kernel

#
# Add post fix such as smp, psmp, smp4G and so on.
#
%define ndas_rpm_name %{ndas_suse_rpm_name}-%{ndas_name_postfix}
%define ndas_rpm_kernel %(echo "kernel-";)%{ndas_name_postfix}

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
#end SuSE
#

# 
# Module's extension for kernel > 2.6
#
%define module_ext ko

#
# The maximum hard disk supported by NDAS Host in the same time
# For kernel 2.6.x  (TODO: should be 1048576)
#
%define ndas_max_slot_26 16
%define ndas_max_slot %(if [ -z "$NDAS_MAX_SLOT" ]; then echo %{ndas_max_slot_26}; else echo $NDAS_MAX_SLOT; fi)

#
# Turn off debuginfo package
#
%define debug_package %{nil}

Summary: NDAS Software for Linux operating system
Name: %{ndas_rpm_name}
Epoch: %{ndas_rpm_epoch}
Version: %{ndas_rpm_version} 
Release: %{ndas_rpm_release}
Vendor: NDAS_VENDOR_NAME
License: Proprietary Copyright (c) NDAS_VENDOR_NAME
Group: System Environment/Kernel
Source0: %{ndas_version_download_url}/ndas-%{ndas_rpm_version}.tar.gz
URL: %{ndas_homepage}
BuildRoot: %{_tmppath}/ndas-buildroot

%if %{ndas_suse}
BuildRequires: /bin/uname /usr/bin/sed 
%else
BuildRequires: /bin/uname /bin/sed 
%endif
 
Obsoletes: ndas-% < %{ndas_min_version} 
Provides: ndas-% >= %{ndas_min_version}
ExclusiveArch: NDAS_RPM_ARCH

%description 
%{ndas_rpm_name} - This software allows the user to access the NDAS device via the Network. This package only works with kernel version %{ndas_kernel_version_simple}.

%package -n ndas-admin
Summary: Administration tool for NDAS device.
Group: System Environment/Base
Release: %{ndas_rpm_release}
Requires: ndas-% >= %{ndas_min_version}
%description -n ndas-admin
ndas-admin - This program allows users to register/enable/disable/unregister NDAS devices.

%prep
%setup -q -n ndas-%{ndas_rpm_version}
%build
export NDAS_EXTRA_CFLAGS="$NDAS_EXTRA_CFLAGS -DNDAS_MAX_SLOT=%{ndas_max_slot}"
make %{?ndas_make_opts} ndas-kernel ndas-admin
echo %{_rpmdir}/%{_target_cpu}/%{name}-%{ndas_rpm_rversion}.rpm > /tmp/.file || true 
rm -rf $RPM_BUILD_ROOT
install -d -m 755 $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas
install -m 644 ndas_sal.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_sal.%{module_ext}
install -m 644 ndas_core.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_core.%{module_ext}
install -m 644 ndas_block.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_block.%{module_ext}
install -m 644 ndas_block.%{module_ext} $RPM_BUILD_ROOT/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_scsi.%{module_ext}
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
/lib/modules/%{ndas_kernel_version}/kernel/drivers/block/ndas/ndas_scsi.%{module_ext}
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
