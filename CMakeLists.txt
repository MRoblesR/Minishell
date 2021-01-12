cmake_minimum_required(VERSION 3.17)
project(minishell C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -static")
set(SOURCE_FILES test.c)

add_Library( myLibrary STATIC IMPORTED )

set_property ( TARGET myLibrary PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/parser/libparser.a )
include_directories( ${CMAKE_SOURCE_DIR}/parser)

add_executable( minishell ${SOURCE_FILES} )
target_link_libraries( minishell myLibrary )