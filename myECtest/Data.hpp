#ifndef DTAPRODUCER_HPP
#define DTAPRODUCER_HPP

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include "erasure_code.h"
#include "config.hpp"

typedef unsigned char u8;

#define SINGLESOURCE    0   // 数据来自同一源文件
#define MULTISOURCE     1   // 数据来自多个源文件

using namespace std;

class Data {
private:
    int type;               // 仿真模式，待编码数据来自一个文件还是多个文件，0为来自一个文件，data中数据有效，datas中无效
    u8 *data;				// 待编码数据（模拟对一个文件进行编码）
    u8 *data_bak;         // 待编码数据备份，用于判断译码结果是否正确
    u8 *data_check;       // 校验数据
	size_t data_size;	    // 待编码有效数据长度，单位Byte
	size_t stripe_size;	    // 一份待编码数据被分为多个条带，一个条带的长度
    size_t stripe_blocks;   // 一个条带中的数据块数量
    // u8 *datas[];          // 条带状的待编码数据（模拟多个文件一起编码）
    // size_t datas_size[];    // 多个条带的大小
    // u8 *datas_check[];    // 条带状的校验数据
    int m;                  // 有效数据+校验数据数量
    int valid;              // 有效数据数量
    int checks;             // 校验数据数量
    u8 *stripe[MAX_STRIPES]; // 编译码和校验条带
    size_t block_size;      // 编译码块大小
    u8 *gen_matrix;         // 生成矩阵
    u8 *g_tbls;             // 辅助编码表格，详见isa-l doc 
    // threadData data;
    /* 将data中的数据按照编译码块大小与条带数量对齐，分割为条带，存储到buffs中，同时为校验数据申请存储空间，首地址记录在buffs中 */
    void alignData();
public:
    Data(size_t data_size, int valid, int checks, size_t block_size) {
        this->type = SINGLESOURCE;
        this->data_size = data_size;
        if (posix_memalign((void **)&data, 64, this->data_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
        // memset(data, 0, this->data_size);
        this->m = valid + checks;
        this->valid = valid;
        this->checks = checks;
        this->block_size = block_size;
        /* 构造生成矩阵 */
        this->gen_matrix = new u8[this->m * this->valid];
        this->g_tbls = new u8[32 * this->valid * this->checks];
        gf_gen_rs_matrix(this->gen_matrix, this->m, this->valid);
        /* 构造辅助表 */
        ec_init_tables(this->valid, this->checks, this->gen_matrix, this->g_tbls);
        // 数据放入条带并对齐补零，同时为校验数据申请内存空间
        alignData();
    }
    Data(size_t data_size) {
        this->type = SINGLESOURCE;
        this->data_size = data_size;
        if (posix_memalign((void **)&data, 64, this->data_size) || posix_memalign((void **)&data_bak, 64, this->data_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
        // memset(data, 0, this->data_size);
        // memset(data_bak, 0, this->data_size);
    }
    /* 用随机数初始化Data中的数据 */
    void initRandom();
    /* 比较目前数据是否和初始化时相同，用于判断译码结果是否正确 */
    bool same();
    /* 获取待编码数据长度 */
    size_t getDataSize();
    /* 获取数据长度，单位为MB */
    size_t getDataSizeMB();
    /* 获取条带中有效数据+校验数据总大小，单位为Byte，data_size乘以repeat_time */
    size_t getEncodeDataSize();
    /* 获取条带中有效数据+校验数据总大小，单位为MB，data_size乘以repeat_time除以10^6 */
    size_t getEncodeDataSizeMB();
    /* 获取一个条带中的数据块数量 */
    size_t getStripeBlocks();
    /* 获取校验数据和待编码数据总条带数 */
    int getM();
    /* 获取有效数据条带数 */
    int getValid();
    /* 获取校验数据条带数 */
    int getChecks();
    /* 获取编译码块大小 */
    int getBlockSize();
    /* 获取编码数据类型，来自一个文件返回0，否则返回1 */
    int getType();
    /* 获取待编码数据的首地址 */
    u8 *getBuff();
    /* 获取数据条带的首地址 */
    u8 *getStripe(int index);
    /* 获取生成矩阵 */
    u8 *getGenMatrix();
    /* 获取编译码复制表格 */
    u8 *getG_tbls();
    /* 将条带中的有效数据写入数据缓冲区data */
    void dumpData();
    /* 将校验条带中的数据写入校验数据缓冲区data_checks */
    void dumpDataChecks();
};

#endif