link_directories(
  ${png++_LIBRARY_DIRS}
  ${assimp_LIBRARY_DIRS}
  ${JPEG_LIBRARY_DIRS}
  )

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

add_custom_command(
	TARGET RGK POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/RGK${CMAKE_EXECUTABLE_SUFFIX}" "${CMAKE_BINARY_DIR}/"
    )
