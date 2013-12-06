cmake_minimum_required(VERSION 2.8)

project(ReadSUN3DCpp)

find_package(CURL)

add_executable(ReadSUN3DCpp 
	copy_sun3d.cpp)

target_link_libraries(ReadSUN3DCpp ${CURL_LIBRARIES})

