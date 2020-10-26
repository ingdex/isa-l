/**********************************************************************
  测试ISA-L库中EC码的多线程性能
**********************************************************************/

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
#include <stdlib.h>
#include <string.h>		// for memset, memcmp
#include <pthread.h>
#include "erasure_code.h"
#include "test.h"
#include "tools.h"

#define CACHED_TEST
#ifdef CACHED_TEST
// Cached test, loop many times over small dataset
# define TEST_SOURCES 32	// 最大线程数
# define TEST_LEN(m)  ((1024*1024*1024 / m) & ~(64-1))	// 编码数据大小
# define TEST_TYPE_STR "_warm"
#define REPEAT_TIME 3000
#else
# ifndef TEST_CUSTOM
// Uncached test.  Pull from large mem base.
#  define TEST_SOURCES 32
#  define GT_L3_CACHE  1024*1024*1024	/* some number > last level cache */
#  define TEST_LEN(m)  ((GT_L3_CACHE / m) & ~(64-1))
#  define TEST_TYPE_STR "_cold"
#define REPEAT_TIME 1
# else
#  define TEST_TYPE_STR "_cus"
# endif
#endif

#define MMAX TEST_SOURCES
#define KMAX TEST_SOURCES

#define M		20	// 有效数据+校验数据数量
#define K		18	// 有效数据数量
#define NERRS	2	// 校验数据数量

#define BAD_MATRIX -1

typedef unsigned char u8;

// 线程间传递的参数
typedef struct _threadData
{
	unsigned threadId;
	int m;
	int k;
	u8 * a;
	u8 * g_tbls;
	u8 * buffs[TEST_SOURCES];
	int len;
	int repeat_time;
	// only for decode
	int nerrs;
	u8 *recov[TEST_SOURCES];
	u8 *temp_buffs[TEST_SOURCES];
} threadData;

/**
 * 编码子线程
 * @param args	指向参数结构体的指针
 * @return 		指向返回值的指针
 */
void *encode_thread_handle(void *args)
{
	threadData *data = (threadData *)args;
	unsigned threadId = data->threadId;
	int m = data->m;
	int k = data->k;
	u8 * a = data->a;
	u8 * g_tbls = data->g_tbls;
	u8 ** buffs = data->buffs;
	int len = data->len;
	// printf("threadId = %d\n", threadId);
	int iter = 0;
	while(iter++ < data->repeat_time)
		ec_encode_data(len, k, m - k, g_tbls, buffs, &buffs[k]);
	return NULL;
}

/**
 * 多线程编码性能测试函数
 * @param m			源数据组数 + 校验数据组数
 * @param k			源数据组数
 * @param a			生成矩阵
 * @param g_tbls	Pointer to array of input tables generated from coding coefficients in ec_init_tables(). Must be of size 32*k*rows
 * @param buffs		(源数据 + 校验数据)buffer
 * @param THREADS	子线程数，当THREADS < 0时不另开子线程
 * @return 			编码总时间
 */
double ec_encode_perf(int m, int k, u8 * a, u8 * g_tbls, u8 ** buffs, int THREADS)
{
	double total;
	// 构造矩阵
	ec_init_tables(k, m - k, &a[k * k], g_tbls);
	// single thread
	if (THREADS < 1)
	{
		threadData data;
		// Start encode test
		data.threadId = 0;
		data.k = k;
		data.m = m;
		data.a = a;
		data.g_tbls = g_tbls;
		data.repeat_time = REPEAT_TIME; // 同一份数据重复编译码的次数
		// 计算子进程的在单位源数据单元中参与编码的长度
		data.len = TEST_LEN(m);
		for (int j = 0; j < m; j++)
		{
			data.buffs[j] = buffs[j];
		}

		// encode
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
			data[i].k = k;
			data[i].m = m;
			data[i].a = a;
			data[i].g_tbls = g_tbls;
			data[i].repeat_time = REPEAT_TIME;	// 同一份数据重复编译码的次数
			// 计算子进程的在单位源数据单元中参与编码的长度
			if (i)
			{
				len = TEST_LEN(m) / THREADS;
				off = len * i + TEST_LEN(m) % THREADS;
			}
			else
			{
				len = TEST_LEN(m) / THREADS + TEST_LEN(m) % THREADS; // 0号子进程
			}
			data[i].len = len;
			for (int j = 0; j < m; j++)
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
			printf("\tk =\t%d\n", data[i].k);
			printf("\tm =\t%d\n", data[i].m);
			printf("\ta =\t%p\n", data[i].a);
			printf("\tg_tbls =\t%p\n", data[i].g_tbls);
			printf("\trepeat_time =\t%d\n", data[i].repeat_time);
			printf("\tlen =\t%d\n", data[i].len);
			printf("\tbuffs\n");
			printf("\t[ %p", data[i].buffs[0]);
			for (int j = 1; j < m; j++)
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
 * 译码子线程，译码调用的函数为ec_encode_data，但传入的矩阵参数是译码时使用的校验矩阵，函数执行后得到译码结果
 * @param args	指向参数结构体的指针
 * @return 		指向返回值的指针
 */
void *decode_thread_handle(void *args)
{
	threadData *data = (threadData *)args;
	unsigned threadId = data->threadId;
	// printf("threadId = %d\n", threadId);
	int k = data->k;
	u8 * g_tbls = data->g_tbls;
	int len = data->len;
	int nerrs = data->nerrs;
	u8 **recov = data->recov;
	int iter = 0;
	u8 ** temp_buffs = data->temp_buffs;
	while(iter++ < data->repeat_time)
		ec_encode_data(len, k, nerrs, g_tbls, recov, temp_buffs);
	return NULL;
}

/**
 * 多线程译码性能测试函数
 * @param m				源数据组数 + 校验数据组数
 * @param k				源数据组数
 * @param a				生成矩阵
 * @param g_tbls		Pointer to array of input tables generated from coding coefficients in ec_init_tables(). Must be of size 32*k*rows
 * @param buffs			(源数据 + 校验数据)buffer
 * @param src_in_err	失效数据的位图，数组元素为0表示对应数据可用，数组元素为1表示对应的数据失效（失效的可能是校验数据）
 * @param src_err_list	失效源数据列表
 * @param nerrs			失效数据组数
 * @param temp_buffs	存放译码结果的buffer。译码函数根据k组有效数据回复得到nerrs组失效数据，存放到temp_buffs中
 * @param THREADS		子线程数，当THREADS < 0时不另开子线程
 * @return 				译码总时间
 */
double ec_decode_perf(int m, int k, u8 * a, u8 * g_tbls, u8 ** buffs, u8 * src_in_err,
		   u8 * src_err_list, int nerrs, u8 ** temp_buffs, int THREADS)
{
	int i, j, r;
	u8 b[MMAX * KMAX], c[MMAX * KMAX], d[MMAX * KMAX];
	u8 *recov[TEST_SOURCES];

	// Construct b by removing error rows
	for (i = 0, r = 0; i < k; i++, r++) {
		while (src_in_err[r])
			r++;
		recov[i] = buffs[r];
		for (j = 0; j < k; j++)
			b[k * i + j] = a[k * r + j];
	}

	if (gf_invert_matrix(b, d, k) < 0)
		return BAD_MATRIX;

	for (i = 0; i < nerrs; i++)
		for (j = 0; j < k; j++)
			c[k * i + j] = d[k * src_err_list[i] + j];

	// Recover data
	// 构造矩阵
	ec_init_tables(k, nerrs, c, g_tbls);

	double total;
	// single thread
	if (THREADS < 1)
	{
		threadData data;
		// Start decode test
		data.threadId = 0;
		data.k = k;
		data.nerrs = nerrs;
		data.g_tbls = g_tbls;
		// 计算子进程的在单位源数据单元中参与编码的长度
		data.len = TEST_LEN(m);
		// 同一份数据重复编译码的次数
		data.repeat_time = REPEAT_TIME; 
		for (i = 0; i < k; i++)
		{
			data.recov[i] = recov[i];
		}
		for (i = 0; i < (m - k); i++)
		{
			data.temp_buffs[i] = temp_buffs[i];
		}
		// decode
		tic(0);
		decode_thread_handle((void *)&data);
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
		// 构造子线程参数
		for (int i = 0; i < THREADS; i++)
		{
			data[i].threadId = i;
			data[i].k = k;
			data[i].nerrs = nerrs;
			data[i].g_tbls = g_tbls;
			data[i].repeat_time = REPEAT_TIME;	// 同一份数据重复编译码的次数
			// 计算子进程的在单位源数据单元中参与编码的长度
			if (i)
			{
				len = TEST_LEN(m) / THREADS;
				off = len * i + TEST_LEN(m) % THREADS;
			}
			else
			{
				len = TEST_LEN(m) / THREADS + TEST_LEN(m) % THREADS; // 0号子进程
			}
			data[i].len = len;
			for (j = 0; j < k; j++)
			{
				data[i].recov[j] = recov[j] + off;
			}
			for (j = 0; j < (m - k); j++)
			{
				data[i].temp_buffs[j] = temp_buffs[j] + off;
			}
		}
		// output data array
// #define PTINT_DECODE_DATA_INFO
#ifdef PTINT_DECODE_DATA_INFO
		printf("decode data	===================================\n");
		for (int i = 0; i < THREADS; i++)
		{
			printf("data[ %d ]\n", i);
			printf("\tthreadId =\t%d\n", data[i].threadId);
			printf("\tk =\t%d\n", data[i].k);
			printf("\tnerrs =\t%d\n", data[i].nerrs);
			printf("\tg_tbls =\t%p\n", data[i].g_tbls);
			printf("\trepeat_time =\t%d\n", data[i].repeat_time);
			printf("\tlen =\t%d\n", data[i].len);
			printf("\trecov\n");
			printf("\t[ %p", data[i].recov[0]);
			for (int j = 1; j < k; j++)
			{
				printf("\t%p", data[i].recov[j]);
			}
			printf(" ]\n");
			printf("\ttemp_buffs\n");
			printf("\t[ %p", data[i].temp_buffs[0]);
			for (int j = 1; j < (m - k); j++)
			{
				printf("\t%p", data[i].temp_buffs[j]);
			}
			printf(" ]\n");
		}
		printf("===========================================\n");
#endif
		// encode
		tic(0);
		for (int i = 0; i < THREADS; i++)
		{
			pthread_create(&pid[i], NULL, decode_thread_handle, (void *)&(data[i]));
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
	int i, j, m, k, nerrs, check;
	void *buf;
	u8 *temp_buffs[TEST_SOURCES], *buffs[TEST_SOURCES];
	u8 a[MMAX * KMAX];
	u8 g_tbls[KMAX * TEST_SOURCES * 32], src_in_err[TEST_SOURCES];
	u8 src_err_list[TEST_SOURCES];
	struct perf start;
	int THREADS	= 0;	// 子线程数，等与0时为单线程
	if (argc >= 2)
	{
		THREADS = atoi(argv[1]);
	}

	// Pick test parameters
	m = M;
	k = K;
	nerrs = NERRS;
	const u8 err_list[] = { 1, 2, 3, 4, 5, 7, 10, 11 };

	printf("erasure_code_perf: %dx%d %d\n", m, TEST_LEN(m), nerrs);
	printf("Threads: %d\n", THREADS);

	if (m > MMAX || k > KMAX || nerrs > (m - k)) {
		printf(" Input test parameter error\n");
		return -1;
	}

	memcpy(src_err_list, err_list, nerrs);
	memset(src_in_err, 0, TEST_SOURCES);
	for (i = 0; i < nerrs; i++)
		src_in_err[src_err_list[i]] = 1;

	// Allocate the arrays
	for (i = 0; i < m; i++) {
		if (posix_memalign(&buf, 64, TEST_LEN(m))) {
			printf("alloc error: Fail\n");
			return -1;
		}
		buffs[i] = buf;
	}

	for (i = 0; i < (m - k); i++) {
		if (posix_memalign(&buf, 64, TEST_LEN(m))) {
			printf("alloc error: Fail\n");
			return -1;
		}
		temp_buffs[i] = buf;
	}

	// Make random data
	for (i = 0; i < k; i++)
		for (j = 0; j < TEST_LEN(m); j++)
			buffs[i][j] = rand();

	gf_gen_rs_matrix(a, m, k);

	// encode test
	printf("start encode\n");
	double total = ec_encode_perf(m, k, a, g_tbls, buffs, THREADS);
	printf("encode ended, bandwidth %lld MB in %lf secs\n", ((long long)(TEST_LEN(m)) * (m) * REPEAT_TIME) / 1024 / 1024, total);
	print_throughtput((long long)(TEST_LEN(m)) * (m) * REPEAT_TIME, total, "erasure_code_encode" TEST_TYPE_STR ": ");
	
	// decode test
	printf("start decode\n");
	total = ec_decode_perf(m, k, a, g_tbls, buffs, src_in_err, src_err_list, nerrs,
						   temp_buffs, THREADS);

	if (total == BAD_MATRIX)
	{
		printf("BAD MATRIX\n");
		return check;
	}
	for (i = 0; i < nerrs; i++)
	{
		if (0 != memcmp(temp_buffs[i], buffs[src_err_list[i]], TEST_LEN(m)))
		{
			printf("Fail error recovery (%d, %d, %d) - ", m, k, nerrs);
			return -1;
		}
	}

	printf("decode ended, bandwidth %lld MB in %lf secs\n", ((long long)(TEST_LEN(m)) * (k + nerrs) * REPEAT_TIME) / 1024 / 1024, total);
	print_throughtput((long long)(TEST_LEN(m)) * (k + nerrs) * REPEAT_TIME, total, "erasure_code_decode" TEST_TYPE_STR ": ");

	printf("done all: Pass\n");
	return 0;
}
