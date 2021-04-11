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
    unsigned int nerrs;              // 错误条带数量
    unsigned int nerrs_valid;        // 有效数据条带中的错误条带数量
    u8 *gen_matrix;
    u8 *g_tbls;
    u8 *buffs[MAX_STRIPES];
    // u8 *recov[MAX_STRIPES];
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
    u8 *b_matrix;   // 去掉错误行之后的生成矩阵
    u8 *inverse_matrix; // b_matrix的逆矩阵
    u8 *recover_matrix; // 用于数据恢复的矩阵
    u8 *recover_g_tbls; // 恢复过程使用的辅助表格
    u8 *recover_stripe[MAX_STRIPES];    // 数据恢复时使用的条带
    threadData threadargs[MAX_THREADS];
    /* 根据threads用data初始化线程参数 */
    void initEncodeThreadData(Data data, unsigned int threads);
    /* 根据threads用data初始化数据恢复所需的矩阵以及线程参数 */
    bool initRecovThreadData(Data data, unsigned int threads);
    /* 子线程编码函数 */
    static void *encode_thread_handle(void *args);
    /* 子线程数据恢复函数 */
    static void *recover_thread_handle(void *args);
    /* 输出吞吐率 */
    void print_throughtput(size_t data_size, double time, string info);
    /* 计时器 */
    // 开始计时
    static void tic(int i);
    // 结束计时
    static double toc(int i);
public:
    Encoder() {
        b_matrix = NULL;
        inverse_matrix = NULL;
        recover_g_tbls = NULL;
        memset(recover_stripe, 0, MAX_STRIPES);
    };
    void encode(Data data, unsigned int threads);
    void encode_perf(Data data, unsigned int threads);
    /* 数据恢复，根据EC数据恢复原理，只能恢复有效数据条带，失效的校验数据需要重新编码得到 */
    bool recover_perf(Data data, unsigned int threads);
};

#endif