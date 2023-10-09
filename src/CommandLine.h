#pragma once

#include <llvm/Support/CommandLine.h>

namespace cl = llvm::cl;

namespace markers {

extern cl::OptionCategory ProgramMarkersOptions;
extern cl::opt<bool> NoPreprocessorDirectives;

} // namespace markers
