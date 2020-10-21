//===-- ExprStats.cpp -----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ExprStats.h"

using namespace klee;

// HASE related statistics
// ** internal function
Statistic stats::Exprdummy1("************************* Expr counters ***********************", "dummy1");
Statistic stats::rewriteVisitedAll("rewriteVisitedAll", "rewriteVisitedAll");
Statistic stats::rewriteVisited("rewriteVisited", "rewriteVisited");
Statistic stats::CMIndepAddCheapCnt("#CMIndepAddCheap", "#CMIndC");
Statistic stats::CMIndepAddExpensiveCnt("#CMIndepAddExpensive", "#CMIndE");

Statistic stats::Exprdummy2("************************* Expr timers ***********************", "dummy1");
Statistic stats::CMaddTime("CMaddTime", "CMaddTime");
Statistic stats::CMaddInternalTime("CMaddTimeInt", "CMaddTimeInt");
Statistic stats::CMrewrite("CMrewrite", "CMrewrite");
Statistic stats::CMupdateEqualities("CMUpdateEq", "CMUpdateEq");
Statistic stats::CMIndepAdd("CMIndepAdd", "CMIndepAdd");
Statistic stats::CMIndepAddCheapTimer("[T]CMIndepAddCheap", "[T]CMIndC");
Statistic stats::CMIndepAddExpensiveTimer("[T]CMIndepAddExpensive", "[T]CMIndE");
Statistic stats::CMIndepDel("CMIndepDel", "CMIndepDel");
Statistic stats::SimplifyExpr("SimplifyExpr", "SimpExpr");
