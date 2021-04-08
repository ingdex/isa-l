#ifndef DTAPRODUCER_HPP
#define DTAPRODUCER_HPP

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
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
    // u8 *datas[];          // 条带状的待编码数据（模拟多个文件一起编码）
    // size_t datas_size[];    // 多个条带的大小
    // u8 *datas_check[];    // 条带状的校验数据
    int m;                  // 有效数据+校验数据数量
    int valid;              // 有效数据数量
    int checks;             // 校验数据数量
    u8 *buffs[MAX_STRIPES]; // 编译码和校验条带
    size_t block_size;      // 编译码块大小
    u8 *gen_matrix;         // 生成矩阵
    u8 *g_tbls;             // 辅助编码表格，详见isa-l doc 
    // threadData data;
public:
    Data(size_t data_size, int valid, int checks, size_t block_size) {
        this->type = SINGLESOURCE;
        this->data_size = data_size;
        this->m = valid + checks;
        this->valid = valid;
        this->checks = checks;
        this->block_size = block_size;
        if (posix_memalign((void **)&data, 64, this->data_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
        
        memset(data, 0, this->data_size);
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
    /* 获取待编码数据的逻辑地址 */
    u8 *getBuff();
    /* 获取生成矩阵 */
    u8 *getGenMatrix();
    u8 *getG_tbls();
};

#endif