#include "JsonDrawer.h"
#include <string>

void JsonDrawer::declareExpr(const Expr *e, const char *category) {
  std::string label;
  if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    label = std::to_string(CE->getZExtValue());
  } else {
    label = e->getKindStr();
  }
  nodes[std::to_string((size_t)e)] = {
      {"label", label},
      {"Kind", e->getKind()},
      {"Width", e->getWidth()},
      {"IDep", IDCalc.query(e)},
      {"Category", category},
      {"KInst", e->getKInstUniqueID()},
      {"DbgInfo", e->getKInstDbgInfo()},
      {"IsPointer", e->getKInstIsPtrType()},
      {"Freq", e->getKInstLoadedFreq()},
  };
  if (const ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
   nodes[std::to_string((size_t)e)]["Root"] = getArrWithSize(RE->updates.root);
  }
};

void JsonDrawer::declareLastLevelRead(const ReadExpr *RE,
                                      const char *category) {
  std::string label;
  const ConstantExpr *CE = dyn_cast<ConstantExpr>(RE->index);
  label =
      RE->updates.root->name + "[" + std::to_string(CE->getZExtValue()) + "]";
  nodes[std::to_string((size_t)RE)] = {
      {"label", label},
      {"Kind", RE->getKind()},
      {"Width", RE->getWidth()},
      {"IDep", IDCalc.query(RE)},
      {"Category", category},
      {"KInst", RE->getKInstUniqueID()},
      {"DbgInfo", RE->getKInstDbgInfo()},
      {"IsPointer", RE->getKInstIsPtrType()},
      {"Freq", RE->getKInstLoadedFreq()},
      {"Root", getArrWithSize(RE->updates.root)},
  };
}

void JsonDrawer::declareUpdateNode(const UpdateNode *un, const Array *root,
                                   const char *category) {
  nodes[std::to_string((size_t)un)] = {
      {"label", "UN"},
      {"Kind", "UN"},
      {"Width", 8},
      {"Root", getArrWithSize(root)},
      {"IDep", IDCalc.query(un)},
      {"Category", category},
      {"KInst", un->getKInstUniqueID()},
      {"DbgInfo", un->getKInstDbgInfo()},
      {"IsPointer", false},
      {"Freq", un->getKInstLoadedFreq()},
  };
}

void JsonDrawer::declareArray(const Array *arr) {
  nodes[std::to_string((size_t)arr)] = {{"label", arr->name},
                                        {"Kind", "Array"},
                                        {"Size", arr->getSize()},
                                        {"IDep", IDCalc.getMax() + 1},
                                        {"Category", "Array"}};
}

void JsonDrawer::drawEdge(const void *from, const void *to, double weight) {
  edges.push_back(json{{"source", std::to_string((size_t)from)},
                       {"target", std::to_string((size_t)to)},
                       {"weight", weight}});
}

void JsonDrawer::draw() {
  Drawer::draw();
  os << jsongraph.dump(4) << std::endl;
}
