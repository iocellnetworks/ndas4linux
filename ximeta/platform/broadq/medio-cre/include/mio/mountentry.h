
#ifndef __MIO_MOUNT_ENTRY_H__
#define __MIO_MOUNT_ENTRY_H__
#include <mio/partinfo.h>
#include <string>
#include <list>
using namespace std;

class MioMountEntry {
public:
    MioMountEntry(const string& raw, const string& mountName);
    ~MioMountEntry();

    string getName() const { return myMountName; };
    const MioPartitionInfo& partitionInfo() const { return myInfo; };
    string asString() const;

private:
    string myMountName;
    MioPartitionInfo myInfo;
};

extern list<MioMountEntry*> *allMountPoints;

#endif // __MIO_MOUNT_ENTRY_H__
