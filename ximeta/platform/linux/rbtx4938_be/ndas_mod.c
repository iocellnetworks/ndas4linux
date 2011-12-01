/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
 All rights reserved.

DEFINITIONS

Software.  "Software" means NDAS core binary as a form of "libndas.a."

Documentation.  "Documentation" means the manual and any other instructional
or descriptive material, printed or on-line, provided with the Software.

License.  "License" means the license to use the Software. The License is
granted pursuant to the provisions of this Agreement.

Customer.  "Customer" means the person or business entity to whom this copy of
the Software is licensed.


LICENSE

Grant of License.  Subject to Customer's compliance with this Agreement,
XIMETA grants to Customer, and Customer purchases, a nonexclusive License to
use the Software for non-commercial use only.  Rights not expressly granted by
this Agreement are reserved by XIMETA.  Customer who wants to use the Software
for commercial use must contact XIMETA to obtain the commercial License.

Customer purchases a License only. XIMETA retains title to and ownership of
the Software and Documentation and any copies thereof, including the originals
provided with this Agreement.

Copies.  Customer may not copy the Software except as necessary to use the
Software.  Such necessary use includes copying the Software to the internal
hard disk, copying the Software to a network file server in order to make the
Software available for use, and copying the Software to archival backup media.
All trademark and copyright notices must be included on any copies made.
Customer may not copy the Documentation.

Transfer and Use.  Customer may not transfer any copy of the Software or
Documentation to any other person or entity unless the transferee first
accepts this Agreement and Customer transfers all copies of the Software and
Documentation, including the originals.

Customer may not rent, loan, lease, sublicense, or otherwise make the Software
or Documentation available for use by any other person except as provided
above.  Customer may not modify, decompile, disassemble, or reverse engineer
the Software or create any derivative works based on the Software or the
Documentation.

Customer may not reverse engineer, decompile, or disassemble the SOFTWARE, nor
attempt in any other manner to obtain the source code or to understand the
protocol.

The Software is protected by national laws and international agreements on
copyright as well by other agreements on intellectual property.

DISCLAIMER

This software is provided 'as is' with no explcit or implied warranties
in respect of any properties, including, but not limited to, correctness 
and fitness for purpose.

GENERAL PROVISIONS

Indemnification.  Customer agrees that Customer shall defend and hold XIMETA
harmless against any liability, claim, or suit and shall pay any related
expense, including but not limited to reasonable attorneys' fees, arising out
of any use of the Software or Content.  XIMETA reserves the right to control
all litigation involving XIMETA, Customer, and third parties.

Entire Agreement.  This Agreement represents the entire agreement between
XIMETA and Customer. No distributor, employee, or other person is authorized
by XIMETA to modify this Agreement or to make any warranty or representation
which is different than, or in addition to, the warranties and representations
of this Agreement.

Governing Law.  This Agreement shall be governed by the laws of the State of
California and of the United States of America.

 -------------------------------------------------------------------------
*/
#include <linux/module.h> // EXPORT_NO_SYMBOLS, MODULE_LICENSE
#include <linux/version.h> // LINUX_VERSION_CODE, KERNEL_VERSION
#include <linux/init.h> // module_init, module_exit
#include "procfs.h"
#include "sal/sal.h"
#include "sal/debug.h"
#include "sal/net.h"
#include "sal/time.h"
#include "ndasuser/ndasuser.h"
#include <ndasuser/persist.h>
#include <ndasuser/io.h>
#include <ndasuser/write.h>
#ifdef NDAS_PROBE
#include <ndasuser/probe.h>
#endif
#include "linux_ver.h"

/* Please note that you can't redistribute the binary by compiling this module. that will break the GPL. so play it with your own. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Proprietary, Send bug reports to support@ximeta.com");
#endif
MODULE_AUTHOR("Ximeta, Inc.");

extern int blk_init(void);
extern int blk_cleanup(void);

#ifndef NDAS_IO_UNIT
#define NDAS_IO_UNIT 32
#endif

int ndas_io_unit = NDAS_IO_UNIT;

NDAS_MODULE_PARAM_INT(ndas_io_unit, 0);
NDAS_MODULE_PARAM_DESC(ndas_io_unit, "NDAS basic input/output transfer size (default 32)");

int ndas_mod_init(void) 
{    
    sal_init();
    init_ndasproc();
    ndas_init(ndas_io_unit,350, NDAS_MAX_SLOT);
    return blk_init();
}

void ndas_mod_exit(void) 
{    
    blk_cleanup();
    ndas_cleanup();
    cleanup_ndasproc();
    sal_cleanup();
}
module_init(ndas_mod_init);
module_exit(ndas_mod_exit);

