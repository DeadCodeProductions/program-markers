#pragma once 

#include <llvm/ADT/StringRef.h>

std::string formatCode(llvm::StringRef Code);
std::string runBranchInstrumenterOnCode(llvm::StringRef Code);
std::string runMakeGlobalsStaticOnCode(llvm::StringRef Code);
