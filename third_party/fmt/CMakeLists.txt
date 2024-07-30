cmake_minimum_required(VERSION 3.28.3)

project(fmt LANGUAGES CXX)

set(FMT_SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/fmt.cc
    ${CMAKE_CURRENT_LIST_DIR}/format.cc
    ${CMAKE_CURRENT_LIST_DIR}/os.cc
)

add_library(fmt STATIC ${FMT_SOURCES})
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++20")

target_include_directories(fmt PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

include(GNUInstallDirs)
install(TARGETS fmt)
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/fmt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )
