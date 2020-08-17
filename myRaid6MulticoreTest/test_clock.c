#include<stdio.h>
#include<stdint.h>
#include<string.h>
#include<stdlib.h>
#include <unistd.h>
#include <isa-l/crc.h>
#include <isa-l/crc64.h>
#include <isa-l/erasure_code.h>
#include <isa-l/gf_vect_mul.h>
#include <isa-l/igzip_lib.h>
#include <isa-l/raid.h>
#include <sys/types.h>
#include <time.h>
#include <isa-l/test.h>

int main()
{
    time_t starttime3 = clock();
    sleep(10);
    // thrd_sleep(&(struct timespec){.tv_sec=1}, NULL); // sleep 1 sec
    time_t endtime3 = clock();
    long long dur_clock = endtime3 - starttime3;
    printf("dur of clock():%lld\n", dur_clock);
    printf("%lld", CLOCKS_PER_SEC);
}