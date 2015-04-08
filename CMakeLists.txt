cmake_minimum_required(VERSION 2.8)
project( Tattoo )
find_package( OpenCV REQUIRED )
add_executable( Tattoo Tattoo.cpp )
target_link_libraries( Tattoo ${OpenCV_LIBS} )

SET(GCC_COVERAGE_COMPILE_FLAGS "-Werror")
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
