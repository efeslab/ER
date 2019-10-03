#ifndef _EXPRCONCRETIZER_H_
#define _EXPRCONCRETIZER_H_

#include "klee/Common.h"
#include "klee/Config/Version.h"
#include "klee/Expr.h"
#include "klee/Constraints.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"
#include "klee/util/OracleEvaluator.h"

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

using namespace klee;

namespace klee {
  class ExprConcretizer : public OracleEvaluator {
  private:
    // concretizedInputs has the symbolic obj name and corresponding index you want to concretize.
    // The actual concrete value is hold by the ktest loaded by super class (OracleEvaluator)
    std::set<std::pair<std::string, unsigned>> concretizedInputs;
    // additionalConstraints contains the additional constraints added while performing the
    // evaluation. For example, while replacing an Expr e with a concrete value x, a constraint
    // (e == x) will be added here.
    std::vector<ref<Expr>> additionalConstraints;
    // concretizedExprs contains the Exprs which need to be concretized, as well as the concrete
    // values to replace them.
    ExprHashMap<uint64_t> concretizedExprs;
    // foundExprs contains the information whether a Expr is replaced or not.
    ExprHashMap<bool> foundExprs;
    // old2new maps update nodes referenced in original constraints to
    // new update nodes processed by this Evaluator.
    std::unordered_map<const UpdateNode *, const UpdateNode *> old2new;
    // farthestUpdates maps symbolic array names to the longest corresponding
    // update list reconstructed by this evaluator.
    std::unordered_map<std::string, UpdateList *> farthestUpdates;

    void cleanUp();
    std::vector<ref<Expr>> doEvaluate(
            const std::vector<ref<Expr>>::const_iterator ib,
            const std::vector<ref<Expr>>::const_iterator ie);

  protected:
    ref<Expr> getInitialValue(const Array &mo, unsigned index);
    ExprVisitor::Action visitRead(const ReadExpr &re);

    /* TODO test visitExpr and visitExprPost */
    ExprVisitor::Action visitExpr(const Expr &re);
    ExprVisitor::Action visitExprPost(const Expr &re);

  public:
    ExprConcretizer(std::string KTestPath)
          :OracleEvaluator(KTestPath, true) {}
    void addConcretizedInputValue(std::string arrayName, unsigned index);
    /* TODO test addConcretizedExprValue */
    void addConcretizedExprValue(ref<Expr> e, uint64_t val);
    // Try to concretize every constraints in the given constraint manager
    ConstraintManager evaluate(ConstraintManager &cm);
    std::vector<ref<Expr>> evaluate(const std::vector<ref<Expr>> &cm);
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
