#include "LLVMSPIRVLib/LLVMSPIRVLib.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "spirv-tools/optimizer.hpp"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include <LLVMSPIRVLib/LLVMSPIRVLib.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/LangStandard.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>
#include <memory>
#include <stdlib.h>
#include <unistd.h>

using namespace std;
namespace cli = llvm::cl;

static fmt::text_style err = fg(fmt::color::crimson) | fmt::emphasis::bold;
static fmt::text_style good = fg(fmt::color::green);
static llvm::ExitOnError LLVMExitOnErr;

/* Command Line Options */
// https://llvm.org/docs/CommandLine.html
static cli::OptionCategory OpenCLCOptions("OpenCLC Options");
static cli::list<string> InputFilenames(cli::Positional, cli::desc("<Input files>"), cli::OneOrMore, cli::cat(OpenCLCOptions));
static cli::opt<string> OutputFileName("o", cli::desc("Output Filename"), cli::init("a.out"), cli::cat(OpenCLCOptions));
static cli::opt<bool> Verbose("v", cli::desc("Verbose"), cli::cat(OpenCLCOptions));
static cli::opt<bool> Werror("Werror", cli::desc("Warnings are errors"), cli::cat(OpenCLCOptions));
static cli::opt<bool> Wall("Wall", cli::desc("Enable all Clang warnings"), cli::cat(OpenCLCOptions));
static cli::list<string> Warnings(cli::Prefix, "W", cli::desc("Enable or disable a warning in Clang"), cli::ZeroOrMore);
static cli::list<std::string> Includes(cli::Prefix, "I", cli::desc("Add a directory to be searched for header files."), cli::ZeroOrMore);
static cli::list<std::string> Defines(cli::Prefix, "D", cli::desc("Define a #define directive."), cli::ZeroOrMore);

namespace openclc {
enum OptLevel {
    O0,
    O1,
    O2,
    O3,
};
};

// library based equivalent to `clang -c -emit-llvm`.
// Kills process with helpful messages if there are compilation errors
// From https://github.com/google/clspv/blob/2776a72da17dfffdd1680eeaff26a8bebdaa60f7/lib/Compiler.cpp#L1079
unique_ptr<llvm::Module> SourceToModule(llvm::LLVMContext& ctx, string& fileName);
void Optimize(openclc::OptLevel optLevel = openclc::O3);
/// String containing the contents of opencl_c.h that defines OpenCL builtin functions
extern const char* opencl_c_h_data;
extern size_t opencl_c_h_size;

int main(int argc, const char** argv)
{
    cli::HideUnrelatedOptions(OpenCLCOptions);
    cli::ParseCommandLineOptions(argc, argv);

    if (Verbose) {
        char exe_path[200];
        int err_code = readlink("/proc/self/exe", exe_path, 200);
        if (err_code < 0)
            fmt::print(err, "Recieved err code '{}' reading `/proc/self/exe`\n", err_code);
        else
            fmt::print("Debug: {}\n", exe_path);
    }

    llvm::LLVMContext ctx;

    vector<unique_ptr<llvm::Module>> mods;
    for (string fileName : InputFilenames) {
        string filePath = filesystem::absolute(fileName);

        if (Verbose)
            fmt::print("Debug: Compilation of {}\n", filePath);

        mods.push_back(SourceToModule(ctx, filePath));

        if (Verbose)
            fmt::print(fg(fmt::color::green), "Debug: Success\n\n", filePath);
    }

    unique_ptr<llvm::Module> mod(mods.back().release());
    mods.pop_back();
    llvm::Linker L(*mod);
    for (unique_ptr<llvm::Module>& mod : mods) {
        bool error = L.linkInModule(std::move(mod), 0);
        if (error) {
            fmt::print(err, "Link Step Failed\n");
            return 1;
        }
    }

    if (Verbose)
        fmt::print("Debug: Successfully linked {} modules\n", mods.size() + 1);

    string llvmSpirvCompilationErrors;
    ofstream outFile(OutputFileName);
    bool success = llvm::writeSpirv(&*mod, outFile, llvmSpirvCompilationErrors);
    if (!success) {
        fmt::print(err, "{}\n", llvmSpirvCompilationErrors);
    } else if (Verbose) {
        fmt::print("Debug: Emitted `{}` Successfully\n", OutputFileName);
    }

    Optimize();

    if (Verbose) {
        fmt::print("Debug: `{}` Optimized Successfully\n", OutputFileName);
    }

    return !success;
}

struct OpenCLBuiltinMemoryBuffer final : public llvm::MemoryBuffer {
    OpenCLBuiltinMemoryBuffer(const void* data, uint64_t data_length)
    {
        const char* dataCasted = reinterpret_cast<const char*>(data);
        init(dataCasted, dataCasted + data_length, true);
    }

    virtual llvm::MemoryBuffer::BufferKind getBufferKind() const override
    {
        return llvm::MemoryBuffer::MemoryBuffer_Malloc;
    }

    virtual ~OpenCLBuiltinMemoryBuffer() override { }
};

// library based equivalent to `clang -c -emit-llvm`
// Kills process with helpful messages if there are compilation errors
// From https://github.com/google/clspv/blob/2776a72da17dfffdd1680eeaff26a8bebdaa60f7/lib/Compiler.cpp#L1079
unique_ptr<llvm::Module>
SourceToModule(llvm::LLVMContext& ctx, string& fileName)
{
    clang::CompilerInstance clangInstance;
    clang::FrontendInputFile clSrcFile(fileName, clang::InputKind(clang::Language::OpenCL)); // TODO: decide language based on `.cl` or `.clpp` exentsion

    // warnings to disable
    clangInstance.getDiagnosticOpts().Warnings.push_back("no-unsafe-buffer-usage");

    // clang compiler instance options
    // diagnostics
    string log;
    llvm::raw_string_ostream diagnosticsStream(log);
    clangInstance.createDiagnostics(
        new clang::TextDiagnosticPrinter(diagnosticsStream, &clangInstance.getDiagnosticOpts()),
        true);
    clangInstance.getDiagnostics().setEnableAllWarnings(Wall);
    clangInstance.getDiagnostics().setWarningsAsErrors(Werror);

    // input
    clangInstance.getFrontendOpts().Inputs.push_back(clSrcFile);

    // target
    clangInstance.getTargetOpts().Triple = "spirv64-unknown-unknown";
    clangInstance.setTarget(clang::TargetInfo::CreateTargetInfo(clangInstance.getDiagnostics(), make_shared<clang::TargetOptions>(clangInstance.getTargetOpts())));

    // instance options
    clangInstance.createFileManager();
    clangInstance.createSourceManager(clangInstance.getFileManager());

    // language options, copied from CLSPV
    // -std=CL2.0 by default.
    clangInstance.getLangOpts().C99 = true;
    clangInstance.getLangOpts().RTTI = false;
    clangInstance.getLangOpts().RTTIData = false;
    clangInstance.getLangOpts().MathErrno = false;
    clangInstance.getLangOpts().Optimize = false;
    clangInstance.getLangOpts().NoBuiltin = true;
    clangInstance.getLangOpts().ModulesSearchAll = false;
    clangInstance.getLangOpts().SinglePrecisionConstants = true;
    clangInstance.getLangOpts().DeclareOpenCLBuiltins = true;
    clangInstance.getLangOpts().NativeHalfType = true; // enabled by default
    clangInstance.getLangOpts().NativeHalfArgsAndReturns = true; // enabled by default
    clangInstance.getCodeGenOpts().StackRealignment = true;
    clangInstance.getCodeGenOpts().SimplifyLibCalls = false;
    clangInstance.getCodeGenOpts().EmitOpenCLArgMetadata = false;
    clangInstance.getCodeGenOpts().DisableO0ImplyOptNone = true;
    clangInstance.getCodeGenOpts().OptimizationLevel = 0;

    vector<string> includes;
    clang::LangOptions::setLangDefaults(
        clangInstance.getLangOpts(),
        clang::Language::OpenCL,
        llvm::Triple { "spir64-unknown-unknown" },
        includes,
        clang::LangStandard::lang_opencl20);

    // include CL definitions
    clangInstance.getPreprocessorOpts().addMacroDef("__OPENCL_VERSION__=200");
    unique_ptr<llvm::MemoryBuffer> opencl_c_h_buffer(new OpenCLBuiltinMemoryBuffer(opencl_c_h_data, opencl_c_h_size));
    clangInstance.getPreprocessorOpts().Includes.push_back("opencl-c.h");
    clang::FileEntryRef opencl_c_h_ref = clangInstance.getFileManager().getVirtualFileRef("include/opencl-c.h", opencl_c_h_buffer->getBufferSize(), 0);
    clangInstance.getSourceManager().overrideFileContents(opencl_c_h_ref, std::move(opencl_c_h_buffer));

    for (auto define : Defines) {
        clangInstance.getPreprocessorOpts().addMacroDef(define);
    }

    for (auto include : Includes) {
        clangInstance.getHeaderSearchOpts().AddPath(include, clang::frontend::After, false, false);
    }

    // Get llvm::Module from `clangInstance`
    clang::EmitLLVMOnlyAction action(&ctx);

    bool success = action.BeginSourceFile(clangInstance, clSrcFile);
    if (!success) {
        fmt::print(err, "Preparation for file '{}' failed\n", fileName);
        exit(1);
    }

    llvm::Error result = action.Execute();
    action.EndSourceFile();

    clang::DiagnosticConsumer* const consumer = clangInstance.getDiagnostics().getClient();
    consumer->finish();

    if ((consumer->getNumWarnings() > 0) || (consumer->getNumErrors() > 0))
        fmt::print(err, "{}", log);
    if (consumer->getNumErrors() > 0)
        exit(1);

    return action.takeModule();
}

/// Applies Optimization level to binary at `OutputFileName`
/// Default all optimizations are enabled, `O3`
void Optimize(openclc::OptLevel optLevel)
{
    if (optLevel == openclc::O0) {
        return;
    }

    vector<uint32_t> optBinary;
    { // force drop the file lock on OutputFileName
        ifstream unoptBinaryIfstream(OutputFileName);
        vector<uint8_t> unoptBinary((istream_iterator<uint8_t>(unoptBinaryIfstream)), {});

        assert(unoptBinary.size() % 4 == 0 && "Tried to optimize SPV binary with size in bytes not divisible by 4");

        spvtools::Optimizer opt(SPV_ENV_OPENCL_2_0); // TODO: change this with arg
        opt.Run(reinterpret_cast<uint32_t*>(unoptBinary.data()), unoptBinary.size() / 4, &optBinary);
    }

    ofstream optBinaryOfStream(OutputFileName);
    optBinaryOfStream.write(reinterpret_cast<const char*>(optBinary.data()), optBinary.size() * 4);
}
