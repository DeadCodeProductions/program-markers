#pragma once

#include <llvm/ADT/StringRef.h>

std::string formatCode(llvm::StringRef Code);
std::string
runBranchInstrumenterOnCode(llvm::StringRef Code, bool emit_disable_macros,
                            bool ignore_functions_with_macros = false);
std::string runMakeGlobalsStaticOnCode(llvm::StringRef Code);

void compare_code(const std::string &code1, const std::string &code2);
