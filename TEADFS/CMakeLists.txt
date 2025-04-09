cmake_minimum_required(VERSION 3.10...4.0)
PROJECT(scene_storage)
 
 
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/src)
##############################
### build kernel          ####
##############################
 
set(KERNEL_DIR /usr/src/linux-headers-${CMAKE_SYSTEM_VERSION})
set(SCENE_MODULE_NAME ${CMAKE_CURRENT_BINARY_DIR}/src/TEADFS.ko)
 
add_custom_target(TEADFS ALL cp -f ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c ${CMAKE_CURRENT_BINARY_DIR}/src/
    COMMAND cp -f  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h ${CMAKE_CURRENT_BINARY_DIR}/src/
    COMMAND cp -f  ${CMAKE_CURRENT_SOURCE_DIR}/src/Makefile ${CMAKE_CURRENT_BINARY_DIR}/src/
    COMMENT "Copy kernel module of TEADFS makefile target"
    
    COMMAND make -C ${KERNEL_DIR} M=${CMAKE_CURRENT_BINARY_DIR}/src/ modules
    COMMENT "Make kernel module of TEADFS makefile target"
 
    COMMAND rm -rf *.o *.mod.c *.symvers *.order *.makers *.mod .*.cmd .*.tmp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/
    COMMENT "Clean kernel module of TEADFS makefile target"
    )
 
## make insmod to install module ####
add_custom_target(insmod COMMAND
    COMMAND make insmod
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/
    COMMENT "Install kernel module of TEADFS makefile target")
 
 
##############################
## install module to path ####
##############################
SET(SCENE_MODULE_PATH ${CMAKE_SYSTEM_VERSION})
MESSAGE("The System Version:${SCENE_MODULE_PATH}")
install(PROGRAMS ${SCENE_MODULE_NAME} DESTINATION lib/kernel/${SCENE_MODULE_PATH})