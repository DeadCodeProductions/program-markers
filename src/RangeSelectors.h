#pragma once

#include <clang/Tooling/Transformer/RangeSelector.h>
#include <llvm/Support/Error.h>

namespace dead {

clang::transformer::RangeSelector
statementWithMacrosExpanded(std::string ID, bool DontExpandTillSemi = false);

clang::transformer::RangeSelector doStmtWhileSelector(std::string ID);

clang::transformer::RangeSelector switchCaseColonLocSelector(std::string ID);

clang::transformer::RangeSelector variableFromDeclRef(std::string ID);

} // namespace dead
