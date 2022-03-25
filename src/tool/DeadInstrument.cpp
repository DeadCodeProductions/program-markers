#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>

#include <DeadInstrumenter.hpp>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace {
cl::OptionCategory DeadInstrOptions("dead-instrument options");

cl::opt<dead::DeadInstrumenter::Mode>
    Mode("mode", cl::desc("dead-instrumenter mode:"),
         cl::values(
             clEnumValN(dead::DeadInstrumenter::Mode::MakeGlobalsStaticOnly,
                        "globals", "Only make globals static"),
             clEnumValN(dead::DeadInstrumenter::Mode::CanonicalizeOnly,
                        "canonicalize", "Only canonicalize branches"),
             clEnumValN(dead::DeadInstrumenter::Mode::CanonicalizeAndInstrument,
                        "instrument",
                        "Only canonicalize and instrument branches (default)")),
         cl::init(dead::DeadInstrumenter::Mode::CanonicalizeAndInstrument));
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
    dead::DeadInstrumenter DI(Tool.getReplacements(), Mode);
    ast_matchers::MatchFinder Finder;
    DI.registerMatchers(Finder);
    std::unique_ptr<tooling::FrontendActionFactory> Factory =
        tooling::newFrontendActionFactory(&Finder);

    if (int Result = Tool.run(Factory.get())) {
        return Result;
    }

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
        return 1;
    }

    return Rewrite.overwriteChangedFiles();
}
