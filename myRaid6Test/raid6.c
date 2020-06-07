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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <isa-l/crc.h>
#include <isa-l/crc64.h>
#include <isa-l/erasure_code.h>
#include <isa-l/gf_vect_mul.h>
#include <isa-l/igzip_lib.h>
#include <isa-l/raid.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <isa-l/test.h>
#include <errno.h>

#define TEST_SOURCES 10
#define GT_L3_CACHE 32 * 1024 * 1024 /* some number > last level cache */
#define TEST_LEN ((GT_L3_CACHE / TEST_SOURCES) & ~(64 - 1))
// #define TEST_LEN 1024
#define TEST_MEM ((TEST_SOURCES + 2) * (TEST_LEN))
#ifndef TEST_SEED
#define TEST_SEED 0x1234
#endif
#define TEST_TYPE_STR "_cold"
#define TEST_TIMES 100000
#define PID_NUM 40
#define UNIT 35 // 每次申请子进程，逐步减少的循环次数

// Generates pseudo-random data

static void rand_buffer(unsigned char *buf, long buffer_size)
{
  long i;
  for (i = 0; i < buffer_size; i++)
    buf[i] = rand();
}

static inline int do_test_pq_check(void **buffs, int test_times)
{
  for (int i = 0; i < test_times; i++)
  {
    // 校验
    // ret = pq_check_base(TEST_SOURCES + 2, TEST_LEN, buffs);
    pq_check(TEST_SOURCES + 2, TEST_LEN, buffs);
    // ret = pq_check_sse(TEST_SOURCES + 2, TEST_LEN, buffs);
  }
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
  // Test rand
  // 产生随机数
  for (i = 0; i < TEST_SOURCES + 2; i++)
    rand_buffer((unsigned char *)buffs[i], TEST_LEN);
  // 编码
  pq_gen_avx(TEST_SOURCES + 2, TEST_LEN, buffs);
  // BENCHMARK(&start, TEST_TIMES, pq_check(TEST_SOURCES + 2, TEST_LEN, buffs));
  // perf_print(start, (long long)TEST_MEM);
  pid_t pid[PID_NUM];
  pid_t tmpPid;
  int errno;
  int status = 0;
  long long real_times = 0;
  for (int i=0; i<PID_NUM; i++)
  {
    real_times += TEST_TIMES - i * UNIT;
  }
  long long starttime = get_time();
  for (int i=0; i<PID_NUM; i++)
  {
    pid[i] = fork();
    if (pid[i] > 0)
    {
      continue;
    }
    else if (pid[i] == 0)
    {
      do_test_pq_check(buffs, TEST_TIMES - i * UNIT);
      exit(0);
    }
    else
    {
      printf("fork() failed\n");
      break;
    }
  }
  // do_test_pq_check(buffs);
  // wait(NULL);
  while ((tmpPid = wait(&status)) > 0);
  // while (tmpPid = waitpid(-1, NULL, 0))
  // {
  //   if (errno == ECHILD)
  //   {
  //     break;
  //   }
  // }
  long long endtime = get_time();
  long long nsec = endtime - starttime;
  long long usec = nsec / 1000000;

  // printf("clock_gettime():%lld\n", nsec);
  printf("校验耗时%lldus\n", usec);
  long long MEM = (((GT_L3_CACHE / 1024 / 1024) * TEST_TIMES) + ((GT_L3_CACHE / 1024 / 1024) * (TEST_TIMES - (PID_NUM - 1) * UNIT))) * PID_NUM / 2;
  printf("MEM%lldMB\n", MEM);
  printf("MEM%lldMB\n", (GT_L3_CACHE / 1024 / 1024) * real_times);
  printf("吞吐率%fMB/s\n", MEM * 1.0 / nsec * 1000000000);

  return 0;
}
