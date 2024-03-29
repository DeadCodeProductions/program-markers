#include "Matchers.h"

#include "CommandLine.h"

using namespace clang::ast_matchers;

namespace markers {

namespace {

cl::opt<bool>
    IgnoreFunctionsWithMacros("ignore-functions-with-macros",
                              cl::desc("Do not instrument code in functions "
                                       "that contain macros (default: false)."),
                              cl::init(false), cl::cat(ProgramMarkersOptions));

} // namespace

void setIgnoreFunctionsWithMacros(bool val) { IgnoreFunctionsWithMacros = val; }

MatcherType0 isNotInFunctionWithMacrosMatcher() {
  if (not IgnoreFunctionsWithMacros)
    return hasAncestor(functionDecl());
  return hasAncestor(functionDecl(unless(containsMacroExpansions())));
}

MatcherType1 isNotInConstexprOrConstevalFunction() {
  return hasAncestor(
      functionDecl(unless(isConstexpr()), unless(isConsteval())));
}

MatcherType2 inMainAndNotMacro() {
  return allOf(notInMacro(), isExpansionInMainFile());
}

} // namespace markers
