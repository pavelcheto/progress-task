cmake_minimum_required(VERSION 3.10)
project(progress_task)

find_package(CURL REQUIRED)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "src/*.cpp")

add_executable(progress_task ${SOURCES})

target_compile_options(progress_task PRIVATE -O3 -Wall -Werror)

target_include_directories(progress_task PRIVATE ${CMAKE_SOURCE_DIR}/third-party)

target_link_libraries(progress_task PRIVATE CURL::libcurl)