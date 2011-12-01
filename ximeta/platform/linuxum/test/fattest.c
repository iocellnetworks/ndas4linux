//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//                        FAT32 File IO Library Linux Test Version
//                                      V1.0L
//                                       Rob Riglar
//                                Copyright ( C ) 2002-2005 XIMETA, Inc. 
//
//                         Email: admin@robs-projects.com
//
//               Compiled and tested on Redhat 'Shrike' with GNU gcc
//-----------------------------------------------------------------------------
//
// This file is part of FAT32 File IO Library.
//
// FAT32 File IO Library is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// FAT32 File IO Library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with FAT32 File IO Library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#include "sal/sal.h"
#include "sal/libc.h"
#include "sal/types.h"
#include "sal/debug.h"
#include "lpx/lpx.h"
#include "ndasuser/ndasuser.h"
#include "netdisk/netdisk.h"
#include "fat32lib/structures.h"

// IDE Includes
#include "fat32lib/FAT32_Base.h"
#include "fat32lib/DirSearchBrowse.h"
#include "fat32lib/filelib.h"



//-----------------------------------------------------------------------------
//                                    Main
//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    sal_init();
    ndas_init(0,0);
    ndas_register_network_interface("eth0");
    ndas_start();
    fattest_init("silv", "VVRSGEKDFY1E8BK5KJPX", "7VT00");
    fattest_show_root();
    fattest_play();
    ndas_stop();
    ndas_unregister_network_interface("eth0");
    ndas_cleanup();
    sal_cleanup();
    return 0;
}

