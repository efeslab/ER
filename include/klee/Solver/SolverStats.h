//===-- SolverStats.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_SOLVERSTATS_H
#define KLEE_SOLVERSTATS_H

#include "klee/Statistic.h"

namespace klee {
namespace stats {

  extern Statistic queries;
  extern Statistic queriesInvalid;
  extern Statistic queriesValid;
  extern Statistic queryCacheHits;
  extern Statistic queryCacheMisses;
  extern Statistic queryCexCacheHits;
  extern Statistic queryCexCacheMisses;
  extern Statistic queryCexCacheSuperset;
  extern Statistic queryCexCacheSubset;
  extern Statistic queryConstructTime;
  extern Statistic queryConstructs;
  extern Statistic queryCounterexamples;
  extern Statistic independentConstraints;
  extern Statistic independentAllConstraints;
  // Solver Time related stats
  extern Statistic independentTime;
  extern Statistic cexCacheTime;
  extern Statistic queryTime;
  extern Statistic queryTimeMaxOnce;
  extern Statistic STPDenseAssignTime;
  extern Statistic Z3DenseAssignTime;
  
#ifdef KLEE_ARRAY_DEBUG
  extern Statistic arrayHashTime;
#endif

}
}

#endif /* KLEE_SOLVERSTATS_H */
