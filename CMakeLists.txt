cmake_minimum_required(VERSION 3.12)

project(tiny_bnf)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -pedantic)
# add_compile_options(-Wall -Wextra -pedantic -fsanitize=address)
# add_link_options(-fsanitize=address)

include_directories(./include)

add_executable(calc examples/calculator/calc.cpp)
add_executable(calc_test examples/calculator/test.cpp)