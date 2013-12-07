cmake_minimum_required(VERSION 2.8)

project(SUN3DCppReader)

find_package(CURL)
find_package(JPEG)
find_package(PNG)

add_executable(SUN3DCppReader 
	sun3d_reader.cpp)

target_link_libraries(SUN3DCppReader ${CURL_LIBRARIES} ${JPEG_LIBRARIES} ${PNG_LIBRARIES})

