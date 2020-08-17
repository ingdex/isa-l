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
#include <sys/time.h>
#include <pthread.h>
#include "raid.h"
#include "test.h"
#include "tools.h"

#define CACHED_TEST
#ifdef CACHED_TEST
// Cached test, loop many times over small dataset
# define TEST_SOURCES 15
# define TEST_LEN  (long long)((256*1024 / (TEST_SOURCES + 1)) & ~(64-1))
#define TEST_TYPE_STR "_warm"
#define REPEAT_TIME 200000
#else
#ifndef TEST_CUSTOM
// Uncached test.  Pull from large mem base.
#  define TEST_SOURCES 15
#  define GT_L3_CACHE  ((long long)1024*1024*1024)	/* some number > last level cache */
#  define TEST_LEN ((GT_L3_CACHE / (TEST_SOURCES + 1)) & ~(64 - 1))
#  define TEST_TYPE_STR "_cold"
#define REPEAT_TIME 8
#else
#define TEST_TYPE_STR "_cus"
#endif
#endif

#define TEST_MEM ((TEST_SOURCES + 1) * (TEST_LEN))

// 线程间传递的参数
typedef struct _threadData
{
	unsigned threadId;
    int repeat_time;
    size_t len;
    void *buffs[TEST_SOURCES + 1];
} threadData;

/**
 * 编码子线程
 * @param args	指向参数结构体的指针
 * @return 		指向返回值的指针
 */
void *encode_thread_handle(void *args)
{
	threadData *data = (threadData *)args;
	// printf("threadId = %d\n", data->threadId);
    // printf("data->len = %lu\n", data->len);
    // printf("data->buffs = %p\n", data->buffs);
	int iter = 0;
	while(iter++ < data->repeat_time)
		xor_gen(TEST_SOURCES + 1, data->len, data->buffs);
	return NULL;
}

/**
 * 多线程编码性能测试函数
 * @param buffs		(源数据 + 校验数据)buffer，默认末尾2个是校验数据
 * @param THREADS	子线程数，当THREADS < 0时不另开子线程
 * @return 			编码总时间
 */
double xor_encode_perf(void ** buffs, int THREADS)
{
	double total;
	// single thread
	if (THREADS < 1)
	{
		threadData data;
		// Start encode test
		data.threadId = 0;
		data.repeat_time = REPEAT_TIME; // 同一份数据重复编译码的次数
		// 计算子进程的在单位源数据单元中参与编码的长度
		data.len = TEST_LEN;
		for (int j = 0; j < TEST_SOURCES + 1; j++)
		{
			data.buffs[j] = buffs[j];
		}
		// encode
        int iter = 0;
		tic(0);
        encode_thread_handle((void *)&data);
		total = toc(0);
	}
	else
	{
		/**
		 * 多个子线程并行编译码，每个子线程负责一部分
		 * 示意图：
		 * 	k个源数据区							m-k个校验数据区（待填充）
		 * ------------------------------------------------------------------------
		 * |       |       | ... |       |--|         |         | ... |           |
		 * | src 1 | src 2 | ... | src k |--| check 1 | check 2 | ... | check m-k |
		 * |       |       | ... |       |--|         |         | ... |           |
		 * ------------------------------------------------------------------------
		 * THREADS个子线程负责区域
		 * ----------------------------------------------------------------------------------
		 * | thread 0 | thread 0 | ... | thread 0 |--| thread 0 | thread 0 | ... | thread 0 |
		 * | thread 1 | thread 1 | ... | thread 1 |--| thread 1 | thread 1 | ... | thread 1 |
		 * | thread 2 | thread 2 | ... | thread 2 |--| thread 2 | thread 2 | ... | thread 2 |
		 * ----------------------------------------------------------------------------------
		 */
		pthread_t pid[THREADS];
		void *ret[THREADS];
		threadData data[THREADS];
		__loff_t off = 0;
		size_t len = 0;
		// Start encode test
		// 构造子线程参数
		for (int i = 0; i < THREADS; i++)
		{
			data[i].threadId = i;
			data[i].repeat_time = REPEAT_TIME;	// 同一份数据重复编译码的次数
			// 计算子进程的在单位源数据单元中参与编码的长度
			if (i)
			{
				len = TEST_LEN / THREADS;
				off = len * i + TEST_LEN % THREADS;
			}
			else
			{
				len = TEST_LEN / THREADS + TEST_LEN % THREADS; // 0号子进程
			}
			data[i].len = len;
			for (int j = 0; j < TEST_SOURCES + 1; j++)
			{
				data[i].buffs[j] = buffs[j] + off;
			}
		}
		// output data array
// #define PTINT_ENCODE_DATA_INFO
#ifdef PTINT_ENCODE_DATA_INFO
		printf("data	===================================\n");
		for (int i = 0; i < THREADS; i++)
		{
			printf("data[ %d ]\n", i);
			printf("\tthreadId =\t%d\n", data[i].threadId);
			printf("\trepeat_time =\t%d\n", data[i].repeat_time);
			printf("\tlen =\t%lu\n", data[i].len);
			printf("\tbuffs\n");
			printf("\t[ %p", data[i].buffs[0]);
			for (int j = 1; j < TEST_SOURCES + 1; j++)
			{
				printf("\t%p", data[i].buffs[j]);
			}
			printf(" ]\n");
		}
		printf("===========================================\n");
#endif
		// encode
		tic(0);
		for (int i = 0; i < THREADS; i++)
		{
			pthread_create(&pid[i], NULL, encode_thread_handle, (void *)&(data[i]));
		}
		for (int i = 0; i < THREADS; i++)
		{
			pthread_join(pid[i], &ret[i]);
		}
		total = toc(0);
		
	}
	return total;
}

/**
 * 校验子线程
 * @param args	指向参数结构体的指针
 * @return 		指向返回值的指针
 */
void *xor_check_thread_handle(void *args)
{
	threadData *data = (threadData *)args;
	unsigned threadId = data->threadId;
    // printf("threadId = %d\n", threadId);
    size_t len = data->len;
	void **buffs = data->buffs;
	int iter = 0;
	while(iter++ < data->repeat_time)
		xor_check(TEST_SOURCES + 1, len, buffs);
	return NULL;
}

/**
 * 多线程校验性能测试函数
 * @param buffs		(源数据 + 校验数据)buffer，默认末尾2个是校验数据
 * @param THREADS	子线程数，当THREADS < 0时不另开子线程
 * @return 			编码总时间
 */
double xor_check_perf(void ** buffs, int THREADS)
{
	double total;
	// single thread
	if (THREADS < 1)
	{
		threadData data;
		// Start encode test
		data.threadId = 0;
		data.repeat_time = REPEAT_TIME; // 同一份数据重复编译码的次数
		// 计算子进程的在单位源数据单元中参与编码的长度
		data.len = TEST_LEN;
		for (int j = 0; j < TEST_SOURCES + 1; j++)
		{
			data.buffs[j] = buffs[j];
		}
		// encode
		tic(0);
		xor_check_thread_handle((void *)&data);
		total = toc(0);
	}
	else
	{
		/**
		 * 多个子线程并行编译码，每个子线程负责一部分
		 * 示意图：
		 * 	k个源数据区							m-k个校验数据区（待填充）
		 * ------------------------------------------------------------------------
		 * |       |       | ... |       |--|         |         | ... |           |
		 * | src 1 | src 2 | ... | src k |--| check 1 | check 2 | ... | check m-k |
		 * |       |       | ... |       |--|         |         | ... |           |
		 * ------------------------------------------------------------------------
		 * THREADS个子线程负责区域
		 * ----------------------------------------------------------------------------------
		 * | thread 0 | thread 0 | ... | thread 0 |--| thread 0 | thread 0 | ... | thread 0 |
		 * | thread 1 | thread 1 | ... | thread 1 |--| thread 1 | thread 1 | ... | thread 1 |
		 * | thread 2 | thread 2 | ... | thread 2 |--| thread 2 | thread 2 | ... | thread 2 |
		 * ----------------------------------------------------------------------------------
		 */
		pthread_t pid[THREADS];
		void *ret[THREADS];
		threadData data[THREADS];
		__loff_t off = 0;
		size_t len = 0;
		// Start encode test
		// 构造子线程参数
		for (int i = 0; i < THREADS; i++)
		{
			data[i].threadId = i;
			data[i].repeat_time = REPEAT_TIME;	// 同一份数据重复编译码的次数
			// 计算子进程的在单位源数据单元中参与编码的长度
			if (i)
			{
				len = TEST_LEN / THREADS;
				off = len * i + TEST_LEN % THREADS;
			}
			else
			{
				len = TEST_LEN / THREADS + TEST_LEN % THREADS; // 0号子进程
			}
			data[i].len = len;
			for (int j = 0; j < TEST_SOURCES + 1; j++)
			{
				data[i].buffs[j] = buffs[j] + off;
			}
		}
		// output data array
// #define PTINT_CHECK_DATA_INFO
#ifdef PTINT_CHECK_DATA_INFO
		printf("data	===================================\n");
		for (int i = 0; i < THREADS; i++)
		{
			printf("data[ %d ]\n", i);
			printf("\tthreadId =\t%d\n", data[i].threadId);
			printf("\trepeat_time =\t%d\n", data[i].repeat_time);
			printf("\tlen =\t%lu\n", data[i].len);
			printf("\tbuffs\n");
			printf("\t[ %p", data[i].buffs[0]);
			for (int j = 1; j < TEST_SOURCES + 1; j++)
			{
				printf("\t%p", data[i].buffs[j]);
			}
			printf(" ]\n");
		}
		printf("===========================================\n");
#endif
		// check
		tic(0);
		for (int i = 0; i < THREADS; i++)
		{
			pthread_create(&pid[i], NULL, xor_check_thread_handle, (void *)&(data[i]));
		}
		for (int i = 0; i < THREADS; i++)
		{
			pthread_join(pid[i], &ret[i]);
		}
		total = toc(0);
		
	}
	return total;
}

int main(int argc, char *argv[])
{
    int i, j;
    void *buffs[TEST_SOURCES + 1];
    int THREADS = 0;    // 子线程数，等与0时为单线程
    if (argc >= 2)
        THREADS = atoi(argv[1]);
    printf("Test xor_gen_perf %d sources X %lld bytes\n", TEST_SOURCES, TEST_LEN);

    // Allocate the arrays
    for (i = 0; i < TEST_SOURCES + 1; i++)
    {
        int ret;
        void *buf;
        ret = posix_memalign(&buf, 64, TEST_LEN);
        if (ret)
        {
            printf("alloc error: Fail");
            return 1;
        }
        buffs[i] = buf;
    }

    // Setup data
    for (i = 0; i < TEST_SOURCES + 1; i++)
        memset(buffs[i], 0, TEST_LEN);

    // Make random data
    // for (i = 0; i < TEST_SOURCES; i++)
    //     for (j = 0; j < TEST_LEN; j++)
    //         ((unsigned char *)buffs[i])[j] = rand();

    // Start encode test
    printf("start encode\n");
    double total = xor_encode_perf(buffs, THREADS);
    printf("encode ended, bandwidth %lld MB in %lf secs\n", ((long long)TEST_MEM * REPEAT_TIME) / 1024 / 1024, total);
	print_throughtput((long long)TEST_MEM * REPEAT_TIME, total, "xor_gen" TEST_TYPE_STR ": ");
    
    // xor_check test
	printf("start xor_check\n");
    total = xor_check_perf(buffs, THREADS);
    printf("xor_check ended, bandwidth %lld MB in %lf secs\n", ((long long)TEST_MEM * REPEAT_TIME) / 1024 / 1024, total);
	print_throughtput((long long)TEST_MEM * REPEAT_TIME, total, "xor_check" TEST_TYPE_STR ": ");
    // Warm up
    // BENCHMARK(&start, BENCHMARK_TIME, xor_gen(TEST_SOURCES + 1, TEST_LEN, buffs));
    // printf("xor_gen" TEST_TYPE_STR ": ");
    // perf_print(start, (long long)TEST_MEM);

    return 0;
}
