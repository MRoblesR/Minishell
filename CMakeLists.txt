cmake_minimum_required(VERSION 3.17)
project(minishell C)

set(CMAKE_C_STANDARD 99)

add_executable(minishell test.c )

target_link_libraries(minishell LINK_PUBLIC ${libparser.a})

