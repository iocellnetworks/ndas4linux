/*
 * linux/fs/msdos/vfatfs_syms.c
 *
 * Exported kernel symbols for the VFAT filesystem.
 * These symbols are used by dmsdos.
 */

#include <linux/version.h>
#include <linux/module.h> 

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <linux/module.h>
#include <linux/init.h>

#include <linux/mm.h>
//#include <linux/msdos_fs.h>
#include "ndasfat_fs.h"

DECLARE_FSTYPE_DEV(vfat_fs_type, "vfat", vfat_read_super);

EXPORT_SYMBOL(vfat_create);
EXPORT_SYMBOL(vfat_unlink);
EXPORT_SYMBOL(vfat_mkdir);
EXPORT_SYMBOL(vfat_rmdir);
EXPORT_SYMBOL(vfat_rename);
EXPORT_SYMBOL(vfat_read_super);
EXPORT_SYMBOL(vfat_lookup);

extern int init_fat_fs(void);

static int __init init_vfat_fs(void)
{
	int err;

	err = init_fat_fs();

	if (err != 0) {

		NDAS_BUG_ON(true);
		return err;
	}

	return register_filesystem(&vfat_fs_type);
}

static void __exit exit_vfat_fs(void)
{
	unregister_filesystem(&vfat_fs_type);
}

module_init(init_vfat_fs)
module_exit(exit_vfat_fs)
MODULE_LICENSE("GPL");

#endif
