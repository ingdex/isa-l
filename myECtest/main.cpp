#include <iostream>
#include "Encoder.hpp"
#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)
#define TEST_LEN  ((size_t)4 * GB)	// 有效数据大小

using namespace std;

// 有效数据条带数量
#define VALID           18
// 校验数据条带数量
#define CHECK           2
// 数据块大小，单位为KB
#define BLOCK_SIZE_KB   4
// 错误条带数量
#define SRC_ERR_COUNT   2
// 错误条带编号列表
#define SRC_ERR_LIST    {2, 4, 5}
// 编码与数据恢复的线程数
#define THREADS         16

int main() {
    Data data(TEST_LEN, VALID, CHECK, BLOCK_SIZE_KB * KB);
    int src_err_list[] = SRC_ERR_LIST;
    // data.initRandom();
    Encoder encoder;
    encoder.encode_perf(data, THREADS);
    data.dataBak();
    cout << "berfore setErrorAndDestroy data is" << (data.isSame()?" ":" not ") << "the same" << endl;
    data.setErrorAndDestroy(SRC_ERR_COUNT, src_err_list);
    cout << "berfore recovery data is" << (data.isSame()?" ":" not ") << "the same" << endl;
    encoder.recover_perf(data, THREADS);
    cout << "recovery completed, data check " << (data.isSame()?"PASSED!":"FAILED!") << endl;
    return 0;
}