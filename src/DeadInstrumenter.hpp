#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
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

class RuleActionEditCollector
    : public clang::ast_matchers::MatchFinder::MatchCallback {
  public:
    RuleActionEditCollector(
        clang::transformer::RewriteRule Rule,
        std::vector<clang::tooling::Replacement> &Replacements,
        std::map<std::string, int> &FileToNumberMarkerDecls)
        : Rule{Rule}, Replacements{Replacements},
          FileToNumberMarkerDecls{FileToNumberMarkerDecls} {}
    void
    run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;
    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

  private:
    clang::transformer::RewriteRule Rule;
    std::vector<clang::tooling::Replacement> &Replacements;
    std::map<std::string, int> &FileToNumberMarkerDecls;
};

} // namespace detail

// Makes global variables static
class GlobalStaticMaker {
  public:
    GlobalStaticMaker(std::map<std::string, clang::tooling::Replacements>
                          &FileToReplacements);
    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

  private:
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
    detail::RuleActionCallback Rule;
};

// Adds DCEMarkers in places where control flow diverges
class Instrumenter {
  public:
    Instrumenter(std::map<std::string, clang::tooling::Replacements>
                     &FileToReplacements);
    Instrumenter(Instrumenter &&) = delete;
    Instrumenter(const Instrumenter &) = delete;

    void registerMatchers(clang::ast_matchers::MatchFinder &Finder);
    void applyReplacements();

  private:
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
    std::vector<detail::RuleActionEditCollector> Rules;
    std::vector<clang::tooling::Replacement> Replacements;
    std::map<std::string, int> FileToNumberMarkerDecls;
};
} // namespace dead
