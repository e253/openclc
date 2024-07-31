cmake_minimum_required(VERSION 3.28.3)

project(openclc VERSION 0.0.1 LANGUAGES CXX)

set(CLANG_LLVM_VERSION 18.1.7)

find_package(LLVM ${CLANG_LLVM_VERSION} REQUIRED)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(OPENCL_H_LIB opencl_headers.lib)
else()
  set(OPENCL_H_LIB libopencl_headers.a)
endif()
add_custom_command(
  OUTPUT ${OPENCL_H_LIB}
  COMMAND ${OPENCLC_ZIG_BUILD_LIB} ${CMAKE_CURRENT_SOURCE_DIR}/opencl_headers.zig
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opencl_headers.zig ${CMAKE_CURRENT_SOURCE_DIR}/opencl-c.h
)
add_custom_target(buildOpenCLHeaderBin DEPENDS ${OPENCL_H_LIB})
add_library(opencl_headers STATIC IMPORTED GLOBAL)
add_dependencies(opencl_headers buildOpenCLHeaderBin)
set_target_properties(opencl_headers PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/${OPENCL_H_LIB})

add_executable(openclc openclc.cpp)

target_include_directories(openclc
  PRIVATE
    ${CMAKE_INSTALL_PREFIX}/include
)
target_link_directories(openclc PUBLIC ${CMAKE_INSTALL_PREFIX}/lib)

target_compile_options(openclc PRIVATE
  # Don't touch, important for windows-gnu
  -fno-exceptions
  -funwind-tables
  -fno-rtti
  -std=c++20
  -D_FILE_OFFSET_BITS=64 
  -D__STDC_CONSTANT_MACROS 
  -D__STDC_FORMAT_MACROS 
  -D__STDC_LIMIT_MACROS
  )

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
    opencl_headers
    LLVMSPIRVLib
    SPIRV-Tools-opt
    SPIRV-Tools
)

install(TARGETS openclc)
