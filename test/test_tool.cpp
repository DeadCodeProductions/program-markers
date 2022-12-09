#include "test_tool.hpp"
#include "print_diff.hpp"

#include <DeadInstrumenter.hpp>

#include <clang/Format/Format.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>

#include <RewriterTestContext.h>

#include <catch2/catch.hpp>
#include <memory>
#include <type_traits>

using namespace clang;

std::string formatCode(llvm::StringRef Code) {
  tooling::Replacements Replaces = format::reformat(
      format::getLLVMStyle(), Code, {tooling::Range(0, Code.size())});
  auto ChangedCode = tooling::applyAllReplacements(Code, Replaces);
  REQUIRE(static_cast<bool>(ChangedCode));
  return *ChangedCode;
}

void compare_code(const std::string &code1, const std::string &code2) {
  auto diff = code1 != code2;
  if (diff)
    print_diff(code1, code2);
  REQUIRE(!diff);
}

template <typename Tool> std::string runToolOnCode(llvm::StringRef Code) {
  clang::RewriterTestContext Context;
  clang::FileID ID = Context.createInMemoryFile("input.cc", Code);

  std::map<std::string, tooling::Replacements> FileToReplacements;
  Tool InstrumenterTool{FileToReplacements};
  ast_matchers::MatchFinder Finder;
  InstrumenterTool.registerMatchers(Finder);
  std::unique_ptr<tooling::FrontendActionFactory> Factory =
      tooling::newFrontendActionFactory(&Finder);
  REQUIRE(tooling::runToolOnCode(Factory->create(), Code, "input.cc"));
  if constexpr (std::is_same_v<Tool, dead::Instrumenter>)
    InstrumenterTool.applyReplacements();
  formatAndApplyAllReplacements(FileToReplacements, Context.Rewrite);
  return formatCode(formatCode(Context.getRewrittenText(ID)));
}

std::string runBranchInstrumenterOnCode(llvm::StringRef Code,
                                        bool emit_disable_macros,
                                        bool ignore_functions_with_macros) {
  dead::detail::setIgnoreFunctionsWithMacros(ignore_functions_with_macros);
  return runToolOnCode<dead::Instrumenter>(Code);
}

std::string runMakeGlobalsStaticOnCode(llvm::StringRef Code) {
  return runToolOnCode<dead::GlobalStaticMaker>(Code);
}
