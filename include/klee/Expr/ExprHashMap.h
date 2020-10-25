//===-- ExprHashMap.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXPRHASHMAP_H
#define KLEE_EXPRHASHMAP_H

#include "klee/Expr/Expr.h"
#include "klee/util/RefHashMap.h"

namespace klee {
  template <typename T> using ExprHashMap = RefHashMap<Expr, T>;
  typedef RefHashSet<Expr> ExprHashSet;
  typedef ExprHashSet Constraints_ty;
} // namespace klee

#endif /* KLEE_EXPRHASHMAP_H */
