cmake_minimum_required(VERSION 3.0)

project(
  RGK
  VERSION 1.0
  LANGUAGES CXX
  )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(TBB REQUIRED)
find_package(OpenGL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW glfw3)
find_package(GLEW)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/external/include
  ${GLEW_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIRS}
  ${GLM_INCLUDE_DIRS}
  )

add_definitions(
	-DTW_STATIC
	-DTW_NO_LIB_PRAGMA
	-DTW_NO_DIRECT3D
	-DGLEW_STATIC
	-D_CRT_SECURE_NO_WARNINGS

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
  ${GLFW_LIBRARIES}
  ${GLEW_LIBRARY}
  ${OPENGL_LIBRARY}
  pthread
  )
