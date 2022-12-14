#pragma once

#include <clang/ASTMatchers/ASTMatchers.h>

using namespace clang;

namespace dead {

AST_MATCHER(VarDecl, isExtern) {
  (void)Finder;
  (void)Builder;
  return Node.hasExternalStorage();
}

AST_MATCHER(Stmt, notInMacro) {
  (void)Finder;
  (void)Builder;
  return !Node.getBeginLoc().isMacroID() && !Node.getEndLoc().isMacroID();
}

AST_MATCHER(IfStmt, ConditionNotInMacroAndInMain) {
  (void)Builder;
  auto RParenLoc = Node.getRParenLoc();
  auto LParenLoc = Node.getLParenLoc();
  auto IfLoc = Node.getIfLoc();
  const auto &SM = Finder->getASTContext().getSourceManager();

  if (RParenLoc.isMacroID() || !SM.isInMainFile(SM.getExpansionLoc(RParenLoc)))
    return false;

  if (LParenLoc.isMacroID() || !SM.isInMainFile(SM.getExpansionLoc(LParenLoc)))
    return false;
  return !IfLoc.isMacroID() && SM.isInMainFile(SM.getExpansionLoc(IfLoc));
}

AST_MATCHER(IfStmt, ElseNotInMacroAndInMain) {
  (void)Builder;
  auto ElseLoc = Node.getElseLoc();
  const auto &SM = Finder->getASTContext().getSourceManager();
  return !ElseLoc.isMacroID() && SM.isInMainFile(SM.getExpansionLoc(ElseLoc));
}

AST_MATCHER(FunctionDecl, containsMacroExpansions) {
  (void)Builder;
  const auto &SM = Finder->getASTContext().getSourceManager();
  auto StartLoc = Node.getBeginLoc();
  auto EndLoc = Node.getEndLoc();
  for (unsigned I = 0, E = SM.local_sloc_entry_size(); I != E; ++I) {
    const auto &Entry = SM.getLocalSLocEntry(I);
    if (!Entry.isExpansion())
      continue;
    const auto &ExpInfo = Entry.getExpansion();
    if (ExpInfo.isMacroBodyExpansion() || ExpInfo.isMacroArgExpansion() ||
        ExpInfo.isFunctionMacroExpansion()) {
      auto Loc = Entry.getExpansion().getExpansionLocStart();
      if (StartLoc < Loc && Loc < EndLoc)
        return true;
    }
  }
  return false;
}

AST_MATCHER(DoStmt, DoAndWhileNotMacroAndInMain) {
  (void)Builder;
  const auto &SM = Finder->getASTContext().getSourceManager();
  const auto &DoLoc = Node.getDoLoc();
  const auto &WhileLoc = Node.getWhileLoc();
  return !Node.getDoLoc().isMacroID() && !Node.getWhileLoc().isMacroID() &&
         SM.isInMainFile(SM.getExpansionLoc(DoLoc)) &&
         SM.isInMainFile(SM.getExpansionLoc(WhileLoc));
}

AST_MATCHER(SwitchCase, colonAndKeywordNotInMacroAndInMain) {
  (void)Builder;
  auto ColonLoc = Node.getColonLoc();
  auto KeywordLoc = Node.getKeywordLoc();
  const auto &SM = Finder->getASTContext().getSourceManager();
  return !ColonLoc.isMacroID() && !KeywordLoc.isMacroID() &&
         SM.isInMainFile(SM.getExpansionLoc(ColonLoc)) &&
         SM.isInMainFile(SM.getExpansionLoc(KeywordLoc));
}

using namespace clang::ast_matchers;

using MatcherType0 =
    class clang::ast_matchers::internal::ArgumentAdaptingMatcherFuncAdaptor<
        clang::ast_matchers::internal::HasAncestorMatcher, clang::Decl,
        clang::ast_matchers::internal::TypeList<
            clang::Decl, clang::NestedNameSpecifierLoc, clang::Stmt,
            clang::TypeLoc, clang::Attr>>;

void setIgnoreFunctionsWithMacros(bool val);

MatcherType0 isNotInFunctionWithMacrosMatcher();

using MatcherType1 =
    class clang::ast_matchers::internal::ArgumentAdaptingMatcherFuncAdaptor<
        clang::ast_matchers::internal::HasAncestorMatcher, clang::Decl,
        clang::ast_matchers::internal::TypeList<
            clang::Decl, clang::NestedNameSpecifierLoc, clang::Stmt,
            clang::TypeLoc, clang::Attr>>;

MatcherType1 isNotInConstexprOrConstevalFunction();

using MatcherType2 =
    class clang::ast_matchers::internal::VariadicOperatorMatcher<
        clang::ast_matchers::internal::Matcher<clang::Stmt>,
        clang::ast_matchers::internal::PolymorphicMatcher<
            clang::ast_matchers::internal::matcher_isExpansionInMainFileMatcher,
            void(clang::ast_matchers::internal::TypeList<
                 clang::Decl, clang::Stmt, clang::TypeLoc>)>>;

MatcherType2 inMainAndNotMacro();

} // namespace dead
