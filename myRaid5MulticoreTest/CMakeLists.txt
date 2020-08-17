cmake_minimum_required(VERSION 3.0.0)
project(raid6 VERSION 0.1.0)

# include(CTest)
# enable_testing()

add_executable(Demo raid6.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)
# link_directories(/usr/include/isa-l) #添加动态连接库的路径
target_link_libraries(Demo /usr/lib/libisal.so)