#ifndef _EXPRDEEPVISITOR_H_
#define _EXPRCONCRETIZER_H_

#include "klee/Common.h"
#include "klee/Config/Version.h"
#include "klee/Expr.h"
#include "klee/Constraints.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cxxabi.h>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <vector>

namespace klee {
  // FIXME: For now we just copy the implementation of ExprConcretizer.
  // We should find a more elegant way to organize the code.
  class ExprDeepVisitor : public ExprVisitor {
  private:
    std::unordered_map<const UpdateNode *, const UpdateNode *> old2new;
    std::unordered_map<std::string, UpdateList *> farthestUpdates;

  protected:
    ExprVisitor::Action visitRead(const ReadExpr &re);

  public:
    ExprDeepVisitor() {}
    void cleanUp();
  };
}

#endif
