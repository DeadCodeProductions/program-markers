#pragma once

#include "ASTEdits.h"

namespace markers {

// Adds DCEMarkers in places where control flow diverges
class DCEInstrumenter {
public:
  DCEInstrumenter(
      std::map<std::string, clang::tooling::Replacements> &FileToReplacements);
  DCEInstrumenter(DCEInstrumenter &&) = delete;
  DCEInstrumenter(const DCEInstrumenter &) = delete;

  void registerMatchers(clang::ast_matchers::MatchFinder &Finder);
  void applyReplacements();

  static std::string makeMarkerMacros(size_t MarkerID);

private:
  std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
  std::vector<RuleActionEditCollector> Rules;
  std::vector<clang::tooling::Replacement> Replacements;
  std::map<std::string, size_t> FileToNumberMarkerDecls;
};
} // namespace markers
