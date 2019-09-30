#ifndef _EXPRCONCRETIZER_H_
#define _EXPRCONCRETIZER_H_

#include "OracleEvaluator.h"

#include "klee/Common.h"
#include "klee/Config/Version.h"
#include "klee/Expr.h"
#include "klee/Constraints.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/ErrorHandling.h"
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

using namespace klee;

namespace klee {
  class ArrayConcretizationEvaluator : public OracleEvaluator {
  private:
    std::set<std::pair<std::string, unsigned>> concretizedValues;
    std::unordered_map<const UpdateNode *, const UpdateNode *> old2new;
    std::unordered_map<std::string, UpdateList *> farthestUpdates;
  protected:
    ref<Expr> getInitialValue(const Array &mo, unsigned index);
    ExprVisitor::Action visitRead(const ReadExpr &re);

  public:
    ArrayConcretizationEvaluator(std::string KTestPath)
          :OracleEvaluator(KTestPath) {}
    void addConcretizedValue(std::string arrayName, unsigned index);
    ConstraintManager evaluate(ConstraintManager &cm);
  };

  class IndirectReadDepthCalculator {
  private:
    std::set<ref<Expr>> lastLevelReads;
    ExprHashMap<int> depthStore;
    int maxLevel;

    int getLevel(const ref<Expr> &e);
    void putLevel(const ref<Expr> &e, int level);
    int assignDepth(const ref<Expr> &e, int readLevel);

  public:
    IndirectReadDepthCalculator(ConstraintManager &cm);
    int getMax();
    std::set<ref<Expr>>& getLastLevelReads();
    int query(const ref<Expr> &e);
  };
}

#endif
