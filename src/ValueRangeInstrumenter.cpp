#include "ValueRangeInstrumenter.h"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <sstream>

#include "Matchers.h"
#include "RangeSelectors.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;

namespace dead {

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
              varDecl(hasType(isInteger()),
                      anyOf(hasInitializer(anything()), parmVarDecl()))
                  .bind("var"),
              hasAncestor(
                  /* Filter for variables used in declRefExprs within the
                   * matched statement, we need some shenanigans to add a
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
    : FileToReplacements{FileToReplacements}, Rules{{valueRangeRule(),
                                                     Replacements,
                                                     FileToNumberMarkerDecls}} {
}

std::string ValueRangeInstrumenter::makeMarkerMacros(size_t MarkerID) {
  auto markerString = [m = MarkerID](StringRef kind, StringRef Op) {
    return ("#if defined DisableVRMarker" + kind + std::to_string(m) + "_\n" +
            "#define VRMARKERMACRO" + kind + std::to_string(m) + "_(VAR)\n" +
            "#elif defined UnreachableVRMarker" + kind + std::to_string(m) +
            "_\n" + "#define VRMARKERMACRO" + kind + std::to_string(m) +
            "_(VAR) if((VAR) " + Op + " VRMarkerConstant" + kind +
            std::to_string(m) + "_) __builtin_unreachable();\n" + "#else\n" +
            +"#define VRMARKERMACRO" + kind + std::to_string(m) +
            "_(VAR) if((VAR) " + Op + " VRMarkerConstant" + kind +
            std::to_string(m) + "_) VRMarker" + kind + std::to_string(m) +
            "_();\n" + "void VRMarker" + kind + std::to_string(m) +
            "_(void);\n#endif\n#ifndef VRMarkerConstant" + kind +
            std::to_string(m) + "_\n#define VRMarkerConstant" + kind +
            std::to_string(m) + "_ 0\n#endif\n")
        .str();
  };
  return markerString("LE", "<=") + markerString("GE", ">=");
}

void ValueRangeInstrumenter::applyReplacements() {
  for (const auto &[File, NumberMarkerDecls] : FileToNumberMarkerDecls) {
    llvm::outs() << File << ":VRMARKERS START\n";
    std::stringstream ss;
    auto gen = [i = 0]() mutable {
      auto m = i++;
      llvm::outs() << "VRMarker" + std::to_string(m) + "_\n";
      return makeMarkerMacros(m);
    };
    std::generate_n(std::ostream_iterator<std::string>(ss), NumberMarkerDecls,
                    gen);
    auto Decls = ss.str();
    auto R = Replacement(File, 0, 0, Decls);
    if (auto Err = FileToReplacements[File].add(R))
      llvm_unreachable(llvm::toString(std::move(Err)).c_str());

    llvm::outs() << "VRMARKERS END\n";
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

} // namespace dead
