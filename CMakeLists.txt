cmake_minimum_required(VERSION 3.0)

project(
  RGK
  VERSION 1.0
  LANGUAGES CXX
  )

# Global C++ flags
add_definitions(
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
