#include "DeadInstrumenter.hpp"

#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersMacros.h>
#include <clang/Lex/Lexer.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Transformer/MatchConsumer.h>
#include <clang/Tooling/Transformer/RewriteRule.h>
#include <limits>
#include <llvm/ADT/DenseMap.h>
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

void detail::RuleActionCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &Result) {
    if (Result.Context->getDiagnostics().hasErrorOccurred())
        return;
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

namespace {

AST_MATCHER_P(CaseStmt, isCaseSubStmt, ast_matchers::internal::Matcher<Stmt>,
              InnerMatcher) {
    const auto *SubStmt = Node.getSubStmt();
    return (SubStmt != nullptr &&
            InnerMatcher.matches(*SubStmt, Finder, Builder));
}

AST_MATCHER_P(DefaultStmt, isDefaultSubStmt,
              ast_matchers::internal::Matcher<Stmt>, InnerMatcher) {
    const auto *SubStmt = Node.getSubStmt();
    return (SubStmt != nullptr &&
            InnerMatcher.matches(*SubStmt, Finder, Builder));
}

AST_MATCHER(VarDecl, isExtern) { return Node.hasExternalStorage(); }

auto globalizeRule() {
    return makeRule(decl(anyOf(varDecl(hasGlobalStorage(), unless(isExtern()),
                                       unless(isStaticStorageClass())),
                               functionDecl(isDefinition(), unless(isMain()),
                                            unless(isStaticStorageClass()))),
                         isExpansionInMainFile())
                        .bind("global"),
                    insertBefore(node("global"), cat(" static ")));
}

class MarkerCallGenerator : public MatchComputation<std::string> {
    static size_t MarkerCallCount;

  public:
    MarkerCallGenerator() = default;
    llvm::Error eval(const ast_matchers::MatchFinder::MatchResult &,
                     std::string *Result) const override {
        Result->append("DCEMarker");
        Result->append(std::to_string(MarkerCallCount++));
        Result->append("_();");

        return llvm::Error::success();
    }

    std::string toString() const override { return "DCEMarkerXXX_();"; }
};

size_t MarkerCallGenerator::MarkerCallCount = 0;

class MarkerDeclGenerator : public MatchComputation<std::string> {
    static size_t MarkerDeclCount;

  public:
    MarkerDeclGenerator() = default;
    llvm::Error eval(const ast_matchers::MatchFinder::MatchResult &,
                     std::string *Result) const override {
        Result->append("void DCEMarker");
        Result->append(std::to_string(MarkerDeclCount++));
        Result->append("_(void);\n");
        return llvm::Error::success();
    }

    std::string toString() const override {
        return "void DCEMarkerXXX_(void);\n";
    }
};

size_t MarkerDeclGenerator::MarkerDeclCount = 0;

Expected<DynTypedNode> getNode(const ast_matchers::BoundNodes &Nodes,
                               StringRef ID) {
    auto &NodesMap = Nodes.getMap();
    auto It = NodesMap.find(ID);
    if (It == NodesMap.end())
        return llvm::make_error<llvm::StringError>(llvm::errc::invalid_argument,
                                                   ID + "not bound");
    return It->second;
}

RangeSelector startOfFile(std::string ID) {
    return [ID](const clang::ast_matchers::MatchFinder::MatchResult &Result)
               -> Expected<CharSourceRange> {
        auto Node = getNode(Result.Nodes, ID);
        if (!Node)
            return Node.takeError();
        const auto &SM = Result.Context->getSourceManager();
        auto Start = SM.getLocForStartOfFile(
            SM.getFileID(Node->getSourceRange().getBegin()));
        return CharSourceRange(SourceRange(Start), false);
    };
}

ASTEdit addMarkerDecl(std::string ID) {
    return insertAfter(startOfFile(ID),
                       std::make_unique<MarkerDeclGenerator>());
}

auto InstrumentCStmtActions = {
    insertBefore(statements("stmts"), std::make_shared<MarkerCallGenerator>()),
    addMarkerDecl("stmts")};

auto InstrumentReturnAction = {
    insertAfter(statement("stmt_with_return_descendant"),
                std::make_shared<MarkerCallGenerator>()),
    addMarkerDecl("stmt_with_return_descendant")};

auto curlyBraceAction = {insertBefore(statement("stmt"), cat("{")),
                         insertAfter(statement("stmt"), cat("}"))};

auto curlyBraceAndInstrumentAction = {
    insertBefore(statement("stmt"),
                 cat("{", std::make_shared<MarkerCallGenerator>())),
    insertAfter(statement("stmt"), cat("}")), addMarkerDecl("stmt")};

auto canonicalizeIfThenRule(bool AlsoInstrument) {
    return makeRule(ifStmt(isExpansionInMainFile(),
                           hasThen(stmt(unless(compoundStmt())).bind("stmt"))),
                    AlsoInstrument ? curlyBraceAndInstrumentAction
                                   : curlyBraceAction);
}

auto canonicalizeIfElseRule(bool AlsoInstrument) {
    return makeRule(ifStmt(isExpansionInMainFile(),
                           hasElse(stmt(unless(compoundStmt())).bind("stmt"))),
                    AlsoInstrument ? curlyBraceAndInstrumentAction
                                   : curlyBraceAction);
}
auto canonicalizeLoopRule(bool AlsoInstrument) {
    return makeRule(
        mapAnyOf(forStmt, whileStmt, doStmt, cxxForRangeStmt)
            .with(isExpansionInMainFile(),
                  hasBody(stmt(unless(compoundStmt())).bind("stmt"))),
        AlsoInstrument ? curlyBraceAndInstrumentAction : curlyBraceAction);
}

auto canonicalizeSwitchRule(bool AlsoInstrument) {
    auto Unless = unless(anyOf(compoundStmt(), caseStmt(), defaultStmt()));
    return applyFirst(
        {makeRule(caseStmt(isExpansionInMainFile(),
                           isCaseSubStmt(stmt(Unless).bind("stmt"))),
                  AlsoInstrument ? curlyBraceAndInstrumentAction
                                 : curlyBraceAction),
         makeRule(defaultStmt(isExpansionInMainFile(),
                              isDefaultSubStmt(stmt(Unless).bind("stmt"))),
                  AlsoInstrument ? curlyBraceAndInstrumentAction
                                 : curlyBraceAction)});
}

auto instrumentCompoundIfThenRule() {
    return makeRule(
        ifStmt(isExpansionInMainFile(), hasThen(compoundStmt().bind("stmts"))),
        InstrumentCStmtActions);
}

auto instrumentCompoundIfElseRule() {
    return makeRule(
        ifStmt(isExpansionInMainFile(), hasElse(compoundStmt().bind("stmts"))),
        InstrumentCStmtActions);
}

auto instrumentStmtAfterReturnRule() {
    return makeRule(
        mapAnyOf(ifStmt, forStmt, whileStmt, doStmt, cxxForRangeStmt,
                 switchStmt)
            .with(isExpansionInMainFile(), hasDescendant(returnStmt()))
            .bind("stmt_with_return_descendant"),
        InstrumentReturnAction);
}

auto instrumentLoopRule() {
    return makeRule(mapAnyOf(forStmt, whileStmt, doStmt, cxxForRangeStmt)
                        .with(isExpansionInMainFile(),
                              hasBody(compoundStmt().bind("stmts"))),
                    InstrumentCStmtActions);
}

auto instrumentCaseSubStmtRule() {
    return makeRule(caseStmt(isCaseSubStmt(compoundStmt().bind("stmts")),
                             isExpansionInMainFile()),
                    InstrumentCStmtActions);
}

auto instrumentDefaultSubStmtRule() {
    return makeRule(defaultStmt(isDefaultSubStmt(compoundStmt().bind("stmts")),
                                isExpansionInMainFile()),
                    InstrumentCStmtActions);
}

} // namespace

DeadInstrumenter::DeadInstrumenter(
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements,
    Mode InstrumenterMode)
    : FileToReplacements{FileToReplacements} {
    if (InstrumenterMode == Mode::MakeGlobalsStaticOnly) {
        Rules.emplace_back(globalizeRule(), FileToReplacements);
        return;
    }

    auto CanonicalizeOnly = InstrumenterMode == Mode::CanonicalizeOnly;
    Rules.emplace_back(canonicalizeIfThenRule(!CanonicalizeOnly),
                       FileToReplacements);
    Rules.emplace_back(canonicalizeIfElseRule(!CanonicalizeOnly),
                       FileToReplacements);
    Rules.emplace_back(canonicalizeLoopRule(!CanonicalizeOnly),
                       FileToReplacements);
    Rules.emplace_back(canonicalizeSwitchRule(!CanonicalizeOnly),
                       FileToReplacements);

    if (CanonicalizeOnly)
        return;

    Rules.emplace_back(instrumentCompoundIfThenRule(), FileToReplacements);
    Rules.emplace_back(instrumentCompoundIfElseRule(), FileToReplacements);
    Rules.emplace_back(instrumentStmtAfterReturnRule(), FileToReplacements);
    Rules.emplace_back(instrumentLoopRule(), FileToReplacements);
    Rules.emplace_back(instrumentCaseSubStmtRule(), FileToReplacements);
    Rules.emplace_back(instrumentDefaultSubStmtRule(), FileToReplacements);
}

void DeadInstrumenter::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
    for (auto &Rule : Rules)
        Rule.registerMatchers(Finder);
}

} // namespace dead
