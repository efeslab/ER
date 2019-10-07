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

  /// Here we define the notion of Indirect Depth
  /// We define that each constraint itself has indirect depth 0.
  /// Note each constraint is an expression as well.
  /// The indirect depth of an expression's children (e and e.kids) is defined recursively:
  /// 1. If e is not a ReadExpr, then e.kids have the same indirect depth as e.
  /// 2. If e is a ReadExpr, then we detail e.kids as:
  ///      (e.index, e.updates = { un[(0.idx=0.value), (1.idx=1.value), ...] @ Array})
  ///    e.index and un[i].idx have 1 more indirect depth then e
  ///    un[i].value have the same indirect depth as e
  ///
  /// Example constraint:
  ///  Eq( 0
  ///      Add( 1
  ///            Read(
  ///              Read(1 objA ) // read index
  ///              [ Read(1 objB ) = Read(1 objC) ] // update list
  ///                @ objD // root Array
  ///            )
  ///      )
  ///  )
  /// Then we say that:
  ///   1. Eq and Add both have indirect depth 0.
  ///   2. The outermost Read has indirect depth 1.
  ///   3. Read(1 objA), Read(1 objB) have indirect depth 2.
  ///   4. Read(1 objC) has indirect depth 1.
  ///
  ///  Note that all Read will end up with some
  ///    Read(constant index, symbolic array), which is called "Last Level Reads".
  ///
  /// We are interested in the distribution of indirect depth, constant index, symbolic array of last level reads.
  class IndirectReadDepthCalculator {
  private:
    // the set of all last level reads
    std::set<ref<Expr>> lastLevelReads;
    // map Expr(by their hash) to indirect depth
    ExprHashMap<int> depthStore;
    // the maximum indirect depth across all constraints
    int maxLevel;

    // return the indirect depth of a given Expr if found
    // init not found Expr with default indirect depth -1
    int getLevel(const ref<Expr> &e);
    void putLevel(const ref<Expr> &e, int level);
    /// assert that Expr `e` occurs in a certain constaint with indirect depth `level` 
    /// \return the maximum indirect depth encountered when recursively analysing the kids of e.
    ///   Note that this is mostly for recursively get the max indirect depth.
    ///
    /// TODO: enrich this framework to analyse more statistics than just maximum.
    int assignDepth(const ref<Expr> &e, int readLevel);

  public:
    // all calculation is done in the constructor
    IndirectReadDepthCalculator(ConstraintManager &cm);
    int getMax();
    std::set<ref<Expr>>& getLastLevelReads();
    int query(const ref<Expr> &e);
  };
}

#endif
