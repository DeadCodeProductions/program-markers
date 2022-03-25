#include "test_tool.hpp"

#include <DeadInstrumenter.hpp>

#include <clang/Format/Format.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>

#include <RewriterTestContext.h>

#include <catch2/catch.hpp>
#include <memory>

using namespace clang;

std::string formatCode(llvm::StringRef Code) {
    tooling::Replacements Replaces = format::reformat(
        format::getLLVMStyle(), Code, {tooling::Range(0, Code.size())});
    auto ChangedCode = tooling::applyAllReplacements(Code, Replaces);
    REQUIRE(static_cast<bool>(ChangedCode));
    return *ChangedCode;
}

std::string runToolOnCode(llvm::StringRef Code,
                          dead::DeadInstrumenter::Mode Mode) {
    clang::RewriterTestContext Context;
    clang::FileID ID = Context.createInMemoryFile("input.cc", Code);

    std::map<std::string, tooling::Replacements> FileToReplacements;
    dead::DeadInstrumenter DeadInstrumenterTool{FileToReplacements, Mode};
    ast_matchers::MatchFinder Finder;
    DeadInstrumenterTool.registerMatchers(Finder);
    std::unique_ptr<tooling::FrontendActionFactory> Factory =
        tooling::newFrontendActionFactory(&Finder);
    REQUIRE(tooling::runToolOnCode(Factory->create(), Code, "input.cc"));
    formatAndApplyAllReplacements(FileToReplacements, Context.Rewrite);
    return formatCode(Context.getRewrittenText(ID));
}

std::string runBranchInstrumenterOnCode(llvm::StringRef Code) {
    return runToolOnCode(
        Code, dead::DeadInstrumenter::Mode::CanonicalizeAndInstrument);
}

std::string runBranchCanonicalizerOnCode(llvm::StringRef Code) {
    return runToolOnCode(Code, dead::DeadInstrumenter::Mode::CanonicalizeOnly);
}

std::string runMakeGlobalsStaticOnCode(llvm::StringRef Code) {
    return runToolOnCode(Code,
                         dead::DeadInstrumenter::Mode::MakeGlobalsStaticOnly);
}
