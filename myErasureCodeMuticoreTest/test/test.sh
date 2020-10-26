#!/bin/bash
# CopyRight@ 孙磊
# 批量测试上一级目录下的EC测试程序，并统计吞吐率信息
###########################################################
# 参数
# 线程数
threads=(2 4 8 16)
# threads=(8 16)
# 每种情况测试次数
test_time=10
# 测试程序路径
name="erasure_code_perf"
path="../$name"
# 测试模式（冷数据/热数据）
pattern="warm"
# pattern="cold"
# 测试结果路径
time=$(TZ=UTC-8 date "+%Y%m%d-%H:%M")
resultPath=results/${name}_${pattern}_${time}
###########################################################
echo "测试程序: $path"          > $resultPath
echo "子线程数-吞吐率(GB/s)"    >> $resultPath
###########################################################
cd ..
make
cd test
rm ./log
for ((i=0;i<$test_time;i++));
do
    for thread in ${threads[@]}
    do
        echo $path $thread
        $path $thread | grep Throughtput | awk '{print $4}' >> log
    done
done
i=0
while read line
do
    encode_t[i]=$line
    read line
    decode_t[i]=$line
    let i++
done < log

# print results
for var in ${threads[@]}
do
    echo -e "$var\t\t\t\c" >> $resultPath
done
echo >> $resultPath

echo encode >> $resultPath
pos=0
for ((i=0;i<$test_time;i++));
do
    for ((j=0;j<${#threads[@]};j++));
    do
        echo -e "${encode_t[pos]}\t\c" >> $resultPath
        let pos++
    done
    echo >> $resultPath
done

echo >> $resultPath
echo decode >> $resultPath
pos=0
for ((i=0;i<$test_time;i++));
do
    for ((j=0;j<${#threads[@]};j++));
    do
        echo -e "${decode_t[pos]}\t\c" >> $resultPath
        let pos++
    done
    echo >> $resultPath
done

