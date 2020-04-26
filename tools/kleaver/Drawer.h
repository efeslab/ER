#ifndef KLEAVER_DRAWER_H
#define KLEAVER_DRAWER_H
#include "klee/Expr/Constraints.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/Parser/Parser.h"
#include "klee/util/ExprConcretizer.h"

#include <iostream>
#include <unordered_set>
#include <vector>

namespace klee {
class Drawer {
protected:
  // nodes are considered visisted if edges between themselves and their kids
  // are already established. node must be declared before added to worklist
  // only non-visited can be added to worklist
  //
  // visited == declared, its edges might be established, if not it is in the
  // worklist
  std::unordered_set<const Expr *> visited_expr;
  std::vector<const Expr *> expr_worklist;
  // update nodes are considered visisted if edges from current node to the
  //   earliest node on this update list have been established.
  std::unordered_set<const UpdateNode *> visited_updatenodes;
  // bookkeeping the latest update node (on the longest update list) of each
  // Array
  std::unordered_map<const Array *, const UpdateNode *> arr2latest_un;
  // Arrays are considered visited if declared.
  std::unordered_set<const Array *> visited_array;

  const klee::expr::QueryCommand &QC;
  IndirectReadDepthCalculator IDCalc;

  virtual void printHeader() {}
  virtual void printFooter() {}
  // Node Category: C for nodes from top-level constraints, Q for nodes from
  // top-level query expressions, Array for Array definiton expression,
  // N for all other nodes
  // Node Kind: defined by KQuery if a node has a definition.
  // Otherwise, UN for UpdateNodes, Array for root Array
  virtual void declareExpr(const Expr *e, const char *category) = 0;
  virtual void declareLastLevelRead(const ReadExpr *RE,
                                    const char *category) = 0;
  virtual void declareUpdateNode(const UpdateNode *un, const Array *root,
                                 const char *category = "N") = 0;
  virtual void declareArray(const Array *arr) = 0;

  virtual void drawEdge(const void *from, const void *to,
                        double weight) = 0;

  // ensure Expr *e is declared. If e has been visited, do nothing;
  // else declare it and add it to expr_worklist
  void ensureExprDeclared(const Expr *e, const char *category = "N");
  void ensureArrayDeclared(const Array *root);

public:
  Drawer(const klee::expr::QueryCommand &_QC)
      : QC(_QC), IDCalc(_QC) {}
  ~Drawer() { printFooter(); }
  // Actually start drawing, which is a pre-order traversal
  // This is designed to be called just once.
  void draw();
};
} // namespace klee
#endif // KLEAVER_DRAWER_H
