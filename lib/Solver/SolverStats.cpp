//===-- SolverStats.cpp ---------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Solver/SolverStats.h"

using namespace klee;

Statistic stats::queries("Queries", "Q");
Statistic stats::queriesInvalid("QueriesInvalid", "Qiv");
Statistic stats::queriesValid("QueriesValid", "Qv");
Statistic stats::queryCacheHits("QueryCacheHits", "QChits") ;
Statistic stats::queryCacheMisses("QueryCacheMisses", "QCmisses");
Statistic stats::queryCexCacheHits("QueryCexCacheHits", "QCexHits") ;
Statistic stats::queryCexCacheMisses("QueryCexCacheMisses", "QCexMisses");
Statistic stats::queryCexCacheSuperset("QueryCexCacheSuperset", "QCexSuper");
Statistic stats::queryCexCacheSubset("QueryCexCacheSubset", "QCexSub");
Statistic stats::queryConstructTime("QueryConstructTime", "QBtime") ;
Statistic stats::queryConstructs("QueriesConstructs", "QB");
Statistic stats::queryCounterexamples("QueriesCEX", "Qcex");
Statistic stats::independentConstraints("IndepentConstraints", "ICons");
Statistic stats::independentAllConstraints("IndependentAllConstraints", "IAllCons");
Statistic stats::independentTime("IndependentTime", "Itime");
Statistic stats::cexCacheTime("CexCacheTime", "CCtime");
Statistic stats::queryTime("QueryTime", "Qtime");
Statistic stats::queryTimeMaxOnce("QueryTimeMaxOnce", "QtimeMO");
Statistic stats::STPDenseAssignTime("STPDenseTime", "STPDenseTime");
Statistic stats::Z3DenseAssignTime("Z3DenseTime", "Z3DenseTime");

#ifdef KLEE_ARRAY_DEBUG
Statistic stats::arrayHashTime("ArrayHashTime", "AHtime");
#endif
