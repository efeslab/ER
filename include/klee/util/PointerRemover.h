#ifndef KLEE_POINTERREMOVER_H
#define KLEE_POINTERREMOVER_H

#include "klee/Expr.h"
#include "klee/util/ExprVisitor.h"
#include "klee/util/ExprDeepVisitor.h"

namespace klee {
  class PointerRemover : public ExprDeepVisitor {
  protected:
    Action visitPointer(const PointerExpr &pe) {
      return Action::changeTo(pe.toConstantExpr());
    }

  public:
    PointerRemover() {}

    static ref<Expr> removePointers(ref<Expr> &e) {
      PointerRemover r;
      return r.visit(e);
    }
  };
}

#endif
