cmake_minimum_required(VERSION 3.20)
project(smarti_cv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# OpenCV: on Windows point -DOpenCV_DIR=C:/opencv/build
# Linux/Docker use the system package (libopencv-dev)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui)

add_executable(smarti-cv
  src/main.cpp
)
target_include_directories(smarti-cv PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(smarti-cv PRIVATE ${OpenCV_LIBS})

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE SMARTI_SOURCES
       ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/include/*.hpp)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${SMARTI_SOURCES}
    COMMENT "Running clang-format")
endif()
