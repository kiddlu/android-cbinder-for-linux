cmake_minimum_required(VERSION 2.8)
project (cbinder)

add_definitions (
   -O3
   -Wall -Wno-unused-parameter
)

set(CBINDER_SOURCES
 ${PROJECT_SOURCE_DIR}/lib/binder_hal.c
 
 ${PROJECT_SOURCE_DIR}/lib/binder_io.c
 
 ${PROJECT_SOURCE_DIR}/lib/binder_ipc.c
)

#static link
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

include_directories (${PROJECT_SOURCE_DIR}/include)

add_library(cbinder STATIC ${CBINDER_SOURCES})

add_executable(svc_mgr  ${PROJECT_SOURCE_DIR}/svc_mgr/svc_mgr.c)
target_link_libraries(svc_mgr cbinder pthread)

add_executable(fd_client  ${PROJECT_SOURCE_DIR}/test/fd_client.c)
target_link_libraries(fd_client cbinder pthread)

add_executable(fd_service  ${PROJECT_SOURCE_DIR}/test/fd_service.c)
target_link_libraries(fd_service cbinder pthread)

