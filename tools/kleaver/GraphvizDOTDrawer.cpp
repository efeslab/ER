#include "GraphvizDOTDrawer.h"
#include <cassert>

void GraphvizDOTDrawer::declareExpr(const Expr *e, const char *category) {
  std::string label;
  if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    label = std::to_string(CE->getZExtValue());
  } else {
    label = e->getKindStr();
  }
  os << (size_t)e << "[ label=\"" << label << "\", "
     << "Kind=" << e->getKind() << ","
     << "Width=" << e->getWidth() << ","
     << "IDep=" << IDCalc.query(e) << ","
     << "Category=" << category << ","
     << "KInst=\"" << e->getKInstUniqueID() << "\""
     << ","
     << "DbgInfo=\"" << e->getKInstDbgInfo() << "\""
     << ","
     << "IsPointer=" << e->getKInstIsPtrType() << ","
     << "Freq=" << e->getKInstLoadedFreq();
  if (const ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
    os << ", Root=" << RE->updates.root->name;
  }
   os << "];\n";
}

void GraphvizDOTDrawer::declareLastLevelRead(const ReadExpr *RE,
                                             const char *category) {
  std::string label;
  const ConstantExpr *CE = dyn_cast<ConstantExpr>(RE->index);
  label =
      RE->updates.root->name + "[" + std::to_string(CE->getZExtValue()) + "]";
  os << (size_t)RE << "[ label=\"" << label << "\", "
     << "Kind=" << RE->getKind() << ","
     << "Width=" << RE->getWidth() << ","
     << "IDep=" << IDCalc.query(RE) << ","
     << "Category=" << category << ","
     << "KInst=\"" << RE->getKInstUniqueID() << "\""
     << ","
     << "DbgInfo=\"" << RE->getKInstDbgInfo() << "\""
     << ","
     << "IsPointer=" << RE->getKInstIsPtrType() << ","
     << "Root=" << RE->updates.root->name << ","
     << "Freq=" << RE->getKInstLoadedFreq() << "];\n";
}

void GraphvizDOTDrawer::declareUpdateNode(const UpdateNode *un,
                                          const Array *root,
                                          const char *category) {
  os << (size_t)un << "[ label=\"UN\", Kind=UN , "
     << "Category=" << category << ","
     << "Width=8,"
     << "Root=" << root->name << ","
     << "IDep=" << IDCalc.query(un) << ","
     << "KInst=\"" << un->getKInstUniqueID() << "\""
     << ","
     << "DbgInfo=\"" << un->getKInstDbgInfo() << "\""
     << ","
     << "Flags=\"" << std::to_string(un->flags) << "\""
     << ","
     << "IsPointer=" << false << ","
     << "Freq=" << un->getKInstLoadedFreq() << "];\n";
}

void GraphvizDOTDrawer::declareArray(const Array *arr) {
  os << (size_t)arr << "[ label=\"" << arr->name << "\", "
     << "Kind=Array,"
     << "Size=" << arr->getSize() << ","
     << "Category=Array,"
     << "IDep=" << IDCalc.getMax() + 1 << "];\n";
}

void GraphvizDOTDrawer::drawEdge(const void *from, const void *to,
                                 double weight) {
  os << (size_t)from << " -> " << (size_t)to
     << "[weight=" << std::to_string(weight) << "]"
     << ";\n";
}

void GraphvizDOTDrawer::printHeader() { os << "digraph{\n"; }
void GraphvizDOTDrawer::printFooter() {
  os << "dummyA[label=\"dummyA\", IDep=\"-1\"];\n"
     << "dummyB[label=\"dummyB\", IDep=\"-1\"];\n"
     << "dummyA -> dummyB [weight=5.0];\n"
     << "}\n";
}
