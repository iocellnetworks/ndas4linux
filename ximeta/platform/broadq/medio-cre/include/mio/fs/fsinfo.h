
#ifndef __MIO_FS_FILESYSTEM_INFO_H__
#define __MIO_FS_FILESYSTEM_INFO_H__
#include <mio/sstring.h>
#include <mio/fs/mntparams.h>

namespace mio {
    namespace fs {

        class fileSystemInfo {
            typedef int (*mounter)(const mountParams& params);
            typedef int (*unmounter)(const sstring& mountName);

        public:
            fileSystemInfo() : myMounter(0), myUnmounter(0) { };
            ~fileSystemInfo() { };

            int operator==(const sstring& fsName) { return myFsName == fsName; };
            int operator==(const fileSystemInfo& o) { return myFsName == o.myFsName; };

            mounter myMounter;
            unmounter myUnmounter;
            sstring myFsName;

        private:
            fileSystemInfo(const fileSystemInfo&); // disable
            fileSystemInfo& operator=(const fileSystemInfo&); // disable
        };

    };
};

#endif // __MIO_FS_FILESYSTEM_INFO_H__
