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
#include <limits>
#include <llvm/ADT/Any.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Error.h>
#include <memory>
#include <sstream>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang::transformer;
using namespace clang::transformer::detail;

namespace dead {

namespace {
enum class EditMetadataKind {
    MarkerDecl,
    MarkerCall,
    MacroDisableBlockBegin,
    MacroDisableBlockBeginPre,
    IfPrologue,
    IfAtMostOneDefined,
    IfAtLeastOneDefined,
    IfAtLeastOneDefinedElse,
    NewElseBranch
};
std::string GetFilenameFromRange(const CharSourceRange &R,
                                 const SourceManager &SM) {
    const std::pair<FileID, unsigned> DecomposedLocation =
        SM.getDecomposedLoc(SM.getSpellingLoc(R.getBegin()));
    const FileEntry *Entry = SM.getFileEntryForID(DecomposedLocation.first);
    return std::string(Entry ? Entry->getName() : "");
}
} // namespace

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
        const auto *Metadata =
            T.Metadata.hasValue()
                ? llvm::any_cast<EditMetadataKind>(&T.Metadata)
                : nullptr;
        if (!Metadata) {
            Replacements.emplace_back(*SM, T.Range, T.Replacement);
            continue;
        }
        if (*Metadata == EditMetadataKind::MarkerCall) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)]++;
            Replacements.emplace_back(*SM, T.Range,
                                      T.Replacement + "DCEMarker" +
                                          std::to_string(N) + "_();");
        } else if (*Metadata == EditMetadataKind::MacroDisableBlockBegin) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)] - 1;
            Replacements.emplace_back(*SM, T.Range,
                                      T.Replacement +
                                          "#ifndef DeleteDCEMarkerBlock" +
                                          std::to_string(N) + "_\n\n");
        } else if (*Metadata == EditMetadataKind::MacroDisableBlockBeginPre) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)] - 1;
            Replacements.emplace_back(*SM, T.Range,
                                      "#ifndef DeleteDCEMarkerBlock" +
                                          std::to_string(N) + "_\n\n" +
                                          T.Replacement);
        } else if (*Metadata == EditMetadataKind::IfPrologue) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)];
            auto text =
                "#if !defined(DeleteDCEMarkerBlock" + std::to_string(N) +
                "_) || "
                "!defined(DeleteDCEMarkerBlock" +
                std::to_string(N + 1) + "_)\n" +
                "\n#if !defined(DeleteDCEMarkerBlock" + std::to_string(N) +
                "_) && "
                "!defined(DeleteDCEMarkerBlock" +
                std::to_string(N + 1) + "_)\n\n" + T.Replacement;
            Replacements.emplace_back(*SM, T.Range, text);
        } else if (*Metadata == EditMetadataKind::IfAtMostOneDefined) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)];
            Replacements.emplace_back(*SM, T.Range,
                                      T.Replacement +
                                          "#if !defined(DeleteDCEMarkerBlock" +
                                          std::to_string(N) +
                                          "_) || "
                                          "!defined(DeleteDCEMarkerBlock" +
                                          std::to_string(N + 1) + "_)\n");
        } else if (*Metadata == EditMetadataKind::IfAtLeastOneDefined) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)];
            Replacements.emplace_back(
                *SM, T.Range,
                T.Replacement + "\n#if !defined(DeleteDCEMarkerBlock" +
                    std::to_string(N) +
                    "_) && "
                    "!defined(DeleteDCEMarkerBlock" +
                    std::to_string(N + 1) + "_)\n\n");

        } else if (*Metadata == EditMetadataKind::IfAtLeastOneDefinedElse) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)];
            Replacements.emplace_back(
                *SM, T.Range,
                T.Replacement + "\n#if !defined(DeleteDCEMarkerBlock" +
                    std::to_string(N - 2) +
                    "_) && "
                    "!defined(DeleteDCEMarkerBlock" +
                    std::to_string(N - 1) + "_)\n\n");

        } else if (*Metadata == EditMetadataKind::NewElseBranch) {
            auto N =
                FileToNumberMarkerDecls[GetFilenameFromRange(T.Range, *SM)]++;
            Replacements.emplace_back(
                *SM, T.Range,
                T.Replacement + "#if !defined(DeleteDCEMarkerBlock" +
                    std::to_string(N) +
                    "_) && "
                    "!defined(DeleteDCEMarkerBlock" +
                    std::to_string(N + 1) + "_) \n\n else {\nDCEMarker" +
                    std::to_string(N) + "_();\n}\n#endif\n");
        } else {
            llvm_unreachable("dead::detail::RuleActionEditCollector::run: "
                             "Unknown EditMetadataKind");
        }
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

ASTEdit addMarker(ASTEdit Edit) {
    return addMetadata(std::move(Edit), EditMetadataKind::MarkerCall);
}

ASTEdit addDeleteMacro(ASTEdit Edit) {
    return addMetadata(std::move(Edit),
                       EditMetadataKind::MacroDisableBlockBegin);
}
ASTEdit addDeleteMacroPre(ASTEdit Edit) {
    return addMetadata(std::move(Edit),
                       EditMetadataKind::MacroDisableBlockBeginPre);
}

ASTEdit addElseBranch(ASTEdit Edit) {
    return addMetadata(std::move(Edit), EditMetadataKind::NewElseBranch);
}

ASTEdit addIfAtMostOneDefined(ASTEdit Edit) {
    return addMetadata(std::move(Edit), EditMetadataKind::IfAtMostOneDefined);
}

ASTEdit addIfAtLeastOneDefined(ASTEdit Edit) {
    return addMetadata(std::move(Edit), EditMetadataKind::IfAtLeastOneDefined);
}

ASTEdit addIfAtLeastOneDefinedElse(ASTEdit Edit) {
    return addMetadata(std::move(Edit),
                       EditMetadataKind::IfAtLeastOneDefinedElse);
}

ASTEdit addIfPrologue(ASTEdit Edit) {
    return addMetadata(std::move(Edit), EditMetadataKind::IfPrologue);
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
                                                    ASTContext &Context) {
    auto &SM = Context.getSourceManager();
    auto Range = CharSourceRange::getTokenRange(Node.getSourceRange());
    Range = SM.getExpansionRange(Range);
    Range.setEnd(handleReturnStmts(Node, Range.getEnd(), SM));
    Range = maybeExtendRange(Range, tok::TokenKind::comment, Context);
    return maybeExtendRange(Range, tok::TokenKind::semi, Context);
}

RangeSelector statementWithMacrosExpanded(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        auto Range = SM.getExpansionRange(
            getExtendedRangeWithCommentsAndSemi(*Node, *Result.Context));
        return Range;
    };
}

auto inMainAndNotMacro = allOf(notInMacro(), isExpansionInMainFile());

AST_POLYMORPHIC_MATCHER(BeginNotInMacroAndInMain,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(IfStmt, SwitchStmt,
                                                        ForStmt, WhileStmt,
                                                        DoStmt,
                                                        CXXForRangeStmt)) {
    (void)Builder;
    auto EndLoc = Node.getBeginLoc();
    const auto &SM = Finder->getASTContext().getSourceManager();
    return !EndLoc.isMacroID() && SM.isInMainFile(SM.getExpansionLoc(EndLoc));
}

auto instrumentStmtAfterReturnRule() {
    // TODO: Add disabling macros
    auto matcher =
        mapAnyOf(ifStmt, switchStmt, forStmt, whileStmt, doStmt,
                 cxxForRangeStmt)
            .with(BeginNotInMacroAndInMain(), hasDescendant(returnStmt()))
            .bind("stmt_with_return_descendant");

    auto action = addMarker(insertAfter(
        statementWithMacrosExpanded("stmt_with_return_descendant"), cat("")));
    return makeRule(matcher, action);
}

RangeSelector CStmtLBrace(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(Node->get<CompoundStmt>()->getLBracLoc());
    };
}

RangeSelector CStmtRBrace(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(Node->get<CompoundStmt>()->getRBracLoc());
    };
}

auto InstrumentCStmt(std::string id, bool with_macro = false) {
    if (with_macro)
        return flattenVector(
            {edit(insertAfter(CStmtRBrace(id), cat("\n#endif\n\n"))),
             edit(addMarker(insertBefore(statements(id), cat("")))),
             edit(addDeleteMacro(insertBefore(CStmtLBrace(id), cat("\n"))))});
    return edit(addMarker(insertBefore(statements(id), cat(""))));
}

EditGenerator InstrumentNonCStmt(std::string id) {
    return flattenVector({edit(addMarker(insertBefore(
                              statementWithMacrosExpanded(id), cat("{")))),
                          edit(addDeleteMacro(insertBefore(
                              statementWithMacrosExpanded(id), cat("")))),
                          edit(insertAfter(statementWithMacrosExpanded(id),
                                           cat("}\n#endif\n\n")))});
}

auto instrumentFunction() {
    auto matcher =
        functionDecl(unless(isImplicit()),
                     hasBody(compoundStmt(inMainAndNotMacro).bind("body")));
    auto action = InstrumentCStmt("body");
    return makeRule(matcher, action);
}

AST_MATCHER(IfStmt, RParenNotInMacroAndInMain) {
    (void)Builder;
    auto RParenLoc = Node.getRParenLoc();
    const auto &SM = Finder->getASTContext().getSourceManager();
    return !RParenLoc.isMacroID() &&
           SM.isInMainFile(SM.getExpansionLoc(RParenLoc));
}

AST_MATCHER(IfStmt, ConditionNotInMacroAndInMain) {
    (void)Builder;
    auto RParenLoc = Node.getRParenLoc();
    auto LParenLoc = Node.getLParenLoc();
    auto IfLoc = Node.getIfLoc();
    const auto &SM = Finder->getASTContext().getSourceManager();

    if (RParenLoc.isMacroID() ||
        !SM.isInMainFile(SM.getExpansionLoc(RParenLoc)))
        return false;

    if (LParenLoc.isMacroID() ||
        !SM.isInMainFile(SM.getExpansionLoc(LParenLoc)))
        return false;
    return !IfLoc.isMacroID() && SM.isInMainFile(SM.getExpansionLoc(IfLoc));
}

AST_MATCHER(IfStmt, ElseNotInMacroAndInMain) {
    (void)Builder;
    auto ElseLoc = Node.getElseLoc();
    const auto &SM = Finder->getASTContext().getSourceManager();
    return !ElseLoc.isMacroID() && SM.isInMainFile(SM.getExpansionLoc(ElseLoc));
}

RangeSelector LParenLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        if (const auto *IfStmt_ = Node->get<IfStmt>()) {
            return SM.getExpansionRange(IfStmt_->getLParenLoc());
        }
        if (const auto *ForLoop_ = Node->get<ForStmt>()) {
            return SM.getExpansionRange(ForLoop_->getLParenLoc());
        }
        if (const auto *WhileLoop_ = Node->get<WhileStmt>()) {
            return SM.getExpansionRange(WhileLoop_->getLParenLoc());
        }
        llvm_unreachable("LParenLoc::invalid node kind");
    };
}

RangeSelector RParenLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        if (const auto *IfStmt_ = Node->get<IfStmt>()) {
            return SM.getExpansionRange(IfStmt_->getRParenLoc());
        }
        if (const auto *ForLoop_ = Node->get<ForStmt>()) {
            return SM.getExpansionRange(ForLoop_->getRParenLoc());
        }
        if (const auto *WhileLoop_ = Node->get<WhileStmt>()) {
            return SM.getExpansionRange(WhileLoop_->getRParenLoc());
        }
        llvm_unreachable("LParenLoc::invalid node kind");
    };
}

RangeSelector IfLParenLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(Node->get<IfStmt>()->getLParenLoc());
    };
}

RangeSelector IfRParenLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(Node->get<IfStmt>()->getRParenLoc());
    };
}

RangeSelector ElseLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(Node->get<IfStmt>()->getElseLoc());
    };
}

RangeSelector IfLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(Node->get<IfStmt>()->getIfLoc());
    };
}

auto instrumentIfStmt() {
    auto matcher =
        ifStmt(ConditionNotInMacroAndInMain(),
               optionally(hasElse(
                   anyOf(compoundStmt(inMainAndNotMacro).bind("celse"),
                         stmt(hasParent(ifStmt(ElseNotInMacroAndInMain())))
                             .bind("else")))),
               hasThen(anyOf(compoundStmt(inMainAndNotMacro).bind("cthen"),
                             stmt().bind("then"))))
            .bind("ifstmt");
    auto actions = flattenVector(
        {// ifdef magic
         edit(insertAfter(statementWithMacrosExpanded("ifstmt"),
                          cat("#endif\n"))),
         edit(insertAfter(IfLParenLoc("ifstmt"), cat("\n#endif\n"))),
         edit(addIfAtLeastOneDefined(
             insertBefore(IfRParenLoc("ifstmt"), cat("")))),
         edit(addIfPrologue(changeTo(IfLoc("ifstmt"), cat("if")))),
         // instrument else branch
         ifBound(
             "celse", InstrumentCStmt("celse", true),
             ifBound("else", InstrumentNonCStmt("else"),
                     edit(addElseBranch(insertAfter(
                         statementWithMacrosExpanded("ifstmt"), cat("")))))),
         // instrument then branch
         ifBound("cthen", InstrumentCStmt("cthen", true),
                 ifBound("then", InstrumentNonCStmt("then"), noEdits())),
         edit(insertAfter(IfRParenLoc("ifstmt"), cat("#else\n\n;\n#endif\n"))),
         ifBound(
             "celse",
             flattenVector(
                 {edit(addIfAtLeastOneDefinedElse(
                      insertBefore(ElseLoc("ifstmt"), cat("")))),
                  edit(insertBefore(statementWithMacrosExpanded("celse"),
                                    cat("\n\n#endif\n")))}),
             ifBound("else",
                     flattenVector(
                         {edit(addIfAtLeastOneDefinedElse(
                              insertBefore(ElseLoc("ifstmt"), cat("")))),
                          edit(insertBefore(statementWithMacrosExpanded("else"),
                                            cat("\n\n#endif\n\n")))}),
                     noEdits()))});
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

auto instrumentLoop() {
    auto compoundMatcher =
        mapAnyOf(forStmt, doStmt, cxxForRangeStmt)
            .with(inMainAndNotMacro,
                  hasBody(compoundStmt(inMainAndNotMacro).bind("body")));
    auto nonCompoundDoWhileMatcher =
        doStmt(DoAndWhileNotMacroAndInMain(), hasBody(stmt().bind("body")))
            .bind("dostmt");
    auto doWhileAction = {
        addMarker(insertBefore(statementWithMacrosExpanded("body"), cat(""))),
        addDeleteMacro(
            insertBefore(statementWithMacrosExpanded("body"), cat("{"))),
        insertBefore(doStmtWhile("dostmt"), cat("\n#endif\n}"))};
    auto nonCompoundLoopMatcher =
        mapAnyOf(forStmt, cxxForRangeStmt)
            .with(inMainAndNotMacro,
                  hasBody(stmt(inMainAndNotMacro).bind("body")))
            .bind("loop");
    return applyFirst(
        {makeRule(compoundMatcher, InstrumentCStmt("body", true)),
         makeRule(nonCompoundDoWhileMatcher, doWhileAction),
         makeRule(nonCompoundLoopMatcher,
                  flattenVector(
                      {edit(addDeleteMacro(insertBefore(
                           statementWithMacrosExpanded("loop"), cat("")))),
                       edit(insertAfter(LParenLoc("loop"), cat("\n#endif"))),
                       InstrumentNonCStmt("body")})

                      )});
}

RangeSelector doBegin(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(
            SourceRange(Node->get<DoStmt>()->getDoLoc()));
    };
}

auto handleDoWhile() {
    auto compoundMatcher =
        doStmt(inMainAndNotMacro,
               hasBody(compoundStmt(inMainAndNotMacro).bind("body")))
            .bind("dostmt");
    auto nonCompoundLoopMatcher =
        doStmt(DoAndWhileNotMacroAndInMain(), hasBody(stmt().bind("body")))
            .bind("dostmt");

    auto macroActions = flattenVector(
        {edit(addDeleteMacroPre(changeTo(doBegin("dostmt"), cat("do")))),
         edit(insertAfter(statementWithMacrosExpanded("dostmt"),
                          cat("\n#endif\n\n")))

        });

    auto doWhileAction = flattenVector(
        {edit(addMarker(
             insertBefore(statementWithMacrosExpanded("body"), cat("")))),
         edit(insertBefore(statementWithMacrosExpanded("body"), cat("{"))),
         edit(insertBefore(doStmtWhile("dostmt"), cat("}")))});

    return applyFirst({makeRule(compoundMatcher,
                                flattenVector({InstrumentCStmt("body", false),
                                               macroActions})),
                       makeRule(nonCompoundLoopMatcher,
                                flattenVector({doWhileAction, macroActions}))});
}

RangeSelector forBegin(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(
            SourceRange(Node->get<ForStmt>()->getForLoc()));
    };
}

auto SecondSemiForLoop(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        if (const Expr *Inc = Node->get<ForStmt>()->getInc())
            return SM.getExpansionRange(SourceRange(Inc->getBeginLoc()));

        return SM.getExpansionRange(
            SourceRange(Node->get<ForStmt>()->getRParenLoc()));
    };
}

auto handleFor() {
    auto compoundMatcher =
        forStmt(inMainAndNotMacro,
                hasBody(compoundStmt(inMainAndNotMacro).bind("body")))
            .bind("loop");
    auto nonCompoundLoopMatcher =
        forStmt(inMainAndNotMacro,
                hasBody(stmt(inMainAndNotMacro).bind("body")))
            .bind("loop");

    auto macroActions = flattenVector(
        {edit(addDeleteMacroPre(changeTo(forBegin("loop"), cat("for")))),
         edit(insertAfter(LParenLoc("loop"), cat("\n#endif\n"))),
         edit(addDeleteMacro(
             insertBefore(SecondSemiForLoop("loop"), cat("\n")))),
         edit(insertAfter(RParenLoc("loop"), cat("\n#endif\n\n")))

        });
    return applyFirst(
        {makeRule(compoundMatcher,
                  flattenVector({InstrumentCStmt("body", true), macroActions})),
         makeRule(nonCompoundLoopMatcher,
                  flattenVector({InstrumentNonCStmt("body"), macroActions}))});
}

RangeSelector whileBegin(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        return SM.getExpansionRange(
            SourceRange(Node->get<WhileStmt>()->getWhileLoc()));
    };
}

auto handleWhile() {
    auto compoundMatcher =
        whileStmt(inMainAndNotMacro,
                  hasBody(compoundStmt(inMainAndNotMacro).bind("body")))
            .bind("loop");
    auto nonCompoundLoopMatcher =
        whileStmt(inMainAndNotMacro,
                  hasBody(stmt(inMainAndNotMacro).bind("body")))
            .bind("loop");
    auto macroActions = flattenVector(
        {edit(addDeleteMacroPre(changeTo(whileBegin("loop"), cat("while")))),
         edit(insertAfter(LParenLoc("loop"), cat("\n#endif\n"))),
         edit(addDeleteMacro(insertBefore(RParenLoc("loop"), cat("\n")))),
         edit(insertAfter(RParenLoc("loop"), cat("\n#endif\n\n")))

        });
    return applyFirst(
        {makeRule(compoundMatcher,
                  flattenVector({InstrumentCStmt("body", true), macroActions})),
         makeRule(nonCompoundLoopMatcher,
                  flattenVector({InstrumentNonCStmt("body"), macroActions}))});
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

AST_MATCHER(SwitchCase, colonNotInMacroAndInMain) {
    (void)Builder;
    auto ColonLoc = Node.getColonLoc();
    const auto &SM = Finder->getASTContext().getSourceManager();
    return !ColonLoc.isMacroID() &&
           SM.isInMainFile(SM.getExpansionLoc(ColonLoc));
}

AST_MATCHER_P(SwitchCase, hasNextSwitchCase,
              ast_matchers::internal::Matcher<SwitchCase>, InnerMatcher) {
    const auto *Next = Node.getNextSwitchCase();
    return (Next != nullptr && InnerMatcher.matches(*Next, Finder, Builder));
}

auto handleSwitchCase() {
    auto matcher =
        switchCase(optionally(hasNextSwitchCase(switchCase().bind("next"))),
                   colonNotInMacroAndInMain())
            .bind("stmt");
    auto action = flattenVector(
        {edit(addMarker(insertAfter(SwitchCaseColonLoc("stmt"), cat("")))),
         edit(addDeleteMacroPre(
             insertBefore(statementWithMacrosExpanded("stmt"), cat("")))),
         ifBound("next",
                 edit(insertBefore(statementWithMacrosExpanded("stmt"),
                                   cat("\n#endif\n"))),
                 noEdits())});
    return makeRule(matcher, action);
}

RangeSelector SwitchStmtEndLoc(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        Expected<DynTypedNode> Node = getNode(Result.Nodes, ID);
        if (!Node) {
            llvm::outs() << "ERROR";
            return Node.takeError();
        }
        const auto &SM = *Result.SourceManager;
        const auto *SS = Node->get<SwitchStmt>();
        return SM.getExpansionRange(SS->getEndLoc());
    };
}

auto handleSwitch() {
    auto matcher = switchStmt(inMainAndNotMacro).bind("stmt");
    auto action = insertBefore(SwitchStmtEndLoc("stmt"), cat("\n#endif\n"));
    return makeRule(matcher, action);
}

} // namespace

Instrumenter::Instrumenter(
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements)
    : FileToReplacements{FileToReplacements},
      Rules{//{instrumentStmtAfterReturnRule(), Replacements,
            // FileToNumberMarkerDecls},
            {instrumentIfStmt(), Replacements, FileToNumberMarkerDecls},
            //{instrumentFunction(), Replacements, FileToNumberMarkerDecls},
            {handleWhile(), Replacements, FileToNumberMarkerDecls},
            {handleFor(), Replacements, FileToNumberMarkerDecls},
            {handleDoWhile(), Replacements, FileToNumberMarkerDecls},
            {handleSwitch(), Replacements, FileToNumberMarkerDecls},
            {handleSwitchCase(), Replacements, FileToNumberMarkerDecls}} {}

void Instrumenter::applyReplacements() {
    for (const auto &[File, NumberMarkerDecls] : FileToNumberMarkerDecls) {
        std::stringstream ss;
        auto gen = [i = 0]() mutable {
            return "void DCEMarker" + std::to_string(i++) + "_(void);\n";
        };
        std::generate_n(std::ostream_iterator<std::string>(ss),
                        NumberMarkerDecls, gen);
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
            auto NewLength = Replacements.getShiftedCodePosition(
                                 R.getOffset() + R.getLength()) -
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
            auto NewLength = Replacements.getShiftedCodePosition(
                                 R.getOffset() + R.getLength()) -
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
    : FileToReplacements{FileToReplacements}, Rule{globalizeRule(),
                                                   FileToReplacements} {}

void GlobalStaticMaker::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
    Rule.registerMatchers(Finder);
}

} // namespace dead
