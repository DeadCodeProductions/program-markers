#include "RangeSelectors.h"

#include <clang/Tooling/Transformer/SourceCode.h>

using namespace clang;
using namespace clang::tooling;
using namespace clang::transformer;

namespace markers {

namespace {

template <typename T>
SourceLocation handleReturnStmts(const T &Node, SourceLocation End,
                                 const SourceManager &SM) {
  if (const auto &Ret = Node.template get<ReturnStmt>()) {
    if (Ret->getRetValue())
      return End;
    // ReturnStmts without an Expr have a broken range...
    return End.getLocWithOffset(7);
  }
  // The end loc might be pointing to a nested return...
  bool Invalid;
  const char *Char = SM.getCharacterData(End, &Invalid);
  if (Invalid)
    return End;
  auto Return = "return";
  for (int i = 0; i < 5; ++i)
    if (Char[i] != Return[i])
      return End;
  return End.getLocWithOffset(7);
}

template <typename T>
CharSourceRange getExtendedRangeWithCommentsAndSemi(const T &Node,
                                                    ASTContext &Context,
                                                    bool DontExpendTillSemi) {
  auto &SM = Context.getSourceManager();
  auto Range = CharSourceRange::getTokenRange(Node.getSourceRange());
  Range = SM.getExpansionRange(Range);
  Range.setEnd(handleReturnStmts(Node, Range.getEnd(), SM));
  Range = maybeExtendRange(Range, tok::TokenKind::comment, Context);
  if (DontExpendTillSemi)
    return Range;
  return maybeExtendRange(Range, tok::TokenKind::semi, Context);
}

Expected<DynTypedNode> getNode(const ast_matchers::BoundNodes &Nodes,
                               StringRef ID) {
  auto &NodesMap = Nodes.getMap();
  auto It = NodesMap.find(ID);
  if (It == NodesMap.end())
    return llvm::make_error<llvm::StringError>(llvm::errc::invalid_argument,
                                               ID + "not bound");
  return It->second;
}
} // namespace

RangeSelector statementWithMacrosExpanded(std::string ID,
                                          bool DontExpandTillSemi) {
  return [ID, DontExpandTillSemi](
             const clang::ast_matchers::MatchFinder::MatchResult &Result)
             -> llvm::Expected<clang::CharSourceRange> {
    llvm::Expected<clang::DynTypedNode> Node = getNode(Result.Nodes, ID);
    if (!Node) {
      llvm::outs() << "ERROR";
      return Node.takeError();
    }
    const auto &SM = *Result.SourceManager;
    auto Range = SM.getExpansionRange(getExtendedRangeWithCommentsAndSemi(
        *Node, *Result.Context, DontExpandTillSemi));
    return Range;
  };
}

RangeSelector doStmtWhileSelector(std::string ID) {
  return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
             -> Expected<CharSourceRange> {
    Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
    if (!Node) {
      llvm::outs() << "ERROR";
      return Node.takeError();
    }
    const auto &SM = *Result.SourceManager;
    return SM.getExpansionRange(
        SourceRange(Node->get<DoStmt>()->getWhileLoc()));
  };
}

RangeSelector switchCaseColonLocSelector(std::string ID) {
  return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
             -> Expected<CharSourceRange> {
    Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
    if (!Node) {
      llvm::outs() << "ERROR";
      return Node.takeError();
    }
    const auto &SM = *Result.SourceManager;
    return SM.getExpansionRange(Node->get<SwitchCase>()->getColonLoc());
  };
}

RangeSelector variableFromDeclRef(std::string ID) {
  return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
             -> Expected<CharSourceRange> {
    Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
    if (!Node) {
      llvm::outs() << "ERROR";
      return Node.takeError();
    }
    const auto &SM = *Result.SourceManager;
    return SM.getExpansionRange(Node->get<DeclRefExpr>()->getLocation());
  };
}

RangeSelector variableTypeFromVarDecl(std::string ID) {
  return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
             -> Expected<CharSourceRange> {
    Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
    if (!Node) {
      llvm::outs() << "ERROR";
      return Node.takeError();
    }
    const auto &SM = *Result.SourceManager;
    const auto *VD = Node->get<VarDecl>();
    return SM.getExpansionRange(
        VD->getTypeSourceInfo()->getTypeLoc().getSourceRange());
  };
}

} // namespace markers
