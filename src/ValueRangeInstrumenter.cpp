#include "ValueRangeInstrumenter.h"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <sstream>
#include <string>

#include "Matchers.h"
#include "RangeSelectors.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;

namespace markers {

ASTEdit addVRMarkerBefore(RangeSelector &&Selection, Stencil Text) {
  return addMetadata(insertBefore(std::move(Selection), std::move(Text)),
                     EditMetadataKind::VRMarker);
}

auto valueRangeRule() {
  auto matcher = stmt(
      isNotInConstexprOrConstevalFunction(), isNotInFunctionWithMacrosMatcher(),
      inMainAndNotMacro(), stmt().bind("stmt"),
      /*Restrict to statements within compounds or within case/default(s)
       * so that we don't need to worry about cases such as if(C) STMT;*/
      anyOf(hasParent(compoundStmt()), hasParent(switchCase())),
      unless(compoundStmt()),
      /* We don't want to instrument before a case/default */
      unless(switchCase()),
      hasAncestor(
          /* Find all variables declared within the surrounding function*/
          functionDecl(forEachDescendant(varDecl(
              varDecl(hasType(isInteger())).bind("var"),
              hasAncestor(
                  /* Filter for variables used in declRefExprs within the
                   * matches statement, we need some shenanigans to add a
                   * filter based on the original statement by finding it
                   * via the surrounding function*/
                  functionDecl(hasDescendant(stmt(
                      equalsBoundNode("stmt"),
                      /* We don't want variables that are declared within this
                       * statement, e.g., for(int i = 0; i < N; ++i) */
                      unless(hasDescendant(varDecl(equalsBoundNode("var")))),
                      hasDescendant(
                          declRefExpr(to(varDecl(equalsBoundNode("var"))))
                              .bind("ref")))))))))));
  return makeRule(matcher,
                  addVRMarkerBefore(statementWithMacrosExpanded("stmt"),
                                    cat(variableFromDeclRef("ref"))));
};

ValueRangeInstrumenter::ValueRangeInstrumenter(
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements)
    : FileToReplacements{FileToReplacements},
      Rules{{valueRangeRule(), Replacements, FileToNumberMarkerDecls}} {}

std::string ValueRangeInstrumenter::makeMarkerMacros(size_t MarkerID) {
  auto ID = std::to_string(MarkerID);
  auto Marker = "VRMarker" + ID + "_";
  auto MarkerMacro = "VRMARKERMACRO" + ID + "_(VAR)";
  auto Condition = "!(VRMarkerLowerBound" + ID +
                   "_ <= (VAR) && (VAR) <= VRMarkerUpperBound" + ID + "_)";

  return "//MARKER_DIRECTIVES:" + Marker +
         "\n"
         "#if defined Disable" +
         Marker + "\n" + "#define " + MarkerMacro + "\n" +
         "#elif defined Unreachable" + Marker + "\n" + "#define " +
         MarkerMacro + "\\\nif(" + Condition + ") __builtin_unreachable();\n" +
         "#else\n" + "#define " + MarkerMacro + "\\\nif(" + Condition + ") " +
         Marker + "();\n" + "void " + Marker +
         "(void);\n#endif\n#ifndef VRMarkerLowerBound" + ID +
         "_\n#define VRMarkerLowerBound" + ID +
         "_ 0\n#endif\n"
         "#ifndef VRMarkerUpperBound" +
         ID + "_\n#define VRMarkerUpperBound" + ID + "_ 0\n#endif\n";
}

void ValueRangeInstrumenter::applyReplacements() {
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

void ValueRangeInstrumenter::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
  for (auto &Rule : Rules)
    Rule.registerMatchers(Finder);
}

} // namespace markers
