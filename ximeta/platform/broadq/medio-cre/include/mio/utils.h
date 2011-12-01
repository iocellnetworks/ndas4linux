
#ifndef __MIO_UTILS_H__
#define __MIO_UTILS_H__
#include <_ansi.h>
#include <tamtypes.h>

//        data passed in extended seek function (hack)

typedef struct _xseek {
        long long offset;
        int whence;
} xseek_t;

#define ASIZE(a) (sizeof(a) / sizeof(a[0]))
#if !defined(__cplusplus)
#define new(a) malloc(sizeof(a))
extern void *malloc(unsigned int size);
#endif

//        Value from errno.h (that don't seem accessible from the IOP compilation system).

#if defined(__IOP__)
static const int EIO = 5;            // I/O error
static const int ENOMEM = 12;        // out of memory
static const int ENODEV    = 19;
static const int EINVAL = 22;
static const int EROFS = 30;        // read-only file system
static const int EPIPE = 32;        // stall on USB pipe
static const int EBADE = 52;
static const int ENOSYS = 88;        // funtion not implemented

//        Values missing from other places.

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif


#undef printf
_BEGIN_STD_C
extern const char* hexify(const void *addr, int bytes);
extern void dumpHex(const char *prefix, const void *addr, int bytes);
extern void u64ToBe(unsigned char *dest, u64 src);
extern void u64ToLe(unsigned char *dest, u64 src);
extern void u32ToBe(unsigned char *dest, u32 src);
extern void u32ToLe(unsigned char *dest, u32 src);
extern void u16ToBe(unsigned char *dest, u16 src);
extern void u16ToLe(unsigned char *dest, u16 src);
extern u64 beToU64(const unsigned char *src);
extern u64 leToU64(const unsigned char *src);
extern u32 beToU32(const unsigned char *src);
extern u32 leToU32(const unsigned char *src);
extern u16 beToU16(const unsigned char *src);
extern u16 leToU16(const unsigned char *src);
//extern void swab(u8 *src, u8 *dst, int b);

#if defined(__IOP__)
extern char *strdup(const char *);
extern char *strtok(char *, const char *);
extern void free(void*);
#endif
_END_STD_C

#endif  // __MIO_UTILS_H__
