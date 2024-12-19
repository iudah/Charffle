#version
cmake_minimum_required(VERSION 3.6.0)
project(Charffle)
set(TARGET_NAME charffle_c)
add_library(charffle_c SHARED src/main/cpp/charffle_c.c src/main/cpp/shuffle.c)
include_directories(src/main/cpp)
target_link_options(${TARGET_NAME} PRIVATE -Wl,--version-script,${CMAKE_SOURCE_DIR}/native_lib.map.txt 
    -Wl,--no-undefined-version
    )
set_target_properties(${TARGET_NAME}
    PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/native_lib.map.txt
)
find_library(ANDROID_LOG_LIB log)
find_library(ANDROID_ANDROID_LIB android)
target_link_libraries(${TARGET_NAME} ${ANDROID_LOG_LIB} ${ANDROID_ANDROID_LIB})
