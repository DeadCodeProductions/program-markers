#include "DeadInstrumenter.hpp"

#include "clang/Tooling/Transformer/SourceCode.h"
#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersMacros.h>
#include <clang/Basic/TokenKinds.h>
#include <clang/Lex/Lexer.h>
#include <clang/Sema/Ownership.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Transformer/MatchConsumer.h>
#include <clang/Tooling/Transformer/RewriteRule.h>
#include <cstddef>
#include <limits>
#include <llvm/ADT/Any.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;
namespace cl = llvm::cl;

namespace dead {

llvm::cl::OptionCategory DeadInstrOptions("dead-instrument options");

namespace {

cl::opt<bool> IgnoreFunctionsWithMacros(
    "ignore-functions-with-macros",
    cl::desc("Do not instrument code in functions that contain macros."),
    cl::init(true), cl::cat(DeadInstrOptions));

enum class EditMetadataKind { MarkerDecl, MarkerCall, NewElseBranch };

std::string GetFilenameFromRange(const CharSourceRange &R,
                                 const SourceManager &SM) {
  const std::pair<FileID, unsigned> DecomposedLocation =
      SM.getDecomposedLoc(SM.getSpellingLoc(R.getBegin()));
  const FileEntry *Entry = SM.getFileEntryForID(DecomposedLocation.first);
  return std::string(Entry ? Entry->getName() : "");
}

} // namespace

void detail::setIgnoreFunctionsWithMacros(bool val) {
  IgnoreFunctionsWithMacros = val;
}

void detail::RuleActionEditCollector::run(
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

void detail::RuleActionEditCollector::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
  for (auto &Matcher : buildMatchers(Rule))
    Finder.addDynamicMatcher(Matcher, this);
}

namespace {

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

Expected<DynTypedNode> getNode(const ast_matchers::BoundNodes &Nodes,
                               StringRef ID) {
  auto &NodesMap = Nodes.getMap();
  auto It = NodesMap.find(ID);
  if (It == NodesMap.end())
    return llvm::make_error<llvm::StringError>(llvm::errc::invalid_argument,
                                               ID + "not bound");
  return It->second;
}

ASTEdit addMetadata(ASTEdit &&Edit, EditMetadataKind Kind) {
  return withMetadata(
      std::move(Edit),
      [Kind](const clang::ast_matchers::MatchFinder::MatchResult &)
          -> EditMetadataKind { return Kind; });
}

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

RangeSelector statementWithMacrosExpanded(std::string ID,
                                          bool DontExpendTillSemi = false) {
  return [ID, DontExpendTillSemi](
             const clang::ast_matchers::MatchFinder::MatchResult &Result)
             -> Expected<CharSourceRange> {
    Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
    if (!Node) {
      llvm::outs() << "ERROR";
      return Node.takeError();
    }
    const auto &SM = *Result.SourceManager;
    auto Range = SM.getExpansionRange(getExtendedRangeWithCommentsAndSemi(
        *Node, *Result.Context, DontExpendTillSemi));
    return Range;
  };
}

auto inMainAndNotMacro = allOf(notInMacro(), isExpansionInMainFile());

auto InstrumentCStmt(std::string id) {
  return edit(addMarkerBefore(statements(id), cat("")));
}

EditGenerator InstrumentNonCStmt(std::string id) {
  return flatten(
      addMarkerBefore(statementWithMacrosExpanded(id), cat("\n\n{\n\n")),
      insertAfter(statementWithMacrosExpanded(id), cat("\n\n}\n\n")));
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

auto isNotInFunctionWithMacrosMatcher() {
  if (not IgnoreFunctionsWithMacros)
    return hasAncestor(functionDecl());
  return hasAncestor(functionDecl(unless(containsMacroExpansions())));
}

auto handleIfStmt() {
  auto matcher =
      ifStmt(isNotInFunctionWithMacrosMatcher(), ConditionNotInMacroAndInMain(),
             optionally(hasElse(
                 anyOf(compoundStmt(inMainAndNotMacro).bind("celse"),
                       stmt(hasParent(ifStmt(ElseNotInMacroAndInMain())))
                           .bind("else")))),
             hasThen(anyOf(compoundStmt(inMainAndNotMacro).bind("cthen"),
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

RangeSelector doStmtWhile(std::string ID) {
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

AST_MATCHER(DoStmt, DoAndWhileNotMacroAndInMain) {
  (void)Builder;
  const auto &SM = Finder->getASTContext().getSourceManager();
  const auto &DoLoc = Node.getDoLoc();
  const auto &WhileLoc = Node.getWhileLoc();
  return !Node.getDoLoc().isMacroID() && !Node.getWhileLoc().isMacroID() &&
         SM.isInMainFile(SM.getExpansionLoc(DoLoc)) &&
         SM.isInMainFile(SM.getExpansionLoc(WhileLoc));
}

auto handleDoWhile() {
  auto compoundMatcher =
      doStmt(isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
             hasBody(compoundStmt(inMainAndNotMacro).bind("body")))
          .bind("dostmt");
  auto nonCompoundLoopMatcher =
      doStmt(isNotInFunctionWithMacrosMatcher(), DoAndWhileNotMacroAndInMain(),
             hasBody(stmt().bind("body")))
          .bind("dostmt");

  auto doWhileAction = flatten(
      addMarkerBefore(statementWithMacrosExpanded("body"), cat("")),
      insertBefore(statementWithMacrosExpanded("body"), cat("\n\n{\n\n")),
      insertBefore(doStmtWhile("dostmt"), cat("\n\n}\n\n")));

  return applyFirst({makeRule(compoundMatcher, InstrumentCStmt("body")),
                     makeRule(nonCompoundLoopMatcher, doWhileAction)});
}

auto handleFor() {
  auto compoundMatcher =
      forStmt(isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
              hasBody(compoundStmt(inMainAndNotMacro).bind("body")))
          .bind("loop");
  auto nonCompoundLoopMatcher =
      forStmt(isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
              hasBody(stmt(inMainAndNotMacro).bind("body")))
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
      whileStmt(isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
                hasBody(compoundStmt(inMainAndNotMacro).bind("body")))
          .bind("loop");
  auto nonCompoundLoopMatcher =
      whileStmt(isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
                hasBody(stmt(inMainAndNotMacro).bind("body")))
          .bind("loop");
  return applyFirst(
      {makeRule(compoundMatcher, InstrumentCStmt("body")),
       makeRule(nonCompoundLoopMatcher, InstrumentNonCStmt("body"))});
}

RangeSelector SwitchCaseColonLoc(std::string ID) {
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

AST_MATCHER(SwitchCase, colonAndKeywordNotInMacroAndInMain) {
  (void)Builder;
  auto ColonLoc = Node.getColonLoc();
  auto KeywordLoc = Node.getKeywordLoc();
  const auto &SM = Finder->getASTContext().getSourceManager();
  return !ColonLoc.isMacroID() && !KeywordLoc.isMacroID() &&
         SM.isInMainFile(SM.getExpansionLoc(ColonLoc)) &&
         SM.isInMainFile(SM.getExpansionLoc(KeywordLoc));
}

auto handleSwitchCase() {
  auto matcher =
      switchStmt(
          isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
          has(compoundStmt(has(switchCase(colonAndKeywordNotInMacroAndInMain())
                                   .bind("firstcase")))),
          forEachSwitchCase(switchCase(colonAndKeywordNotInMacroAndInMain(),
                                       unless(equalsBoundNode("firstcase")))
                                .bind("case")))
          .bind("stmt");
  auto actions = addMarkerAfter(SwitchCaseColonLoc("case"), cat(""));
  return makeRule(matcher, actions);
}

auto handleSwitch() {
  auto matcher =
      switchStmt(
          isNotInFunctionWithMacrosMatcher(), inMainAndNotMacro,
          has(compoundStmt(has(switchCase(colonAndKeywordNotInMacroAndInMain())
                                   .bind("firstcase")))))
          .bind("stmt");
  auto actions = addMarkerAfter(SwitchCaseColonLoc("firstcase"), cat(""));
  return makeRule(matcher, actions);
}

} // namespace

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
    llvm::outs() << File << ":MARKERS START\n";
    std::stringstream ss;
    auto gen = [i = 0]() mutable {
      auto m = i++;
      llvm::outs() << "DCEMarker" + std::to_string(m) + "_\n";
      return "#if defined DisableDCEMarker" + std::to_string(m) + "_\n" +
             "#define DCEMARKERMACRO" + std::to_string(m) + "_ ;\n" +
             "#elif defined UnreachableDCEMarker" + std::to_string(m) + "_\n" +
             "#define DCEMARKERMACRO" + std::to_string(m) +
             "_ __builtin_unreachable();\n" + "#else\n" +
             +"#define DCEMARKERMACRO" + std::to_string(m) + "_ DCEMarker" +
             std::to_string(m) + "_();\n" + "void DCEMarker" +
             std::to_string(m) + "_(void);\n" + "#endif\n";
    };
    std::generate_n(std::ostream_iterator<std::string>(ss), NumberMarkerDecls,
                    gen);
    auto Decls = ss.str();
    auto R = Replacement(File, 0, 0, Decls);
    if (auto Err = FileToReplacements[File].add(R))
      llvm_unreachable(llvm::toString(std::move(Err)).c_str());

    llvm::outs() << "MARKERS END\n";
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

} // namespace dead
