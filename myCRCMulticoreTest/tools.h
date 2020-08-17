#ifndef TOOLS
#define TOOLS
#include <stdlib.h>
#include <stdio.h>

// #define GTD
#define CGT

#ifdef GTD
#include <sys/time.h>
extern timeval start_clock[10];
extern timeval end_clock[10];
#endif
#ifdef CGT
#include <time.h>
extern struct timespec start_clock[10];
extern struct timespec end_clock[10];
#endif

#ifdef __FreeBSD__
# define CLOCK_ID CLOCK_MONOTONIC_PRECISE
#else
# define CLOCK_ID CLOCK_MONOTONIC
#endif

void tic(int i);
double toc(int i);
void print_throughtput(long long data_length, double past_time, char *info);

// #define default_throughtput(a, b) print_throughtput(a, b, __FILE__)

#endif
