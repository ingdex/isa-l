#ifndef ENCODER_HPP
#define ENCODER_HPP
#include <stdlib.h>
#include <stdio.h>
#include "erasure_code.h"
#include "Data.hpp"
#include "config.hpp"

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


typedef struct _threadData {
    unsigned threadId;
    int m;
    int valid;
    int checks;
    size_t block_size;     // 
    size_t blocks;
    int repeat_time;
    u8 *gen_matrix;
    u8 *g_tbls;
    u8 *buffs[MAX_STRIPES];
    u8 *recov[MAX_STRIPES];
    // u8 **temp_buffs;
    // threadData(){};
    // threadData(unsigned threadId, int m, int valid, int checks, size_t len, int repeat_time, u8 *gen_matrix, u8 *g_tbls)
    //      : threadId(threadId), m(m), valid(valid), checks(checks), len(len), repeat_time(repeat_time), gen_matrix(gen_matrix), g_tbls(g_tbls) {
    //     this->buffs = NULL;
    //     this->recov = NULL;
    //     // this->temp_buffs = NULL;
    // };
    // threadData(Data data, int threadId) {
    //     this->threadId = threadId;
    //     this->m = data.getM();
    //     this->valid = data.getValid();
    //     this->checks = data.getChecks();
    //     this->len = data.ge
    //     this->buffs = NULL;
    //     this->recov = NULL;
    //     // this->temp_buffs = NULL;
    // };
} threadData;

class Encoder {
private:
    int m;                      // 有效数据+校验数据数量
    int valid;                  // 有效数据数量
    int checks;                 // 校验数据数量
    size_t block_size;          // 编译码块大小
    u8 *gen_matrix;             // 生成矩阵
    u8 *g_tbls;                 // 复制表格，详见isa-l doc
    u8 *buffs[MAX_STRIPES];   // valid组为有效数据+checks组校验数据
    size_t stripe_size;         // 条带大小
    size_t stripe_blocks;       // 一个条带中编译码块的数量
    u8 src_in_err[MAX_STRIPES];
    u8 src_err_list[MAX_STRIPES]; 
    threadData threadargs[MAX_THREADS];
    /* 根据threads用data初始化线程参数 */
    void initThreadData(Data data, unsigned int threads);
    /* 将data中的数据按照编译码块大小与条带数量对齐，分割为条带，存储到buffs中，同时为校验数据申请存储空间，首地址记录在buffs中 */
    void alignData(Data data);
    /* 子线程编码函数 */
    static void *encode_thread_handle(void *args);
    /* 输出吞吐率 */
    void print_throughtput(size_t data_size, double time, string info);
    /* 计时器 */
    // 开始计时
    static void tic(int i);
    // 结束计时
    static double toc(int i);
public:
    Encoder(int valid, int checks, size_t block_size) : m(valid + checks), valid(valid), checks(checks), block_size(block_size) {
        /* 构造生成矩阵 */
        gen_matrix = new u8[m * valid];
        g_tbls = new u8[32 * valid * checks];
        gf_gen_rs_matrix(gen_matrix, m, valid);
        /* 构造辅助表 */
        ec_init_tables(valid, checks, gen_matrix, g_tbls);
    };
    void encode(Data data, unsigned int threads);
    void encode_perf(Data data, unsigned int threads);
    /* 获取对齐后的编码总数据长度，单位为Byte，data_size乘以repeat_time */
    size_t getEncodeDataSize();
    /* 获取对齐后的编码总数据长度，单位为MB，data_size乘以repeat_time除以10^6 */
    size_t getEncodeDataSizeMB();
};

#endif