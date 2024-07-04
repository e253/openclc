cmake_minimum_required(VERSION 3.28.3)

project(openclc VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

set(CLANG_LLVM_VERSION 18.1.7)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${LLVM_CMAKE_DIR}
  )
include(AddClang)
include(AddLLVM)

set(LLVM_LINK_COMPONENTS support)

add_clang_executable(openclc
  openclc.cpp
)

target_link_libraries(openclc
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
)
