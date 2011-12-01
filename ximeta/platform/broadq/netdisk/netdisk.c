#include <_ansi.h>
#include <tamtypes.h>
#include <fileio.h>
#include <stdlib.h>
#include <stdio.h>
#include <kernel.h>
#include <stdarg.h>
#include <mio/bcache.h>
#include <mio/iopmods.h>
#include <mio/modules.h>
#include <mio/mutex.h>
#include <mio/utils.h>
#include <sal/types.h>
#include <sal/thread.h>
#include <sal/time.h>
#include <ndasuser/info.h>
#include <ndasuser/io.h>

#define printf sal_debug_print
#define DelayThread delay


typedef struct _DeviceInfo {
    iop_device_t driver;        // must be first in DeviceInfo (must be same address, 'cuz we cast it)
    int dev_id;                    // device id (taken from filesystem/device name)
    u64 blockSize;
    u64 numBlocks;                // number of blocks on device
    u64 maxBlocksPerTransfer;
    MioBufferCache bcache;
    int slot;
} DeviceInfo;

typedef struct _privateFileInfo {
    off_t currentByteOffset;
} PrivateFileInfo;

typedef void (*ctor)();

//    Externs, globals and statics.

static const char *myName = "netdisk";
extern int _ftext;
static u32 cycle = 0;
int startupDelay = 30;
int oldDiskNaming = 1;
const char *diskKey = "77k5c1m34gpgrgq5ycrg";
extern ctor _CTORS[];
_BEGIN_STD_C
_EXFUN(int registerEntryPoints, ());
_END_STD_C

static void
callCtors(ctor table[])
{
    int s = 0;
    while (table[s]) {
        table[s++]();
    }
}

static char *
getDeviceName(int slot)
{
    char name[20];

    if (oldDiskNaming)
        snprintf(name, sizeof(name), "xnd%03ua", slot);
    else
        snprintf(name, sizeof(name), "xd%c", slot-1 + 'a');

    return strdup(name);
}


static u64
bigSeek(iop_file_t *fd, long long offset, int whence)
{
    PrivateFileInfo *privInfo = (PrivateFileInfo*)fd->privdata;
    DeviceInfo *devInfo = (DeviceInfo*)fd->device;
    u64 maxSize = devInfo->numBlocks * devInfo->blockSize;
    u64 newoffset;

    switch (whence) {
    case SEEK_SET:
        newoffset = offset;
        break;

    case SEEK_CUR:
        newoffset = privInfo->currentByteOffset + offset;
        break;

    case SEEK_END:
        newoffset = maxSize + offset;
        break;

    default: // invalid request
        return -ENOSYS;

    }

    if (newoffset < 0) newoffset = 0;
    if (newoffset > maxSize) newoffset = maxSize;
    privInfo->currentByteOffset = newoffset;
    //printf(__FILE__ "::bigSeek(0x%x, %llu, %u), new offset: %llu\n", fd, offset, whence, newoffset);
    return privInfo->currentByteOffset;
}

static int
readCapacity(DeviceInfo *devInfo)
{
    ndas_slot_info info;
    ndas_error_t r = ndas_query_slot(devInfo->slot, &info);

    printf("netdisk: dev: %u, state: %u, version: %u, sectors: %Lu\n",
        devInfo->slot, 0 /* info.state*/, 0 /*info.version*/, info.sectors );

    devInfo->blockSize = 512;
    devInfo->numBlocks = info.sectors; 
    devInfo->maxBlocksPerTransfer = 5; // REPLACE THIS WITH ENTRY FROM INFO WHEN AVAILABLE

    printf("netdisk: capacity of device: %s, sectors: %lu\n", devInfo->driver.name, devInfo->numBlocks);

    return r;
}

static void
showMemoryUsage()
{
    int size = 0x200;
    void *lastMem = 0;
    int lastSize = 0;
    char *prefix = "netdisk.irx";
    while (1) {
        void *mem = (void*)AllocSysMemory(0, size, 0);
        if (!mem) break;
        FreeSysMemory(mem);
        lastSize = size;
        lastMem = mem;
        size += 0x200;
    }
    printf("%s: biggest block size: %d bytes, at 0x%x\n", prefix, lastSize, lastMem);
}



static int
netdisk_init(iop_device_t *driver)
{
    DeviceInfo *devInfo = (DeviceInfo*)driver;
    printf(__FILE__ ": initializing driver: 0x%x, dev_id: %d, name: %s\n", driver, devInfo->dev_id, devInfo->driver.name);
    notifyDeviceAvailable(driver->name);
    showMemoryUsage();
    return 0;
}

static int
netdisk_deinit(iop_device_t *driver)
{
    DeviceInfo *devInfo = (DeviceInfo*)driver;
    printf(__FILE__ ": deinitializing driver: 0x%x, dev_id: %d, name: %s\n", driver, devInfo->dev_id, devInfo->driver.name);
    notifyDeviceRemoval(driver->name);
    return 0;
}

static int
netdisk_format(iop_file_t *f, ...)
{
    printf("netdisk_format:\n");
    return -1;
}

static int
netdisk_open(iop_file_t *fd, const char *name, int mode, ...)
{
    static int numOpens = 0;
    DeviceInfo *devInfo = (DeviceInfo*)fd->device;
    PrivateFileInfo *privInfo;

    printf(__FILE__ ": driver 0x%x, open requested for %s, mode: %d\n", devInfo, name, mode);
    // tjd: the latest version of newlib and ps2lib disagree about the value of O_RDONLY...
    //if (mode != O_RDONLY) return -EROFS; // can only read this device

    privInfo = malloc(sizeof(PrivateFileInfo));
    privInfo->currentByteOffset = 0;
    fd->privdata = privInfo;

    readCapacity(devInfo);

    return ++numOpens; // file opened
}


static int
netdisk_close(iop_file_t *fd)
{
  printf("netdisk: close\n");
  return 0;
}

static int
netdisk_read(iop_file_t *fd, void *userAddress, int count)
{
    int res;
    PrivateFileInfo *privInfo = (PrivateFileInfo*)fd->privdata;
    DeviceInfo *devInfo = (DeviceInfo*)fd->device;
    u32 bytesRemaining;
    u64 byteAddress = privInfo->currentByteOffset;
    //printf("rd: fd=%#x, cy: %u, u: %#x, c: %d, o: %llu\n", fd, cycle, userAddress, count, byteAddress);
    cycle++;

    res = 0;
    bytesRemaining = count;
    byteAddress = privInfo->currentByteOffset;
    while (bytesRemaining) {
        u32 chunkSize;
        void *chunkAddress;
        u32 bytesLeftInThisEntry;
        MioBufferCacheEntry c;
        if (!MioBufferCacheContains(&devInfo->bcache, byteAddress, &c)) { // need to read and cache stuff
            u64 blockNumber = byteAddress / devInfo->blockSize;
            u64 startingBlock = blockNumber / devInfo->maxBlocksPerTransfer * devInfo->maxBlocksPerTransfer;
            u64 xferSizeBlocks = ((devInfo->numBlocks - startingBlock) > devInfo->maxBlocksPerTransfer)?
                devInfo->maxBlocksPerTransfer : devInfo->numBlocks - blockNumber;
            MioBufferCacheEntryInit(&c);
            c.ramAddress = malloc(devInfo->maxBlocksPerTransfer * devInfo->blockSize);
            if (0 == c.ramAddress) {
                res = -ENOMEM;
                break;
            }
            c.externalByteAddress = startingBlock * devInfo->blockSize;
            c.blockSize = xferSizeBlocks * devInfo->blockSize;
            //MioBufferCacheEntryPrint(&c, "");
            ndas_io_request req = {0};
            
            req.start_sec = startingBlock;
            req.num_sec = xferSizeBlocks;
            req.buf = c.ramAddress;
            req.done = NULL;
            req.done_arg = NULL;
            //printf("netdisk: request, slot: %u, sec_low: %u, sec_hi: %u, num_sec: %u, buf: %#x\n",
                //devInfo->slot, req.start_sec_low, req.start_sec_high, req.num_sec, req.buf);
            res = ndas_read(devInfo->slot, &req);
            if (res < 0) { // uh, oh...
                MioBufferCacheEntryTerm(&c);
                break;
            }
            MioBufferCacheAddEntry(&devInfo->bcache, &c);
            res = 0;
        } else {
            //MioBufferCacheEntryPrint(&c, "cache hit- ");
        }
        bytesLeftInThisEntry = c.blockSize - (byteAddress - c.externalByteAddress);
        chunkSize = (bytesLeftInThisEntry > bytesRemaining)? bytesRemaining : bytesLeftInThisEntry;
        chunkAddress = (byteAddress - c.externalByteAddress) + c.ramAddress;
        //printf("avail: %d, sz: %d, src: 0x%x, dst: 0x%x", bytesLeftInThisEntry, chunkSize, chunkAddress, userAddress);
        bcopy(chunkAddress, userAddress, chunkSize);
        bytesRemaining -= chunkSize;
        byteAddress += chunkSize;
        userAddress += chunkSize;
    }
    privInfo->currentByteOffset = byteAddress;

    if (0 == res) res = count;
    //printf("netdisk: end of read, r: %d, cycle: %d\n", res, cycle);
    return res;
}


static int
netdisk_write(iop_file_t *fd, void *wbuf, int count)
{
    xseek_t *req = wbuf;
    //dumpHex("big seek", req, sizeof(xseek_t));
    bigSeek(fd, req->offset, req->whence);
    return -EROFS;
}


static int
netdisk_lseek(iop_file_t *fd, int offset, int whence)
{
    return (int)bigSeek(fd, offset, whence);
    return 0;
}


static int
netdisk_ioctl(iop_file_t *f, unsigned long arg, void *param)
{
    printf("netdisk_ioctl");
    return -1;
}

static int
netdisk_remove(iop_file_t *f, const char *name)
{
    printf("netdisk_remove:\n");
    return -1;
}

static int
netdisk_mkdir(iop_file_t *f, const char *path)
{
    printf("netdisk_mkdir:\n");
    return -1;
}

static int
netdisk_rmdir(iop_file_t *f, const char *path)
{
    printf("netdisk_rmdir");
    return -1;
}

static int
netdisk_dopen(iop_file_t *f, const char *name)
{
    return -1;
}

static int netdisk_dclose(iop_file_t *f)
{
  printf("netdisk_dclose: fd=0x%x\n", f);
  return -1;
}

static int
netdisk_dread(iop_file_t *f, fio_dirent_t *dirent)
{
  printf("netdisk_dread: fd=0x%x\n", f);
  return -1;
}

static int
netdisk_getstat(iop_file_t *f, const char *name, fio_stat_t *stat)
{
    return -1;
}

static int
netdisk_chstat(iop_file_t *f, const char *name, fio_stat_t *stat, unsigned int statmask)
{
    printf("netdisk_chstat:\n");
    return -1;
}

static int attachDisk(int slot)
{

    static iop_device_ops_t driverFunctions = {
          netdisk_init,
          netdisk_deinit,
          netdisk_format,
          netdisk_open,
          netdisk_close,
          netdisk_read,
          netdisk_write,
          netdisk_lseek,
          netdisk_ioctl,
          netdisk_remove,
          netdisk_mkdir,
          netdisk_rmdir,
          netdisk_dopen,
          netdisk_dclose,
          netdisk_dread,
          netdisk_getstat,
          netdisk_chstat
    };
    DeviceInfo *devInfo;

    if (0 == slot) return -1; // should already know about this device config...

    devInfo = malloc(sizeof(DeviceInfo));
    if (0 == devInfo) {
        printf("netdisk: no memory for DeviceInfo\n");
        return -1;
    }
    bzero(devInfo, sizeof(*devInfo));

    MioBufferCacheInit(&devInfo->bcache, 4);
    devInfo->driver.name = getDeviceName(slot);
    devInfo->driver.type = 0x01000010;
    devInfo->driver.version = 1;
    devInfo->driver.desc = "mass storage driver for Ximeta netdisk";
    devInfo->driver.ops = &driverFunctions;
    devInfo->slot = slot;
    printf("netdisk: adding raw device, fs: %s, device: %d (%s)\n", devInfo->driver.name, slot, devInfo->driver.desc);
    DelDrv(devInfo->driver.name);
    AddDrv(&devInfo->driver);

    return 0;
}


void
probie(char *name, int st)
{
    sal_debug_print("probie: %s, st: %d", name, st);
}

static void*
starter(void *arg)
{
    int r;
    ndas_device_info info;
    DelayThread(startupDelay * 1000 * 1000);

    //MioModuleSetTracing(MIO_MODULE_BCACHE_C, 1);

    sal_init();

    ndas_init(50, 16*1024, 8);

    ndas_register_network_interface("se1");

    ndas_start();

    sal_error_print("diskkey=%s\n",diskKey);

    r = ndas_register_device("Tom's tunes", "AF7R22MKK4UHG9S5RHTG", 0);
    if (!NDAS_SUCCESS(r)) sal_debug_print("ndas_register_device returned: %d\n", r);
    showMemoryUsage();

    r = ndas_query_device("Tom's tunes", &info);
    if (!NDAS_SUCCESS(r)) sal_debug_print("ndas_query_device returned: %d\n", r);
    
    printf("about to enable slot: %u\n", info.slot[0]);
    r = ndas_enable_slot(info.slot[0]);
    printf("enable_device ret: %d, &r: %#x\n", r, &r);
    if (NDAS_SUCCESS(r)) {
        attachDisk(info.slot[0]);
    }

    ExitDeleteThread();
    return 0;
}


int
_start(int argc, char *argv[])
{
    int returnFlag = MODULE_RESIDENT_END;
#if 0
    callCtors(_CTORS);
#endif

    printf("netdisk initializing, %u arguments passed\n", argc);
    //TraceAdd(0, "this is STILL netdisk");

    registerIopModule("netdisk", &_ftext);

    if (parseArguments(argc, argv)) {

        sal_thread_id tid;
        //sal_thread_create( &tid, "starter", starter, 0, 20, 0x2000);
        sal_thread_create( &tid, "starter", 20, 0x2000, starter, (void*) 0);
        showMemoryUsage();

        printf("netdisk: driver has been initialized.\n");
    } else {
        returnFlag = MODULE_NO_RESIDENT_END;
    }

    return returnFlag;
}

void mprintf(const char *format, ...) {
}
