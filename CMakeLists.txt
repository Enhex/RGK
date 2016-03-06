cmake_minimum_required(VERSION 3.0)

project(
  RGK
  VERSION 1.0
  LANGUAGES CXX
  )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(TBB)
message(${TBB_tbb_LIBRARY})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/external/include
  )

add_definitions(
  -std=c++14
  -Wall
  -Wextra
  -Werror
  -g
  )

file(GLOB SOURCES
  ./*.cpp
  )

SET(EXTLIBS
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libembree.a
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libembree_avx.a
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libembree_avx2.a
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libembree_sse42.a
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libsimd.a
  ${CMAKE_CURRENT_SOURCE_DIR}/external/liblexers.a
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libsys.a
  )

add_executable(
  RGK
  ${SOURCES}
  )

target_link_libraries(
  RGK
  ${EXTLIBS}
  ${TBB_tbb_LIBRARY}
  pthread
  )
