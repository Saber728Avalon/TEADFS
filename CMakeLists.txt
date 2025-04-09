project(TEADFS 
    LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.10...4.0)

# ---------------------------------------------------------------------------------------
# Start myproject project
# ---------------------------------------------------------------------------------------
set(TEST_VERSION "1.0.0")

message(STATUS "Configuring ${PROJECT_NAME}, version:${TEST_VERSION}")

# ---------------------------------------------------------------------------------------
# Set default build to release
# ---------------------------------------------------------------------------------------
# set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Debug" FORCE)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()
message(STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# -fno-exceptions -fno-rtti
set(TEST_COMPILE_FLAGS_RELEASE "-O3 -DNDEBUG -flto -fdata-sections -ffunction-sections")
set(TEST_LINK_FLAGS_RELEASE "-O3 -flto -Wl,--gc-sections -Wl,-s -Wl,-Bsymbolic -Wl,--exclude-libs,ALL")
set(CMAKE_C_FLAGS_RELEASE ${TEST_COMPILE_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS_RELEASE ${TEST_COMPILE_FLAGS_RELEASE})
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE ${TEST_LINK_FLAGS_RELEASE})
#指定安装路径的前缀 如下设置后：安装后的文件将被放置在/usr/local/bin（可执行文件）、/usr/local/lib（库文件）、/usr/local/include（头文件）目录下
set(CMAKE_INSTALL_PREFIX /usr/local)

# message (STATUS "TEST_COMPILE_FLAGS_RELEASE:${TEST_COMPILE_FLAGS_RELEASE}")
# message (STATUS "TEST_LINK_FLAGS_RELEASE:${TEST_LINK_FLAGS_RELEASE}")
message (STATUS "CMAKE_C_FLAGS_RELEASE:${CMAKE_C_FLAGS_RELEASE}")
message (STATUS "CMAKE_CXX_FLAGS_RELEASE:${CMAKE_CXX_FLAGS_RELEASE}")
message (STATUS "CMAKE_SHARED_LINKER_FLAGS_RELEASE:${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

message (STATUS "CMAKE_C_FLAGS_DEBUG:${CMAKE_C_FLAGS_DEBUG}")
message (STATUS "CMAKE_CXX_FLAGS_DEBUG:${CMAKE_CXX_FLAGS_DEBUG}")
message (STATUS "CMAKE_C_FLAGS_MINSIZEREL:${CMAKE_C_FLAGS_MINSIZEREL}")
message (STATUS "CMAKE_CXX_FLAGS_MINSIZEREL:${CMAKE_CXX_FLAGS_MINSIZEREL}")
message (STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO:${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message (STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO:${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

#下面几个变量在交叉编译时：可以根据不同的目标系统进行条件判断或设置特定的编译选项和路径
#目标系统信息
message (STATUS "CMAKE_SYSTEM:${CMAKE_SYSTEM}")
#目标系统名字
message (STATUS "CMAKE_SYSTEM_NAME:${CMAKE_SYSTEM_NAME}")
#目标系统的处理器类型
message (STATUS "CMAKE_SYSTEM_PROCESSOR:${CMAKE_SYSTEM_PROCESSOR}")
#目标系统使用的C++编译器 如：GNU Clang MSVC
message (STATUS "CMAKE_CXX_COMPILER_ID:${CMAKE_CXX_COMPILER_ID}")
#当前编译环境下指针类型（通常是void*）的大小（以字节为单位）
message (STATUS "CMAKE_SIZEOF_VOID_P:${CMAKE_SIZEOF_VOID_P}")
#当前实际所在系统的处理器类型
message (STATUS "CMAKE_HOST_SYSTEM_PROCESSOR:${CMAKE_HOST_SYSTEM_PROCESSOR}")
message (STATUS "ANDROID:${ANDROID}")

#CMAKE_SYSTEM_PROCESSOR 目标系统处理器类型。交叉编译时，编译好的库只能在CMAKE_SYSTEM_PROCESSOR平台上运行
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
    message("x86 32")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message("x86_64 64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message("arm 64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7-a")
    message("arm 32")
endif()

message (STATUS "CMAKE_INSTALL_PREFIX:${CMAKE_INSTALL_PREFIX}")

#增加设置项  在执行cmake时，可以通过 -DDEBUG_LOG=ON来开启此flag
option(BUILD_TEST "Build test" ON)

message (STATUS "BUILD_TEST:${BUILD_TEST}")

#设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)


add_subdirectory(TEADFS)
add_subdirectory(TEADFS-utils)

if (BUILD_TEST)
    message(STATUS "Build tests")
    add_subdirectory(test)
endif()



