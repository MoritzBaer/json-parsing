cmake_minimum_required(VERSION 3.28)

project(JSON_PARSING)

set(CMAKE_CXX_STANDARD 23)

add_library(JsonParsing INTERFACE)
target_include_directories(JsonParsing INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib")

include(CheckSourceCompiles)
check_source_compiles(CXX "
#include <iostream>
#define TEST(lbl, ...) int lbl[] = {5 __VA_OPT__(,) __VA_ARGS__}

int main() {
    TEST(t1);
    TEST(t2, 2, 3);
}
"    VA_OPT)

if(NOT VA_OPT)
    if(MSVC)
        target_compile_options(JsonParsing INTERFACE /Zc:preprocessor)
    else()
        message("Macro __VA_OPT__ undefined (needed by json-parsing).")
    endif()
endif()

add_executable(JSONDemo "demo.cpp")
target_link_libraries(JSONDemo PRIVATE JsonParsing)
