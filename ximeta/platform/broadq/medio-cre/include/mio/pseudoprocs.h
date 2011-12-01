
#ifndef __MIO_PSUEDO_PROCESSES_H__
#define __MIO_PSUEDO_PROCESSES_H__
#include <string>
using namespace std;

typedef unsigned int MioPid;

struct MioProcessEntry {
    enum Status {UNUSED, STARTED, ZOMBIE, EXITED, MAXSTATUS};
    MioProcessEntry();
    ~MioProcessEntry();
    MioPid pid;
    MioPid parentPid;
    string command;
    Status status;
    int exitStatus;
private:
    friend class MioProcessTable;
    MioProcessEntry *next;
};

class MioProcessTable {
public:
    typedef int (*OutputHandler)(const char *format, ...);
    static const MioPid MAX_PID = 255;
    static const MioPid INVALID_PID = MAX_PID+1;
    MioProcessEntry& operator[](MioPid pid) const;
    MioProcessEntry* operator()(MioPid pid) const;
    void insert(MioProcessEntry*);
    int remove(MioPid pid);
    static void setCommand(const char *);
    static void setCommand(int argc, const char **argv);

    static void exitDeleteThread(int exitStatus);
    static int initialize();
    static void reconcileOrphans();
    static void show(OutputHandler = printf);
    static MioPid wait(int *status);

private:
    static MioProcessEntry *ourHead;
    static MioProcessEntry *ourDummy;
};

extern MioProcessTable allProcs;

#endif // __MIO_PSUEDO_PROCESSES_H__
