project(Holo-VR)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
FIND_PACKAGE(OpenCV REQUIRED )

message("-- GLM includes: " ${GLM_INCLUDE_DIR})
message("-- OpenGL includes: " ${OPENGL_INCLUDE_DIR})
message("-- OpenGL library: " ${OPENGL_LIBRARY})
message("-- GLEW library: " ${GLEW_LIBRARY})

include_directories(
  ${OPENGL_INCLUDE_DIR}
  ${MINVR_INCLUDE_DIR}
  ${GLM_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
  )

# tgm
add_executable(Holo-VR
  main.cpp
  tinyxml2.cpp
)
INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})

target_link_libraries(Holo-VR
  ${MINVR_LIBRARY}
  ${OPENGL_LIBRARY}
  ${GLEW_LIBRARY}
  ${OpenCV_LIBS}
  ${ALL_LIBS}
)

