#pragma once

#include <llvm/ADT/StringRef.h>

std::string formatCode(llvm::StringRef Code);
std::string runDCEInstrumenterOnCode(llvm::StringRef Code,
                                     bool ignore_functions_with_macros = false);
std::string runVRInstrumenterOnCode(llvm::StringRef Code,
                                    bool ignore_functions_with_macros = false);
std::string runMakeGlobalsStaticOnCode(llvm::StringRef Code);

void compare_code(const std::string &code1, const std::string &code2);
