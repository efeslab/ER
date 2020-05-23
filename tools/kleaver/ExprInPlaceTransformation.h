#ifndef KLEE_EXPRINPLACETRANSFORMATION_H
#define KLEE_EXPRINPLACETRANSFORMATION_H
#include "klee/Expr/Expr.h"
#include "klee/Expr/Constraints.h"
#include "klee/Expr/Parser/Parser.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
using namespace klee;

// Expr in-place transformed is no longer "hashable" and previous hash becomes
// unreliable
class ExprInPlaceTransformer {
  // the QueryCommand to simplify
  const klee::expr::QueryCommand &QC;
  // points to the simplified QC, which is dynamically allocated
  const klee::expr::QueryCommand *new_QCp;

  struct WorkListEntry {
    enum EntryType { EExpr, EUNode};
    EntryType t;
    union {
      Expr *e;
      UpdateNode *un;
    };
    WorkListEntry(Expr *_e): t(EExpr), e(_e) {}
    WorkListEntry(UpdateNode *_un): t(EUNode), un(_un) {}
    ~WorkListEntry() {}
    bool isExpr() const { return t == EExpr; }
    bool isUNode() const { return t == EUNode; }
  };

  // track visited Expr * and cache the transformation result
  std::unordered_map<Expr*, Expr*> visited_expr;
  // same as above, but for update nodes
  std::unordered_map<UpdateNode* , UpdateNode*> visited_un;

  // Each entry in this list may have two status: visited or unvisisted
  // For visited entry, it should be poped up from this worklist, then use
  // kidstack to rebuild itself. The rebuilt new Expr should be pushed to
  // kidstack.
  //
  // For unvisited entry, it should be kept in this worklist, marked as
  // visited, then push its kids to worklist. Each kid is expected to push
  // another Expr* as its replacement to kidstack.
  //
  // How to handle updatelist in ReadExpr:
  // 1. I do not use separate worklist for update nodes to avoid handling the
  // interaction between expr worklist and un worklist -- to do DFS, you may
  // have a expr->un->expr->un->... worklist.
  // 2. Instead, I will walk through the update list until meet a visited un.
  // For each unvisited un, push its index&value to expr worklist.
  // 3. When this ReadExpr become the stack top again (visited entry in the
  // worklist), all Expr* replacement should already be pushed to kidstack. Now
  // we have everything needed to rebuild the entire updatelist.
  //
  // Note:
  // 1. Only unvisited Expr* can be pushed to the worklist.
  std::vector<WorkListEntry> expr_worklist;
  // Example:
  // worklist: E0 -> E0 E1 E2 -> E0 E1 -> E0      ->
  // kidstack:    ->          -> E2'   -> E2' E1' -> E0'
  // note the order of kids
  std::vector<WorkListEntry> expr_kidstack;
  inline Expr *popKidExpr() {
    WorkListEntry &we = expr_kidstack.back();
    assert(we.isExpr());
    Expr *e = we.e;
    expr_kidstack.pop_back();
    return e;
  }
  inline UpdateNode *popKidUNode() {
    WorkListEntry &we = expr_kidstack.back();
    assert(we.isUNode());
    UpdateNode *un = we.un;
    expr_kidstack.pop_back();
    return un;
  }

  void rebuild_pop_expr();
  public:
    // \param _constraints input constraints
    // \output out_constraints will be cleared and put into transformed constraints
    ExprInPlaceTransformer(const klee::expr::QueryCommand &_QC);
    ~ExprInPlaceTransformer() { delete new_QCp; }
    // return bool: changed or not.
    void visitExpr(Expr *e);
    // this is actually a post-order traversal
    void visitDFS(Expr *e);
    // Historically, UpdateList.head is const. So we need to allocate new UpdateNode
    void visitUNode(UpdateNode *un);
    const klee::expr::QueryCommand *getNewQCptr() const { return new_QCp; }
};
#endif // KLEE_EXPRTRANSFORMATION_H
