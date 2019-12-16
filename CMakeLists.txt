cmake_minimum_required(VERSION 3.0)
project (jsonparser)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/bin)

set (SOURCES
  jsonparser.cpp
  main.cpp
  ${INCLUDE_DIRECTORIES}
)

add_executable(jsonparser ${SOURCES})