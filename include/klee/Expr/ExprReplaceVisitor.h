#ifndef KLEE_EXPR_EXPRREPLACEVISITOR_H
#define KLEE_EXPR_EXPRREPLACEVISITOR_H
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprVisitor.h"
#include "klee/util/RefHashMap.h"
namespace klee {
typedef RefHashMap<UpdateNode, ref<UpdateNode>> UNMap_ty;
/**
 * ExprReplaceVisitor can find and replace occurrence of a single expression.
 */
class ExprReplaceVisitorBase : public ExprVisitor {

protected:
  // This is for replacement deduplication in a ConstraintManager level
  // All optimized UpdateNodes are mapped to a unique UpdateNode according to
  // their **content** (including the following chain of UpdateNodes)
  UNMap_ty &replacedUN;
  // This is a replacement cache only lives inside this ExprVisitor
  UNMap_ty &visitedUN;
  // Keep all Exprs to be replaced. This is to delay their memory reclaim for
  // equivalency cache management.
  // By delay memory reclaim via controlling this buffer, you can avoid flushing
  // equivalency cache within a ExprRepalceVisitor2
  // All derived classed should maintain these two replaced buffers when they
  // change/replace any Expr.
  std::vector<ref<Expr>> replacedExprBuffer;
  // Same as above, but keep all UpdateNodes to be replaced instead
  std::vector<ref<UpdateNode>> replacedUNBuffer;

  // clear_replaced_buffer should be called after finishing one replacement
  void clear_replaced_buffer() {
    replacedExprBuffer.clear();
    replacedUNBuffer.clear();
  }

public:
  ExprReplaceVisitorBase(UNMap_ty &_replacedUN, UNMap_ty &_visitedUN)
      : ExprVisitor(true), replacedUN(_replacedUN), visitedUN(_visitedUN) {}
  ref<UpdateNode> visitUpdateNode(const ref<UpdateNode> &un) override;
  // There is a visited cache in all ExprVisitor, which caches already visited
  // Exprs and corresponding visit results.
  // However in the case of ExprReplaceVisitorBase, new replacement rules could
  // invalidate previous visited results.
  // As a result, resetVisited is added to flush all visited results when
  // replacement rules change (new equality statement added).
  void resetVisited() { visited.clear(); }

  // replace() is the entrance of performing a replacement.
  // it will delay the memory deallocation so that "equivalency cache"
  // Expr::equivs and UpdateNode::UNequivs can cache stuff longer.
  // NOTE: you should not directly call visit() anymore
  ref<Expr> replace(const ref<Expr> &e) {
    // semaphore here may not work
    // I try to delay all Expr/UpdateNode memory reclaim during this replacement
    // visit.
    // After the replacement visit finishes and equivalency caches are cleared, I
    // should manually make the delayed memory reclaim takes effect
    // (clear_replaced_buffer)
    CompareCacheSemaphoreInc();
    ref<Expr> e_ret = visit(e);
    CompareCacheSemaphoreDec();
    clear_replaced_buffer();
    return e_ret;
  }
};

class ExprReplaceVisitorSingle : public ExprReplaceVisitorBase {
private:
  ref<Expr> src, dst;

public:
  ExprReplaceVisitorSingle(UNMap_ty &_replaceUN, UNMap_ty &_visitedUN, ref<Expr> _src, ref<Expr> _dst)
      : ExprReplaceVisitorBase(_replaceUN, _visitedUN), src(_src), dst(_dst) {}

  Action visitExpr(const Expr &e) {
    if (e == *src.get()) {
      ref<Expr> e_ref = ref<Expr>(const_cast<Expr*>(&e));
      replacedExprBuffer.push_back(e_ref);
      return Action::changeTo(dst);
    } else {
      return Action::doChildren();
    }
  }

  Action visitExprPost(const Expr &e) {
    if (e == *src.get()) {
      ref<Expr> e_ref = ref<Expr>(const_cast<Expr*>(&e));
      replacedExprBuffer.push_back(e_ref);
      return Action::changeTo(dst);
    } else {
      return Action::doChildren();
    }
  }
};

/**
 * ExprReplaceVisitorMulti can find and replace multiple expressions.
 * The replacement mapping is passed in as a std::map
 */
class ExprReplaceVisitorMulti : public ExprReplaceVisitorBase {
private:
  const ExprHashMap< ref<Expr> > &replacements;

public:
  ExprReplaceVisitorMulti(UNMap_ty &_replaceUN, UNMap_ty &_visitedUN,
                      const ExprHashMap<ref<Expr>> &_replacements)
      : ExprReplaceVisitorBase(_replaceUN, _visitedUN), replacements(_replacements) {}

  Action visitExprPost(const Expr &e) {
    ref<Expr> e_ref = ref<Expr>(const_cast<Expr*>(&e));
    ExprHashMap< ref<Expr> >::const_iterator it =
      replacements.find(e_ref);
    if (it!=replacements.end()) {
      replacedExprBuffer.push_back(e_ref);
      return Action::changeTo(it->second);
    } else {
      return Action::doChildren();
    }
  }
};

} // namespace klee
#endif // KLEE_EXPR_EXPRREPLACEVISITOR_H
