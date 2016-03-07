cmake_minimum_required(VERSION 3.0)

project(
  RGK
  VERSION 1.0
  LANGUAGES CXX
  )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(FindPackageHandleStandardArgs)

find_package(TBB REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(assimp REQUIRED)
find_package(embree REQUIRED)

## GLM
set(GLM_FIND_REQUIRED TRUE)
find_path(
  GLM_INCLUDE_DIRS "glm/glm.hpp"
  )
find_package_handle_standard_args(
  GLM
  DEFAULT_MSG
  GLM_INCLUDE_DIRS
  )

## PNG++
find_package(PNG REQUIRED)
set(png++_FIND_REQUIRED TRUE)
find_path(
  png++_INCLUDE_DIRS
  NAMES "png++/color.hpp" "png++/png.hpp"
  )
find_package_handle_standard_args(
  png++
  DEFAULT_MSG
  png++_INCLUDE_DIRS
  )

if   (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(OPENGL_LIBRARY ${OPENGL_LIBRARY} -lGL -lGLU -lXxf86vm -lXi -lXrandr -lXinerama -lXcursor -lXext -lX11 -lrt -ldl)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

include_directories(
  ${GLEW_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIRS}
  ${GLM_INCLUDE_DIRS}
  ${png++_INCLUDE_DIRS}
  ${assimp_INCLUDE_DIRS}
  ${embree_INCLUDE_DIRS}/embree2
  )

add_definitions(
	-DTW_STATIC
	-DTW_NO_LIB_PRAGMA
	-DTW_NO_DIRECT3D
	-DGLEW_STATIC
	-D_CRT_SECURE_NO_WARNINGS

  -std=c++11
  -Wall
  -Wextra
  -Werror
  -g
  )

file(GLOB SOURCES
  ./*.cpp
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
  ${assimp_LIBRARIES}
  ${embree_LIBRARY}
  ${PNG_LIBRARY}
  ${OPENGL_LIBRARY}
  pthread
  )
