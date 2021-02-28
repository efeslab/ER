//===-- MiscCmdLine.cpp -----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * This file groups command line options definitions and associated
 * data that are common to both KLEE and Kleaver.
 */

#include "klee/Internal/Support/MiscCmdLine.h"

using namespace llvm;

namespace klee {

cl::OptionCategory MiscCat("Miscellaneous options", "");
cl::opt<bool> WarningsOnlyToFile(
    "warnings-only-to-file", cl::init(false),
    cl::desc("All warnings will be written to warnings.txt only.  If disabled, "
             "they are also written on screen."),
    cl::cat(MiscCat));
} // namespace klee
