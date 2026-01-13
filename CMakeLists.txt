cmake_minimum_required(VERSION 3.28)
project(myproject)

# add vcpkg installed path to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "/home/lukaleko/repos/vcpkg/installed/x64-linux/share")

add_executable(${PROJECT_NAME} main.cpp)

find_package(nlohmann_json CONFIG REQUIRED) # installed with vcpkg
find_library(VECTORSCAN_LIB hs REQUIRED) # installed in /usr/local/include

target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json ${VECTORSCAN_LIB})

