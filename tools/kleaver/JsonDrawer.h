#ifndef KLEAVER_JSONDRAWER_H
#define KLEAVER_JSONDRAWER_H
#include "Drawer.h"
#include "json.hpp"
using json = nlohmann::json;
using namespace klee;

class JsonDrawer : public Drawer {
protected:
  std::ostream &os;
  json jsongraph;
  json &nodes;
  json &edges;
  virtual void declareExpr(const Expr *e, const char *category) override;
  virtual void declareLastLevelRead(const ReadExpr *RE,
                                    const char *category) override;
  virtual void declareUpdateNode(const UpdateNode *un, const Array *root,
                                 const char *category = "N") override;
  virtual void declareArray(const Array *arr) override;

  virtual void drawEdge(const void *from, const void *to,
                        double weight) override;

public:
  JsonDrawer(std::ostream &_os, const klee::expr::QueryCommand &QC)
      : Drawer(QC), os(_os), nodes(jsongraph["nodes"]),
        edges(jsongraph["edges"]) {}
  // declareXXX will only adding stuff to in-memory the json object
  // Output Json to os at the end of JsonDrawer::draw()
  void draw();
};
#endif // KLEAVER_JSONDRAWER_H
