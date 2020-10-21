//===-- ExprStats.h ---------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXPRSTATS_H
#define KLEE_EXPRSTATS_H

#include "klee/Statistic.h"

namespace klee {
namespace stats {

  // HASE related statistics
  // ** counters
  extern Statistic Exprdummy1;
  extern Statistic rewriteVisitedAll;
  extern Statistic rewriteVisited;
  extern Statistic CMIndepAddCheapCnt;
  extern Statistic CMIndepAddExpensiveCnt;
  // ** timers
  extern Statistic Exprdummy2;
  extern Statistic CMaddTime;
  extern Statistic CMaddInternalTime;
  extern Statistic CMrewrite;
  extern Statistic CMupdateEqualities;
  extern Statistic CMIndepAdd;
  extern Statistic CMIndepAddCheapTimer;
  extern Statistic CMIndepAddExpensiveTimer;
  extern Statistic CMIndepDel;
  extern Statistic SimplifyExpr;
}
}

#endif /* KLEE_EXPRSTATS_H */
