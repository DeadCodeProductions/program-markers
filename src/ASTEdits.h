#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Transformer/RangeSelector.h>
#include <clang/Tooling/Transformer/RewriteRule.h>
#include <clang/Tooling/Transformer/Stencil.h>

namespace markers {

enum class EditMetadataKind { MarkerCall, NewElseBranch, VRMarker };

clang::transformer::ASTEdit addMetadata(clang::transformer::ASTEdit &&Edit,
                                        EditMetadataKind Kind);

class RuleActionEditCollector
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  RuleActionEditCollector(
      clang::transformer::RewriteRule Rule,
      std::vector<clang::tooling::Replacement> &Replacements,
      std::map<std::string, size_t> &FileToNumberMarkerDecls)
      : Rule{Rule}, Replacements{Replacements},
        FileToNumberMarkerDecls{FileToNumberMarkerDecls} {}
  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;
  void registerMatchers(clang::ast_matchers::MatchFinder &Finder);

private:
  clang::transformer::RewriteRule Rule;
  std::vector<clang::tooling::Replacement> &Replacements;
  std::map<std::string, size_t> &FileToNumberMarkerDecls;
};

} // namespace markers
