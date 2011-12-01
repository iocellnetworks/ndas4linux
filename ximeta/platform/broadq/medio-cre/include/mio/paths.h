
#ifndef __MIO_PATH_UTILS_H__
#define __MIO_PATH_UTILS_H__
#include <string>
using namespace std;

class MioPathUtils {
public:
    static const char MIO_PATH_SEP;
    static const char MIO_DEVICENAME_SEP;
    static const char * const MIO_NULL_DEVICENAME;

    static int isAbsolutePath(const string& path);
    static int isDeviceName(const string& name);
    static int isOnMemoryCard(const string& path);
    static int isOnCDRom(const string& path);
    static string dirname(const string& path);
    static string basename(const string& path);
    static string devicename(const string& path);
};

#endif // __MIO_PATH_UTILS_H__
