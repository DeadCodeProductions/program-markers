#include "DeadInstrumenter.h"

#include <sstream>

#include "Matchers.h"
#include "RangeSelectors.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;

namespace dead {

namespace {

ASTEdit addMarkerAfter(RangeSelector &&Selection, Stencil Text) {
  return addMetadata(insertAfter(std::move(Selection), std::move(Text)),
                     EditMetadataKind::MarkerCall);
}

ASTEdit addMarkerBefore(RangeSelector &&Selection, Stencil Text) {
  return addMetadata(insertBefore(std::move(Selection), std::move(Text)),
                     EditMetadataKind::MarkerCall);
}

ASTEdit addElseBranch(RangeSelector &&Selection, Stencil Text) {
  return addMetadata(insertAfter(std::move(Selection), std::move(Text)),
                     EditMetadataKind::NewElseBranch);
}

auto InstrumentCStmt(std::string id) {
  return edit(addMarkerBefore(statements(id), cat("")));
}

EditGenerator InstrumentNonCStmt(std::string id) {
  return flatten(
      addMarkerBefore(statementWithMacrosExpanded(id), cat("\n\n{\n\n")),
      insertAfter(statementWithMacrosExpanded(id), cat("\n\n}\n\n")));
}

auto handleIfStmt() {
  auto matcher =
      ifStmt(isNotInConstexprOrConstevalFunction(),
             isNotInFunctionWithMacrosMatcher(), ConditionNotInMacroAndInMain(),
             optionally(hasElse(
                 anyOf(compoundStmt(inMainAndNotMacro()).bind("celse"),
                       stmt(hasParent(ifStmt(ElseNotInMacroAndInMain())))
                           .bind("else")))),
             hasThen(anyOf(compoundStmt(inMainAndNotMacro()).bind("cthen"),
                           stmt().bind("then"))))
          .bind("ifstmt");
  auto actions = flatten(
      // instrument else branch
      ifBound(
          "celse", InstrumentCStmt("celse"),
          ifBound(
              "else", InstrumentNonCStmt("else"),
              ifBound("cthen",
                      addElseBranch(statementWithMacrosExpanded("ifstmt", true),
                                    cat("")),
                      addElseBranch(statementWithMacrosExpanded("ifstmt"),
                                    cat(""))))),
      // instrument then branch
      ifBound("cthen", InstrumentCStmt("cthen"),
              ifBound("then", InstrumentNonCStmt("then"), noEdits())));
  return makeRule(matcher, actions);
}

auto handleDoWhile() {
  auto compoundMatcher =
      doStmt(isNotInConstexprOrConstevalFunction(),
             isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
             hasBody(compoundStmt(inMainAndNotMacro()).bind("body")))
          .bind("dostmt");
  auto nonCompoundLoopMatcher =
      doStmt(isNotInConstexprOrConstevalFunction(),
             isNotInFunctionWithMacrosMatcher(), DoAndWhileNotMacroAndInMain(),
             hasBody(stmt().bind("body")))
          .bind("dostmt");

  auto doWhileAction = flatten(
      addMarkerBefore(statementWithMacrosExpanded("body"), cat("")),
      insertBefore(statementWithMacrosExpanded("body"), cat("\n\n{\n\n")),
      insertBefore(doStmtWhileSelector("dostmt"), cat("\n\n}\n\n")));

  return applyFirst({makeRule(compoundMatcher, InstrumentCStmt("body")),
                     makeRule(nonCompoundLoopMatcher, doWhileAction)});
}

auto handleFor() {
  auto compoundMatcher =
      forStmt(isNotInConstexprOrConstevalFunction(),
              isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
              hasBody(compoundStmt(inMainAndNotMacro()).bind("body")))
          .bind("loop");
  auto nonCompoundLoopMatcher =
      forStmt(isNotInConstexprOrConstevalFunction(),
              isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
              hasBody(stmt(inMainAndNotMacro()).bind("body")))
          .bind("loop");
  return applyFirst(
      {makeRule(compoundMatcher, addMarkerBefore(statements("body"), cat(""))),
       makeRule(nonCompoundLoopMatcher,
                flatten(addMarkerBefore(statementWithMacrosExpanded("body"),
                                        cat("\n\n{\n\n")),
                        insertAfter(statementWithMacrosExpanded("body"),
                                    cat("\n\n}\n\n"))))});
}

auto handleWhile() {
  auto compoundMatcher =
      whileStmt(isNotInConstexprOrConstevalFunction(),
                isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
                hasBody(compoundStmt(inMainAndNotMacro()).bind("body")))
          .bind("loop");
  auto nonCompoundLoopMatcher =
      whileStmt(isNotInConstexprOrConstevalFunction(),
                isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
                hasBody(stmt(inMainAndNotMacro()).bind("body")))
          .bind("loop");
  return applyFirst(
      {makeRule(compoundMatcher, InstrumentCStmt("body")),
       makeRule(nonCompoundLoopMatcher, InstrumentNonCStmt("body"))});
}

auto handleSwitchCase() {
  auto matcher =
      switchStmt(
          isNotInConstexprOrConstevalFunction(),
          isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
          has(compoundStmt(has(switchCase(colonAndKeywordNotInMacroAndInMain())
                                   .bind("firstcase")))),
          forEachSwitchCase(switchCase(colonAndKeywordNotInMacroAndInMain(),
                                       unless(equalsBoundNode("firstcase")))
                                .bind("case")))
          .bind("stmt");
  auto actions = addMarkerAfter(switchCaseColonLocSelector("case"), cat(""));
  return makeRule(matcher, actions);
}

auto handleSwitch() {
  auto matcher =
      switchStmt(
          isNotInConstexprOrConstevalFunction(),
          isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro(),
          has(compoundStmt(has(switchCase(colonAndKeywordNotInMacroAndInMain())
                                   .bind("firstcase")))))
          .bind("stmt");
  auto actions =
      addMarkerAfter(switchCaseColonLocSelector("firstcase"), cat(""));
  return makeRule(matcher, actions);
}

} // namespace

std::string Instrumenter::makeMarkerMacros(size_t MarkerID) {
  auto Marker = "DCEMarker" + std::to_string(MarkerID) + "_";
  return "//MARKER_DIRECTIVES:" + Marker + "\n" + "#if defined Disable" +
         Marker + "\n" + "#define DCEMARKERMACRO" + std::to_string(MarkerID) +
         "_ ;\n" + "#elif defined Unreachable" + Marker + "\n" +
         "#define DCEMARKERMACRO" + std::to_string(MarkerID) +
         "_ __builtin_unreachable();\n" + "#else\n" +
         +"#define DCEMARKERMACRO" + std::to_string(MarkerID) + "_ " + Marker +
         "();\n" + "void " + Marker + "(void);\n" + "#endif\n";
}

Instrumenter::Instrumenter(
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements)
    : FileToReplacements{FileToReplacements},
      Rules{{handleIfStmt(), Replacements, FileToNumberMarkerDecls},
            {handleWhile(), Replacements, FileToNumberMarkerDecls},
            {handleFor(), Replacements, FileToNumberMarkerDecls},
            {handleDoWhile(), Replacements, FileToNumberMarkerDecls},
            {handleSwitch(), Replacements, FileToNumberMarkerDecls},
            {handleSwitchCase(), Replacements, FileToNumberMarkerDecls}} {}

void Instrumenter::applyReplacements() {
  for (const auto &[File, NumberMarkerDecls] : FileToNumberMarkerDecls) {

    std::stringstream ss;
    auto gen = [i = 0]() mutable {
      auto m = i++;
      return makeMarkerMacros(m);
    };
    ss << "//MARKERS START\n";
    std::generate_n(std::ostream_iterator<std::string>(ss), NumberMarkerDecls,
                    gen);
    ss << "//MARKERS END\n";
    auto Decls = ss.str();
    auto R = Replacement(File, 0, 0, Decls);
    if (auto Err = FileToReplacements[File].add(R))
      llvm_unreachable(llvm::toString(std::move(Err)).c_str());
  }

  for (auto Rit = Replacements.rbegin(); Rit != Replacements.rend(); ++Rit) {
    auto &R = *Rit;
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

void Instrumenter::registerMatchers(clang::ast_matchers::MatchFinder &Finder) {
  for (auto &Rule : Rules)
    Rule.registerMatchers(Finder);
}

} // namespace dead
