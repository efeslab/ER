#ifndef KLEE_EXPR_EXPRREPLACEVISITOR_H
#define KLEE_EXPR_EXPRREPLACEVISITOR_H
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprVisitor.h"
#include "klee/util/RefHashMap.h"
namespace klee {
//typedef llvm::DenseSet<std::pair<const UpdateNode *, const UpdateNode *>>
//    UpdateNodeEquivSet;
//extern UpdateNodeEquivSet equivUN;
//extern uint64_t UNCacheHit;
//extern uint64_t UNCacheMiss;
//struct CachedUpdateNodeEqu {
//  bool operator()(const ref<UpdateNode> &a, const ref<UpdateNode> &b) const {
//    const UpdateNode *ap, *bp;
//    if (a.get() < b.get()) {
//      ap = a.get(); bp = b.get();
//    } else {
//      ap = b.get(); bp = a.get();
//    }
//    if (equivUN.count(std::make_pair(ap, bp))) {
//      UNCacheHit++;
//      return 0;
//    }
//    UNCacheMiss++;
//    if (a == b) {
//      equivUN.insert(std::make_pair(ap, bp));
//      return true;
//    } else {
//      return false;
//    }
//  }
//};
//typedef std::unordered_map<ref<UpdateNode>, ref<UpdateNode>,
//                           util::RefHash<UpdateNode>, CachedUpdateNodeEqu>
//    UNMap_ty;
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

public:
  ExprReplaceVisitorBase(UNMap_ty &_replacedUN, UNMap_ty &_visitedUN)
      : ExprVisitor(true), replacedUN(_replacedUN), visitedUN(_visitedUN) {}
  ref<UpdateNode> visitUpdateNode(const ref<UpdateNode> &un) override;
};

class ExprReplaceVisitor : public ExprReplaceVisitorBase {
private:
  ref<Expr> src, dst;

public:
  ExprReplaceVisitor(UNMap_ty &_replaceUN, UNMap_ty &_visitedUN, ref<Expr> _src, ref<Expr> _dst)
      : ExprReplaceVisitorBase(_replaceUN, _visitedUN), src(_src), dst(_dst) {}

  Action visitExpr(const Expr &e) {
    if (e == *src.get()) {
      return Action::changeTo(dst);
    } else {
      return Action::doChildren();
    }
  }

  Action visitExprPost(const Expr &e) {
    if (e == *src.get()) {
      return Action::changeTo(dst);
    } else {
      return Action::doChildren();
    }
  }
};

/**
 * ExprReplaceVisitor2 can find and replace multiple expressions.
 * The replacement mapping is passed in as a std::map
 */
class ExprReplaceVisitor2 : public ExprReplaceVisitorBase {
private:
  const ExprHashMap< ref<Expr> > &replacements;

public:
  ExprReplaceVisitor2(UNMap_ty &_replaceUN, UNMap_ty &_visitedUN,
                      const ExprHashMap<ref<Expr>> &_replacements)
      : ExprReplaceVisitorBase(_replaceUN, _visitedUN), replacements(_replacements) {}

  Action visitExprPost(const Expr &e) {
    ExprHashMap< ref<Expr> >::const_iterator it =
      replacements.find(ref<Expr>(const_cast<Expr*>(&e)));
    if (it!=replacements.end()) {
      return Action::changeTo(it->second);
    } else {
      return Action::doChildren();
    }
  }
};

} // namespace klee
#endif // KLEE_EXPR_EXPRREPLACEVISITOR_H
