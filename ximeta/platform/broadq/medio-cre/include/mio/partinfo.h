
#ifndef __MIO_PARTITION_INFO_H__
#define __MIO_PARTITION_INFO_H__
#include <string>
class MioRawDisk;
class MioDiskPartition;
using namespace std;

class MioPartitionInfo {
public:
    MioPartitionInfo(const string& name);
    ~MioPartitionInfo();

    int isValid() const;
    int isMounted() const;
    string getName() const { return myName; };
    MioRawDisk *rawDisk() const;
    MioDiskPartition *partitionEntry() const;

private:
    string myName;
    MioRawDisk *myRawDisk;
    MioDiskPartition *myPartitionEntry;
};

#endif // __MIO_PARTITION_INFO_H__
