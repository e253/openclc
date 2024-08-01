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
#include <LLVMSPIRVLib/LLVMSPIRVOpts.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/LangStandard.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>
#include <memory>
#include <spirv-tools/libspirv.h>

#define OPENCLC_VERSION "0.0.2"

namespace cli = llvm::cl;

extern const char* opencl_c_h_data;
extern size_t opencl_c_h_size;

static fmt::text_style err = fg(fmt::color::crimson) | fmt::emphasis::bold;
static fmt::text_style good = fg(fmt::color::green);
static llvm::ExitOnError LLVMExitOnErr;

/* Command Line Options */
// https://llvm.org/docs/CommandLine.html
static cli::OptionCategory OpenCLCOptions("OpenCLC Options");
static cli::list<std::string> InputFilenames(cli::Positional, cli::desc("<Input files>"), cli::OneOrMore, cli::cat(OpenCLCOptions));
static cli::opt<std::string> OutputFileName("o", cli::desc("Output Filename"), cli::init("spvbin.c"), cli::cat(OpenCLCOptions));
static cli::opt<std::string> CSym("sym", cli::desc("Output C Initializer List Sym"), cli::init("__spv_src"), cli::cat(OpenCLCOptions));
static cli::opt<bool> Verbose("v", cli::desc("Verbose"), cli::cat(OpenCLCOptions));
static cli::opt<bool> Werror("Werror", cli::desc("Warnings are errors"), cli::cat(OpenCLCOptions));
static cli::opt<bool> Wall("Wall", cli::desc("Enable all Clang warnings"), cli::cat(OpenCLCOptions));
static cli::list<std::string> Warnings(cli::Prefix, "W", cli::desc("Enable or disable a warning in Clang"), cli::ZeroOrMore, cli::cat(OpenCLCOptions));
static cli::list<std::string> Includes(cli::Prefix, "I", cli::desc("Add a directory to be searched for header files"), cli::ZeroOrMore, cli::cat(OpenCLCOptions));
static cli::list<std::string> Defines(cli::Prefix, "D", cli::desc("Define a #define directive"), cli::ZeroOrMore, cli::cat(OpenCLCOptions));
static cli::opt<bool> Debug("g", cli::desc("Skip optimization passes and leave debug information"), cli::cat(OpenCLCOptions));
enum CLStd {
    CL_STD_100,
    CL_STD_110,
    CL_STD_120,
    CL_STD_200,
    CL_STD_300,
    CL_CPP_STD,
    CL_CPP_2021,
};
static cli::opt<CLStd> CLStd(
    "cl-std",
    cli::desc("Select OpenCL Language Standard"),
    cli::values(
        clEnumValN(CL_STD_100, "CL1.0", "OpenCL C 1.0.0"),
        clEnumValN(CL_STD_110, "CL1.1", "OpenCL C 1.1.0"),
        clEnumValN(CL_STD_120, "CL1.2", "OpenCL C 1.2.0"),
        clEnumValN(CL_STD_200, "CL2.0", "OpenCL C 2.0.0"),
        clEnumValN(CL_STD_300, "CL3.0", "OpenCL C 3.0.0"),
        clEnumValN(CL_CPP_STD, "CLC++", "OpenCL C++"),
        clEnumValN(CL_CPP_STD, "CLC++2021", "OpenCL C++ 2021")),
    cli::init(CL_STD_120),
    cli::cat(OpenCLCOptions));
static cli::opt<SPIRV::VersionNumber> SpvVersion(
    "spv-version",
    cli::desc("Select SPIR-V Version"),
    cli::values(
        clEnumValN(SPIRV::VersionNumber::SPIRV_1_0, "1.0", "SPIR-V 1.0"),
        clEnumValN(SPIRV::VersionNumber::SPIRV_1_1, "1.1", "SPIR-V 1.1"),
        clEnumValN(SPIRV::VersionNumber::SPIRV_1_2, "1.2", "SPIR-V 1.2"),
        clEnumValN(SPIRV::VersionNumber::SPIRV_1_3, "1.3", "SPIR-V 1.3"),
        clEnumValN(SPIRV::VersionNumber::SPIRV_1_4, "1.4", "SPIR-V 1.4"),
        clEnumValN(SPIRV::VersionNumber::SPIRV_1_5, "1.5", "SPIR-V 1.5")),
    cli::init(SPIRV::VersionNumber::SPIRV_1_0),
    cli::cat(OpenCLCOptions));

/// https://stackoverflow.com/questions/786555/c-stream-to-memory
///
/// https://gist.github.com/stephanlachnit/4a06f8475afd144e73235e2a2584b000
///
/// Stream Buffer backed by a std::vector<char>
///
/// For interfaces heavily bound to std::ostream << patterns
struct membuf : std::streambuf {
public:
    membuf() { }
    std::streamsize xsputn(const char* s, std::streamsize count) override
    {
        for (int i = 0; i < count; i++)
            vec.push_back(s[i]);

        pbump(count);

        return count;
    }
    std::vector<char> vec;
};

/// function that acts as a stdout logger for the spvtools::Optimizer
void optimizerMessageConsumer(spv_message_level_t level, const char* source, const spv_position_t& position, const char* message)
{
    std::string strLevel;
    switch (level) {
    case SPV_MSG_FATAL:
        strLevel = "SPV_MSG_FATAL";
    case SPV_MSG_ERROR:
        strLevel = "SPV_MSG_ERROR";
    case SPV_MSG_INTERNAL_ERROR:
        strLevel = "SPV_MSG_INTERL_ERROR";
    case SPV_MSG_WARNING:
        strLevel = "SPV_MSG_WARNING";
    case SPV_MSG_DEBUG:
        strLevel = "SPV_MSG_DEBUG";
    case SPV_MSG_INFO:
        strLevel = "SPV_MSG_INFO";
    }

    fmt::print(err, "OPTIMIZER_{}: `{}`\n", strLevel, message);
}

/// utility needed in SourceToModule, to set contents of `opencl-c.h`
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

/// library based equivalent to `clang -c -emit-llvm`.
///
/// Kills process with helpful messages if there are compilation errors.
///
/// Credit: https://github.com/google/clspv/blob/2776a72da17dfffdd1680eeaff26a8bebdaa60f7/lib/Compiler.cpp#L1079
std::unique_ptr<llvm::Module> SourceToModule(llvm::LLVMContext& ctx, std::string& fileName)
{
    clang::CompilerInstance clangInstance;
    clang::FrontendInputFile clSrcFile(fileName, clang::InputKind(clang::Language::OpenCL)); // TODO: decide language based on `.cl` or `.clpp` exentsion

    // warnings to disable
    clangInstance.getDiagnosticOpts().Warnings.push_back("no-unsafe-buffer-usage");
    if (fileName.ends_with(".clpp") || fileName.ends_with(".clcpp")) {
        clangInstance.getDiagnosticOpts().Warnings.push_back("no-c++98-compat");
        clangInstance.getDiagnosticOpts().Warnings.push_back("no-missing-prototypes");
    }
    for (std::string warning : Warnings) {
        clangInstance.getDiagnosticOpts().Warnings.push_back(warning);
    }

    // clang compiler instance options
    // diagnostics
    std::string log;
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
    clangInstance.setTarget(clang::TargetInfo::CreateTargetInfo(clangInstance.getDiagnostics(), std::make_shared<clang::TargetOptions>(clangInstance.getTargetOpts())));

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

    clang::Language lang;
    clang::LangStandard::Kind langStd;

    if (fileName.ends_with(".cl")) {
        lang = clang::Language::OpenCL;
        switch (CLStd) {
        case CL_STD_100:
            langStd = clang::LangStandard::lang_opencl10;
            break;
        case CL_STD_110:
            langStd = clang::LangStandard::lang_opencl11;
            break;
        case CL_STD_120:
            langStd = clang::LangStandard::lang_opencl12;
            break;
        case CL_STD_200:
            langStd = clang::LangStandard::lang_opencl20;
            break;
        case CL_STD_300:
            langStd = clang::LangStandard::lang_opencl30;
            break;
        case CL_CPP_STD:
            fmt::print(err, "Cannot specify CLC++ language standard with file extension `.cl`. Use `.clpp` or `.clcpp` instead\n");
            exit(1);
        case CL_CPP_2021:
            fmt::print(err, "Cannot specify CLC++2021 language standard with file extension `.cl`. Use `.clpp` or `.clcpp` instead\n");
            exit(1);
        }
    } else if (fileName.ends_with(".clpp") || fileName.ends_with(".clcpp")) {
        lang = clang::Language::OpenCLCXX;
        if (CLStd == CL_CPP_2021) {
            langStd = clang::LangStandard::lang_openclcpp2021;
        } else {
            langStd = clang::LangStandard::lang_openclcpp10;
        }
    } else {
        fmt::print(err, "Invalid file extension supplied. Use `.cl` for OpenCL C and `.clpp` or `.clcpp` for OpenCL C++ source\n");
    }

    std::vector<std::string> includes;
    clang::LangOptions::setLangDefaults(
        clangInstance.getLangOpts(),
        lang,
        llvm::Triple { "spirv64-unknown-unknown" },
        includes,
        langStd);

    clangInstance.getPreprocessorOpts().addMacroDef("__SPIRV__");
    std::unique_ptr<llvm::MemoryBuffer> opencl_c_h_buffer(new OpenCLBuiltinMemoryBuffer(opencl_c_h_data, opencl_c_h_size));
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

int main(int argc, const char** argv)
{
    if (argc == 2) {
        // llvm::cl --version returns llvm version
        if (std::string(argv[1]) == std::string("--version")) {
            fmt::println("{}", OPENCLC_VERSION);
            return 0;
        }
    }

    cli::HideUnrelatedOptions(OpenCLCOptions);
    cli::ParseCommandLineOptions(argc, argv, "OpenCL Compiler");

    llvm::LLVMContext ctx;

    std::vector<std::unique_ptr<llvm::Module>> mods;
    for (std::string fileName : InputFilenames) {
        if (Verbose)
            fmt::print("Debug: Compiling {}\n", fileName);
        mods.push_back(SourceToModule(ctx, fileName));
        if (Verbose)
            fmt::print(fg(fmt::color::green), "Debug: Success\n");
    }

    std::unique_ptr<llvm::Module> mod(mods.back().release());
    mods.pop_back();
    llvm::Linker L(*mod);
    for (std::unique_ptr<llvm::Module>& mod : mods) {
        bool error = L.linkInModule(std::move(mod), 0);
        if (error) {
            fmt::print(err, "Link Step Failed\n");
            return 1;
        }
    }

    if (Verbose)
        fmt::println("Debug: Successfully linked {} modules", mods.size() + 1);

    SPIRV::TranslatorOpts translatorOptions(SpvVersion);

    // ofstream outFile(OutputFileName);
    membuf mbuf;
    std::ostream os(&mbuf);

    std::string llvmSpirvCompilationErrors;
    bool success = llvm::writeSpirv(&*mod, translatorOptions, os, llvmSpirvCompilationErrors);
    if (!success) {
        fmt::print(err, "{}\n", llvmSpirvCompilationErrors);
    } else if (Verbose) {
        fmt::println("Debug: Emitted `{}` Debug", OutputFileName);
    }

    assert(mbuf.vec.size() % 4 == 0 && "Generated SPIR-V is corrupt, exiting.");
    std::vector<uint32_t> optSPV;

    spv_target_env env;
    switch (SpvVersion) {
    case SPIRV::VersionNumber::SPIRV_1_0:
        env = SPV_ENV_UNIVERSAL_1_0;
    case SPIRV::VersionNumber::SPIRV_1_1:
        env = SPV_ENV_UNIVERSAL_1_1;
    case SPIRV::VersionNumber::SPIRV_1_2:
        env = SPV_ENV_UNIVERSAL_1_2;
    case SPIRV::VersionNumber::SPIRV_1_3:
        env = SPV_ENV_UNIVERSAL_1_3;
    case SPIRV::VersionNumber::SPIRV_1_4:
        env = SPV_ENV_UNIVERSAL_1_4;
    case SPIRV::VersionNumber::SPIRV_1_5:
        env = SPV_ENV_UNIVERSAL_1_5;
    }

    spvtools::Optimizer opt(env);
    opt.RegisterPerformancePasses(!Debug);
    opt.SetMessageConsumer(optimizerMessageConsumer);
    opt.SetValidateAfterAll(true);
    success = opt.Run(reinterpret_cast<const uint32_t*>(mbuf.vec.data()), mbuf.vec.size() / 4, &optSPV);
    if (!success) {
        fmt::print(err, "Optimization Passes for {} failed\n", OutputFileName);
        return 1;
    } else if (Verbose) {
        fmt::println("Debug: Optimized `{}` Successfully", OutputFileName);
    }

    std::ofstream outFile(OutputFileName);

    // we want a c initializer list
    if (OutputFileName.ends_with(".c")) {
        outFile << "// Generated by OpenCLC " << std::endl;
        outFile << std::endl;
        outFile << "#pragma once" << std::endl;
        outFile << std::endl;
        outFile << "const unsigned char " << CSym << "[] = {";

        const uint8_t* optSpvBytePtr = reinterpret_cast<const uint8_t*>(optSPV.data());
        for (int byte_i = 0; byte_i < optSPV.size() * 4; byte_i++) {
            outFile << "0x" << std::hex << (int)optSpvBytePtr[byte_i] << ",";
        }

        outFile << "};" << std::endl;
    } else { // we want the spv bin directly
        outFile.write(reinterpret_cast<const char*>(optSPV.data()), optSPV.size() * 4);
    }

    outFile.close();
}
