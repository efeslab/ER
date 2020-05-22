//===-- CoreStats.h ---------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_CORESTATS_H
#define KLEE_CORESTATS_H

#include "klee/Statistic.h"

namespace klee {
namespace stats {

  extern Statistic allocations;
  extern Statistic resolveTime;
  extern Statistic resolveTimeCheapGetValue;
  extern Statistic resolveTimeCheapLookup;
  extern Statistic resolveTimeSearch;
  extern Statistic instructions;
  extern Statistic instructionTime;
  extern Statistic instructionRealTime;
  extern Statistic coveredInstructions;
  extern Statistic uncoveredInstructions;  
  extern Statistic trueBranches;
  extern Statistic falseBranches;
  extern Statistic solverTime;

  // HASE related statistics
  // ** internal function
  extern Statistic dummy1;
  extern Statistic forkTime;
  extern Statistic branchTime;
  extern Statistic executeAllocTime;
  extern Statistic executeMemopTimeS1;
  extern Statistic executeMemopOOBCheck;
  extern Statistic executeMemopTimeInBounds;
  extern Statistic executeMemopTimeErrHandl;
  extern Statistic concreteBr;
  extern Statistic concreteIndirectBr;
  extern Statistic concreteSwitch;
  extern Statistic concreteSelect;
  extern Statistic concreteCall;
  extern Statistic symbolicBr;
  extern Statistic symbolicIndirectBr;
  extern Statistic symbolicSwitch;
  extern Statistic symbolicSelect;
  extern Statistic symbolicCall;
  // ** llvm ir instruction
  extern Statistic dummy2;
  extern Statistic switchTime;
  extern Statistic indirectBrTime;
  extern Statistic brTime;
  extern Statistic callTime;
  extern Statistic allocaTime;
  extern Statistic dummy3;
  extern Statistic dataRecLoadedEffective;

  extern Statistic instMain;
  extern Statistic instLibc;
  extern Statistic instPosix;
  extern Statistic instInit;

  /// The number of process forks.
  extern Statistic forks;

  /// Number of states, this is a "fake" statistic used by istats, it
  /// isn't normally up-to-date.
  extern Statistic states;

  /// Instruction level statistic for tracking number of reachable
  /// uncovered instructions.
  extern Statistic reachableUncovered;

  /// Instruction level statistic tracking the minimum intraprocedural
  /// distance to an uncovered instruction; this is only periodically
  /// updated.
  extern Statistic minDistToUncovered;

  /// Instruction level statistic tracking the minimum intraprocedural
  /// distance to a function return.
  extern Statistic minDistToReturn;

}
}

#endif /* KLEE_CORESTATS_H */
