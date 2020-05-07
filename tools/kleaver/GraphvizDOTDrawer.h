#ifndef KLEE_GRAPHVIZDOTDRAWER_H
#define KLEE_GRAPHVIZDOTDRAWER_H
#include "Drawer.h"

using namespace klee;

class GraphvizDOTDrawer : public Drawer {
protected:
  std::ostream &os;
  virtual void printHeader() override;
  virtual void printFooter() override;
  virtual void declareExpr(const Expr *e, const char *category) override;
  virtual void declareLastLevelRead(const ReadExpr *RE,
                                    const char *category) override;
  virtual void declareUpdateNode(const UpdateNode *un, const Array *root,
                                 const char *category = "N") override;
  virtual void declareArray(const Array *arr) override;

  virtual void drawEdge(const void *from, const void *to,
                        double weight) override;

public:
  GraphvizDOTDrawer(std::ostream &_os, const klee::expr::QueryCommand &QC)
      : Drawer(QC), os(_os) {}
};
#endif // KLEE_GRAPHVIZDOTDRAWER_H
