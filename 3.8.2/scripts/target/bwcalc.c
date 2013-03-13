#include <stdio.h>
#include <stdlib.h>
int main(int argc, char* argv[])
{
    int size;
    double run_s, idle_s, run_e, idle_e;
    if (argc!=6) {
        printf("Invalid number of arg\n");
        return 0;
    }
    size = atoi(argv[1]);
    run_s = atof(argv[2]);
    idle_s = atof(argv[3]);
    run_e = atof(argv[4]);
    idle_e = atof(argv[5]);
    if (run_s==run_e) {
        printf("Zero execution time\n");
        return 0;
    }
    if (run_s ==0 || run_e ==0) {
        printf("Zero run time. atof may fail\n");
    }
    printf("Run time: %.1f sec, Idle time %.1f sec, Throughput=%.1fMbps\n", 
        run_e-run_s, idle_e-idle_s, size/(run_e-run_s));
    return 0;    
}
