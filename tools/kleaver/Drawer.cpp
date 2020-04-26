#include "Drawer.h"
using namespace klee::expr;

void Drawer::ensureExprDeclared(const Expr *e, const char *category) {
  if (visited_expr.find(e) == visited_expr.end()) {
    // first-time visit this kid
    // handle last level read specially
    if (const ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
      if (!RE->index.isNull() && isa<ConstantExpr>(RE->index) &&
          (RE->updates.head == nullptr)) {
        declareLastLevelRead(RE, category);
        visited_expr.insert(RE);
        return;
      }
    }
    // non-last-level-read
    declareExpr(e, category);
    expr_worklist.push_back(e);
    visited_expr.insert(e);
  }
}

void Drawer::ensureArrayDeclared(const Array *root) {
  std::pair<std::unordered_set<const Array *>::iterator, bool> insert_res =
      visited_array.insert(root);
  if (insert_res.second) {
    // first-time visit this array, need declaration
    declareArray(root);
  }
}

void Drawer::draw() {
  printHeader();
  // add each top-level query expression to drawing todo-list first
  // we need to do this first because the query expression may overlap with
  // constraints and we need to make sure query expressions are guaranteed to be
  // categorized to "Q"
  for (const ref<Expr> &e : QC.Values) {
    ensureExprDeclared(e.get(), "Q");
  }
  // add each top-level constraint to drawing todo-list
  for (const ref<Expr> &e : QC.Constraints) {
    ensureExprDeclared(e.get(), "C");
  }
  while (!expr_worklist.empty()) {
    const Expr *e = expr_worklist.back();
    expr_worklist.pop_back();
    if (const ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
      // note that last level read is guaranteed to not appear here (see
      // ensureExprDeclared) handle read index
      Expr *read_idx = RE->index.get();
      if (read_idx) {
        ensureExprDeclared(read_idx);
        drawEdge(RE, read_idx, 1.5);
      }
      // Here we handle updatelists.
      const UpdateList &ul = RE->updates;
      const Array *root = ul.root;
      const UpdateNode *head = ul.head;

      if (head) {
        // if this read has update list
        std::pair<std::unordered_set<const UpdateNode *>::iterator, bool>
            insert_res = visited_updatenodes.insert(head);
        if (insert_res.second) {
          // haven't visited this updateNode

          // sentinel is the latest update node on the update list of this Array
          //   if known, null otherwise.
          const UpdateNode *sentinel = NULL;
          auto latest_un_it = arr2latest_un.find(root);
          if (latest_un_it != arr2latest_un.end()) {
            sentinel = latest_un_it->second;
            assert(head->getSize() > sentinel->getSize() &&
                   "sentinel found is shorter than current update list, "
                   "update lists are possbily diverged");
          }
          // the start pointer here is guaranteed to be different from sentinel
          // update node is guaranteed to be declared before visit
          const UpdateNode *it = head;
          declareUpdateNode(head, root);
          while (1) {
            // when visiting an update node:
            //   1) mark it as visited
            //   2) add unvisited index and value of this update node to
            //   worklist 3) establish the edge from this update node to its
            //   index/value
            //      as well as next update node
            visited_updatenodes.insert(it);
            Expr *index = it->index.get();
            Expr *value = it->value.get();
            if (index) {
              ensureExprDeclared(index);
              drawEdge(it, index, 1.5);
            }
            if (value) {
              ensureExprDeclared(value);
              drawEdge(it, value, 1.0);
            }
            if (it->next != sentinel) {
              // I am not the last pointer to be processed
              declareUpdateNode(it->next, root);
              drawEdge(it, it->next, 1.0);
              it = it->next;
            } else {
              // no matter we are processing an entire update list or only some
              //   new updates, sentinel is guaranteed to be declared here.
              if (sentinel) {
                drawEdge(it, sentinel, 1.0);
              } else {
                // only establish edges with non-const array
                // (constant array is not a concretization dependency)
                if (root->isSymbolicArray()) {
                  ensureArrayDeclared(root);
                  drawEdge(it, root, 1.0);
                }
              }
              break;
            }
          }
          // this unvisited update node will be the latest one anyway.
          arr2latest_un[root] = head;
        } else {
          // do nothing, only sanity check
          assert(arr2latest_un.find(root) != arr2latest_un.end());
        }
        drawEdge(e, head, 1.0);
      } else {
        // no updatelist, connect current Read to symbolic root array
        if (root->isSymbolicArray()) {
          ensureArrayDeclared(root);
          drawEdge(e, root, 1.0);
        }
      }
    } else {
      for (unsigned int i = 0, N = e->getNumKids(); i < N; ++i) {
        ref<Expr> kidref = e->getKid(i);
        Expr *kid = kidref.get();
        if (kid) {
          ensureExprDeclared(kid);
          drawEdge(e, kid, 1.0);
        }
      }
    }
  }
}
