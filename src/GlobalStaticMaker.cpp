#include "GlobalStaticMaker.h"

#include <clang/Tooling/Transformer/Stencil.h>

#include "Matchers.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;

namespace dead {

namespace {
auto globalizeRule() {
  return makeRule(decl(anyOf(varDecl(hasGlobalStorage(), unless(isExtern()),
                                     unless(isStaticStorageClass())),
                             functionDecl(isDefinition(), unless(isMain()),
                                          unless(isStaticStorageClass()))),
                       isExpansionInMainFile())
                      .bind("global"),
                  insertBefore(node("global"), cat(" static ")));
}
} // namespace

void detail::RuleActionCallback::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
  for (auto &Matcher : buildMatchers(Rule))
    Finder.addDynamicMatcher(Matcher, this);
}

GlobalStaticMaker::GlobalStaticMaker(
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements)
    : Rule{globalizeRule(), FileToReplacements} {}

void GlobalStaticMaker::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
  Rule.registerMatchers(Finder);
}

void detail::RuleActionCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &Result) {
  if (Result.Context->getDiagnostics().hasErrorOccurred()) {
    llvm::errs() << "An error has occured.\n";
    return;
  }
  Expected<SmallVector<transformer::Edit, 1>> Edits =
      findSelectedCase(Result, Rule).Edits(Result);
  if (!Edits) {
    llvm::errs() << "Rewrite failed: " << llvm::toString(Edits.takeError())
                 << "\n";
    return;
  }
  auto SM = Result.SourceManager;
  for (const auto &T : *Edits) {
    assert(T.Kind == transformer::EditKind::Range);
    auto R = tooling::Replacement(*SM, T.Range, T.Replacement);
    auto &Replacements = FileToReplacements[std::string(R.getFilePath())];
    auto Err = Replacements.add(R);
    if (Err) {
      auto NewOffset = Replacements.getShiftedCodePosition(R.getOffset());
      auto NewLength =
          Replacements.getShiftedCodePosition(R.getOffset() + R.getLength()) -
          NewOffset;
      if (NewLength == R.getLength()) {
        R = Replacement(R.getFilePath(), NewOffset, NewLength,
                        R.getReplacementText());
        Replacements = Replacements.merge(tooling::Replacements(R));
      } else {
        llvm_unreachable(llvm::toString(std::move(Err)).c_str());
      }
    }
  }
}

} // namespace dead
