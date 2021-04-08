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
    // 填充可以完全填充的条带，使用data所在的内存空间
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
    for (int i=this->valid; i<this->m; i++) {
        if (posix_memalign((void**)&this->stripe[i], 64, this->stripe_size)) {
            cout << "alloc error: Fail" << endl;
            exit(1);
        }
        // memset(this->stripe[i], 0, this->stripe_size);
    }
    if (posix_memalign((void**)&this->data_check, 64, this->stripe_size * this->checks)) {
        cout << "alloc error: Fail" << endl;
        exit(1);
    }
}

/* 将条带中的有效数据写入数据缓冲区data */
void Data::dumpData() {
    for (int i=0; i<this->valid; i++) {
        memcpy(this->data + i * this->stripe_size, this->stripe[i], this->stripe_size);
    }
}

/* 将校验条带中的数据写入校验数据缓冲区data_checks */
void Data::dumpDataChecks() {
    for (int i=0; i<this->checks; i++) {
        memcpy(this->data_check + i * this->stripe_size, this->stripe[this->valid+i], this->stripe_size);
    }
}