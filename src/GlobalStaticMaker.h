#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Transformer/RewriteRule.h>

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

// Makes global variables static
class GlobalStaticMaker {
public:
  GlobalStaticMaker(
      std::map<std::string, clang::tooling::Replacements> &FileToReplacements);
  void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

private:
  detail::RuleActionCallback Rule;
};
} // namespace dead
