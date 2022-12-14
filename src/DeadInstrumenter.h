#pragma once

#include "ASTEdits.h"

namespace dead {

// Adds DCEMarkers in places where control flow diverges
class Instrumenter {
public:
  Instrumenter(
      std::map<std::string, clang::tooling::Replacements> &FileToReplacements);
  Instrumenter(Instrumenter &&) = delete;
  Instrumenter(const Instrumenter &) = delete;

  void registerMatchers(clang::ast_matchers::MatchFinder &Finder);
  void applyReplacements();

private:
  std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
  std::vector<RuleActionEditCollector> Rules;
  std::vector<clang::tooling::Replacement> Replacements;
  std::map<std::string, int> FileToNumberMarkerDecls;
};
} // namespace dead
