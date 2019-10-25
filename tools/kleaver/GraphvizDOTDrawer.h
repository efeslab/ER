#ifndef KLEE_GRAPHVIZDOTDRAWER_H
#define KLEE_GRAPHVIZDOTDRAWER_H
#include "klee/Expr.h"

#include <iostream>
#include <unordered_set>
#include <vector>

using namespace klee;

class GraphvizDOTDrawer {
  std::ostream &os;
  // nodes are considered visisted if edges between themselves and their kids are already established.
  // node must be declared before added to worklist
  // only non-visited can be added to worklist
  //
  // visited == declared, its edges might be established, if not it is in the worklist
  std::unordered_set<const Expr *> visited_expr;
  std::vector<const Expr *> expr_worklist;
  // update nodes are considered visisted if edges from current node to the
  //   earliest node on this update list have been established.
  std::unordered_set<const UpdateNode *> visited_updatenodes;
  // bookkeeping the latest update node (on the longest update list) of each Array
  std::unordered_map<const Array *, const UpdateNode *> arr2latest_un;
  // Arrays are considered visited if declared.
  std::unordered_set<const Array *> visited_array;

  void printHeader();
  void printFooter();
  // node category: C for top-level constraint, Q for top-level query
  // E for Expr nodes, UN for UpdateNodes, Array for root Array
  void declareExpr(const Expr *e, const char *category);
  void declareLastLevelRead(const ReadExpr *RE, const char *category);
  void declareUpdateNode(const UpdateNode *un, const Array *root);
  void declareArray(const Array *arr);
  // edge category: N for normal edge, I for indirect edg
  void drawEdge(const void *from, const void *to, const char *category="N");

  // ensure Expr *e is declared. If e has been visited, do nothing;
  // else declare it and add it to expr_worklist
  void ensureExprDeclared(const Expr *e, const char *category="E");
  void ensureArrayDeclared(const Array *root);

  public:
  GraphvizDOTDrawer(std::ostream &_os): os(_os) { printHeader(); }
  ~GraphvizDOTDrawer() { printFooter(); }
  // add Expr *e as a top-level constraint to drawing todo-list
  void addConstraint(const Expr *e);
  // actually start drawing, which is a pre-order traversal
  void draw();
};
#endif // KLEE_GRAPHVIZDOTDRAWER_H
