#include "llvm/IR/Module.h"

using namespace std;

/// library based equivalent to `clang -c -emit-llvm`.
///
/// Kills process with helpful messages if there are compilation errors.
///
/// Credit: https://github.com/google/clspv/blob/2776a72da17dfffdd1680eeaff26a8bebdaa60f7/lib/Compiler.cpp#L1079
unique_ptr<llvm::Module> SourceToModule(llvm::LLVMContext& ctx, string& fileName);

/// Reads unoptimized `OutputFileName`, applies performance passes in spirv-opt,
/// then overwrites the contents of `OutputFileName` with the optimized code.
///
/// Returns true on success, false on failure. On failure messages are emitted
bool Optimize(string& fileName);

/// String containing the contents of opencl_c.h that defines OpenCL builtin functions
extern const char* opencl_c_h_data;
extern size_t opencl_c_h_size;