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
#include "crc.h"
#include "test.h"
#include "tools.h"

#define CACHED_TEST
#ifdef CACHED_TEST
// Cached test, loop many times over small dataset
# define TEST_SOURCES 32    // crc码字数量
#  define GT_L3_CACHE  ((long long)256*1024)	/* some number > last level cache */
#  define TEST_LEN (long long)((GT_L3_CACHE / (TEST_SOURCES)) & ~(64 - 1)) // 单个crc码字长度
#define TEST_TYPE_STR "_warm"
#define REPEAT_TIME 200000
#else
#ifndef TEST_CUSTOM
// Uncached test.  Pull from large mem base.
#  define TEST_SOURCES 128    // crc码字数量
#  define GT_L3_CACHE  ((long long)1024*1024*1024)	/* some number > last level cache */
#  define TEST_LEN (long long)((GT_L3_CACHE / (TEST_SOURCES)) & ~(64 - 1)) // 单个crc码字长度
#  define TEST_TYPE_STR "_cold"
#define REPEAT_TIME 32
#else
#define TEST_TYPE_STR "_cus"
#endif
#endif

#ifndef TEST_SEED
# define TEST_SEED 0x1234
#endif

#define TEST_MEM ((TEST_SOURCES) * (TEST_LEN))  // 所有码字占用内存大小

// 线程间传递的参数
typedef struct _threadData
{
	unsigned threadId;
    int repeat_time;
    size_t len;
    int buffs_size;   
    void *buffs[TEST_SOURCES];
    int crc;
} threadData;

/**
 * crc_ieee编码子线程
 * @param args	指向参数结构体的指针
 * @return 		指向返回值的指针
 */
void *crc_encode_thread_handle(void *args)
{
	threadData *data = (threadData *)args;
    for (int i = 0; i < data->buffs_size; i++)
    {
        int iter = 0;
        while (iter++ < data->repeat_time)
            data->crc = crc32_ieee(TEST_SEED, data->buffs[i], data->len);
    }

    return NULL;
}

/**
 * 多线程crc_ieee编码性能测试函数
 * @param buffs		源数据buffer
 * @param THREADS	子线程数，当THREADS < 0时不另开子线程
 * @return 			编码总时间
 */
double crc_encode_perf(void ** buffs, int THREADS)
{
	double total;
	// single thread
	if (THREADS < 1)
	{
		threadData data;
		// Start encode test
		data.threadId = 0;
		data.repeat_time = REPEAT_TIME; // 同一份数据重复编译码的次数
		data.buffs_size = TEST_SOURCES;
        // 计算子进程的在单位源数据单元中参与编码的长度
		data.len = TEST_LEN;
		for (int j = 0; j < TEST_SOURCES; j++)
		{
			data.buffs[j] = buffs[j];
		}
		// encode
		tic(0);
        crc_encode_thread_handle((void *)&data);
		total = toc(0);
	}
	else
	{
		/**
		 * 多个子线程并行编译码，每个子线程负责一部分
		 * 示意图：
		 * k个源数据							
		 * ------------------------------------------------------------------------
		 * |       |       | ... |       |
		 * | src 1 | src 2 | ... | src k |
		 * |       |       | ... |       |
		 * ------------------------------------------------------------------------
		 * THREADS个子线程负责区域，每个子线程负责x个src
		 * ----------------------------------------------------------------------------------
		 * |          |          | ... |          |          | ... | ......
		 * | thread 0 | thread 0 | ... | thread 1 | thread 1 | ... | ......
		 * |          |          | ... |          |          | ... | ......
		 * ----------------------------------------------------------------------------------
		 */
		pthread_t pid[THREADS];
		void *ret[THREADS];
		threadData data[THREADS];
		__loff_t off = 0;
        size_t chunk;
		// Start encode test
		// 构造子线程参数
		for (int i = 0; i < THREADS; i++)
		{
			data[i].threadId = i;
			data[i].repeat_time = REPEAT_TIME * THREADS;	// 同一份数据重复编译码的次数
			data[i].len = TEST_LEN;
            // 计算子进程负责的编码数量
			if (i)
			{
				chunk = TEST_SOURCES / THREADS;
				off = chunk * i + TEST_SOURCES % THREADS;
			}
			else
			{
				chunk = TEST_SOURCES / THREADS + TEST_SOURCES % THREADS; // 0号子进程
			}
            data[i].buffs_size = chunk;
			for (int j = 0; j < chunk; j++)
			{
				data[i].buffs[j] = buffs[j + off];
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
            printf("\tbuffs_size =\t%lu\n", data[i].buffs_size);
			printf("\tbuffs\n");
			printf("\t[ %p", data[i].buffs[0]);
			for (int j = 1; j < TEST_SOURCES; j++)
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
			pthread_create(&pid[i], NULL, crc_encode_thread_handle, (void *)&(data[i]));
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
    void *buffs[TEST_SOURCES];
	uint32_t crc;
	int THREADS = 0;    // 子线程数，等与0时为单线程
    if (argc >= 2)
        THREADS = atoi(argv[1]);
    printf("Test crc32_ieee_perf %d sources X %lld bytes\n", TEST_SOURCES, TEST_LEN);

    // Allocate the arrays
    for (int i = 0; i < TEST_SOURCES; i++)
    {
        int ret;
        void *buf;
        ret = posix_memalign(&buf, 1024, TEST_LEN);
        if (ret)
        {
            printf("alloc error: Fail");
            return 1;
        }
        buffs[i] = buf;
    }

    // void *buf;
	// // uint32_t crc;
	// struct perf start;
    // printf("crc32_ieee_perf:\n");

	// if (posix_memalign(&buf, 1024, TEST_LEN)) {
	// 	printf("alloc error: Fail");
	// 	return -1;
	// }
	// memset(buf, 0, TEST_LEN);
	// BENCHMARK(&start, BENCHMARK_TIME, crc = crc32_ieee(TEST_SEED, buf, TEST_LEN));
	// printf("crc32_ieee" TEST_TYPE_STR ": ");
	// perf_print(start, (long long)TEST_LEN);

    // Start encode test
    printf("start encode\n");
    double total = crc_encode_perf(buffs, THREADS);
    printf("encode ended, bandwidth %lld MB in %lf secs\n", ((long long)TEST_MEM * REPEAT_TIME * (THREADS < 1 ? 1 : THREADS)) / 1024 / 1024, total);
	print_throughtput((long long)TEST_MEM * REPEAT_TIME * (THREADS < 1 ? 1 : THREADS), total, "crc32_ieee" TEST_TYPE_STR ": ");

    return 0;
}
