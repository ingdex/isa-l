#include "Encoder.hpp"
#include "erasure_code.h"
#include <pthread.h>

/* 将要编码的数据分割，分配给threads个threadData结构体 */
void Encoder::initEncodeThreadData(Data data, unsigned int threads)
{
    // 除了第一个外，每个子线程编译码块的数量，当条带所包括的块数量不能被线程数整除时，第一个线程处理更多的编译码块
    size_t tblocks = data.getStripeBlocks() / threads; // 下标不为0的线程的编译码块数
    size_t t0blocks = data.getStripeBlocks() % threads + tblocks; // 下标为0的线程的编译码块数
    size_t off_blocks = 0; // 下标为i的子线程处理的起始块的块偏移
    for (int i = 0; i < threads; i++) {
        this->threadargs[i].threadId = i;
        this->threadargs[i].m = data.getM();
        this->threadargs[i].valid = data.getValid();
        this->threadargs[i].checks = data.getChecks();
        this->threadargs[i].repeat_time = REPEAT_TIME;
        this->threadargs[i].gen_matrix = data.getGenMatrix();
        this->threadargs[i].g_tbls = data.getG_tbls();
        this->threadargs[i].blocks = (i == 0) ? t0blocks : tblocks;
        this->threadargs[i].block_size = data.getBlockSize();
        for (int j = 0; j < data.getM(); j++) {
            this->threadargs[i].buffs[j] = data.getStripe(j) + off_blocks * data.getBlockSize();
        }
        off_blocks += this->threadargs[i].blocks;
    }
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
        initEncodeThreadData(data, 1);
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
        initEncodeThreadData(data, threads);
        // encode
        tic(0);
        for (int i = 0; i < threads; i++) {
            pthread_create(&pid[i], NULL, encode_thread_handle, (void*)&(this->threadargs[i]));
        }
        for (int i = 0; i < threads; i++) {
            pthread_join(pid[i], &ret[i]);
        }
        totaltime = toc(0);
        /* 将校验条带中的数据写入校验数据缓冲区data_checks */
        data.dumpDataChecks();
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
         << data.getEncodeDataSizeMB()
         << "MB in "
         << totaltime
         << "secs" << endl;
    print_throughtput(data.getEncodeDataSize(), totaltime, "erasure_code_encode");
    // return total;
}

/* 根据threads用data初始化数据恢复所需的矩阵以及线程参数 */
bool Encoder::initRecovThreadData(Data data, unsigned int threads) {
    this->b_matrix = new u8[data.getM() * data.getValid()];
    this->inverse_matrix = new u8[data.getM() * data.getValid()];
    this->recover_matrix = new u8[data.getM() * data.getValid()];
    this->recover_g_tbls = new u8[data.getValid() * MAX_STRIPES * 32];
    // 获取任意valid个有效的数据条带，以及对应的valid行矩阵，用于数据恢复
    for (int i=0, r=0; i<data.getValid(); i++, r++) {
        // 跳过失效条带对应生成矩阵的行
        while (data.isError(r))
            r++;
        this->recover_stripe[i] = data.getStripe(r);
        for (int j=0; j<data.getValid(); j++)
            this->b_matrix[data.getValid() * i + j] = data.getGenMatrix(r, j);
    }
    // 将需要恢复的nerrs_valid个有效数据条带首地址写入recover_stripe末尾
    for (int i=0, r=0; i<data.getNerrsValid(); i++, r++) {
        while (!data.isError(r))
            r++;
        this->recover_stripe[data.getValid() + i] = data.getStripe(r);
    }
    // 求b_matrix的逆矩阵inverse_matrix
    if (gf_invert_matrix(this->b_matrix, this->inverse_matrix, data.getValid()) < 0) {
        delete this->b_matrix;
        delete this->inverse_matrix;
        delete this->recover_g_tbls;
        return false;
    }
    // 取出需要恢复的有效数据条带在recover_matrix中对应的行，校验数据的恢复需要使用恢复后的有效数据重新编码得到
    for (int i = 0, r = 0; i < data.getNerrsValid(); i++, r++) {
        while (!data.isError(r))
            r++;
        for (int j = 0; j < data.getValid(); j++)
            this->recover_matrix[data.getValid() * i + j] = this->inverse_matrix[data.getValid() * r + j];
    }
    ec_init_tables(data.getValid(), data.getNerrsValid(), this->recover_matrix, this->recover_g_tbls);
    /* 初始化线程参数 */
    // 除了第一个外，每个子线程恢复的块数量，当条带所包括的块数量不能被线程数整除时，第一个线程处理更多的编译码块
    size_t tblocks = data.getStripeBlocks() / threads; // 下标不为0的线程的编译码块数
    size_t t0blocks = data.getStripeBlocks() % threads + tblocks; // 下标为0的线程的编译码块数
    size_t off_blocks = 0; // 下标为i的子线程处理的起始块的块偏移
    for (int i = 0; i < threads; i++) {
        this->threadargs[i].threadId = i;
        // this->threadargs[i].m = data.getM();
        this->threadargs[i].valid = data.getValid();
        this->threadargs[i].nerrs_valid = data.getNerrsValid();
        // this->threadargs[i].checks = data.getChecks();
        this->threadargs[i].repeat_time = REPEAT_TIME;
        // this->threadargs[i].gen_matrix = data.getGenMatrix();
        this->threadargs[i].g_tbls = this->recover_g_tbls;
        this->threadargs[i].blocks = (i == 0) ? t0blocks : tblocks;
        this->threadargs[i].block_size = data.getBlockSize();
        for (int j = 0; j < data.getValid()+data.getNerrsValid(); j++) {
            this->threadargs[i].buffs[j] = this->recover_stripe[j] + off_blocks * data.getBlockSize();
        }
        off_blocks += this->threadargs[i].blocks;
    }
    return true;
}

/* 子线程数据恢复函数，使用任意k个未失效的数据条带，恢复nerrs_valid个有效数据条带 */
void *Encoder::recover_thread_handle(void *args) {
    threadData* data = (threadData*)args;
    unsigned threadId = data->threadId;
    u8* recov_bak[MAX_STRIPES];
    for (int i = 0; i < data->valid+data->nerrs_valid; i++) {
        recov_bak[i] = data->buffs[i];
    }
    int iter = 0;
    while (iter++ < data->repeat_time) {
        // ec_encode_data(len, k, m - k, g_tbls, buffs, &buffs[k]);
        // break;
        for (size_t i = 0; i < data->blocks; i++) {
            ec_encode_data(data->block_size, data->valid, data->nerrs_valid, data->g_tbls, recov_bak, &recov_bak[data->valid]);
            for (int j = 0; j < data->valid+data->nerrs_valid; j++) {
                recov_bak[j] += data->block_size;
            }
        }
        if (REPEAT_TIME > 1) {
            for (int i = 0; i < data->valid+data->nerrs_valid; i++) {
                recov_bak[i] = data->buffs[i];
            }
        }
    }
    return NULL;
}

/* 数据恢复 */
bool Encoder::recover_perf(Data data, unsigned int threads) {
    if (!data.recoveralble()) {
        cout << "data has too much errors, cannot be recovered";
        return false;
    }
    cout << "start recover" << endl;
    double totaltime;
    // single thread
    if (threads < 1) {
        /* threads赋值为0时不创建子线程 */
        if (!initRecovThreadData(data, 1))
            return false;
        // encode
        tic(0);
        recover_thread_handle((void*)&(this->threadargs[0]));
        totaltime = toc(0);
        /* 将有效数据条带中的数据写入有效数据缓冲区data */
        data.dumpData();
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
        if (!initRecovThreadData(data, threads))
            return false;
        // decode
        tic(0);
        for (int i = 0; i < threads; i++) {
            pthread_create(&pid[i], NULL, recover_thread_handle, (void*)&(this->threadargs[i]));
        }
        for (int i = 0; i < threads; i++) {
            pthread_join(pid[i], &ret[i]);
        }
        totaltime = toc(0);
        /* 将有效数据条带中的数据写入有效数据缓冲区data */
        data.dumpData();
    }
    cout << "recovery ended, bandwidth "
         << data.getEncodeDataSizeMB() + data.getNerrsValidDataSizeMB()
         << "MB in "
         << totaltime
         << "secs" << endl;
    print_throughtput(data.getEncodeDataSize()+data.getNerrsValidDataSize(), totaltime, "erasure_code_encode");
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