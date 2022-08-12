#include "ValueTagger.hpp"

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/Transformer/RewriteRule.h>
#include <clang/Tooling/Transformer/Stencil.h>

#include <sstream>
#include <string>

using namespace clang;
using namespace ast_matchers;
using namespace transformer;

namespace protag {

detail::ValueTaggerCallback::ValueTaggerCallback(
    RewriteRule Rule,
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements,
    std::map<std::string, int> &FileToNumberValueTrackers)
    : Rule{Rule}, FileToReplacements{FileToReplacements},
      FileToNumberValueTrackers{FileToNumberValueTrackers} {}

void detail::ValueTaggerCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &Result) {
    if (Result.Context->getDiagnostics().hasErrorOccurred()) {
        llvm::errs() << "An error has occured.\n";
        return;
    }
    Expected<SmallVector<transformer::Edit, 1>> Edits =
        transformer::detail::findSelectedCase(Result, Rule).Edits(Result);
    if (!Edits) {
        llvm::errs() << "Rewrite failed: " << llvm::toString(Edits.takeError())
                     << "\n";
        return;
    }
    auto SM = Result.SourceManager;
    for (const auto &T : *Edits) {
        assert(T.Kind == transformer::EditKind::Range);
        auto FilePath = dead::GetFilenameFromRange(T.Range, *SM);
        auto N = FileToNumberValueTrackers[FilePath]++;
        auto R = tooling::Replacement(
            *SM, T.Range, "ValueTag" + std::to_string(N) + "_" + T.Replacement);
        auto &Replacements = FileToReplacements[FilePath];
        auto Err = Replacements.add(R);
        if (Err) {
            auto NewOffset = Replacements.getShiftedCodePosition(R.getOffset());
            auto NewLength = Replacements.getShiftedCodePosition(
                                 R.getOffset() + R.getLength()) -
                             NewOffset;
            if (NewLength == R.getLength()) {
                R = clang::tooling::Replacement(R.getFilePath(), NewOffset,
                                                NewLength,
                                                R.getReplacementText());
                Replacements = Replacements.merge(tooling::Replacements(R));
            } else {
                llvm_unreachable(llvm::toString(std::move(Err)).c_str());
            }
        }
    }
}

void detail::ValueTaggerCallback::registerMatchers(
    clang::ast_matchers::MatchFinder &Finder) {
    for (auto &Matcher : transformer::detail::buildMatchers(Rule))
        Finder.addDynamicMatcher(
            Matcher.withTraversalKind(clang::TK_IgnoreUnlessSpelledInSource),
            this);
}

namespace {

auto handleLocalVars() {
    auto matcher = expr(
        declRefExpr(to(varDecl(hasType(asString("int")),
                               anyOf(hasLocalStorage(), hasGlobalStorage()))))
            .bind("varref"),
        hasAncestor(compoundStmt()),
        unless(hasParent(memberExpr())), // hack to avoid member expressions
        unless(hasAncestor(binaryOperator(isAssignmentOperator(),
                                          hasLHS(equalsBoundNode("varref"))))),
        unless(hasAncestor(unaryOperator(hasOperatorName("&")))),
        unless(hasParent(
            unaryOperator(hasAnyOperatorName("++", "--"),
                          hasUnaryOperand(equalsBoundNode("varref"))))));
    return makeRule(matcher,
                    changeTo(node("varref"), cat("(", node("varref"), ")")));
}

} // namespace

ValueTagger::ValueTagger(
    std::map<std::string, clang::tooling::Replacements> &FileToReplacements)
    : FileToReplacements{FileToReplacements},
      Callbacks{detail::ValueTaggerCallback{
          handleLocalVars(), FileToReplacements, FileToNumberValueTrackers}} {}

void ValueTagger::registerMatchers(clang::ast_matchers::MatchFinder &Finder) {
    for (auto &Callback : Callbacks)
        Callback.registerMatchers(Finder);
}

void ValueTagger::emitTagDefinitions() {
    for (const auto &[File, NTrackers] : FileToNumberValueTrackers) {
        std::stringstream ss;
        auto gen = [i = 0]() mutable {
            auto N = i++;
            return "int ValueTag" + std::to_string(N) +
                   "_(int v){\nprintf(\"ValueTag" + std::to_string(N) +
                   "_%d\",v);\nreturn v;\n}";
        };
        std::generate_n(std::ostream_iterator<std::string>(ss), NTrackers, gen);
        auto Decls = "#include <stdio.h>\n\n" + ss.str();
        auto R = clang::tooling::Replacement(File, 0, 0, Decls);
        if (auto Err = FileToReplacements[File].add(R))
            llvm_unreachable(llvm::toString(std::move(Err)).c_str());
    }
}

} // namespace protag
