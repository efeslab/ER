//===-- CoreStats.cpp -----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CoreStats.h"

using namespace klee;

Statistic stats::allocations("Allocations", "Alloc");
Statistic stats::coveredInstructions("CoveredInstructions", "Icov");
Statistic stats::falseBranches("FalseBranches", "Bf");
Statistic stats::forks("Forks", "Forks");
Statistic stats::instructionRealTime("InstructionRealTimes", "Ireal");
Statistic stats::instructionTime("InstructionTimes", "Itime");
Statistic stats::instructions("Instructions", "I");
Statistic stats::minDistToReturn("MinDistToReturn", "Rdist");
Statistic stats::minDistToUncovered("MinDistToUncovered", "UCdist");
Statistic stats::reachableUncovered("ReachableUncovered", "IuncovReach");
Statistic stats::resolveTime("ResolveTime", "RtimeC");
Statistic stats::resolveTimeCheapGetValue("ResolveTime-Cheap-GetValue", "RtimeC");
Statistic stats::resolveTimeCheapLookup("ResolveTime-Cheap-Lookup", "RtimeC");
Statistic stats::resolveTimeSearch("ResolveTime-Search", "RtimeS");
Statistic stats::solverTime("SolverTime", "Stime");
Statistic stats::states("States", "States");
Statistic stats::trueBranches("TrueBranches", "Bt");
Statistic stats::uncoveredInstructions("UncoveredInstructions", "Iuncov");

// HASE related statistics
// ** internal function
Statistic stats::dummy1("************************* internal function ***********************", "dummy1");
Statistic stats::forkTime("internal_forkTime", "Ftime");
Statistic stats::branchTime("internal_branchTime", "FBTime");
Statistic stats::executeAllocTime("internal_executeAllocTime", "eATime");
Statistic stats::executeMemopTimeS1("internal_executeMemopTimeS1", "memopTimeS1");
Statistic stats::executeMemopOOBCheck("internal_executeMemopOOBCheck", "memopOOBC");
Statistic stats::executeMemopTimeInBounds("internal_executeMemopTimeInBounds", "memopTimeInBounds");
Statistic stats::executeMemopTimeErrHandl("internal_executeMemopTimeErrHandl", "memopTimeErrHandl");
// ** llvm ir instruction
Statistic stats::dummy2("************************* llvm ir instruction *********************", "dummy2");
Statistic stats::switchTime("SwitchTime", "SwitchTime");
Statistic stats::indirectBrTime("indirectBrTime", "indibrTime");
Statistic stats::brTime("BrTime", "BrTime");
Statistic stats::callTime("CallTime", "CTime");
Statistic stats::allocaTime("allocTime", "allocTime");
Statistic stats::concreteBr("ConcreteBr", "CBr");
Statistic stats::concreteIndirectBr("ConcreteIndirectBr", "CIBr");
Statistic stats::concreteSwitch("ConcreteSwitch", "CSwitch");
Statistic stats::concreteSelect("ConcreteSelect", "CSelect");
Statistic stats::concreteCall("ConcreteCall", "CCall");
Statistic stats::symbolicBr("SymbolicBr", "SBr");
Statistic stats::symbolicIndirectBr("SymbolicIndirectBr", "SIBr");
Statistic stats::symbolicSwitch("SymbolicSwitch", "SSwitch");
Statistic stats::symbolicSelect("SymbolicSelect", "SSelect");
Statistic stats::symbolicCall("SymbolicCall", "SCall");
Statistic stats::dataRecLoadedEffective("DataLoadedEffective", "DataLoadedEffective");

Statistic stats::instMain("InstMain", "IM");
Statistic stats::instLibc("InstLibc", "IL");
Statistic stats::instPosix("InstPosix", "IP");
Statistic stats::instInit("InstInit", "II");

Statistic stats::dummy3("******************************** hase end *************************", "dummy3");

