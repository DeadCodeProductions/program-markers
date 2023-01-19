#include <clang/Basic/Version.h>
#include <llvm/Config/llvm-config.h>

#if CLANG_VERSION_MAJOR < 14
#error "Only clang versions >= 14 are supported."
#endif

#if LLVM_VERSION_MAJOR != CLANG_VERSION_MAJOR
#error "Missmatch between LLVM and CLANG version"
#endif

#if LLVM_VERSION_MINOR != CLANG_VERSION_MINOR
#error "Missmatch between LLVM and CLANG version"
#endif

#if LLVM_VERSION_PATCH != CLANG_VERSION_PATCHLEVEL
#error "Missmatch between LLVM and CLANG version"
#endif
