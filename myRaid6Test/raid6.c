/**********************************************************************
  Copyright(c) 2011-2015 Intel Corporation All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

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

#define TEST_SOURCES 10
#  define GT_L3_CACHE  32*1024*1024	/* some number > last level cache */
#define TEST_LEN     ((GT_L3_CACHE / TEST_SOURCES) & ~(64-1))
// #define TEST_LEN 1024
#define TEST_MEM ((TEST_SOURCES + 2)*(TEST_LEN))
#ifndef TEST_SEED
# define TEST_SEED 0x1234
#endif
#  define TEST_TYPE_STR "_cold"

// # define CLOCK_ID CLOCK_MONOTONIC

// static inline long long get_time(void) {
// 	struct timespec time;
// 	long long nano_total;
// 	 clock_gettime(CLOCK_ID, &time);
// 	 nano_total = time.tv_sec;
// 	 nano_total *= NANO_SCALE;
// 	 nano_total += time.tv_nsec;
// 	 return nano_total;
// }

int ref_multi_pq(int vects, int len, void **array)
{
	int i, j;
	unsigned char p, q, s;
	unsigned char **src = (unsigned char **)array;
  
  for (i = 0; i < len; i++) {
		q = p = src[vects - 3][i];

		for (j = vects - 4; j >= 0; j--) {
			p ^= s = src[j][i];
			q = s ^ ((q << 1) ^ ((q & 0x80) ? 0x1d : 0));	// mult by GF{2}
		}

		src[vects - 2][i] = p;	// second to last pointer is p
		src[vects - 1][i] = q;	// last pointer is q
	}
	return 0;
}

// Generates pseudo-random data

void rand_buffer(unsigned char *buf, long buffer_size)
{
	long i;
	for (i = 0; i < buffer_size; i++)
		buf[i] = rand();
}

int main(int argc, char *argv[])
{
    // pq_check()
    long long sum = 0;
    struct perf start;
    int i, j, k, ret, fail = 0;
    void *buffs[TEST_SOURCES + 2];
    char c;
    char *tmp_buf[TEST_SOURCES + 2];
    int serr, lerr;

    printf("Test pq_check_test %d sources X %d bytes\n", TEST_SOURCES, TEST_LEN);

    srand(TEST_SEED);

    // Allocate the arrays
    for (i = 0; i < TEST_SOURCES + 2; i++)
    {
      void *buf;
      // posix_memalign返回TEST_LEN长度的buffer，地址是16的倍数
      if (posix_memalign(&buf, 16, TEST_LEN))
      {
        printf("alloc error: Fail");
        return 1;
      }
      buffs[i] = buf;
    }

    // Test rand1
    // 产生随机数
    for (i = 0; i < TEST_SOURCES + 2; i++)
      rand_buffer((unsigned char *)buffs[i], TEST_LEN);

    // ret = pq_check(TEST_SOURCES + 2, TEST_LEN, buffs);
    // 编码
    ref_multi_pq(TEST_SOURCES + 2, TEST_LEN, buffs);
    int test_times = 1;
    // 使用自带的测试程序
    BENCHMARK(&start, test_times, pq_check(TEST_SOURCES + 2, TEST_LEN, buffs););
    perf_print(start, (long long)TEST_MEM);
    time_t starttime2 = clock();
    long long starttime = get_time();
    for (int i = 0; i < test_times; i++)
    {
        // ref_multi_pq(TEST_SOURCES + 2, TEST_LEN, buffs);
        // pq_gen_avx(TEST_SOURCES + 2, TEST_LEN, buffs);
        // 校验
        // ret = pq_check_base(TEST_SOURCES + 2, TEST_LEN, buffs);
        ret = pq_check(TEST_SOURCES + 2, TEST_LEN, buffs);     
        // ret = pq_check_sse(TEST_SOURCES + 2, TEST_LEN, buffs);   
        // printf("有效数据量%dx%d\n", TEST_SOURCES, TEST_LEN);
    }
    long long endtime = get_time();
    time_t endtime2 = clock();
    long long clock_ms = endtime2 - starttime2;
    long long nsec = endtime - starttime;
    long long usec = nsec / 1000000;
    
    printf("clock():%lld\n", clock_ms);
    printf("clock_gettime():%lld\n", nsec);
    printf("校验耗时%lldus\n", usec);
    printf("吞吐率%fMB/s\n", ((GT_L3_CACHE / 1024 / 1024) * test_times) * 1.0 / nsec * 1000000000);
    printf("吞吐率%fMB/s\n", ((GT_L3_CACHE / 1024 / 1024) * test_times) * 1.0 / clock_ms * 1000000);
    // printf("吞吐率%fMbps\n", (TEST_LEN / 1024 / 1024 * TEST_SOURCES *  8 * 1.0 * test_times) / sum * 1000);
    // c = ((char *)(buffs[0]))[TEST_LEN - 2];
	// ((char *)(buffs[0]))[TEST_LEN - 2] = c ^ 0x1;
	// ret = pq_check(TEST_SOURCES + 2, TEST_LEN, buffs);
	// if (ret == 0) {
	// 	fail++;
	// 	printf("\nFail corrupt buffer test, passed when should have failed\n");
	// }
	// ((char *)(buffs[0]))[TEST_LEN - 2] = c;	// un-corrupt buffer


	return 0;

}
