#include <stdlib.h>
#include "tools.h"
#include <stdio.h>
#include <time.h>

#ifdef GTD
timeval start_clock[10];
timeval end_clock[10];
void tic(int i)
{
    gettimeofday(&start_clock[i], NULL);
}

double toc(int i)
{
    gettimeofday(&end_clock[i], NULL);

    long long time_use = (end_clock[i].tv_sec - start_clock[i].tv_sec) * 1000000 + (end_clock[i].tv_usec - start_clock[i].tv_usec); //微秒
    return time_use * 1.0 / 1000000;
}
#elif defined CGT

struct timespec start_clock[10];
struct timespec end_clock[10];
void tic(int i)
{
    clock_gettime(1, &start_clock[i]);
}
double toc(int i)
{
    clock_gettime(CLOCK_ID, &end_clock[i]);

    unsigned long time_use = (end_clock[i].tv_sec - start_clock[i].tv_sec)*1000000000 + (end_clock[i].tv_nsec - start_clock[i].tv_nsec);
    return time_use * 1.0 / 1000000000;
}
#endif

void print_throughtput(long long data_length, double past_time, char *info)
{
    printf( "[%12s] "
            "Throughtput: \t%lf\tGB/s\n", 
            info, 
            data_length / past_time / 1024 / 1024 / 1024);
}