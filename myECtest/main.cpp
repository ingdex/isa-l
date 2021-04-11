#include <iostream>
#include "Encoder.hpp"
#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)
#define TEST_LEN  ((size_t)1 * GB)	// 有效数据大小

using namespace std;

int main() {
    Data data(TEST_LEN, 18, 2, 4 * KB);
    int src_err_list[] = {2, 4};
    // data.initRandom();
    Encoder encoder;
    encoder.encode_perf(data, 0);
    data.dataBak();
    cout << "berfore setErrorAndDestroy data is" << (data.isSame()?" ":" not ") << "the same" << endl;
    data.setErrorAndDestroy(2, src_err_list);
    cout << "berfore recovery data is" << (data.isSame()?" ":" not ") << "the same" << endl;
    encoder.recover_perf(data, 0);
    cout << "recovery completed, data check " << (data.isSame()?"PASSED!":"FAILED!") << endl;
    return 0;
}