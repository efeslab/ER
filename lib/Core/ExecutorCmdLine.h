#ifndef KLEE_EXECUTORCMDLINE_H
#define KLEE_EXECUTORCMDLINE_H
#include <string>
#include "llvm/Support/CommandLine.h"
namespace klee {
  extern cl::opt<std::string> OracleKTest;
}
#endif
