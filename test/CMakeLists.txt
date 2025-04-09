project ( test 
		LANGUAGES C CXX )
cmake_minimum_required ( VERSION 2.8 )


include_directories(../include)

#链接目录
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/../lib)

#设置库目录
set(CMAKE_SKIP_BUILD_RPATH FALSE)

set(TEST_HDRS
	test.h
)

# 添加文件
set(TEST_SRC 
	test.cpp 
)

add_executable (${PROJECT_NAME} ${TEST_HDRS} ${TEST_SRC})
target_link_libraries(${PROJECT_NAME} pthread TEADFS-utils)
