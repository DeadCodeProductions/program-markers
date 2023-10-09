#include "CommandLine.h"

namespace markers {

cl::OptionCategory ProgramMarkersOptions("program-markers options");

cl::opt<bool> NoPreprocessorDirectives(
    "no-preprocessor-directives",
    cl::desc("Do not emit preprocessor directives for markers in the modified "
             "output. Instead the name of each inserted marker is printed in "
             "stdout."),
    cl::cat(ProgramMarkersOptions), cl::init(false));

} // namespace markers
