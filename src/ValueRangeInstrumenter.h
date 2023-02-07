#pragma once

#include "ASTEdits.h"

namespace markers {

// TODO: Make a common parent class for all instrumenters?
class ValueRangeInstrumenter {
public:
  ValueRangeInstrumenter(
      std::map<std::string, clang::tooling::Replacements> &FileToReplacements);
  ValueRangeInstrumenter(ValueRangeInstrumenter &&) = delete;
  ValueRangeInstrumenter(const ValueRangeInstrumenter &) = delete;

  void registerMatchers(clang::ast_matchers::MatchFinder &Finder);
  void applyReplacements();

  static std::string makeMarkerMacros(size_t MarkerID);

private:
  std::map<std::string, clang::tooling::Replacements> &FileToReplacements;
  std::vector<RuleActionEditCollector> Rules;
  std::vector<clang::tooling::Replacement> Replacements;
  std::map<std::string, int> FileToNumberMarkerDecls;
};

} // namespace markers
