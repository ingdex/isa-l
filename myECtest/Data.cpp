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
/* 比较目前数据是否和备份的数据相同，用于判断译码结果是否正确 */
bool Data::isSame() {
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

/* 获取一个条带中的数据块数量 */
size_t Data::getStripeBlocks() {
    return this->stripe_blocks;
}

/* 获取数据条带的首地址 */
u8 *Data::getStripe(int index) {
    if (index < 0 || index >= MAX_STRIPES) {
        cout << "Data::getStripe(): index out of range" << endl;
        return NULL;
    }
    return this->stripe[index];
}

/* 获取条带中有效数据+校验数据总大小，单位为Byte，data_size乘以repeat_time除以10^6 */
size_t Data::getEncodeDataSize() {
    return this->stripe_size * this->m * REPEAT_TIME;
}

/* 获取条带中有效数据+校验数据总大小，单位为MB，data_size乘以repeat_time除以10^6 */
size_t Data::getEncodeDataSizeMB() {
    return this->stripe_size * this->m * REPEAT_TIME / 1024 / 1024;
}

/* 获取条带中失效的有效数据总大小，单位为Byte，data_size乘以repeat_time */
size_t Data::getNerrsValidDataSize() {
    return this->stripe_size * this->nerrs_valid * REPEAT_TIME;
}

/* 获取条带中失效的有效数据总大小，单位为MB，data_size乘以repeat_time除以10^6 */
size_t Data::getNerrsValidDataSizeMB() {
    return this->stripe_size * this->nerrs_valid * REPEAT_TIME / 1024 / 1024;
}

/* 将data中的数据按照编译码块大小与条带数量对齐，分割为条带，存储到buffs中，同时为校验数据申请存储空间，首地址记录在buffs中 */
void Data::alignData()
{
    /* 将要编码的数据分配给threads个threadData结构体 */
    if (this->type != SINGLESOURCE) {
        cout << "type for data != 0" << endl;
        exit(0);
    }
    // size_t data_size = data.getDataSize();
    // 对齐前数据块的数量
    size_t total_blocks_before = this->data_size / this->block_size;
    if (this->data_size % this->block_size)
        total_blocks_before = total_blocks_before + 1;
    this->stripe_blocks = (total_blocks_before + this->valid - 1) / this->valid; 
    this->stripe_size = this->stripe_blocks * this->block_size;
    // // 条带中数据按照块大小对齐
	// if (this->stripe_size % this->block_size)
	//     this->stripe_size += (this->block_size - this->stripe_size % this->block_size);
    // // 对齐后单个条带数据块数量
    // size_t stripe_blocks_after = this->stripe_size / this->block_size;
    // this->stripe_blocks = stripe_blocks_after;
    /* 缺少的数据补零填充 */
    // 计算有效数据可以完全填充的条带数量
    size_t fullfilled_stripes = total_blocks_before / this->stripe_blocks;
    if (this->data_size % this->block_size) // 有效数据最后一块末尾需要被补零
        fullfilled_stripes = (total_blocks_before - 1) / this->stripe_blocks;
    this->stripe[0] = this->data;
    // 填充可以完全填充的条带，使用data所在的内存空间，之后dumpData时此部分数据不需要复制
    for (int i = 1; i < fullfilled_stripes; i++) {
        this->stripe[i] = this->stripe[i - 1] + this->stripe_size;
    }
    // 对于不能完全填充的条带，申请新的内存空间并补零，补零不是必要的
    for (int i = fullfilled_stripes; i < this->valid; i++) {
        if (posix_memalign((void**)&this->stripe[i], 64, this->stripe_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
        // memset(this->stripe[i], 0, this->stripe_size);
    }
    // 有效数据最后一块的数据需要补零时，其中的有效数据当前还未被写入条带中，需要单独写入新申请的第一个block中
    size_t piece_size = data_size - fullfilled_stripes * stripe_size;
    memcpy(this->stripe[fullfilled_stripes], this->data + stripe_size * fullfilled_stripes, piece_size);
    // 为校验数据申请内存空间
    // for (int i=this->valid; i<this->m; i++) {
    //     if (posix_memalign((void**)&this->stripe[i], 64, this->stripe_size)) {
    //         cout << "alloc error: Fail" << endl;
    //         exit(1);
    //     }
    //     // memset(this->stripe[i], 0, this->stripe_size);
    // }
    if (posix_memalign((void**)&this->data_check, 64, this->stripe_size * this->checks)) {
        cout << "alloc error: Fail" << endl;
        exit(1);
    }
    // 将校验数据地址赋值给校验条带，共用内存空间
    for (int i=0; i<this->checks; i++) {
        this->stripe[i + this->valid] = this->data_check + this->stripe_size * i;
        // memset(this->stripe[i], 0, this->stripe_size);
    }
}

/* 将条带中的有效数据写入数据缓冲区data */
void Data::dumpData() {
    size_t fullfilled_stripe = this->data_size/this->stripe_size;
    // 将完全填充的数据条带数据写入data缓冲区，stripe数组中记录的地址与data相同，故此步骤不需要
    // for (int i=0; i<fullfilled_stripe; i++) {
    //     memcpy(this->data + i * this->stripe_size, this->stripe[i], this->stripe_size);
    // }
    // 将未完全填充的数据条带中的有效数据写入data缓冲区末尾
    size_t piece = this->data_size % this->stripe_size;
    if (piece)
        memcpy(this->data + fullfilled_stripe * this->stripe_size, this->stripe[fullfilled_stripe] , piece);
}

/* 将校验条带中的数据写入校验数据缓冲区data_checks */
void Data::dumpDataChecks() {
    for (int i=0; i<this->checks; i++) {
        memcpy(this->data_check + i * this->stripe_size, this->stripe[this->valid+i], this->stripe_size);
    }
}

/** 设置错误条带
 * @return nerrs大于校验数据条带数量时数据不可恢复，返回false，否则返回true
 */
bool Data::setError(int nerrs, int *stripe_err_list) {
    if (nerrs < 0) {
        cout << "Data::setError: cannot set nerrs = " << nerrs << endl;
        return false;
    }
    this->nerrs = nerrs;
    this->nerrs_valid = 0;
    memset(this->stripe_in_error, 0, MAX_STRIPES);
    for (int i=0; i<nerrs; i++) {
        int stripe_err_index = stripe_err_list[i];
        if (stripe_err_index < 0 || stripe_err_index >= this->m
            || this->stripe_in_error[stripe_err_index]) {
            // stripe_err_index不在合法范围或者已经被记录过
            this->nerrs = 0;
            this->nerrs_valid = 0;
            memset(this->stripe_in_error, 0, MAX_STRIPES);
        }
        this->stripe_in_error[stripe_err_index] = 1;
        if (stripe_err_index < this->valid) {
            this->nerrs_valid++;
        }
    }
    return true;
}

/* 设置错误条带，并将对应条带中的数据随机改写 */
bool Data::setErrorAndDestroy(int nerrs, int *stripe_err_list) {
    if (!setError(nerrs, stripe_err_list))
        return false;
    for (int i=0; i<nerrs; i++) {
        u8 *stripe = this->stripe[stripe_err_list[i]];
        // size_t pos = rand() % this->stripe_size;
        size_t pos = 1;
        cout << "stripe[" << (unsigned int)stripe_err_list[i] << "]: stripe[" << pos << "] from " << (int)stripe[pos];
        stripe[pos] = 1;
        cout << " change to " << (unsigned int)stripe[pos] << endl;
        if (this->stripe_size * stripe_err_list[i] + pos < this->data_size) {
            cout << "data[" << (unsigned int)(this->stripe_size * stripe_err_list[i] + pos) << "] from " << (unsigned int)data[this->stripe_size * stripe_err_list[i] + pos] << " change to " << (unsigned int)stripe[pos] << endl;
            data[this->stripe_size * stripe_err_list[i] + pos] = stripe[pos];
        }
    }
    return true;
}

/* 判断下标为index的条带数据是否失效，失效返回true，否则返回true */
bool Data::isError(unsigned int index) {
    return this->stripe_in_error[index] ? true : false;
}

/* 获取生成矩阵中的第i行第j列的数字 */
u8 Data::getGenMatrix(unsigned int i, unsigned int j) {
    return this->gen_matrix[i * this->valid + j];
}

/* 获取失效数据条带的数量 */
unsigned int Data::getNerrs() {
    return this->nerrs;
}

/* 获取失效的有效数据条带的数量 */
unsigned int Data::getNerrsValid() {
    return this->nerrs_valid;
}

/* 将data缓冲区中的数据备份到data_bak缓冲区 */
void Data::dataBak() {
    memcpy(this->data_bak, this->data, this->data_size);
    return;
}

/* 判断数据是否可恢复，可恢复返回true，不可恢复返回false */
bool Data::recoveralble() {
    return this->checks >= this->nerrs;
}