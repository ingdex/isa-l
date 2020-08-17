# Raid6性能测试程序
## 测试程序还比较简单，通过多个线程对同一地址空间的数据反复校验测试多核性能

## 编译
需要先安装isa-l库
https://github.com/intel/isa-l
使用自带的CMakeLists.txt即可
    mkdir build
    cd build
    cmake ..
    make
    ./Demo