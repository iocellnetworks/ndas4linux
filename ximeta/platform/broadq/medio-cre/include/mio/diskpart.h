
#ifndef __MIO_DISK_PARTITION_H__
#define __MIO_DISK_PARTITION_H__
#include <tamtypes.h>
#include <string>
using namespace std;

const u8 ROMFS_PARTITION_TYPE = 0xff;

class MioDiskPartition {
public:
    MioDiskPartition(u32 partition, const string& name, u32 lba, u32 size, u8 type);
    ~MioDiskPartition();

    u32 getPartition() const { return myPartition; };
    string getName() const { return myName; };
    u32 getLba() const { return myLba; };
    u32 getSize() const { return mySize; };
    u8 getType() const { return myType; };

    string asString() const;

private:
    u32 myPartition;
    string myName;
    u32 myLba;
    u32 mySize;
    u8 myType;
};

#endif // __MIO_DISK_PARTITION_H__
