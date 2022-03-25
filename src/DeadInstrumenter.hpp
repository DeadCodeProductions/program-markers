#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Transformer/RewriteRule.h>
#include <clang/Tooling/Transformer/Stencil.h>

namespace dead {
namespace detail {
class RuleActionCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback {
  public:
    RuleActionCallback(
        clang::transformer::RewriteRule Rule,
        std::map<std::string, clang::tooling::Replacements> &FileToReplacements)
        : Rule{Rule}, FileToReplacements{FileToReplacements} {}
    void
    run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;
    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

  private:
    clang::transformer::RewriteRule Rule;
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
};
} // namespace detail

class DeadInstrumenter {
  public:
    enum class Mode {
        MakeGlobalsStaticOnly,
        CanonicalizeOnly,
        CanonicalizeAndInstrument,
    };
    DeadInstrumenter(
        std::map<std::string, clang::tooling::Replacements> &FileToReplacements,
        Mode InstrumenterMode);
    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

  private:
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
    std::vector<detail::RuleActionCallback> Rules;
};
} // namespace dead
