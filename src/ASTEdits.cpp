#include "ASTEdits.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;

namespace dead {

ASTEdit addMetadata(ASTEdit &&Edit, EditMetadataKind Kind) {
  return withMetadata(
      std::move(Edit),
      [Kind](const clang::ast_matchers::MatchFinder::MatchResult &)
          -> EditMetadataKind { return Kind; });
}

namespace {

std::string GetFilenameFromRange(const CharSourceRange &R,
                                 const SourceManager &SM) {
  const std::pair<FileID, unsigned> DecomposedLocation =
      SM.getDecomposedLoc(SM.getSpellingLoc(R.getBegin()));
  const FileEntry *Entry = SM.getFileEntryForID(DecomposedLocation.first);
  return std::string(Entry ? Entry->getName() : "");
}

} // namespace

void RuleActionEditCollector::run(
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
    const auto *Metadata = T.Metadata.hasValue()
                               ? llvm::any_cast<EditMetadataKind>(&T.Metadata)
                               : nullptr;

    auto UpdateReplacements = [&](llvm::StringRef Text) {
      Replacements.emplace_back(*SM, T.Range, Text);
    };

    if (!Metadata) {
      UpdateReplacements(T.Replacement);
      continue;
    }

    auto GetMarkerN = [&]() -> int & {
      return FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)];
    };

    switch (*Metadata) {
    case EditMetadataKind::MarkerCall: {
      auto N = GetMarkerN()++;
      UpdateReplacements(T.Replacement + "\n\nDCEMARKERMACRO" +
                         std::to_string(N) + "_\n\n");
      break;
    }
    case EditMetadataKind::NewElseBranch: {
      auto N = GetMarkerN()++;
      UpdateReplacements(T.Replacement + std::string{"\n\n"} +
                         " else {\nDCEMARKERMACRO" + std::to_string(N) +
                         "_\n}" + +"\n\n");
      break;
    }
    default:
      llvm_unreachable("dead::detail::RuleActionEditCollector::run: "
                       "Unknown EditMetadataKind");
    };
  }
}

void RuleActionEditCollector::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
  for (auto &Matcher : buildMatchers(Rule))
    Finder.addDynamicMatcher(
        Matcher.withTraversalKind(TK_IgnoreUnlessSpelledInSource), this);
}

} // namespace dead
