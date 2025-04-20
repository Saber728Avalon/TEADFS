project ( TEADFS-utils 
		LANGUAGES C CXX )
cmake_minimum_required ( VERSION 3.10...4.0 )



include_directories(../include
	../TEADFS/src
	)

#设置库目录
set(CMAKE_SKIP_BUILD_RPATH FALSE)

set(TEADFS_utils_HDRS
	 ../include/lib_tead_fs.h
	 thread_pool.h
	 pevents.h
	 netlink.h
	 request.h
)

# 添加文件
set(TEADFS_utils_SRC 
	main.cpp
	thread_pool.cpp
	pevents.cpp
	netlink.cpp
	request.cpp
)

#编译lib
add_library(${PROJECT_NAME} SHARED ${TEADFS_utils_HDRS} ${TEADFS_utils_SRC})
