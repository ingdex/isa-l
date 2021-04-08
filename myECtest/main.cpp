#include "Encoder.hpp"
#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)
#define TEST_LEN  ((size_t)64 * GB)	// 有效数据大小

int main() {
    Data data(TEST_LEN, 18, 2, 4 * KB);
    // data.initRandom();
    Encoder encoder;
    encoder.encode_perf(data, 32);
    return 0;
}