cmake_minimum_required(VERSION 2.8)

project(ReadSUN3DCpp)

find_package(CURL)
find_package(JPEG)
find_package(PNG)

add_executable(ReadSUN3DCpp 
	copy_sun3d.cpp)

target_link_libraries(ReadSUN3DCpp ${CURL_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES})

