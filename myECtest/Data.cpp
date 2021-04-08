#include <string.h>
#include "Data.hpp"

using namespace std;

typedef unsigned char u8;

/* 用随机数初始化Data中的数据 */
void Data::initRandom() {
    for (size_t i=0; i<this->data_size; i++)
        ((u8 *)this->data)[i] = rand();
    memcpy(this->data_bak, this->data, this->data_size);
}

bool Data::same() {
    return memcmp(this->data, this->data_bak, this->data_size) ? false : true;
}

int Data::getM() {
    return this->m;
}

int Data::getValid() {
    return this->valid;
}

int Data::getChecks() {
    return this->checks;
}

int Data::getBlockSize() {
    return this->block_size;
}

unsigned char *Data::getGenMatrix() {
    return this->gen_matrix;
}

unsigned char *Data::getG_tbls() {
    return this->g_tbls;
}

int Data::getType() {
    return this->type;
}

size_t Data::getDataSize() {
    return this->data_size;
}

/* 获取编码总数据长度，单位为MB，data_size乘以repeat_time除以10^6 */
size_t Data::getDataSizeMB() {
    return this->data_size / 1024 /1024;
}

u8 *Data::getBuff() {
    return this->data;
}

