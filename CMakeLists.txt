cmake_minimum_required(VERSION 3.12)

project(tiny_bnf)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -pedantic -Wno-parentheses)
#add_compile_options(-Wall -Wextra -pedantic -fsanitize=address -Wno-parentheses)
#add_link_options(-fsanitize=address)

include_directories(./src)

add_library(tiny_bnf src/tiny_bnf.cpp)

add_executable(calc examples/calculator/calc.cpp)
target_link_libraries(calc tiny_bnf)

add_executable(calc_test examples/calculator/test.cpp)
target_link_libraries(calc_test tiny_bnf)

add_executable(lang examples/lang/parser.cpp)
target_link_libraries(lang tiny_bnf)
