#include "Encoder.hpp"
#include "erasure_code.h"
#include <pthread.h>

/* 将要编码的数据分割，分配给threads个threadData结构体 */
void Encoder::initThreadData(Data data, unsigned int threads)
{
    alignData(data);
	// 为校验数据申请内存空间
    for (int i = this->valid; i < this->m; i++) {
		if (posix_memalign((void **)&this->buffs[i], 64, this->stripe_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
    }
    // 除了第一个外，每个子线程编译码的长度，当条带所包括的块数量不能被线程数整除时，第一个线程处理更多的编译码块
    size_t tblocks = this->stripe_blocks / threads; // 下标不为0的线程的编译码块数
    size_t t0blocks = this->stripe_blocks % threads + tblocks; // 下标为0的线程的编译码块数
    size_t off_blocks = 0; // 下标为i的子线程处理的起始块的块偏移
    for (int i = 0; i < threads; i++) {
        this->threadargs[i].threadId = i;
        this->threadargs[i].m = this->m;
        this->threadargs[i].valid = this->valid;
        this->threadargs[i].checks = this->checks;
        this->threadargs[i].repeat_time = REPEAT_TIME;
        this->threadargs[i].gen_matrix = this->gen_matrix;
        this->threadargs[i].g_tbls = this->g_tbls;
        this->threadargs[i].blocks = (i == 0) ? t0blocks : tblocks;
        this->threadargs[i].block_size = this->block_size;
        for (int j = 0; j < this->m; j++) {
            this->threadargs[i].buffs[j] = this->buffs[j] + off_blocks * this->block_size;
        }
        off_blocks += this->threadargs[i].blocks;
    }
}

/* 将data中的数据按照编译码块大小与条带数量对齐，分割为条带，存储到buffs中，同时为校验数据申请存储空间，首地址记录在buffs中 */
void Encoder::alignData(Data data)
{
    /* 将要编码的数据分配给threads个threadData结构体 */
    if (data.getType() != 0) {
        cout << "type for data != 0" << endl;
        exit(0);
    }
    size_t data_size = data.getDataSize();
    // 对齐前数据块的数量
    size_t total_blocks_before = data_size / this->block_size;
    if (data_size % this->block_size)
        total_blocks_before = total_blocks_before + 1;
    this->stripe_size = data_size / this->valid;
    // 条带中数据按照块大小对齐
	if (this->stripe_size % this->block_size)
	    this->stripe_size += (this->block_size - this->stripe_size % this->block_size);
    // 对齐后单个条带数据块数量
    size_t stripe_blocks_after = this->stripe_size / this->block_size;
    this->stripe_blocks = stripe_blocks_after;
    /* 缺少的数据补零填充 */
    // 计算有效数据可以完全填充的条带数量
    size_t fullfilled_stripes = total_blocks_before / stripe_blocks_after;
    if (data_size % this->block_size) // 有效数据最后一块末尾被补零
        fullfilled_stripes = (total_blocks_before - 1) / stripe_blocks_after;
    this->buffs[0] = data.getBuff();
    // 填充可以完全填充的条带，使用原有内存空间
    for (int i = 1; i < fullfilled_stripes; i++) {
        this->buffs[i] = this->buffs[i - 1] + stripe_size;
    }
    // 对于不能完全填充的条带，申请新的内存空间
    for (int i = fullfilled_stripes; i < this->valid; i++) {
        if (posix_memalign((void**)&this->buffs[i], 64, this->stripe_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
        memset(this->buffs[i], 0, this->stripe_size);
    }
    // 将未填充到条带中的数据写入新申请的空间中
    size_t piece_size = data_size - fullfilled_stripes * stripe_size;
    memcpy(this->buffs[fullfilled_stripes], data.getBuff() + stripe_size * fullfilled_stripes, piece_size);
}

void* Encoder::encode_thread_handle(void* args)
{
    threadData* data = (threadData*)args;
    unsigned threadId = data->threadId;
    // int m = data->m;
    // int valid = data->valid;
    // int checks = data->checks;
    // size_t
    // u8 * a = data->a;
    // u8 * g_tbls = data->g_tbls;
    // u8 ** buffs = data->buffs;
    u8* buffs_bak[MAX_STRIPES];
    for (int i = 0; i < data->m; i++) {
        buffs_bak[i] = data->buffs[i];
    }
    // long long len = data->len;
    // long long chunks = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // long long size;	// 编码块大小
    // printf("threadId = %d\n", threadId);
    int iter = 0;
    while (iter++ < data->repeat_time) {
        // ec_encode_data(len, k, m - k, g_tbls, buffs, &buffs[k]);
        // break;
        for (size_t i = 0; i < data->blocks; i++) {
            ec_encode_data(data->block_size, data->valid, data->checks, data->g_tbls, buffs_bak, &buffs_bak[data->valid]);
            for (int j = 0; j < data->m; j++) {
                buffs_bak[j] += data->block_size;
            }
        }
        if (REPEAT_TIME > 1) {
            for (int i = 0; i < data->m; i++) {
                buffs_bak[i] = data->buffs[i];
            }
        }
    }
    return NULL;
}

void Encoder::encode_perf(Data data, unsigned int threads)
{
    cout << "start encode" << endl;
    double totaltime;
    // single thread
    if (threads < 1) {
        /* threads赋值为0时不创建子线程 */
        initThreadData(data, 1);
        // encode
        tic(0);
        encode_thread_handle((void*)&(this->threadargs[0]));
        totaltime = toc(0);
    } else {
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
        pthread_t pid[threads];
        void* ret[threads];
        initThreadData(data, threads);
        // encode
        tic(0);
        for (int i = 0; i < threads; i++) {
            pthread_create(&pid[i], NULL, encode_thread_handle, (void*)&(this->threadargs[i]));
        }
        for (int i = 0; i < threads; i++) {
            pthread_join(pid[i], &ret[i]);
        }
        totaltime = toc(0);
        // output data array
// #define PTINT_ENCODE_DATA_INFO
#ifdef PTINT_ENCODE_DATA_INFO
        printf("data	===================================\n");
        for (int i = 0; i < THREADS; i++) {
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
            for (int j = 1; j < m; j++) {
                printf("\t%p", data[i].buffs[j]);
            }
            printf(" ]\n");
        }
        printf("===========================================\n");
#endif
    }
    cout << "encode ended, bandwidth "
         << getEncodeDataSizeMB()
         << "MB in "
         << totaltime
         << "secs" << endl;
    print_throughtput(getEncodeDataSize(), totaltime, "erasure_code_encode");
    // return total;
}

#ifdef GTD
timeval start_clock[10];
timeval end_clock[10];
void Encoder::tic(int i)
{
    gettimeofday(&start_clock[i], NULL);
}

double Encoder::toc(int i)
{
    gettimeofday(&end_clock[i], NULL);
    long long time_use = (end_clock[i].tv_sec - start_clock[i].tv_sec) * 1000000 + (end_clock[i].tv_usec - start_clock[i].tv_usec); //微秒
    return time_use * 1.0 / 1000000;
}
#elif defined CGT

struct timespec start_clock[10];
struct timespec end_clock[10];
void Encoder::tic(int i)
{
    clock_gettime(1, &start_clock[i]);
}
double Encoder::toc(int i)
{
    clock_gettime(CLOCK_ID, &end_clock[i]);
    unsigned long time_use = (end_clock[i].tv_sec - start_clock[i].tv_sec) * 1000000000 + (end_clock[i].tv_nsec - start_clock[i].tv_nsec);
    return time_use * 1.0 / 1000000000;
}
#endif

/* 输出吞吐率 */
void Encoder::print_throughtput(size_t data_size, double time, string info)
{
    cout << info
         << " Throughtput: \t"
         << data_size / time / 1024 / 1024 / 1024
         << "\tGB/s"
         << endl;
}

/* 获取对齐后的编码总数据长度，单位为Byte，data_size乘以repeat_time */
size_t Encoder::getEncodeDataSize()
{
    size_t data_size = this->stripe_size * this->m * REPEAT_TIME;
    return data_size;
}

/* 获取对齐后的编码总数据长度，单位为MB，data_size乘以repeat_time除以10^6 */
size_t Encoder::getEncodeDataSizeMB()
{
    size_t data_size_MB = this->stripe_size * this->m / 1024 / 1024 * REPEAT_TIME;
    return data_size_MB;
}
