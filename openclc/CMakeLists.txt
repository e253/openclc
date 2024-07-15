cmake_minimum_required(VERSION 3.28.3)

project(openclc VERSION 0.0.1 LANGUAGES CXX)

set(CLANG_LLVM_VERSION 18.1.7)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM ${CLANG_LLVM_VERSION} REQUIRED)
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${LLVM_CMAKE_DIR}
  )
include(AddLLVM)
include(LLVM-Config)

set(FMT_SOURCES 
  ${CMAKE_CURRENT_LIST_DIR}/../fmt/fmt.cc 
  ${CMAKE_CURRENT_LIST_DIR}/../fmt/format.cc 
  ${CMAKE_CURRENT_LIST_DIR}/../fmt/os.cc
)

add_library(fmt STATIC ${FMT_SOURCES})
target_include_directories(fmt
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../fmt/include
)

add_custom_command(
  OUTPUT libopencl_headers.a
  COMMAND zig build-lib ${CMAKE_CURRENT_SOURCE_DIR}/opencl_headers.zig
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opencl_headers.zig ${CMAKE_CURRENT_SOURCE_DIR}/opencl-c.h
)
add_custom_target(buildOpenCLHeaderBin DEPENDS libopencl_headers.a)
add_library(opencl_headers STATIC IMPORTED GLOBAL)
add_dependencies(opencl_headers buildOpenCLHeaderBin)
set_target_properties(opencl_headers PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libopencl_headers.a)

add_executable(openclc openclc.cpp)

target_include_directories(openclc
  PRIVATE
    ${CMAKE_INSTALL_PREFIX}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../fmt/include
)

# possibly redundant
target_link_directories(openclc PUBLIC ${CMAKE_INSTALL_PREFIX}/lib)

add_definitions("-fno-rtti")

#set(OPENCL_BASE_H_PATH "${CMAKE_CURRENT_LIST_DIR}/../")
#configure_file(opencl_headers.zig.in opencl_headers.zig @ONLY) 


# Kalasnikov approach: link everything and rely on DCE to get rid of what's not needed
target_link_libraries(openclc
  PRIVATE

    # LLVMAggressiveInstCombine
    # LLVMAnalysis
    # LLVMAsmParser
    # LLVMAsmPrinter
    # LLVMBinaryFormat
    LLVMBitReader
    # LLVMBitstreamReader
    LLVMBitWriter
    # LLVMCFGuard
    # LLVMCFIVerify
    LLVMCodeGen
    # LLVMCodeGenTypes
    LLVMCore
    # LLVMCoroutines
    LLVMCoverage
    # LLVMDebugInfoBTF
    # LLVMDebugInfoCodeView
    # LLVMDebuginfod
    # LLVMDebugInfoDWARF
    # LLVMDebugInfoGSYM
    # LLVMDebugInfoLogicalView
    # LLVMDebugInfoMSF
    # LLVMDebugInfoPDB
    # LLVMDemangle
    # LLVMDiff
    # LLVMDlltoolDriver
    # LLVMDWARFLinker
    # LLVMDWARFLinkerClassic
    # LLVMDWARFLinkerParallel
    # LLVMDWP
    # LLVMExecutionEngine
    # LLVMExtensions
    # LLVMFileCheck
    LLVMFrontendDriver
    LLVMFrontendHLSL
    # LLVMFrontendOffloading
    # LLVMFrontendOpenACC
    LLVMFrontendOpenMP
    # LLVMFuzzerCLI
    # LLVMFuzzMutate
    # LLVMGlobalISel
    # LLVMHipStdPar
    # LLVMInstCombine
    # LLVMInstrumentation
    # LLVMInterfaceStub
    # LLVMInterpreter
    # LLVMipo
    # LLVMIRPrinter
    # LLVMIRReader
    # LLVMJITLink
    # LLVMLibDriver
    # LLVMLineEditor
    LLVMLinker
    LLVMLTO
    # LLVMMC
    # LLVMMCA
    # LLVMMCDisassembler
    # LLVMMCJIT
    # LLVMMCParser
    # LLVMMIRParser
    # LLVMObjCARCOpts
    # LLVMObjCopy
    # LLVMObject
    # LLVMObjectYAML
    LLVMOption
    # LLVMOrcDebugging
    # LLVMOrcJIT
    # LLVMOrcShared
    # LLVMOrcTargetProcess
    # LLVMPasses
    # LLVMProfileData
    # LLVMRemarks
    # LLVMRuntimeDyld
    # LLVMScalarOpts
    # LLVMSelectionDAG
    # LLVMSPIRVLib
    LLVMSupport
    # LLVMSymbolize
    # LLVMTableGen
    # LLVMTableGenCommon
    # LLVMTableGenGlobalISel
    # LLVMTarget
    # LLVMTargetParser
    # LLVMTextAPI
    # LLVMTextAPIBinaryReader
    # LLVMTransformUtils
    # LLVMVectorize
    LLVMWindowsDriver
    # LLVMWindowsManifest
    # LLVMXRay

    clangAnalysis  
    # clangAnalysisFlowSensitive  
    # clangAnalysisFlowSensitiveModels
    clangAPINotes
    clangAST
    clangASTMatchers
    clangBasic                     
    clangCodeGen                   
    # clangCrossTU 
    # clangDependencyScanning
    # clangDirectoryWatcher
    clangDriver
    # clangDynamicASTMatchers
    clangEdit           
    # clangExtractAPI
    # clangFormat              
    clangFrontend
    # clangFrontendTool
    # clangHandleCXX
    # clangHandleLLVM
    # clangIndex 
    # clangIndexSerialization
    # clangInterpreter
    clangLex   
    clangParse
    # clangRewrite
    # clangRewriteFrontend
    clangSema 
    clangSerialization
    # clangStaticAnalyzerCheckers
    # clangStaticAnalyzerCore
    clangSupport        
    # clangTooling

    fmt
    #${CMAKE_CURRENT_BINARY_DIR}/libopencl_headers.a
    opencl_headers
    LLVMSPIRVLib
)

install(TARGETS openclc)
