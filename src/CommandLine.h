#pragma once

#include <llvm/Support/CommandLine.h>

namespace cl = llvm::cl;

namespace dead {

extern cl::OptionCategory DeadInstrOptions;

}
