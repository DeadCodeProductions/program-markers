#pragma once

#include "Common.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Transformer/RewriteRule.h>

namespace protag {

namespace detail {
class ValueTaggerCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback {
  public:
    ValueTaggerCallback(
        clang::transformer::RewriteRule Rule,
        std::map<std::string, clang::tooling::Replacements> &FileToReplacements,
        std::map<std::string, int> &FileToNumberValueTrackers);
    void
    run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;
    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

  private:
    clang::transformer::RewriteRule Rule;
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
    std::map<std::string, int> &FileToNumberValueTrackers;
};

} // namespace detail

class ValueTagger {
  public:
    ValueTagger(std::map<std::string, clang::tooling::Replacements>
                    &FileToReplacements);
    ValueTagger(const ValueTagger &) = delete;
    ValueTagger(ValueTagger &&) = delete;

    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

    void emitTagDefinitions();

  private:
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
    std::vector<detail::ValueTaggerCallback> Callbacks;
    std::map<std::string, int> FileToNumberValueTrackers;
};
} // namespace protag
