#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>

#include <type_traits>

#include <DeadInstrumenter.hpp>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace {
cl::OptionCategory DeadInstrOptions("dead-instrument options");

enum class ToolMode { MakeGlobalsStaticOnly, InstrumentBranches };

cl::opt<ToolMode>
    Mode("mode", cl::desc("dead-instrumenter mode:"),
         cl::values(
             clEnumValN(ToolMode::MakeGlobalsStaticOnly, "globals",
                        "Only make globals static"),
             clEnumValN(ToolMode::InstrumentBranches, "instrument",
                        "Only canonicalize and instrument branches (default)")),
         cl::init(ToolMode::InstrumentBranches), cl::cat(DeadInstrOptions));

template <typename InstrTool> int runToolOnCode(RefactoringTool &Tool) {
    InstrTool Instr(Tool.getReplacements());
    ast_matchers::MatchFinder Finder;
    Instr.registerMatchers(Finder);
    std::unique_ptr<tooling::FrontendActionFactory> Factory =
        tooling::newFrontendActionFactory(&Finder);

    auto Ret = Tool.run(Factory.get());
    if constexpr (std::is_same_v<InstrTool, dead::Instrumenter>)
        if (!Ret)
            Instr.applyReplacements();
    return Ret;
}

bool applyReplacements(RefactoringTool &Tool) {
    LangOptions DefaultLangOptions;
    IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
    clang::TextDiagnosticPrinter DiagnosticPrinter(errs(), &*DiagOpts);
    DiagnosticsEngine Diagnostics(
        IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*DiagOpts,
        &DiagnosticPrinter, false);
    auto &FileMgr = Tool.getFiles();
    SourceManager Sources(Diagnostics, FileMgr);

    Rewriter Rewrite(Sources, DefaultLangOptions);
    if (!formatAndApplyAllReplacements(Tool.getReplacements(), Rewrite)) {
        llvm::errs() << "Failed applying all replacements.\n";
        return false;
    }
    return !Rewrite.overwriteChangedFiles();
}

} // namespace

int main(int argc, const char **argv) {
    auto ExpectedParser =
        CommonOptionsParser::create(argc, argv, DeadInstrOptions);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    const auto &Compilations = OptionsParser.getCompilations();
    const auto &Files = OptionsParser.getSourcePathList();
    RefactoringTool Tool(Compilations, Files);
    if (ToolMode::MakeGlobalsStaticOnly == Mode) {
        RefactoringTool Tool(Compilations, Files);

        if (int Result = runToolOnCode<dead::GlobalStaticMaker>(Tool)) {
            llvm::errs() << "Something went wrong...\n";
            return Result;
        }
        if (!applyReplacements(Tool)) {
            llvm::errs() << "Failed to overwrite the input files.\n";
            return 1;
        }
    } else {
        RefactoringTool Tool(Compilations, Files);
        if (int Result = runToolOnCode<dead::Instrumenter>(Tool)) {
            llvm::errs() << "Something went wrong...\n";
            return Result;
        }
        if (!applyReplacements(Tool)) {
            llvm::errs() << "Failed to overwrite the input files.\n";
            return 1;
        }
    }

    return 0;
}
