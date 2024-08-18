#include "DeviceFrontendDiagnosticPrinter.h"
#include "LLVMSPIRVLib/LLVMSPIRVLib.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/ranges.h"
#include "spirv-tools/optimizer.hpp"
#include "unistd.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Tooling/Tooling.h"
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
#include <sstream>

#define OPENCLC_VERSION "0.0.3"

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
static cli::opt<std::string> OutputFileName("o", cli::desc("Output Filename"), cli::init("a.out"), cli::cat(OpenCLCOptions));
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

/// Kills process with helpful messages if there are compilation errors.
///
/// Credit: https://github.com/google/clspv/blob/2776a72da17dfffdd1680eeaff26a8bebdaa60f7/lib/Compiler.cpp#L1079
std::unique_ptr<llvm::Module> SourceToModule(llvm::LLVMContext& ctx, std::string& fileContents, std::string& fileName)
{
    clang::CompilerInstance clangInstance;

    // TODO: decide language based on `.cl` or `.clpp` exentsion
    llvm::MemoryBufferRef membufref = llvm::MemoryBufferRef(llvm::StringRef(fileContents), llvm::StringRef(fileName));
    clang::FrontendInputFile clSrcFile(membufref, clang::InputKind(clang::Language::OpenCL));

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

    if (fileName.ends_with(".cl") || fileName.ends_with(".ocl")) {
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
            fmt::print(err, "Cannot specify CLC++ language standard with file extension `.cl` or `.ocl`. Use `.clpp` or `.clcpp` instead\n");
            exit(1);
        case CL_CPP_2021:
            fmt::print(err, "Cannot specify CLC++2021 language standard with file extension `.cl` or `.ocl`. Use `.clpp` or `.clcpp` instead\n");
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
        fmt::print(err, "Invalid file extension supplied. Use `.cl` or `.ocl` for OpenCL C and `.clpp` or `.clcpp` for OpenCL C++ source\n");
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

struct Kernel {
    /// Function Name
    std::string kName;
    /// Function Arg Type and Names
    std::vector<std::string> kParamTypes;
    std::vector<std::string> kParams;
    // (x,y): x lines, then y chars to the correct place in the source string
    std::pair<std::size_t, std::size_t> beginSourceLocation;
    std::pair<std::size_t, std::size_t> endSourceLocation;

    std::string toString()
    {
        std::string decl = fmt::format("int {}(dim3 gd, dim3 bd, ", this->kName);
        for (int i = 0; i < kParams.size(); i++) {
            decl.append(fmt::format("{} {}", kParamTypes[i], kParams[i]));
            if (i < kParams.size() - 1)
                decl.append(", ");
        }
        decl.append(")");
        return decl;
    }

    std::size_t beginSourceOffset(std::string src)
    {
        std::size_t charOffset = 0;
        std::size_t newlinesEncountered = 0;
        while (newlinesEncountered < (std::get<0>(this->beginSourceLocation) - 1)) {
            if (src.at(charOffset) == '\n')
                newlinesEncountered++;
            charOffset++;
        }
        charOffset += (std::get<1>(this->beginSourceLocation) - 1);

        return charOffset;
    }

    std::size_t endSourceOffset(std::string src)
    {
        std::size_t charOffset = 0;
        std::size_t newlinesEncountered = 0;
        while (newlinesEncountered < (std::get<0>(this->endSourceLocation) - 1)) {
            if (src.at(charOffset) == '\n')
                newlinesEncountered++;
            charOffset++;
        }
        charOffset += (std::get<1>(this->endSourceLocation) - 1);

        return charOffset;
    }
};
static std::vector<Kernel> KernelDecls;

class FindKernelDeclVisitor
    : public clang::RecursiveASTVisitor<FindKernelDeclVisitor> {
public:
    explicit FindKernelDeclVisitor(clang::ASTContext* Context)
        : Context(Context)
    {
    }

    bool VisitFunctionDecl(clang::FunctionDecl* Declaration)
    {
        clang::FullSourceLoc startFullLocation = Context->getFullLoc(Declaration->getBeginLoc());
        if (!startFullLocation.isValid() || startFullLocation.isInSystemHeader() || Declaration->getFunctionType()->getCallConv() != clang::CallingConv::CC_OpenCLKernel)
            return true;

        if (Declaration->getReturnType().getAsString() != std::string("void")) {
            fmt::print(err, "Kernel Declaration `{}` has return type `{}`\n", Declaration->getNameAsString(), Declaration->getReturnType().getAsString());
            exit(1);
        }

        std::vector<std::string> kParamTypes;
        std::vector<std::string> kParams;

        for (int i = 0; i < Declaration->getNumParams(); i++) {
            clang::ParmVarDecl* pvd = Declaration->getParamDecl(i);
            std::string paramType = pvd->getOriginalType().getAsString();

            if (paramType.find("__constant") != std::string::npos)
                paramType.replace(0, sizeof("__constant ") - 1, "");
            if (paramType.find("__global") != std::string::npos)
                paramType.replace(0, sizeof("__global ") - 1, "");
            if (paramType.find("__local") != std::string::npos) {
                fmt::print("__local memory used in kernel `{}`, but dynamically allocated smem is not supported\n", Declaration->getNameAsString());
                exit(1);
            }

            kParamTypes.push_back(paramType);
            kParams.push_back(std::string(pvd->getName()));
        }

        clang::FullSourceLoc endFullLocation = Context->getFullLoc(Declaration->getEndLoc());

        KernelDecls.push_back(
            Kernel {
                .kName = Declaration->getNameAsString(),
                .kParamTypes = kParamTypes,
                .kParams = kParams,
                .beginSourceLocation = std::pair(startFullLocation.getSpellingLineNumber(), startFullLocation.getSpellingColumnNumber()),
                .endSourceLocation = std::pair(endFullLocation.getSpellingLineNumber(), endFullLocation.getSpellingColumnNumber()),
            });

        if (Verbose) {
            fmt::println(
                "Debug: Found Kernel `{}` at {}:{}",
                Declaration->getNameAsString(),
                startFullLocation.getSpellingLineNumber(),
                startFullLocation.getSpellingColumnNumber());
        }

        return true;
    }

private:
    clang::ASTContext* Context;
};

class FindKernelDeclConsumer : public clang::ASTConsumer {
public:
    explicit FindKernelDeclConsumer(clang::ASTContext* Context)
        : Visitor(Context)
    {
    }

    virtual void HandleTranslationUnit(clang::ASTContext& Context)
    {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    FindKernelDeclVisitor Visitor;
};

class FindKernelDeclAction : public clang::ASTFrontendAction {
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& Compiler, llvm::StringRef InFile)
    {
        return std::make_unique<FindKernelDeclConsumer>(&Compiler.getASTContext());
    }
};

/// Populatates global `KernelDecls` with Kernels found in the input source code
void ExtractDeviceCode(llvm::LLVMContext& ctx, std::string& fileContents, std::string& fileName)
{
    clang::CompilerInstance clangInstance;

    llvm::MemoryBufferRef membufref = llvm::MemoryBufferRef(llvm::StringRef(fileContents), llvm::StringRef(fileName));
    clang::FrontendInputFile clSrcFile(membufref, clang::InputKind(clang::Language::OpenCL));

    // warnings to disable
    clangInstance.getDiagnosticOpts().Warnings.push_back("no-unsafe-buffer-usage");
    if (fileName.ends_with(".clpp") || fileName.ends_with(".clcpp")) {
        clangInstance.getDiagnosticOpts().Warnings.push_back("no-c++98-compat");
        clangInstance.getDiagnosticOpts().Warnings.push_back("no-missing-prototypes");
    }

    // diagnostics
    std::string log;
    llvm::raw_string_ostream diagnosticsStream(log);
    clangInstance.createDiagnostics(
        new clang::DeviceFrontendDiagnosticPrinter(diagnosticsStream, &clangInstance.getDiagnosticOpts()),
        true);

    // input
    clangInstance.getFrontendOpts().Inputs.push_back(clSrcFile);

    // target
    clangInstance.getTargetOpts().Triple = "spirv64-unknown-unknown";
    clangInstance.setTarget(clang::TargetInfo::CreateTargetInfo(clangInstance.getDiagnostics(), std::make_shared<clang::TargetOptions>(clangInstance.getTargetOpts())));

    // instance options
    clangInstance.createFileManager();
    clangInstance.createSourceManager(clangInstance.getFileManager());

    // language options, copied from CLSPV
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

    clang::Language lang;
    clang::LangStandard::Kind langStd;

    if (fileName.ends_with(".cl") || fileName.ends_with(".ocl")) {
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
            fmt::print(err, "Cannot specify CLC++ language standard with file extension `.cl` or `.ocl`. Use `.clpp` or `.clcpp` instead\n");
            exit(1);
        case CL_CPP_2021:
            fmt::print(err, "Cannot specify CLC++2021 language standard with file extension `.cl` or `.ocl`. Use `.clpp` or `.clcpp` instead\n");
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
        fmt::print(err, "Invalid file extension supplied. Use `.cl` or `.ocl` for OpenCL C and `.clpp` or `.clcpp` for OpenCL C++ source\n");
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

    clangInstance.ExecuteAction(*std::make_unique<FindKernelDeclAction>());

    clang::DiagnosticConsumer* const consumer = clangInstance.getDiagnostics().getClient();
    consumer->finish();

    if ((consumer->getNumWarnings() > 0) || (consumer->getNumErrors() > 0))
        fmt::print(err, "{}", log);
    if (consumer->getNumErrors() > 0)
        exit(1);
}

std::string GenerateKernelInvocation(Kernel k)
{
    std::stringstream outFile;
    outFile << R"(
        #include "openclc_rt.h"
        #include <stdbool.h>
        #include <stdio.h>

        static cl_program prog = NULL;
        static bool prog_built = false;
    )";

    for (Kernel kDecl : KernelDecls) {
        outFile << kDecl.toString() << "\n{\n";
        outFile << R"(
            if (!prog_built) {
                int err = oclcBuildSpv(__spv_bin, sizeof(__spv_bin), &prog);
                if (err != 0) {
                    return 1;
                } else {
                    prog_built = true;
                }
            }

            cl_int err;
        )";

        outFile << fmt::format("cl_kernel kernel = clCreateKernel(prog, \"{}\", &err);\n", kDecl.kName);
        outFile << "CL_CHECK(err)\n\n";

        for (int i = 0; i < kDecl.kParams.size(); i++) {
            std::string paramName = kDecl.kParams[i];
            std::string paramType = kDecl.kParamTypes[i];
            bool typeIsPointer = paramType.find("*") != std::string::npos;
            if (typeIsPointer) {
                outFile << fmt::format("err = clSetKernelArg(kernel, {}, sizeof(cl_mem), (cl_mem*)&{});\n", i, paramName);
                outFile << "CL_CHECK(err)\n";
            } else {
                outFile << fmt::format("err = clSetKernelArg(kernel, {}, sizeof({}), {});\n", i, paramType, paramName);
                outFile << "CL_CHECK(err)\n";
            }
        }
    }

    outFile << R"(
        cl_uint work_dim;
        if (oclcValidateWorkDims(gd, bd, &work_dim) != 0) {
            return 1;
        }

        const size_t global_work_offset = 0;
        const size_t global_work_size[3] = { gd.x * bd.x, gd.y * bd.y, gd.z * bd.z };
        const size_t local_work_size[3] = { bd.x, bd.y, bd.z };

        err = clEnqueueNDRangeKernel(oclcQueue(), kernel, work_dim, &global_work_offset, global_work_size, local_work_size, 0, NULL, NULL);
        CL_CHECK(err)

        return 0;
    }
    )";
    return outFile.str();
}

std::filesystem::path GetRuntimeSourcesDir()
{
    char exePath[200];
    int _err = readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (_err == -1) {
        fmt::print(err, "readlink(\"/proc/self/exe\") failed with exit code {}", _err);
        exit(1);
    }

    std::filesystem::path runtimeSourcesDir(exePath);

    runtimeSourcesDir.remove_filename();

#ifndef OCLC_RELEASE
    runtimeSourcesDir.append("..");
    runtimeSourcesDir.append("..");
    runtimeSourcesDir.append("..");
    runtimeSourcesDir.append("runtime");
#endif

    return runtimeSourcesDir;
}

static void PrintVersion(llvm::raw_ostream& ros)
{
    ros << OPENCLC_VERSION << "\n";
}

int main(int argc, const char** argv)
{
    cli::SetVersionPrinter(PrintVersion);
    cli::HideUnrelatedOptions(OpenCLCOptions);
    cli::ParseCommandLineOptions(argc, argv, "OpenCL Compiler");

    llvm::LLVMContext ctx;

    std::vector<std::string> hostCompilerInputFiles;

    // For each file
    //     Read the contents manually
    //     Get the KernelDecls and compile the sources to spv
    //     Replace the decl in the source with a cpu function that invokes the kernel
    for (std::string fileName : InputFilenames) {
        // Read file contents
        std::ifstream inFileStream(fileName);
        inFileStream.seekg(0, std::ios_base::end);
        std::size_t fSize = inFileStream.tellg();
        inFileStream.seekg(0);
        std::string fileContents(fSize, '\0');
        inFileStream.read(&fileContents[0], fSize);

        ExtractDeviceCode(ctx, fileContents, fileName);

        // Get Kernel definition strings found in AST traversal
        // TODO: Move to `ExtractDeviceCode` code
        std::string deviceCode;
        for (Kernel kDecl : KernelDecls) {
            std::size_t start = kDecl.beginSourceOffset(fileContents);
            std::size_t end = kDecl.endSourceOffset(fileContents);
            deviceCode.append(fileContents.substr(start, end - start + 1));
        }

        if (Verbose)
            fmt::print("Debug: Found Device Code '{}'\n", deviceCode);

        // Compile device sources in `KernelDefinitions` to LLVM IR
        std::unique_ptr<llvm::Module> mod = SourceToModule(ctx, deviceCode, fileName);

        // Compile device code in LLVM IR to SPIR-V
        membuf mbuf;
        std::ostream os(&mbuf);
        SPIRV::TranslatorOpts translatorOptions(SpvVersion);
        std::string llvmSpirvCompilationErrors;
        bool success = llvm::writeSpirv(&*mod, translatorOptions, os, llvmSpirvCompilationErrors);
        if (!success) {
            fmt::print(err, "{}\n", llvmSpirvCompilationErrors);
            return 1;
        }

        assert(mbuf.vec.size() % 4 == 0 && "Generated SPIR-V is corrupt, exiting.");
        std::vector<uint32_t> optSPV;

        // Optimize SPIR-V
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
            fmt::print(err, "Optimization Passes for `a.out` failed\n");
            return 1;
        }

        // Convert generated SPIR-V to a c initializer list
        std::stringstream spvInitListStream;
        spvInitListStream << "static const unsigned char __spv_bin[] = {";
        const uint8_t* optSpvBytePtr = reinterpret_cast<const uint8_t*>(optSPV.data());
        for (int byte_i = 0; byte_i < optSPV.size() * 4; byte_i++) {
            spvInitListStream << "0x" << std::hex << (int)optSpvBytePtr[byte_i] << ",";
        }
        spvInitListStream << "};\n";
        std::string spvInitList = spvInitListStream.str();

        // Write the new contents to ./openclc-tmp/openclc_gen.inputfile.[c,cc,cxx,cpp]
        std::filesystem::create_directory("./openclc-tmp");
        std::string outFileName = std::string(std::filesystem::path(fileName).filename());
        if (outFileName.ends_with(".cl")) {
            outFileName.replace(outFileName.size() - 3, 3, ".c");
        } else if (outFileName.ends_with(".ocl")) {
            outFileName.replace(outFileName.size() - 4, 4, ".c");
        } else if (outFileName.ends_with(".clpp")) {
            outFileName.replace(outFileName.size() - 5, 5, ".cpp");
        } else {
            outFileName.replace(outFileName.size() - 6, 6, ".cpp");
        }
        std::string outFilePath = fmt::format("./openclc-tmp/{}", outFileName);
        hostCompilerInputFiles.push_back(outFilePath);

        std::ofstream postProcessedOutFile(outFilePath);
        postProcessedOutFile.write(spvInitList.c_str(), spvInitList.size());
        std::size_t offset = 0;
        for (auto kDecl : KernelDecls) {
            std::size_t start = kDecl.beginSourceOffset(fileContents);
            std::size_t end = kDecl.endSourceOffset(fileContents);

            postProcessedOutFile.write(fileContents.c_str() + offset, start); // write until kernel start

            std::string replacementRoutine = GenerateKernelInvocation(kDecl); // write new invocation
            postProcessedOutFile.write(replacementRoutine.c_str(), replacementRoutine.size());

            offset += end + 1; // move fileContents offset to start after the kernel
        }
        if (offset != fileContents.size()) { // we may have some source left to write out
            postProcessedOutFile.write(fileContents.c_str() + offset, fileContents.size() - offset);
        }

        // Reset Global
        KernelDecls = std::vector<Kernel>();
    }

    // Invoke host compiler on generated file
    std::filesystem::path runtimeSourceDir = GetRuntimeSourcesDir();
    std::filesystem::path runtimeSource(runtimeSourceDir);
    runtimeSource.append("openclc_rt.c");

    std::string hostCompilerInputs;
    for (auto hostCompilerInput : hostCompilerInputFiles) {
        hostCompilerInputs.append(hostCompilerInput);
        hostCompilerInputs.push_back(' ');
    }

    std::string hostCompilerInvocation = fmt::format("zig cc {} {} -I{} -lOpenCL -o {}", hostCompilerInputs, std::string(runtimeSource), std::string(runtimeSourceDir), OutputFileName);
    if (Verbose)
        fmt::print("Debug: Host compiler invocation '{}'\n", hostCompilerInvocation);
    std::system(hostCompilerInvocation.c_str());
}
