cmake_minimum_required(VERSION 3.28)

project(JSON_PARSING)

set(CMAKE_CXX_STANDARD 23)

add_compile_options(/Zc:preprocessor)

add_executable(TestApp "main.cpp")