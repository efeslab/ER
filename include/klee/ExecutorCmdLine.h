#ifndef KLEE_EXECUTORCMDLINE_H
#define KLEE_EXECUTORCMDLINE_H
#include <string>
#include "llvm/Support/CommandLine.h"
namespace klee {
  extern llvm::cl::opt<std::string> OracleKTest;
  extern llvm::cl::opt<unsigned int> DumpFunctionListSuffixLen;
}
#endif
