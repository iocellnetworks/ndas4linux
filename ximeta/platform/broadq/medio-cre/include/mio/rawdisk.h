
#ifndef __MIO_RAW_DISK_H__
#define __MIO_RAW_DISK_H__
#include <tamtypes.h>
#include <mio/diskpart.h>
#include <string>
#include <list>
using namespace std;

class MioRawDisk {
public:
    MioRawDisk(const string& name, u32 deviceNumber, u32 capacity = 0);
    ~MioRawDisk();

    string getName() const { return myName; };
    u32 getDeviceNumber() const { return myDeviceNumber; };
    u32 getCapacity() const { return myCapacity; };
    void setCapacity(u32 capacity) { myCapacity = capacity; };

    void insert(MioDiskPartition*);
    MioDiskPartition *findPartition(u32 partitionNumber);
    MioDiskPartition *findPartition(const string& name);
    list<MioDiskPartition*>& allPartitions() { return myPartitions; };
    u32 numPartitions() const;

    string asString() const;

private:
    string myName;
    u32 myDeviceNumber;
    u32 myCapacity;
    list<MioDiskPartition*> myPartitions;
};

extern list<MioRawDisk*> *allRawDisks;

#endif  // __MIO_RAW_DISK_H__
