#include "ValueRangeInstrumenter.h"
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>
#include <llvm/Support/raw_ostream.h>
#include <type_traits>

#include <CommandLine.h>
#include <DCEInstrumenter.h>
#include <ValueRangeInstrumenter.h>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace {

enum class ToolMode { InstrumentBranches, InstrumentValueRanges };

cl::opt<ToolMode>
    Mode("mode", cl::desc("program-markers mode:"),
         cl::values(clEnumValN(ToolMode::InstrumentBranches, "dce",
                               "Only canonicalize and instrument branches with "
                               "DCE markers (default)"),
                    clEnumValN(ToolMode::InstrumentValueRanges, "vr",
                               "Only instrument for value ranges")),
         cl::init(ToolMode::InstrumentBranches),
         cl::cat(markers::ProgramMarkersOptions));

template <typename InstrTool> int runToolOnCode(RefactoringTool &Tool) {
  InstrTool Instr(Tool.getReplacements());
  ast_matchers::MatchFinder Finder;
  Instr.registerMatchers(Finder);
  std::unique_ptr<tooling::FrontendActionFactory> Factory =
      tooling::newFrontendActionFactory(&Finder);

  auto Ret = Tool.run(Factory.get());
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

  bool Result = true;
  for (const auto &FileAndReplaces : groupReplacementsByFile(
           Rewrite.getSourceMgr().getFileManager(), Tool.getReplacements())) {
    auto &CurReplaces = FileAndReplaces.second;

    Result = applyAllReplacements(CurReplaces, Rewrite) && Result;
  }
  if (!Result) {
    llvm::errs() << "Failed applying all replacements.\n";
    return false;
  }

  return !Rewrite.overwriteChangedFiles();
}

void versionPrinter(llvm::raw_ostream &S) { S << "v0.5.4\n"; }

} // namespace

int main(int argc, const char **argv) {
  cl::SetVersionPrinter(versionPrinter);
  auto ExpectedParser =
      CommonOptionsParser::create(argc, argv, markers::ProgramMarkersOptions);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser &OptionsParser = ExpectedParser.get();

  const auto &Compilations = OptionsParser.getCompilations();
  const auto &Files = OptionsParser.getSourcePathList();
  if (ToolMode::InstrumentBranches == Mode) {
    RefactoringTool Tool(Compilations, Files);
    if (int Result = runToolOnCode<markers::DCEInstrumenter>(Tool)) {
      llvm::errs() << "Something went wrong...\n";
      return Result;
    }
    if (!applyReplacements(Tool)) {
      llvm::errs() << "Failed to overwrite the input files.\n";
      return 1;
    }
  } else {
    RefactoringTool Tool(Compilations, Files);
    if (int Result = runToolOnCode<markers::ValueRangeInstrumenter>(Tool)) {
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
