#ifndef KLEE_EXECUTOR_CONFIG_H
#define KLEE_EXECUTOR_CONFIG_H
#include <cstdint>
#include <llvm/ADT/StringMap.h>
// SymbolicPOSIXWhiteList whitelists posix functions which accept symbolic
// arguments.
// the uint32_t value denotes which argument(s) can be symbolic.
// e.g. the first and thrid arguments can be symbolic, then value = 0b101
typedef uint32_t whitelist_mask_t;
typedef llvm::StringMap<whitelist_mask_t> SymbolicCallWhiteList_t;
extern SymbolicCallWhiteList_t SymbolicPOSIXWhiteList;
#endif // KLEE_EXECUTOR_CONFIG_H
