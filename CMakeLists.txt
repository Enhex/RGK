cmake_minimum_required(VERSION 3.0)

project(
  RGK
  VERSION 1.0
  LANGUAGES CXX
  )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(FindPackageHandleStandardArgs)

## find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)

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

## libpng, PNG++
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

## libjpeg
find_package(JPEG REQUIRED)

include_directories(
  ${GLM_INCLUDE_DIRS}
  ${png++_INCLUDE_DIRS}
  ${assimp_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
  )

add_definitions(
  -std=c++11
  -Wall
  -Wextra
  -Werror
  -O3
  )

file(GLOB SOURCES
  ./*.cpp
  )

add_executable(
  RGK
  ${SOURCES}
  )

## SET_TARGET_PROPERTIES(RGK PROPERTIES LINK_FLAGS -pg)

target_link_libraries(
  RGK
  ${assimp_LIBRARIES}
  ${PNG_LIBRARY}
  ${JPEG_LIBRARY}
  pthread
  )
