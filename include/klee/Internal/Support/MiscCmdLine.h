//===-- MiscCmdLine.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * This header groups command-line options and associated declarations
 * that are common to both KLEE and Kleaver.
 */

#ifndef KLEE_MISCCMDLINE_H
#define KLEE_MISCCMDLINE_H

#include "llvm/Support/CommandLine.h"

namespace klee {
// Misc Options for every component of klee (executor, solver, expr, debugging
// helper, etc.)
extern llvm::cl::OptionCategory MiscCat;
extern llvm::cl::opt<bool> WarningsOnlyToFile;
extern llvm::cl::opt<bool> DebugDumpKQuery;
} // namespace klee

#endif /* KLEE_SOLVERCMDLINE_H */
