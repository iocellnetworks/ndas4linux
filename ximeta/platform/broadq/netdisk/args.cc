
#include <stdio.h>
#include <stdlib.h>
#include <mio/argparser.h>

extern int startupDelay;
extern int oldDiskNaming;
extern const char *diskKey;

extern "C" int
parseArguments(int argc, const char *argv[])
{
    int r = 1; // assume everything is ok
    MioArgParser args(argc, argv);
    args.get(); // skip over name of program...

    while (const char *a = args.peek()) {
        const char *opt = args.isOption();
        if (opt) {
            if (0 != opt[1]) {
                printf("invalid option: %s\n", opt);
                return 0;
            }
            switch (opt[0]) {
                case 'd': { // delay before start
                    char *eptr;
                    unsigned long v = strtoul(args.get(), &eptr, 10);
                    if (0 != eptr[0]) {
                        printf("invalid startup delay given.\n");
                        r = 0;
                        break;
                    }
                    startupDelay = v;
                    break;
                }

                case 'o': { // old disk naming style
                    oldDiskNaming = 1;
                    break;
                }

                case 'n': { // new disk naming style
                    oldDiskNaming = 0;
                    break;
                }

                default: {
                    printf("invalid option: -%c\n", opt[0]);
                    r = 0;
                    break;
                }
            }
        } else {
            diskKey = args.get();
        }
    }
    return r;
}
